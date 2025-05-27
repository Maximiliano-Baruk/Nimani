#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// Configuración básica requerida
#define configUSE_16_BIT_TICKS          0  // Usar ticks de 32 bits (1 para 16 bits)
#define configTICK_RATE_HZ              1000  // Frecuencia del tick (5kHz = 0.2ms)
#define configUSE_PREEMPTION            1  // Planificación con desalojo
#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0
#define configMAX_PRIORITIES            (25)
#define configMINIMAL_STACK_SIZE        ((unsigned short)2048)
#define configTOTAL_HEAP_SIZE           ((size_t)(1024 * 75))  // 75KB heap

// Configuraciones específicas para ESP32
#define configENABLE_BACKWARD_COMPATIBILITY 0
#define configUSE_NEWLIB_REENTRANT      1

#include "sdkconfig.h"  // Incluye configuraciones específicas del ESP32

#endif // FREERTOS_CONFIG_H