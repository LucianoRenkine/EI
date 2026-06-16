#define LED_PIN 4
 // Usá el GPIO que prefieras (4, 5, 18, 19, etc.)

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
}

void loop() {
  // No hace falta nada acá
}