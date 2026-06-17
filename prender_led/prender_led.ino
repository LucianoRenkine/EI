// ================================================================
//  CASCO INTELIGENTE - CÓDIGO UNIFICADO v5.0
//  1 click = SOS
//  2 clicks = Info (velocidad, distancia, ubicación)
// ================================================================

#include <Wire.h>
#include <math.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPS++.h>
#include <Adafruit_BMP085.h>

// ================================================================
// --- FRECUENCIAS MUSICALES ---
// ================================================================
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_E5  659
#define REST     0

// ================================================================
// --- CONFIGURACIÓN WIFI Y TELEGRAM ---
// ================================================================
const char* ssid     = "UA-Alumnos";
const char* password = "41umn05WLC";
String botToken      = "8776262827:AAExGVFKaNem87wQS7OW-DkU2fnt9W2WvOI";
String chatId        = "1364823864";

// ================================================================
// --- MAPA DE PINES ---
// ================================================================
#define CS_IZQ_PIN   17
#define CS_DER_PIN   5
#define LED_IZQ_PIN  26
#define LED_DER_PIN  27
#define BUZZER_PIN   14
#define BUTTON_PIN   33
#define GPS_RX_PIN   13
#define GPS_TX_PIN   12
#define SWITCH_PIN   32

// ================================================================
// --- ACELERÓMETRO MMA845x ---
// ================================================================
#define MMA845x_ADDRESS 0x1C

// ================================================================
// --- OBJETOS ---
// ================================================================
Adafruit_SSD1306 display(128, 32, &Wire, -1);
TinyGPSPlus gps;
HardwareSerial SerialGPS(2);
Adafruit_BMP085 bmp;

// ================================================================
// --- PARÁMETROS ---
// ================================================================
const int   umbralGiro     = 550;
const int   umbralAdelante = 300;
const unsigned long duracionSinal = 5000;

// ================================================================
// --- VARIABLES GLOBALES ---
// ================================================================
int16_t x_ref = 0, z_ref = 0;
float   magnitud_ref = 0.0f;
unsigned long tiempoInicio = 0;
int estadoActual = 0;

double latAnt = 0, lonAnt = 0;
double distanciaTotal = 0;
float  velFiltrada = 0.0f;
const float alpha = 0.2f;

unsigned long lastSOS = 0;

int estadoCaida = 0;
unsigned long tiempoCaida = 0;
unsigned long lastAlertaCaida = 0;
const unsigned long COOLDOWN_CAIDA = 30000;

float presionAnterior = 0;
unsigned long lastLecturPresion = 0;
const unsigned long INTERVALO_PRESION = 15000;

bool melodiaActiva = false;
int notaActual = 0;
unsigned long tiempoNotaAnterior = 0;
bool bmpDisponible = false;

bool alertaLluviaActiva = false;

unsigned long ultimaActPantalla = 0;
const unsigned long INTERVALO_OLED = 250;

bool switchEstadoAnterior = false;

// ── Variables para detección de doble click ──────────────────
int contadorClicks         = 0;
unsigned long primerClick  = 0;
bool botonAnterior         = false;
const unsigned long VENTANA_DOBLE_CLICK = 500; // ms entre clicks

// ================================================================
// --- DIBUJOS MATRICES LED ---
// ================================================================
byte f_izq[8] = {0x10, 0x30, 0x70, 0xFF, 0xFF, 0x70, 0x30, 0x10};
byte f_der[8] = {0x10, 0x30, 0x70, 0xFF, 0xFF, 0x70, 0x30, 0x10};
byte f_rec[8] = {0x00, 0x18, 0x3C, 0x7E, 0x7E, 0x3C, 0x18, 0x00};
byte vacio[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// ================================================================
// --- MELODÍA: MARCHA IMPERIAL ---
// ================================================================
const int melodia[] = {
  440, 440, 440, 349, 523,
  440, 349, 523, 440,
  659, 659, 659, 698, 523,
  440, 349, 523, 440,
  880, 440, 440,
  880, 831, 784, 740,
  698, 523, 523,
  698, 659, 622, 587,
  523, 349, 349,
  415, 392, 349, 523, 440
};

const int duracionNotas[] = {
  500, 500, 500, 375, 125,
  500, 375, 125, 750,
  500, 500, 500, 375, 125,
  500, 375, 125, 750,
  500, 375, 125,
  500, 375, 125, 250,
  250, 250, 250,
  500, 375, 125, 250,
  250, 250, 250,
  500, 375, 125, 500, 750
};

const int numNotas = sizeof(melodia) / sizeof(melodia[0]);


// ================================================================
//  FUNCIONES — MATRICES LED
// ================================================================

void enviarDato(int csPin, byte reg, byte val) {
  digitalWrite(csPin, LOW);
  SPI.transfer(reg);
  SPI.transfer(val);
  digitalWrite(csPin, HIGH);
}

void iniciarMatriz(int csPin) {
  pinMode(csPin, OUTPUT);
  enviarDato(csPin, 0x0F, 0x00);
  enviarDato(csPin, 0x09, 0x00);
  enviarDato(csPin, 0x0A, 0x03);
  enviarDato(csPin, 0x0B, 0x07);
  enviarDato(csPin, 0x0C, 0x01);
  for (int i = 1; i <= 8; i++) enviarDato(csPin, i, 0x00);
}

void dibujar(int csPin, byte img[]) {
  for (int i = 0; i < 8; i++) {
    enviarDato(csPin, i + 1, img[i]);
  }
}


// ================================================================
//  FUNCIONES — ACELERÓMETRO
// ================================================================

void leerSensor(int16_t* x, int16_t* y, int16_t* z) {
  Wire.beginTransmission(MMA845x_ADDRESS);
  Wire.write(0x01);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MMA845x_ADDRESS, (uint8_t)6);
  if (Wire.available() >= 6) {
    *x = (int16_t)(Wire.read() << 8 | Wire.read()) >> 4;
    *y = (int16_t)(Wire.read() << 8 | Wire.read()) >> 4;
    *z = (int16_t)(Wire.read() << 8 | Wire.read()) >> 4;
  }
}

bool detectarVibracion(int16_t ax, int16_t ay, int16_t az) {
  float magnitud = sqrt(sq((float)ax) + sq((float)ay) + sq((float)az));
  return (magnitud > magnitud_ref * 1.12f || magnitud < magnitud_ref * 0.88f);
}

bool detectarCaida(int16_t ax, int16_t ay, int16_t az, unsigned long ahora) {
  float magnitud = sqrt(sq((float)ax) + sq((float)ay) + sq((float)az));
  const float UMBRAL_CAIDA_LIBRE = magnitud_ref * 0.3f;
  const float UMBRAL_IMPACTO     = magnitud_ref * 2.5f;

  switch (estadoCaida) {
    case 0:
      if (magnitud < UMBRAL_CAIDA_LIBRE) {
        estadoCaida = 1;
        tiempoCaida = ahora;
        Serial.println("Caída libre detectada");
      }
      break;
    case 1:
      if (magnitud > UMBRAL_IMPACTO) {
        estadoCaida = 0;
        Serial.println("¡IMPACTO DETECTADO!");
        return true;
      } else if (ahora - tiempoCaida > 500) {
        estadoCaida = 0;
      }
      break;
  }
  return false;
}


// ================================================================
//  FUNCIÓN — INICIALIZAR BMP180
// ================================================================

bool inicializarBMP180() {
  Serial.println("\n=== INICIALIZANDO BMP180 ===");
  if (!bmp.begin()) {
    Serial.println("✗ BMP180 no responde");
    return false;
  }
  Serial.println("✓ BMP180 inicializado correctamente");
  delay(200);
  float testPres = bmp.readPressure() / 100.0f;
  float testTemp = bmp.readTemperature();
  Serial.print("  Presión: ");
  Serial.print(testPres, 2);
  Serial.println(" hPa");
  Serial.print("  Temperatura: ");
  Serial.print(testTemp, 1);
  Serial.println(" °C");
  if (testPres > 0 && testPres < 1200) {
    Serial.println("✓ Sensor funcionando");
    return true;
  }
  Serial.println("✗ Lecturas fuera de rango");
  return false;
}


// ================================================================
//  FUNCIÓN — INICIALIZACIÓN DEL SISTEMA
// ================================================================

void inicializarSistema() {
  Serial.println("\n╔══════════════════════════════╗");
  Serial.println("║   INICIALIZANDO SISTEMA...   ║");
  Serial.println("╚══════════════════════════════╝\n");

  estadoActual        = 0;
  estadoCaida         = 0;
  melodiaActiva       = false;
  notaActual          = 0;
  velFiltrada         = 0.0f;
  distanciaTotal      = 0.0;
  latAnt              = 0;
  lonAnt              = 0;
  lastSOS             = 0;
  lastAlertaCaida     = 0;
  lastLecturPresion   = 0;
  presionAnterior     = 0;
  alertaLluviaActiva  = false;
  contadorClicks      = 0;
  primerClick         = 0;
  botonAnterior       = false;

  noTone(BUZZER_PIN);
  digitalWrite(LED_IZQ_PIN, LOW);
  digitalWrite(LED_DER_PIN, LOW);
  dibujar(CS_IZQ_PIN, vacio);
  dibujar(CS_DER_PIN, vacio);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.print("Iniciando...");
  display.display();

  Wire.beginTransmission(MMA845x_ADDRESS);
  Wire.write(0x2A);
  Wire.write(0x01);
  Wire.endTransmission();
  Serial.println("✓ Acelerómetro activado");

  delay(500);
  int16_t cx, cy, cz;
  leerSensor(&cx, &cy, &cz);
  x_ref = cx;
  z_ref = cz;
  magnitud_ref = sqrt(sq((float)cx) + sq((float)cy) + sq((float)cz));
  Serial.println("✓ Acelerómetro calibrado");
  Serial.printf("  Ref X=%d  Z=%d  |Mag|=%.1f\n", x_ref, z_ref, magnitud_ref);

  bmpDisponible = inicializarBMP180();
  if (bmpDisponible) {
    float presionInicial = bmp.readPressure() / 100.0f;
    float tempInicial    = bmp.readTemperature();
    presionAnterior = presionInicial;
    Serial.println("\n╔═══════════════════════╗");
    Serial.println("║  PRESIÓN INICIAL      ║");
    Serial.println("╠═══════════════════════╣");
    Serial.print("║  ");
    Serial.print(presionInicial, 2);
    Serial.println(" hPa        ║");
    Serial.print("║  ");
    Serial.print(tempInicial, 1);
    Serial.println(" °C            ║");
    Serial.println("╚═══════════════════════╝");

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Presion inicial:");
    display.setCursor(0, 12);
    display.print(presionInicial, 1);
    display.print(" hPa");
    display.setCursor(0, 24);
    display.print(tempInicial, 1);
    display.print(" C");
    display.display();
    delay(2000);
  } else {
    Serial.println("⚠️ BMP180 no disponible");
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.print("Conectando WiFi...");
  display.display();

  Serial.print("Conectando WiFi");
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi conectado: " + WiFi.localIP().toString());
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.print("WiFi OK");
    display.display();
    delay(500);
  } else {
    Serial.println("\n✗ WiFi no conectado");
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.print("WiFi FALLO");
    display.display();
    delay(1000);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 8);
  display.print("LISTO!");
  display.display();
  delay(1000);

  Serial.println("\n✓✓✓ SISTEMA LISTO ✓✓✓\n");
}


// ================================================================
//  FUNCIÓN — DETECCIÓN DE LLUVIA
// ================================================================

bool detectarLluvia() {
  if (!bmpDisponible) return false;
  float presionActual = bmp.readPressure() / 100.0f;
  if (presionAnterior == 0) {
    presionAnterior = presionActual;
    return false;
  }
  float caida = presionAnterior - presionActual;
  Serial.print("Presión: ");
  Serial.print(presionActual, 1);
  Serial.print(" hPa | Caída: ");
  Serial.print(caida, 2);
  Serial.println(" hPa");
  if (caida > 0.5f) {
    Serial.println("⚠️ ¡Caída de presión! Probable lluvia");
    presionAnterior = presionActual;
    alertaLluviaActiva = true;
    return true;
  }
  presionAnterior = presionActual;
  return false;
}


// ================================================================
//  FUNCIÓN — MELODÍA
// ================================================================

void actualizarMelodia(unsigned long ahora) {
  if (!melodiaActiva) return;
  if (ahora - tiempoNotaAnterior >= (unsigned long)duracionNotas[notaActual]) {
    notaActual++;
    if (notaActual >= numNotas) {
      noTone(BUZZER_PIN);
      melodiaActiva = false;
      notaActual = 0;
      Serial.println("Melodía finalizada");
    } else {
      tone(BUZZER_PIN, melodia[notaActual]);
      tiempoNotaAnterior = ahora;
    }
  }
}

void iniciarMelodia() {
  melodiaActiva = true;
  notaActual = 0;
  tiempoNotaAnterior = millis();
  tone(BUZZER_PIN, melodia[0]);
  Serial.println("🎵 Marcha Imperial...");
}


// ================================================================
//  FUNCIONES — TELEGRAM
// ================================================================

String obtenerUbicacion() {
  if (gps.location.isValid()) {
    return "http://maps.google.com/?q="
           + String(gps.location.lat(), 6) + ","
           + String(gps.location.lng(), 6);
  }
  return "GPS sin señal";
}

void enviarMensaje(String texto) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi no conectado");
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url = "https://api.telegram.org/bot" + botToken + "/sendMessage";
  https.begin(client, url);
  https.addHeader("Content-Type", "application/json");
  String body = "{\"chat_id\":\"" + chatId + "\",\"text\":\"" + texto + "\"}";
  int httpCode = https.POST(body);
  Serial.println("HTTP: " + String(httpCode));
  https.end();
}

// 1 click → SOS
void enviarAlertaTelegram() {
  Serial.println(">> Enviando SOS");
  String texto = "🚨 SOS - CASCO INTELIGENTE\\n";
  texto += "Ubicacion: " + obtenerUbicacion() + "\\n";
  texto += "Velocidad: " + String(velFiltrada, 1) + " km/h\\n";
  texto += "Distancia: " + String(distanciaTotal * 1000.0, 0) + " m";
  enviarMensaje(texto);
}

// 2 clicks → Info
void enviarInfoTelegram() {
  Serial.println(">> Enviando INFO");
  String texto = "📍 INFO - CASCO INTELIGENTE\\n";
  texto += "Ubicacion: " + obtenerUbicacion() + "\\n";
  texto += "Velocidad: " + String(velFiltrada, 1) + " km/h\\n";
  texto += "Distancia: " + String(distanciaTotal * 1000.0, 0) + " m";
  enviarMensaje(texto);
}

// Caída automática
void enviarAlertaCaida() {
  Serial.println(">> Enviando alerta caída");
  String texto = "⚠️ CAÍDA DETECTADA\\n";
  texto += "Ubicacion: " + obtenerUbicacion() + "\\n";
  texto += "Velocidad: " + String(velFiltrada, 1) + " km/h\\n";
  texto += "Distancia: " + String(distanciaTotal * 1000.0, 0) + " m";
  enviarMensaje(texto);
}


// ================================================================
//  OTRAS FUNCIONES
// ================================================================

double calcularHaversine(double lat1, double lon1, double lat2, double lon2) {
  double dLat = (lat2 - lat1) * M_PI / 180.0;
  double dLon = (lon2 - lon1) * M_PI / 180.0;
  double a = sin(dLat/2)*sin(dLat/2)
           + cos(lat1*M_PI/180.0)*cos(lat2*M_PI/180.0)
           * sin(dLon/2)*sin(dLon/2);
  return 6371.0 * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}

void actualizarPantalla(bool vibracion) {
  display.clearDisplay();

  if (alertaLluviaActiva) {
    bool parpadeo = (millis() / 1000) % 2;
    if (parpadeo) {
      display.setTextSize(1);
      display.setCursor(10, 0);
      display.print("*** ALERTA ***");
      display.setCursor(18, 12);
      display.print("POSIBLE LLUVIA");
      display.setCursor(28, 24);
      display.display();
      return;
    }
  }

  display.setTextSize(1);
  display.setCursor(0, 0);
  if (alertaLluviaActiva) {
    display.print("LLUVIA ");
  } else {
    display.print("VEL: ");
  }

  display.setTextSize(2);
  display.setCursor(30, 0);
  display.print(velFiltrada, 1);
  display.setTextSize(1);
  display.print(" km/h");

  display.setCursor(0, 20);
  display.print("DIST:");
  display.print(distanciaTotal, 3);
  display.print("km");

  display.setCursor(100, 20);
  display.print("S:");
  display.print(gps.satellites.value());

  if (WiFi.status() == WL_CONNECTED) {
    display.setCursor(115, 0);
    display.print("W");
  }

  if (vibracion) {
    display.fillCircle(110, 5, 2, WHITE);
  }

  display.display();
}


// ================================================================
//  SETUP
// ================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);
  delay(100);

  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_IZQ_PIN, OUTPUT);
  pinMode(LED_DER_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_IZQ_PIN, LOW);
  digitalWrite(LED_DER_PIN, LOW);
  noTone(BUZZER_PIN);

  SerialGPS.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.display();

  SPI.begin();
  iniciarMatriz(CS_IZQ_PIN);
  iniciarMatriz(CS_DER_PIN);

  if (digitalRead(SWITCH_PIN) == LOW) {
    switchEstadoAnterior = true;
    inicializarSistema();
  }
}


// ================================================================
//  LOOP
// ================================================================

void loop() {
  bool switchEncendido = (digitalRead(SWITCH_PIN) == LOW);

  if (switchEncendido && !switchEstadoAnterior) {
    switchEstadoAnterior = true;
    inicializarSistema();
  }

  if (!switchEncendido && switchEstadoAnterior) {
    switchEstadoAnterior = false;
    noTone(BUZZER_PIN);
    digitalWrite(LED_IZQ_PIN, LOW);
    digitalWrite(LED_DER_PIN, LOW);
    dibujar(CS_IZQ_PIN, vacio);
    dibujar(CS_DER_PIN, vacio);
    display.clearDisplay();
    display.display();
    melodiaActiva = false;
    estadoActual  = 0;
    Serial.println("Sistema apagado");
  }

  if (!switchEncendido) {
    delay(100);
    return;
  }

  // ── Sistema activo ──────────────────────────────────────────
  unsigned long ahora = millis();

  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());
  }

  int16_t ax, ay, az;
  leerSensor(&ax, &ay, &az);
  int difZ = az - z_ref;
  int difX = ax - x_ref;
  bool hayVibracion = detectarVibracion(ax, ay, az);

  if (ahora - lastAlertaCaida > COOLDOWN_CAIDA) {
    if (detectarCaida(ax, ay, az, ahora)) {
      enviarAlertaCaida();
      lastAlertaCaida = ahora;
    }
  }

  if (ahora - lastLecturPresion >= INTERVALO_PRESION) {
    if (detectarLluvia() && !melodiaActiva) {
      iniciarMelodia();
    }
    lastLecturPresion = ahora;
  }

  if (gps.location.isValid()) {
    float velActual = gps.speed.kmph();
    if (velActual < 1.0f) velActual = 0.0f;
    velFiltrada = (velActual * alpha) + (velFiltrada * (1.0f - alpha));

    if (velActual > 1.5f || (velActual > 0.5f && hayVibracion)) {
      if (latAnt != 0) {
        distanciaTotal += calcularHaversine(latAnt, lonAnt,
                                            gps.location.lat(),
                                            gps.location.lng());
      }
      latAnt = gps.location.lat();
      lonAnt = gps.location.lng();
    }
  }

  if (difX > umbralAdelante) estadoActual = 0;
  if (difZ < -umbralGiro && estadoActual != 1) {
    estadoActual = 1;
    tiempoInicio = ahora;
  } else if (difZ > umbralGiro && estadoActual != 2) {
    estadoActual = 2;
    tiempoInicio = ahora;
  }

  // ── Detección de clicks (simple y doble) ───────────────────
  bool botonPresionado = (digitalRead(BUTTON_PIN) == LOW);

  // Flanco descendente: botón presionado
  if (botonPresionado && !botonAnterior) {
    contadorClicks++;
    if (contadorClicks == 1) {
      primerClick = ahora;
    }
  }
  botonAnterior = botonPresionado;

  // Ventana de doble click cerrada → resolver acción
  if (contadorClicks > 0 && !botonPresionado && ahora - primerClick >= VENTANA_DOBLE_CLICK) {
    if (ahora - lastSOS > 3000) {
      if (contadorClicks == 1) {
        Serial.println(">> 1 click → SOS");
        enviarAlertaTelegram();
        tone(BUZZER_PIN, 1000, 200);
      } else if (contadorClicks >= 2) {
        Serial.println(">> 2 clicks → INFO");
        enviarInfoTelegram();
        tone(BUZZER_PIN, 1200, 100);
        delay(150);
        tone(BUZZER_PIN, 1200, 100);
      }
      lastSOS = ahora;
    }
    contadorClicks = 0;
    primerClick    = 0;
  }

  // ── Buzzer y matrices ───────────────────────────────────────
  if (melodiaActiva) {
    actualizarMelodia(ahora);
  } else if (estadoActual != 0) {
    if (ahora - tiempoInicio > duracionSinal && abs(difZ) < umbralGiro) {
      estadoActual = 0;
    } else {
      bool encendido = (ahora / 300) % 2;
      unsigned long fase = ahora % 300;
      if (fase < 50) {
        tone(BUZZER_PIN, encendido ? 1200 : 600);
      } else {
        noTone(BUZZER_PIN);
      }
      if (estadoActual == 1) {
        dibujar(CS_IZQ_PIN, encendido ? f_izq : vacio);
        digitalWrite(LED_IZQ_PIN, encendido);
        dibujar(CS_DER_PIN, vacio);
        digitalWrite(LED_DER_PIN, LOW);
      } else {
        dibujar(CS_DER_PIN, encendido ? f_der : vacio);
        digitalWrite(LED_DER_PIN, encendido);
        dibujar(CS_IZQ_PIN, vacio);
        digitalWrite(LED_IZQ_PIN, LOW);
      }
    }
  } else {
    dibujar(CS_IZQ_PIN, f_rec);
    dibujar(CS_DER_PIN, f_rec);
    digitalWrite(LED_IZQ_PIN, LOW);
    digitalWrite(LED_DER_PIN, LOW);
    noTone(BUZZER_PIN);
  }

  if (ahora - ultimaActPantalla >= INTERVALO_OLED) {
    actualizarPantalla(hayVibracion);
    ultimaActPantalla = ahora;
  }

  delay(40);
}