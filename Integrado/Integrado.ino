#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

// ─── CREDENCIALES ────────────────────────────────────────────────
const char* ssid        = "UA-Alumnos";
const char* password    = "41umn05WLC";
const char* mqtt_server = "98.81.32.212";
const int   mqtt_port   = 1883;

// ─── PINES RFID ──────────────────────────────────────────────────
#define RST_PIN      27
#define SS_PIN       15

// ─── PINES ULTRASÓNICO ───────────────────────────────────────────
#define TRIG_PIN     5
#define ECHO_PIN     14

// ─── PINES MOTOR (L298N) ─────────────────────────────────────────
#define MOTOR_IN1    26
#define MOTOR_IN2    25

// ─── PINES SERVO Y BOTONES ───────────────────────────────────────
#define SERVO_PIN    4
#define BTN_ACEPTAR  32
#define BTN_RECHAZAR 33

// ─── ÁNGULOS DEL SERVO ───────────────────────────────────────────
// Si el servo vibra en REPOSO, subí SERVO_REPOSO de a 5 grados
// hasta que deje de vibrar (ej: 10, 15, 20...)
#define SERVO_REPOSO   10   // Posición de paso libre (con margen del tope)
#define SERVO_DESCARTE 90   // Posición de descarte

// ─── CONSTANTES ──────────────────────────────────────────────────
const float          DISTANCIA_VACIA   = 28.0;
const unsigned long  MQTT_RETRY_MS     = 5000;
const unsigned long  TIMEOUT_DB_MS     = 4000;
const unsigned long  TIMEOUT_HUMANO_MS = 10000;

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
  Serial.println("[MOTOR] Encendido");
}

void motorOff() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  Serial.println("[MOTOR] Detenido");
}

void servoDescartar() {
  Serial.println("[SERVO] Descartando...");
  brazoServo.write(SERVO_DESCARTE);
  delay(2000);                        // Tiempo en posición de descarte
  brazoServo.write(SERVO_REPOSO);
  delay(1500);                        // Tiempo para volver físicamente
  Serial.println("[SERVO] En reposo.");
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
  Serial.println("\n=== Iniciando sistema IoT Industrial ===");

  // ─── Pines motor
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);

  // ─── Pines ultrasónico
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // ─── Botones — conectados entre GPIO y GND (presionado = LOW)
  pinMode(BTN_ACEPTAR,  INPUT_PULLUP);
  pinMode(BTN_RECHAZAR, INPUT_PULLUP);

  // ─── Servo — se mueve a posición inicial al arrancar
  brazoServo.setPeriodHertz(50);
  brazoServo.attach(SERVO_PIN, 500, 2400);
  Serial.println("[SERVO] Centrando en posición inicial...");
  brazoServo.write(SERVO_REPOSO);
  delay(1500); // Espera a que llegue físicamente antes de seguir
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

  // 6. Publicar por MQTT
  String payload = "{\"objectId\":\"" + idTarjeta +
                   "\",\"measuredHeight\":" + String(altura, 2) + "}";

  llegoRespuestaDB = false;
  respuestaDB      = "";

  if (client.connected()) {
    client.publish("factory/height", payload.c_str());
    Serial.println("Enviado. Esperando respuesta DB (4s)...");

    unsigned long esperaDB = millis();
    while (!llegoRespuestaDB && millis() - esperaDB < TIMEOUT_DB_MS) {
      client.loop();
      delay(10);
    }
  } else {
    Serial.println("⚠ Sin MQTT — procesando sin DB");
  }

  // 7. Si no llegó respuesta → REJECT por seguridad
  if (!llegoRespuestaDB) {
    Serial.println("⚠ Timeout DB → REJECT por defecto");
    respuestaDB = "REJECT";
  }

  // 8. Ejecutar resultado
  if (respuestaDB == "ACCEPT") {

    Serial.println(">>> DB: ACEPTADO ✓");

  } else {

    Serial.println(">>> DB: RECHAZADO");
    Serial.println(">>> Validación humana — 10 segundos...");
    Serial.println("    BTN ACEPTAR = forzar paso | BTN RECHAZAR = descartar");

    unsigned long timerHumano = millis();
    String decisionFinal = "REJECT";
    static unsigned long ultimoPrint = 0;
    ultimoPrint = 0;

    while (millis() - timerHumano < TIMEOUT_HUMANO_MS) {

      if (digitalRead(BTN_ACEPTAR) == LOW) {
        decisionFinal = "ACCEPT";
        Serial.println(">>> Operario: FORZÓ ACEPTAR");
        break;
      }
      if (digitalRead(BTN_RECHAZAR) == LOW) {
        decisionFinal = "REJECT";
        Serial.println(">>> Operario: CONFIRMÓ RECHAZO");
        break;
      }

      // Cuenta regresiva cada segundo
      unsigned long restante = TIMEOUT_HUMANO_MS - (millis() - timerHumano);
      if (millis() - ultimoPrint > 1000) {
        Serial.println("    Tiempo: " + String(restante / 1000) + "s");
        ultimoPrint = millis();
      }

      client.loop();
      delay(50);
    }

    if (decisionFinal == "ACCEPT") {
      Serial.println(">>> Pieza ACEPTADA por operario.");
    } else {
      servoDescartar(); // Función unificada con delays correctos
    }
  }

  // 9. Finalizar tarjeta y reanudar cinta
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  motorOn();
  Serial.println("Línea reanudada.\n");

  delay(1000);
}