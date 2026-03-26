#include <WiFi.h>
#include <PubSubClient.h>

// 1. Credenciales de Wi-Fi
const char* ssid = "UA-Alumnos";
const char* password = "41umn05WLC";

// 2. Datos del servidor MQTT (Tu instancia EC2)
const char* mqtt_server = "98.80.100.211"; // Ej: "18.222.111.99"
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
int contadorMensajes = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Bucle hasta que estemos conectados
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    
    // Creamos un ID de cliente aleatorio
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    // Intentamos conectar
    if (client.connect(clientId.c_str())) {
      Serial.println("¡Conectado al broker MQTT en EC2!");
      // Aquí podríamos suscribirnos a un topic si quisiéramos recibir comandos
      // client.subscribe("cinta/comandos"); 
    } else {
      Serial.print("Fallo, rc=");
      Serial.print(client.state());
      Serial.println(" intentando de nuevo en 5 segundos");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  setup_wifi();
  
  // Configuramos la dirección del servidor MQTT y el puerto
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  // Si se desconecta el Wi-Fi o el broker, intentamos reconectar
  if (!client.connected()) {
    reconnect();
  }
  
  // Mantiene viva la conexión MQTT y procesa mensajes entrantes
  client.loop();

  // Enviar un mensaje cada 5 segundos de forma no bloqueante
  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    contadorMensajes++;
    
    // Preparamos el mensaje a enviar (simulando un dato de control)
    String mensaje = "Lectura de sistema #" + String(contadorMensajes) + " - OK";
    
    // Publicamos en el topic "cinta/control/estado"
    Serial.print("Publicando mensaje: ");
    Serial.println(mensaje);
    
    client.publish("cinta/control/estado", mensaje.c_str());
  }
}