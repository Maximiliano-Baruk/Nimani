const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const mqtt = require('mqtt');

// Configuración de Express y Socket.IO
const app = express();
const server = http.createServer(app);
const io = socketIo(server, { cors: { origin: "*" } });

// Conexión MQTT
const mqttClient = mqtt.connect('mqtt://172.20.10.4:1883');

mqttClient.on('connect', () => {
  console.log('Conectado al broker MQTT');

  mqttClient.subscribe('sensor/sfm3003/raw', err => {
    if (err) console.error('Error al suscribirse a raw:', err);
  });

  mqttClient.subscribe('sensor/sfm3003/summary', err => {
    if (err) console.error('Error al suscribirse a summary:', err);
  });
});

mqttClient.on('message', (topic, message) => {
  try {
    const data = JSON.parse(message.toString());
    console.log(`Mensaje recibido [${topic}]:`, data);

    switch (topic) {
      case 'sensor/sfm3003/raw':
        io.emit('data', {
          seq: data.seq,
          flow: data.flow,
          breath_volume: data.volume || data.breath_volume,
          temp: data.temp,
          heart_rate: data.heart_rate,
          hr_valid: data.hr_valid,
          spo2: data.spo2,
          spo2_valid: data.spo2_valid,
          exercise_active: data.exercise_active,
          time_remaining: data.time_remaining,
          breath_count: data.breath_count,
          timestamp: data.timestamp
        });

        break;

      case 'sensor/sfm3003/summary':
        // Puedes agregar lógica adicional aquí si lo deseas
        break;
    }

  } catch (error) {
    console.error(`Error al procesar mensaje [${topic}]:`, error);
  }
});

mqttClient.on('error', (err) => {
  console.error('Error MQTT:', err);
});

server.listen(3000, () => {
  console.log("Servidor Node.js en http://localhost:3000");
});

process.on('SIGINT', () => {
  console.log('Cerrando conexiones...');
  mqttClient.end();
  process.exit();
});