#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>

// --- 1. Credenciales de Wi-Fi y MQTT ---
const char* ssid = "UA-Alumnos";
const char* password = "41umn05WLC";
const char* mqtt_server = "44.192.23.249"; // Tu instancia EC2
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// --- 2. Configuración de Pines RFID ---
#define RST_PIN  27
#define SS_PIN   15
#define SCK_PIN  19
#define MISO_PIN 18
#define MOSI_PIN 23

MFRC522 mfrc522(SS_PIN, RST_PIN);

// --- 3. Configuración de Pines Ultrasónico ---
#define TRIG_PIN 5
#define ECHO_PIN 14 // ¡IMPORTANTE! Asegurate de que el cable físico esté en el 14, no en el 18
float distanciaVacia = 28.0; 

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
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("¡Conectado al broker MQTT en EC2!");
    } else {
      Serial.print("Fallo, rc=");
      Serial.print(client.state());
      Serial.println(" intentando de nuevo en 5 segundos");
      delay(5000);
    }
  }
}

float medirAltura() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duracion = pulseIn(ECHO_PIN, HIGH);
  
  float lecturaSensor = duracion * 0.034 / 2;
  float alturaCaja = distanciaVacia - lecturaSensor; 
  if (alturaCaja < 0) alturaCaja = 0; 
  
  return alturaCaja;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  Serial.println("\nIniciando sistema IoT...");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Iniciar Wi-Fi y MQTT ANTES que el RFID para estabilizar voltajes
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  // Iniciar SPI con los pines personalizados
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  
  Serial.println("Sensores listos. Esperando caja (tarjeta RFID)...");
}

void loop() {
  // 1. Mantener la conexión de red
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // 2. Nueva lógica de detección amable con el procesador
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    
    String idTarjeta = ""; 
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      idTarjeta += mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""; 
      idTarjeta += String(mfrc522.uid.uidByte[i], HEX);
    }
    idTarjeta.toUpperCase(); 

    float alturaCalculada = medirAltura();

    String payload = "{\"objectId\":\"" + idTarjeta + "\",\"measuredHeight\":" + String(alturaCalculada, 2) + "}";

    Serial.println("\n--- NUEVA LECTURA ---");
    Serial.print("Publicando en factory/height: ");
    Serial.println(payload);
    
    client.publish("factory/height", payload.c_str());

    mfrc522.PICC_HaltA();    
    mfrc522.PCD_StopCrypto1();
    
    delay(1500); // Pausa para que avance la cinta
  }

  // 3. Oxígeno para el ESP32 (Previene bloqueos de hardware)
  delay(10); 
}