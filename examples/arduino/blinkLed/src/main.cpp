#include <Arduino.h>

#define LED 2

// put function definitions here:
int myFunction(int x, int y)
{
  return x + y;
}

int result = 0;
// put function declarations here:

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  result = myFunction(2, 3);
  // Set pin mode
  Serial.print("GPIO-FLag on ");
  Serial.println(LED);
  pinMode(LED, OUTPUT);
}

void loop()
{
  delay(500);
  digitalWrite(LED, HIGH);
  delay(500);
  digitalWrite(LED, LOW);
}
