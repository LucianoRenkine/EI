#include <SPI.h>
#include <MFRC522.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- CONFIGURACIÓN PINES ---
#define SS_PIN 15
#define RST_PIN 27
#define TRIG_PIN 5   // Ajustado
#define ECHO_PIN 18  // Ajustado
#define BTN_ACEPTAR 32
#define BTN_RECHAZAR 33
#define MOTOR_PIN 26

// --- CREDENCIALES DE RED ---
const char* ssid = "UA-Alumnos";       // Completar
const char* password = "41umn05WLC"; // Completar
const char* mqtt_server = "44.213.77.194"; // Cambiar por IP pública de AWS

// --- OBJETOS ---
MFRC522 rfid(SS_PIN, RST_PIN);
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClient espClient;
PubSubClient client(espClient);

// --- VARIABLES ---
float distanciaVacia = 28.0; 
float alturaMinima = 5.0;   

// --- FUNCIONES DE RED ---
void setup_wifi() {
  delay(10);
  lcd.clear();
  lcd.print("Conectando WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
  lcd.clear();
  lcd.print("WiFi OK");
  delay(1000);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando a AWS...");
    lcd.clear();
    lcd.print("Buscando AWS...");
    String clientId = "Cinta_ESP32_";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado");
      lcd.clear();
      lcd.print("AWS Conectado");
      delay(1000);
    } else {
      Serial.print("Fallo, reintentando...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  
  if (!rtc.begin()) {
    Serial.println("No se encuentra RTC");
  }

  lcd.init();
  lcd.backlight();
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BTN_ACEPTAR, INPUT);
  pinMode(BTN_RECHAZAR, INPUT);
  pinMode(MOTOR_PIN, OUTPUT);

  // Conectar a Internet y AWS
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  digitalWrite(MOTOR_PIN, HIGH); // Inicia cinta
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("CINTA ACTIVA");
}

float medirAltura() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duracion = pulseIn(ECHO_PIN, HIGH);
  float lectura = duracion * 0.034 / 2;
  return distanciaVacia - lectura; // Altura real de la caja
}

void enviarDatos(String id, float altura, String resultado, DateTime fecha) {
  // Construcción del JSON para AWS
  String payload = "{\"id\":\"" + id + "\",";
  payload += "\"altura\":" + String(altura) + ",";
  payload += "\"resultado\":\"" + resultado + "\",";
  payload += "\"hora\":\"" + String(fecha.hour()) + ":" + String(fecha.minute()) + ":" + String(fecha.second()) + "\"}";
  
  client.publish("planta/produccion", payload.c_str());
  Serial.println("Datos enviados a AWS: " + payload);
}

void loop() {
  // Mantener conexión AWS viva
  if (!client.connected()) {
    reconnect();
    lcd.clear();
    lcd.print("CINTA ACTIVA");
  }
  client.loop();

  // 1. Detección RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    digitalWrite(MOTOR_PIN, LOW); // Freno para proceso
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    
    lcd.clear();
    lcd.print("ID: " + uid);
    delay(1000);

    // 2. Medición Altura
    lcd.setCursor(0,1);
    lcd.print("Midiendo...");
    float alturaCaja = medirAltura();
    
    // 3. Lógica de Decisión Humana
    lcd.clear();
    lcd.print("Alt: " + String(alturaCaja) + "cm");
    lcd.setCursor(0,1);
    lcd.print("ACEPTA / RECHAZA");

    bool decidido = false;
    String estadoFinal = "";

    while (!decidido) {
      if (digitalRead(BTN_ACEPTAR) == HIGH) {
        estadoFinal = "ACEPTADO";
        decidido = true;
      }
      if (digitalRead(BTN_RECHAZAR) == HIGH) {
        estadoFinal = "RECHAZADO";
        decidido = true;
      }
    }

    // 4. Registro de Tiempo y Envío
    DateTime ahora = rtc.now();
    enviarDatos(uid, alturaCaja, estadoFinal, ahora);
    
    lcd.clear();
    lcd.print(estadoFinal);
    delay(2000);
    
    // 5. Reanudar
    lcd.clear();
    lcd.print("CINTA ACTIVA");
    digitalWrite(MOTOR_PIN, HIGH);
    
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}