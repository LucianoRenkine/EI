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
    Serial.begin(115200);           
    while (!Serial);                
    
    // Agregamos un mensaje inicial para saber que arrancó bien y no ver la pantalla en blanco
    Serial.println("\nIniciando sistema...");
    
    SPI.begin();                    
    mfrc522.PCD_Init();             
    mfrc522.PCD_SetAntennaGain(110);  
    
    Serial.println("Lector listo. Esperando tarjeta...");
}

void loop() {
    // 1. Preguntamos si hay una tarjeta. Si no la hay, el return hace que el loop vuelva a empezar al instante.
    // Esto se ejecuta miles de veces por segundo, logrando la detección inmediata.
    if (!mfrc522.PICC_IsNewCardPresent()) {
        return; 
    }

    // 2. Si detectó una tarjeta, intentamos leer su código.
    if (!mfrc522.PICC_ReadCardSerial()) {
        return; 
    }

    // 3. Si llegamos a esta línea, la lectura fue exitosa e instantánea
    Serial.println("Contenido de la tarjeta:"); 
    
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "); 
        Serial.print(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println(); 
    
    // 4. Detenemos la comunicación actual de la tarjeta
    mfrc522.PICC_HaltA();    
    mfrc522.PCD_StopCrypto1();
    
    // 5. EL CAMBIO CLAVE: El delay va aquí adentro.
    // Así, el sistema solo se pausa por 1 segundo LUEGO de leer exitosamente, 
    // dándote tiempo de retirar la mano antes de volver a leer la misma tarjeta.
    delay(1000); 
}