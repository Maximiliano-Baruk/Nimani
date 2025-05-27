<!DOCTYPE html>
<html>
<head>
    <title>Datos en Tiempo Real</title>
    <script>
    function cargarDatos() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
                document.getElementById("tablaDatos").innerHTML = this.responseText;
            }
        };
        xhttp.open("GET", "datos.php", true);
        xhttp.send();
    }

    setInterval(cargarDatos, 1000); // Cargar cada 1 segundo
    window.onload = cargarDatos;
    </script>
</head>
<body>
    <h1>Datos en tiempo real (sensor/sfm3003/raw)</h1>
    <div id="tablaDatos"></div>
</body>
</html>