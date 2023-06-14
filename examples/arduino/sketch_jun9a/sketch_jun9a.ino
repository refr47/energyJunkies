#include <SPI.h>
#include <SD.h>
const int chipSelect = SS;

void setup() {
  // put your setup code here, to run once:

    Serial.begin(115200);
   while (!Serial)
     ;
    Serial.println("Hello T-Display-S3");
    Serial.print("CS: ");
    Serial.println(SS);
    Serial.print("MOSI: ");
    Serial.println(MOSI);
    Serial.print("MISO: ");
    Serial.println(MISO);
    Serial.print("SCK: ");
    Serial.println(SCK);
    Serial.print("SS: ");
    Serial.println(SS);
  
  
    if (!SD.begin(chipSelect))
    {
      Serial.println("initialization failed!");
      return;
    }
    else
    {
      Serial.println("INIT done.");
    }
    Serial.println("initialization done.");

}

void loop() {
  // put your main code here, to run repeatedly:

}
