<?php
$servername = "localhost";
$username = "root";      
$password = ""; 
$dbname = "sensor_data";

// Crear conexión
$conn = new mysqli($servername, $username, $password, $dbname);

// Verifica conexión
if ($conn->connect_error) {
    die("Conexión fallida: " . $conn->connect_error);
}

// Consulta los últimos 10 datos
$sql = "SELECT id, breath_volume, timestamp, created_at FROM breath_measurements ORDER BY id DESC LIMIT 10";
$result = $conn->query($sql);

// Regresa una tabla HTML
if ($result->num_rows > 0) {
    echo "<table border='1'><tr><th>ID</th><th>Breath Volume</th><th>Timestamp</th><th>Created At</th></tr>";
    while($row = $result->fetch_assoc()) {
        echo "<tr>
                <td>".$row["id"]."</td>
                <td>".$row["breath_volume"]."</td>
                <td>".$row["timestamp"]."</td>
                <td>".$row["created_at"]."</td>
              </tr>";
    }
    echo "</table>";
} else {
    echo "No hay datos";
}
$conn->close();
?>