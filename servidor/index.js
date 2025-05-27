const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const mqtt = require('mqtt');
const mysql = require('mysql2');

// Configuracion de Express y Socket.IO
const app = express();
const server = http.createServer(app);
const io = socketIo(server, { cors: { origin: "*" } });

// Conexion a MySQL/MariaDB
const db = mysql.createConnection({
  host: 'localhost',
  user: 'root',
  password: '',
  database: 'sensor_data'
});

// Verificar conexion a la base de datos
db.connect(err => {
  if (err) {
    console.error('Error al conectar a la base de datos:', err);
    process.exit(1);
  }
  console.log('Conectado a la base de datos MySQL');
});



// Conexion MQTT
const mqttClient = mqtt.connect('mqtt://172.20.10.3:1883');

mqttClient.on('connect', () => {
  console.log('Conectado al broker MQTT');
  
  // Suscripcion a los topicos
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
    
    // Para todos los mensajes MQTT recibidos
    console.log(`Mensaje recibido [${topic}]:`, data);
    
    // Procesamiento segun el tópico
    switch(topic) {
      case 'sensor/sfm3003/raw':
        // 1. Enviar datos a la aplicación via Socket.IO
        io.emit('data', {
          breath_volume: data.breath_volume,
          timestamp: data.timestamp
        });
        
        // 2. Insertar en base de datos
        db.query(
          'INSERT INTO breath_measurements (breath_volume, timestamp) VALUES (?, ?)',
          [data.breath_volume, data.timestamp],
          (err) => {
            if (err) console.error('Error al insertar raw:', err);
          }
        );
        break;
        
      case 'sensor/sfm3003/summary':
        // Solo insertar en base de datos (no se envia a la app)
        if (data.avg_inhaled_volume && data.total_breaths && data.duration) {
          db.query(
            'INSERT INTO breath_measurements_average (avg_inhaled_volume, total_breaths, duration) VALUES (?, ?, ?)',
            [data.avg_inhaled_volume, data.total_breaths, data.duration],
            (err) => {
              if (err) console.error('Error al insertar summary:', err);
            }
          );
        }
        break;
    }
    
  } catch (error) {
    console.error(`Error al procesar mensaje [${topic}]:`, error);
  }
});

// Manejo de errores
mqttClient.on('error', (err) => {
  console.error('Error MQTT:', err);
});

// Iniciar servidor
server.listen(3000, () => {
  console.log("Servidor Node.js en http://172.20.10.3:3000");
});

// Manejo de cierre limpio
process.on('SIGINT', () => {
  console.log('Cerrando conexiones...');
  mqttClient.end();
  db.end();
  process.exit();
});