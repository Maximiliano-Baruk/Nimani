#include <Arduino.h>
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
    #include <freertos/semphr.h>
    #include <ArduinoJson.h>
    #include <WiFi.h>
    #include <PubSubClient.h>
    #include <SensirionI2cSfmSf06.h>
    #include <Wire.h>
    #include "MAX30105.h"
    #include "spo2_algorithm.h"

    // ========== CONFIGURACIÓN MQTT ========== //
    const char* mqtt_server = "172.20.10.4";
    const int mqtt_port = 1883;
    const char* raw_topic = "sensor/sfm3003/raw";
    const char* summary_topic = "sensor/sfm3003/summary";
    const char* command_topic = "sensor/commands";

    // ========== ESTRUCTURA DE DATOS COMBINADOS ========== //
    typedef struct {
        // Datos de flujo (SFM3003)
        float flowRate;         // Flujo en L/s
        float temperature;      // Temperatura en °C
        float volume;           // Volumen acumulado en L
        
        // Datos de salud (MAX30102)
        int32_t heartRate;      // Ritmo cardíaco (BPM)
        int8_t validHeartRate;  // 1 si el dato es válido
        int32_t spo2;           // SpO2 (%)
        int8_t validSPO2;       // 1 si el dato es válido
        
        // Estado del ejercicio
        bool exerciseActive;
        unsigned long exerciseTimeRemaining;
        uint16_t breathCount;
        float currentBreathVolume;
        
        // Tiempo
        unsigned long timestamp;
        uint32_t sequenceNumber;  // Para seguimiento de mensajes
    } CombinedData_t;

    // ========== VARIABLES GLOBALES ========== //
    SensirionI2cSfmSf06 sfmSensor;
    MAX30105 particleSensor;
    WiFiClient espClient;
    PubSubClient mqttClient(espClient);
    SemaphoreHandle_t xDataMutex;
    CombinedData_t currentData;

    // Variables de control del ejercicio
    bool isExerciseActive = false;
    unsigned long exerciseDuration = 0;
    unsigned long exerciseStartTime = 0;
    float breathVolumes[10] = {0};  // Almacena los últimos 10 volúmenes
    int breathIndex = 0;
    bool isInhaling = false;
    const float SLM_TO_LPS = 1.0 / 60.0;  // Conversión de SLM a L/s

    // Buffers para MAX30102
    uint32_t irBuffer[100];
    uint32_t redBuffer[100];

    // ========== PROTOTIPOS DE FUNCIONES ========== //
    void connectToWiFi();
    void initializeSensors();
    void sendExerciseSummary();

    // ========== TAREA MQTT (Núcleo 1) ========== //
    void vTaskMQTT(void *pvParameters) {
        mqttClient.setServer(mqtt_server, mqtt_port);
    // En la función de callback MQTT del ESP32:
    mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
        StaticJsonDocument<200> cmdDoc;
        deserializeJson(cmdDoc, payload, length);
        
        if (cmdDoc.containsKey("duration")) {
            xSemaphoreTake(xDataMutex, portMAX_DELAY);
            exerciseDuration = cmdDoc["duration"].as<int>() * 1000; // Convertir a milisegundos
            exerciseStartTime = millis();
            isExerciseActive = true;
            
            // Resetear contadores
            currentData.volume = 0;
            currentData.breathCount = 0;
            currentData.currentBreathVolume = 0;
            memset(breathVolumes, 0, sizeof(breathVolumes));
            
            xSemaphoreGive(xDataMutex);
            
            // Publicar confirmación
            char ackMsg[50];
            snprintf(ackMsg, sizeof(ackMsg), 
                    "{\"status\":\"started\",\"duration\":%d}",
                    (int)(exerciseDuration/1000));
            mqttClient.publish("sensor/status", ackMsg);
            
            Serial.printf("Ejercicio iniciado. Duración: %d segundos\n", 
                        (int)(exerciseDuration/1000));
        }
    });

        while (1) {
            if (!mqttClient.connected()) {
                while (!mqttClient.connected()) {
                    Serial.print("[MQTT] Intentando conectar al broker... ");
                    if (mqttClient.connect("ESP32_SFM3003")) {
                        mqttClient.subscribe(command_topic);
                        Serial.println("¡Conectado y suscrito a '" + String(command_topic) + "'!");
                    } else {
                        Serial.print("Fallo (rc=");
                        Serial.print(mqttClient.state());
                        Serial.println("). Reintentando en 2s...");
                        vTaskDelay(pdMS_TO_TICKS(2000));
                    }
                }
            }
            mqttClient.loop();

            xSemaphoreTake(xDataMutex, portMAX_DELAY);
            if (isExerciseActive) {
                unsigned long elapsedTime = millis() - exerciseStartTime;
                if (elapsedTime >= exerciseDuration) {
                    isExerciseActive = false;
                    currentData.exerciseActive = false;
                    currentData.exerciseTimeRemaining = 0;
                    xSemaphoreGive(xDataMutex);
                    sendExerciseSummary();
                } else {
                    currentData.exerciseActive = true;
                    currentData.exerciseTimeRemaining = exerciseDuration - elapsedTime;
                    currentData.timestamp = millis();
                    currentData.sequenceNumber++;
                    xSemaphoreGive(xDataMutex);
                
                // Preparar mensaje JSON
                StaticJsonDocument<400> jsonDoc;
                jsonDoc["seq"] = currentData.sequenceNumber;
                jsonDoc["flow"] = currentData.flowRate;
                jsonDoc["volume"] = currentData.volume;
                jsonDoc["temp"] = currentData.temperature;
                jsonDoc["heart_rate"] = currentData.heartRate;
                jsonDoc["hr_valid"] = currentData.validHeartRate;
                jsonDoc["spo2"] = currentData.spo2;
                jsonDoc["spo2_valid"] = currentData.validSPO2;
                jsonDoc["exercise_active"] = true;
                jsonDoc["time_remaining"] = currentData.exerciseTimeRemaining;
                jsonDoc["breath_count"] = currentData.breathCount;
                jsonDoc["timestamp"] = currentData.timestamp;

                char jsonBuffer[400];
                serializeJson(jsonDoc, jsonBuffer);
                mqttClient.publish(raw_topic, jsonBuffer);

                Serial.print("[DATA] ");
                Serial.println(jsonBuffer);
                
                // Verificar si el ejercicio ha terminado
                if (currentData.exerciseTimeRemaining <= 0) {
                    isExerciseActive = false;
                    sendExerciseSummary();
                }
            }
            }else {
                xSemaphoreGive(xDataMutex);
            }
            xSemaphoreGive(xDataMutex);

            vTaskDelay(pdMS_TO_TICKS(200)); // Enviar datos cada 200ms
        }
    }

    // ========== TAREA DEL SENSOR DE FLUJO (Núcleo 0) ========== //
    void vTaskReadFlowSensor(void *pvParameters) {
        Wire.begin();
        sfmSensor.begin(Wire, 0x2A);
        sfmSensor.stopContinuousMeasurement();
        vTaskDelay(pdMS_TO_TICKS(100));

        if (sfmSensor.startO2ContinuousMeasurement() != 0) {
            Serial.println("[FLOW] Error al iniciar el sensor");
            vTaskDelete(NULL);
        }

        while (1) {
            float flow, temperature;
            uint16_t statusWord;
            if (sfmSensor.readMeasurementData(flow, temperature, statusWord) == 0) {
                flow *= SLM_TO_LPS;
                
                xSemaphoreTake(xDataMutex, portMAX_DELAY);
                currentData.flowRate = flow;
                currentData.temperature = temperature;
                
                if (isExerciseActive) {
                    if (flow > 0.1) { // Umbral para detectar inhalación
                        if (!isInhaling) {
                            isInhaling = true;
                        }
                        currentData.currentBreathVolume += flow * 0.1; // Integración del volumen
                        currentData.volume = currentData.currentBreathVolume;
                    } else if (flow < -0.1 && isInhaling) { // Umbral para detectar exhalación
                        isInhaling = false;
                        breathVolumes[breathIndex] = currentData.currentBreathVolume;
                        breathIndex = (breathIndex + 1) % 10;
                        currentData.breathCount++;
                        currentData.currentBreathVolume = 0.0;
                    }
                }
                xSemaphoreGive(xDataMutex);
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // ========== TAREA DEL SENSOR DE SALUD (Núcleo 0) ========== //
    void vTaskReadHealthSensor(void *pvParameters) {
        if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
            Serial.println("[HEALTH] Sensor no encontrado");
            vTaskDelete(NULL);
        }

        // Configuración del sensor
        particleSensor.setup(35, 8, 2, 400, 411, 4096);
        particleSensor.enableDIETEMPRDY();

        // Lectura inicial (100 muestras)
        for (byte i = 0; i < 100; i++) {
            while (!particleSensor.available())
                particleSensor.check();

            redBuffer[i] = particleSensor.getRed();
            irBuffer[i] = particleSensor.getIR();
            particleSensor.nextSample();
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Cálculo inicial
        maxim_heart_rate_and_oxygen_saturation(irBuffer, 100, redBuffer, 
                                            &currentData.spo2, &currentData.validSPO2, 
                                            &currentData.heartRate, &currentData.validHeartRate);

        while (1) {
            // Desplazar buffers (eliminar las 25 muestras más antiguas)
            for (byte i = 25; i < 100; i++) {
                redBuffer[i - 25] = redBuffer[i];
                irBuffer[i - 25] = irBuffer[i];
            }

            // Tomar 25 nuevas muestras
            for (byte i = 75; i < 100; i++) {
                while (!particleSensor.available())
                    particleSensor.check();

                redBuffer[i] = particleSensor.getRed();
                irBuffer[i] = particleSensor.getIR();
                particleSensor.nextSample();
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            // Recalcular HR y SpO2
            xSemaphoreTake(xDataMutex, portMAX_DELAY);
            maxim_heart_rate_and_oxygen_saturation(irBuffer, 100, redBuffer, 
                                                &currentData.spo2, &currentData.validSPO2, 
                                                &currentData.heartRate, &currentData.validHeartRate);
            xSemaphoreGive(xDataMutex);

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // ========== FUNCIÓN PARA ENVIAR RESUMEN ========== //
    void sendExerciseSummary() {
        float avgVolume = 0.0;
        int validBreaths = 0;
        
        for (int i = 0; i < 10; i++) {
            if (breathVolumes[i] > 0) {
                avgVolume += breathVolumes[i];
                validBreaths++;
            }
        }
        
        if (validBreaths > 0) {
            avgVolume /= validBreaths;
        }

        StaticJsonDocument<200> summaryDoc;
        summaryDoc["avg_volume"] = avgVolume;
        summaryDoc["total_breaths"] = currentData.breathCount;
        summaryDoc["duration"] = exerciseDuration / 1000;
        summaryDoc["final_heart_rate"] = currentData.heartRate;
        summaryDoc["final_spo2"] = currentData.spo2;

        char jsonBuffer[200];
        serializeJson(summaryDoc, jsonBuffer);
        mqttClient.publish(summary_topic, jsonBuffer);

        Serial.println("Ejercicio finalizado. Resumen enviado:");
        Serial.println(jsonBuffer);
    }

    // ========== SETUP ========== //
    void setup() {
        Serial.begin(115200);
        delay(1000);
        Serial.println("Iniciando sistema...");

        // Inicializar semáforo para acceso seguro a datos
        xDataMutex = xSemaphoreCreateMutex();
        memset(&currentData, 0, sizeof(currentData));
        currentData.sequenceNumber = 0;

        // Conexión WiFi
        WiFi.begin("iPhone de Maximiliano Baruk","123456789");
        Serial.print("Conectando a WiFi");
        while (WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        Serial.println("\nConectado! IP: " + WiFi.localIP().toString());

        // Crear tareas
        xTaskCreatePinnedToCore(vTaskMQTT, "MQTT", 8192, NULL, 1, NULL, 1);
        xTaskCreatePinnedToCore(vTaskReadFlowSensor, "Flow", 4096, NULL, 2, NULL, 0);
        xTaskCreatePinnedToCore(vTaskReadHealthSensor, "Health", 8192, NULL, 2, NULL, 0);

        vTaskDelete(NULL);  // Eliminar la tarea de setup
    }

    void loop() {}