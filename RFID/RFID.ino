/*
   Conexiones para el módulo MFRC522 con ESP32:

   Módulo MFRC522  -   ESP32
   3.3V             -   3.3V
   RST              -   27
   GND              -   GND
   IRQ              -   No utilizado
   MISO             -   19
   MOSI             -   23
   SCK              -   18
   SDA (SS)         -   15
*/

#include "SPI.h"          // Incluimos la librería SPI para la comunicación serial periférica
#include "MFRC522.h"      // Incluimos la librería MFRC522 para interactuar con el lector RFID

#define RST_PIN  27       // Definimos el pin de reinicio del lector RFID
#define SS_PIN   15       // Definimos el pin de selección (SS) del lector RFID

MFRC522 mfrc522(SS_PIN, RST_PIN); // Creamos una instancia del lector RFID

void setup() {
    Serial.begin(115200);           // Inicializamos la comunicación serial con una velocidad de 115200 baudios
    while (!Serial);                // Esperamos a que se abra el monitor serial
    SPI.begin();                    // Inicializamos la comunicación SPI
    mfrc522.PCD_Init();             // Inicializamos el lector RFID
    mfrc522.PCD_SetAntennaGain(110);  // cambia al maximo la ganancia de la antena 
                                    // (permite leer a mayor distancia) OPCIONAL
                                    // si el lector es el V133 quizas.... pueda hacerlo funcionar
}

void loop() {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) { // Verificamos si hay una nueva tarjeta RFID presente y la leemos
        Serial.println("Contenido de la tarjeta:"); // Imprimimos un mensaje en el monitor serial
        
        // Iteramos sobre cada byte del UUID de la tarjeta RFID
        for (byte i = 0; i < mfrc522.uid.size; i++) {
            // Imprimimos cada byte en formato hexadecimal
            Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "); // Añadimos un 0 delante si el byte es menor que 0x10
            Serial.print(mfrc522.uid.uidByte[i], HEX);
        }
        Serial.println(); // Imprimimos un salto de línea
        
        mfrc522.PICC_HaltA();    // Detenemos la comunicación con la tarjeta RFID
        mfrc522.PCD_StopCrypto1();// Detenemos la encriptación
    }
    delay(1000); // Esperamos 1 segundo antes de realizar la próxima lectura
}