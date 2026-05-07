#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>

// ─── CREDENCIALES ────────────────────────────────────────────────
const char* ssid        = "UA-Alumnos";
const char* password    = "41umn05WLC";
const char* mqtt_server = "98.81.32.212";
const int   mqtt_port   = 1883;

// ─── PINES RFID (igual al código funcional) ──────────────────────
#define RST_PIN 27
#define SS_PIN  15
// ► NO definir SCK/MISO/MOSI — SPI.begin() sin args usa los defaults
//   correctos del ESP32: SCK=18, MISO=19, MOSI=23

// ─── PINES ULTRASÓNICO ───────────────────────────────────────────
#define TRIG_PIN 5
#define ECHO_PIN 14

const float DISTANCIA_VACIA = 28.0;

// ─── OBJETOS ─────────────────────────────────────────────────────
WiFiClient   espClient;
PubSubClient client(espClient);
MFRC522      mfrc522(SS_PIN, RST_PIN);

// ─── ESTADO MQTT no bloqueante ───────────────────────────────────
unsigned long lastMqttAttempt = 0;
const unsigned long MQTT_RETRY_MS = 5000;

// ═════════════════════════════════════════════════════════════════
void setup_wifi() {
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado — IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nSin WiFi. Continuando igual...");
  }
}

// ─── Reconexión MQTT sin bloquear el loop ────────────────────────
void mqttReconnectIfNeeded() {
  if (client.connected()) return;

  unsigned long ahora = millis();
  if (ahora - lastMqttAttempt < MQTT_RETRY_MS) return;
  lastMqttAttempt = ahora;

  String clientId = "ESP32-" + String(random(0xffff), HEX);
  Serial.print("Intentando MQTT... ");

  if (client.connect(clientId.c_str())) {
    Serial.println("¡Conectado!");
  } else {
    Serial.print("Fallo rc=");
    Serial.println(client.state());
  }
}

// ─── Ultrasónico con timeout (no bloquea) ────────────────────────
float medirAltura() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Timeout 30ms — si no hay eco no congela el loop
  long duracion = pulseIn(ECHO_PIN, HIGH, 30000UL);

  if (duracion == 0) {
    Serial.println("[ULTRASÓNICO] Sin eco — retorna 0");
    return 0.0;
  }

  float lectura = duracion * 0.034f / 2.0f;
  float altura  = DISTANCIA_VACIA - lectura;
  if (altura < 0) altura = 0;
  return altura;
}

// ═════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  // ► Sin while(!Serial) — el ESP32 no necesita esperar la UART
  delay(200);

  Serial.println("\n=== Iniciando sistema IoT ===");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  // ► SPI.begin() SIN argumentos — igual al código que funciona
  SPI.begin();
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  Serial.println("Lector listo. Esperando tarjeta RFID...\n");
}

// ═════════════════════════════════════════════════════════════════
void loop() {
  // 1. Red — no bloqueante
  mqttReconnectIfNeeded();
  client.loop();

  // 2. RFID — mismo patrón exacto que el código funcional
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial())   return;

  // 3. Leer UID — igual al código funcional
  String idTarjeta = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    idTarjeta += mfrc522.uid.uidByte[i] < 0x10 ? "0" : "";
    idTarjeta += String(mfrc522.uid.uidByte[i], HEX);
  }
  idTarjeta.toUpperCase();

  // 4. Medir altura sólo cuando hay tarjeta
  float altura = medirAltura();

  // 5. Log serial
  Serial.println("\n─── NUEVA LECTURA ───────────────");
  Serial.println("ID     : " + idTarjeta);
  Serial.print  ("Altura : ");
  Serial.print  (altura, 2);
  Serial.println(" cm");

  // 6. Publicar MQTT
  String payload = "{\"objectId\":\"" + idTarjeta +
                   "\",\"measuredHeight\":" +
                   String(altura, 2) + "}";

  if (client.connected()) {
    bool ok = client.publish("factory/height", payload.c_str());
    Serial.println(ok ? "✓ MQTT publicado" : "✗ Falló publicación");
    Serial.println("Payload: " + payload);
  } else {
    Serial.println("⚠ Sin MQTT — lectura no enviada");
  }

  // 7. Finalizar tarjeta — igual al código funcional
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  delay(1000); // igual al código funcional
}