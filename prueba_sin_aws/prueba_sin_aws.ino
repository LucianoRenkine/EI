#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

// ─── PINES RFID ──────────────────────────────────────────────────
#define RST_PIN  27
#define SS_PIN   15

// ─── PINES ULTRASÓNICO ───────────────────────────────────────────
#define TRIG_PIN 5
#define ECHO_PIN 14

// ─── PINES MOTOR (L298N) ─────────────────────────────────────────
#define MOTOR_IN1 26
#define MOTOR_IN2 25
#define MOTOR_IN3 13
#define MOTOR_IN4 33

// ─── PINES SERVO ─────────────────────────────────────────────────
#define SERVO_PIN 4

// ─── ÁNGULOS DEL SERVO ───────────────────────────────────────────
#define SERVO_CERRADO 0
#define SERVO_ABIERTO 90

// ─── LÓGICA DE CONTROL DE CALIDAD ────────────────────────────────
// Editá ALTURA_REFERENCIA con la altura real de tu pieza correcta en cm
// El sistema acepta si la medición está dentro del ±15% de ese valor
#define ALTURA_REFERENCIA  5.0   // cm — altura de la pieza correcta
#define TOLERANCIA         0.15  // 15%

// ─── CONSTANTES ──────────────────────────────────────────────────
const float DISTANCIA_VACIA = 9.0; // cm con la cinta vacía

// ─── OBJETOS ─────────────────────────────────────────────────────
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo   brazoServo;

// ═════════════════════════════════════════════════════════════════
void motorOn() {
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  digitalWrite(MOTOR_IN3, HIGH);
  digitalWrite(MOTOR_IN4, LOW);
  Serial.println("[MOTOR] Encendido");
}

void motorOff() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  digitalWrite(MOTOR_IN3, LOW);
  digitalWrite(MOTOR_IN4, LOW);
  Serial.println("[MOTOR] Detenido");
}

void servoAbrir() {
  Serial.println("[SERVO] Abriendo paso...");
  brazoServo.write(SERVO_ABIERTO);
  motorOn();
  delay(3000);              // Cinta andando con servo abierto 3s
  brazoServo.write(SERVO_CERRADO);
  delay(1500);              // Tiempo para cerrar físicamente
  Serial.println("[SERVO] Cerrado.");
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

// ─── DECISIÓN LOCAL ──────────────────────────────────────────────
bool esAceptable(float altura) {
  float minimo = ALTURA_REFERENCIA * (1.0 - TOLERANCIA); // 4.25 cm
  float maximo = ALTURA_REFERENCIA * (1.0 + TOLERANCIA); // 5.75 cm
  Serial.print("[QC] Rango aceptable: ");
  Serial.print(minimo, 2);
  Serial.print(" — ");
  Serial.print(maximo, 2);
  Serial.println(" cm");
  return (altura >= minimo && altura <= maximo);
}

// ═════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Sistema QC Industrial (modo local) ===");

  // Motor — IN3/IN4 primero para evitar problema de GPIO 12
  pinMode(MOTOR_IN3, OUTPUT); digitalWrite(MOTOR_IN3, LOW);
  pinMode(MOTOR_IN4, OUTPUT); digitalWrite(MOTOR_IN4, LOW);
  pinMode(MOTOR_IN1, OUTPUT); digitalWrite(MOTOR_IN1, LOW);
  pinMode(MOTOR_IN2, OUTPUT); digitalWrite(MOTOR_IN2, LOW);

  // Ultrasónico
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Servo
  brazoServo.setPeriodHertz(50);
  brazoServo.attach(SERVO_PIN, 500, 2400);
  Serial.println("[SERVO] Centrando...");
  brazoServo.write(SERVO_CERRADO);
  delay(1500);
  Serial.println("[SERVO] Listo.");

  // RFID
  SPI.begin();
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  motorOn();
  Serial.println("Sistema listo. Esperando tarjeta RFID...\n");
}

// ═════════════════════════════════════════════════════════════════
void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial())   return;

  motorOff();

  // Leer UID
  String idTarjeta = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    idTarjeta += mfrc522.uid.uidByte[i] < 0x10 ? "0" : "";
    idTarjeta += String(mfrc522.uid.uidByte[i], HEX);
  }
  idTarjeta.toUpperCase();

  // Medir altura
  float altura = medirAltura();

  Serial.println("\n─── NUEVA LECTURA ───────────────────");
  Serial.println("ID     : " + idTarjeta);
  Serial.print  ("Altura : "); Serial.print(altura, 2); Serial.println(" cm");
  Serial.print  ("Ref.   : "); Serial.print(ALTURA_REFERENCIA, 2); Serial.println(" cm ±15%");

  // Decisión local — sin internet, sin backend
  if (esAceptable(altura)) {
    Serial.println(">>> ACEPTADO ✓");
    servoAbrir();
  } else {
    Serial.println(">>> RECHAZADO — fuera de tolerancia.");
    motorOn();
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  Serial.println("Línea reanudada.\n");
  delay(1000);
}