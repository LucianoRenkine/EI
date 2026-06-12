#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

// ─── CREDENCIALES ────────────────────────────────────────────────
const char* ssid        = "UA-Alumnos";
const char* password    = "41umn05WLC";
const char* mqtt_server = "3.214.255.9";
const int   mqtt_port   = 1883;

// ─── PINES RFID ──────────────────────────────────────────────────
#define RST_PIN  27
#define SS_PIN   15

// ─── PINES ULTRASÓNICO ───────────────────────────────────────────
#define TRIG_PIN 5
#define ECHO_PIN 14

// ─── PINES MOTOR (L298N) ─────────────────────────────────────────++º
#define MOTOR_IN1 26
#define MOTOR_IN2 25
#define MOTOR_IN3 13 
#define MOTOR_IN4 33

// ─── PINES SERVO ─────────────────────────────────────────────────
#define SERVO_PIN 4

// ─── ÁNGULOS DEL SERVO ───────────────────────────────────────────
// Si el servo vibra en REPOSO, subí SERVO_REPOSO de a 5 grados
// hasta que deje de vibrar (ej: 10, 15, 20...)
#define SERVO_CERRADO  0    
#define SERVO_ABIERTO  90

// ─── CONSTANTES ──────────────────────────────────────────────────
const float          DISTANCIA_VACIA  = 9.0;
const unsigned long  MQTT_RETRY_MS    = 5000;
const unsigned long  TIMEOUT_TOTAL_MS = 15000; // Seguridad si la web se cae

// ─── OBJETOS ─────────────────────────────────────────────────────
WiFiClient   espClient;
PubSubClient client(espClient);
MFRC522      mfrc522(SS_PIN, RST_PIN);
Servo        brazoServo;

// ─── ESTADO ──────────────────────────────────────────────────────
unsigned long lastMqttAttempt  = 0;
bool          llegoRespuestaDB = false;
String        respuestaDB      = "";

// ═════════════════════════════════════════════════════════════════
void motorOn() {
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  digitalWrite(MOTOR_IN3, HIGH);  // ← nuevo
  digitalWrite(MOTOR_IN4, LOW);   // ← nuevo 
  Serial.println("[MOTOR] Encendido");
}

void motorOff() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  digitalWrite(MOTOR_IN3, LOW);   // ← nuevo
  digitalWrite(MOTOR_IN4, LOW);   // ← nuevo
  Serial.println("[MOTOR] Detenido");
}

void servoAbrir() {
  Serial.println("[SERVO] Abriendo paso...");
  brazoServo.write(SERVO_ABIERTO);
  motorOn();        // ← arranca la cinta mientras el servo está abierto
  delay(1000);      // ← servo abierto 3 segundos con la cinta andando
  brazoServo.write(SERVO_CERRADO);
  delay(1500);      // ← tiempo para que cierre físicamente
  Serial.println("[SERVO] Cerrado.");
}

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

// ─── CALLBACK MQTT ───────────────────────────────────────────────
// Cualquier mensaje en factory/result con ACCEPT o REJECT es válido
// sin importar si viene del backend o de la web
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.println("[MQTT IN] " + String(topic) + " → " + msg);

  if (String(topic) == "factory/result") {
    if      (msg.indexOf("ACCEPT") != -1) respuestaDB = "ACCEPT";
    else if (msg.indexOf("REJECT") != -1) respuestaDB = "REJECT";
    else                                   respuestaDB = "UNKNOWN";
    llegoRespuestaDB = true;
  }
}

// ─── RECONEXIÓN MQTT no bloqueante ───────────────────────────────
void mqttReconnectIfNeeded() {
  if (client.connected()) return;

  unsigned long ahora = millis();
  if (ahora - lastMqttAttempt < MQTT_RETRY_MS) return;
  lastMqttAttempt = ahora;

  String clientId = "ESP32-" + String(random(0xffff), HEX);
  Serial.print("Intentando MQTT... ");

  if (client.connect(clientId.c_str())) {
    Serial.println("¡Conectado!");
    client.subscribe("factory/result");
  } else {
    Serial.print("Fallo rc=");
    Serial.println(client.state());
  }
}

// ─── ULTRASÓNICO ─────────────────────────────────────────────────
float medirAltura() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH, 30000UL);

  if (duracion == 0) {
    Serial.println("[ULTRASÓNICO] Sin eco");
    return 0.0;
  }

  float altura = DISTANCIA_VACIA - (duracion * 0.034f / 2.0f);
  return max(altura, 0.0f);
}

// ═════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(MOTOR_IN3, OUTPUT);
  digitalWrite(MOTOR_IN3, LOW);
  pinMode(MOTOR_IN4, OUTPUT);
  digitalWrite(MOTOR_IN4, LOW);
  Serial.println("\n=== Iniciando sistema IoT Industrial ===");

  // ─── Motor
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_IN3, OUTPUT);  // ← nuevo
  pinMode(MOTOR_IN4, OUTPUT);  // ← nuevo
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  digitalWrite(MOTOR_IN3, LOW);  // ← nuevo
  digitalWrite(MOTOR_IN4, LOW);

  // ─── Ultrasónico
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // ─── Servo — va a posición inicial al arrancar
  brazoServo.setPeriodHertz(50);
  brazoServo.attach(SERVO_PIN, 500, 2400);
  Serial.println("[SERVO] Centrando en posición inicial...");
  brazoServo.write(SERVO_CERRADO);
  delay(1500);
  Serial.println("[SERVO] Listo.");

  // ─── Red
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  // ─── RFID
  SPI.begin();
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  // ─── Arrancar cinta
  motorOn();
  Serial.println("Sistema listo. Esperando tarjeta RFID...\n");
}

// ═════════════════════════════════════════════════════════════════
void loop() {
  // 1. Mantener red
  mqttReconnectIfNeeded();
  client.loop();

  // 2. Detección RFID
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial())   return;

  // 3. Detener cinta para inspección
  motorOff();

  // 4. Leer UID
  String idTarjeta = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    idTarjeta += mfrc522.uid.uidByte[i] < 0x10 ? "0" : "";
    idTarjeta += String(mfrc522.uid.uidByte[i], HEX);
  }
  idTarjeta.toUpperCase();

  // 5. Medir altura
  float altura = medirAltura();

  Serial.println("\n─── NUEVA LECTURA ───────────────────");
  Serial.println("ID     : " + idTarjeta);
  Serial.print  ("Altura : "); Serial.print(altura, 2); Serial.println(" cm");

  // 6. Publicar y esperar decisión (la web maneja el timeout)
  String payload = "{\"objectId\":\"" + idTarjeta +
                   "\",\"measuredHeight\":" + String(altura, 2) + "}";

  llegoRespuestaDB = false;
  respuestaDB      = "";

  if (client.connected()) {
    client.publish("factory/height", payload.c_str());
    Serial.println("Enviado. Esperando decisión final...");

    unsigned long espera     = millis();
    unsigned long ultimoPrint = millis();

    while (!llegoRespuestaDB && millis() - espera < TIMEOUT_TOTAL_MS) {
      client.loop();
      delay(10);

      // Log de espera cada 5 segundos para no saturar el monitor
      if (millis() - ultimoPrint > 5000) {
        unsigned long restante = TIMEOUT_TOTAL_MS - (millis() - espera);
        Serial.println("    Esperando... " + String(restante / 1000) + "s restantes");
        ultimoPrint = millis();
      }
    }
  } else {
    Serial.println("⚠ Sin MQTT — procesando sin conexión");
  }

  // 7. Ejecutar resultado
  if (!llegoRespuestaDB) {
    Serial.println("⚠ Sin respuesta tras 60s → REJECT por seguridad");
    respuestaDB = "REJECT";
  }

  if (respuestaDB == "ACCEPT") {
    Serial.println(">>> ACEPTADO ✓");
    servoAbrir();         // ya arranca el motor internamente
  } else {
    Serial.println(">>> RECHAZADO — servo quieto.");
    motorOn();            // solo en rechazo arranca la cinta acá
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  Serial.println("Línea reanudada.\n");

  delay(1000);
}