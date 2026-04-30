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
    
    // Agregamos un mensaje inicial para saber que arrancó bien y no ver la pantalla en blanco
    Serial.println("\nIniciando sistema...");
    
    SPI.begin();                    // Inicializamos la comunicación SPI
    mfrc522.PCD_Init();             // Inicializamos el lector RFID
    
    // Usamos la constante oficial de la librería para forzar la ganancia máxima de forma segura
    mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);  
    
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
    
    // --- NUEVO: CREAMOS LA VARIABLE PARA GUARDAR EL ID ---
    String idTarjeta = ""; 
    
    // Iteramos sobre cada byte del UUID de la tarjeta RFID y lo guardamos en la variable
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        // Añadimos un 0 delante si el byte es menor que 0x10 para mantener el formato
        idTarjeta += mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""; 
        // Convertimos el byte a hexadecimal y lo concatenamos
        idTarjeta += String(mfrc522.uid.uidByte[i], HEX);
    }
    
    // Pasamos todo el texto a mayúsculas para mayor prolijidad
    idTarjeta.toUpperCase();
    
    // Imprimimos la variable final guardada
    Serial.println("Variable guardada con éxito. Contenido:"); 
    Serial.println(idTarjeta); 
    
    // 4. Detenemos la comunicación actual de la tarjeta
    mfrc522.PICC_HaltA();    
    mfrc522.PCD_StopCrypto1();
    
    // 5. El delay va aquí adentro.
    // Así, el sistema solo se pausa por 1 segundo LUEGO de leer exitosamente, 
    // dándote tiempo de retirar la mano antes de volver a leer la misma tarjeta.
    delay(1000); 
}