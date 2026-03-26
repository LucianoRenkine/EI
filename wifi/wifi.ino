#include <WiFi.h>

// Reemplaza con los datos EXACTOS (cuidado con mayúsculas o espacios extra)
const char* ssid = "UA-Alumnos";
const char* password = "41umn05WLC";

void setup() {
  Serial.begin(115200);
  delay(10);

  Serial.println();
  Serial.print("Intentando conectar a la red: ");
  Serial.println(ssid);

  // Inicia el proceso de conexión
  WiFi.begin(ssid, password);

  // Bucle de espera. Si los datos están bien, debería salir de aquí en unos segundos.
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Mensaje de éxito
  Serial.println("");
  Serial.println("¡Conexión Wi-Fi establecida exitosamente!");
  Serial.print("Dirección IP asignada: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // El código principal iría aquí
}