/*!
 * @file simpletest.ino
 *
 * A simple test for the SPA06_003 pressure and temperature sensor.
 * This example shows basic pressure and temperature readings using the sensor.
 *
 * The begin() function automatically configures the sensor for:
 * - Highest precision (128x oversampling)
 * - Highest sample rate (200 Hz)
 * - Continuous measurement mode
 *
 * Designed specifically to work with the Adafruit SPA06_003 Breakout
 * ----> https://www.adafruit.com/products/xxxx
 *
 * These sensors use I2C to communicate, 2 pins are required to interface.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code
 * for Adafruit Industries.
 * MIT license, check license.txt for more information
 * All text above must be included in any redistribution
 */

#include <Adafruit_SPA06_003.h>

Adafruit_SPA06_003 spa;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);  // will pause Zero, Leonardo, etc until serial console opens

  Serial.println("Adafruit SPA06_003 Simple Test");

  // Try to initialize!
  if (!spa.begin()) {
    Serial.println("Failed to find SPA06_003 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("SPA06_003 Found!");
}

void loop() {
  // Only read and print when new data is available
  if (spa.isTempDataReady() || spa.isPresDataReady()) {
    float temperature = spa.readTemperature();
    float pressure = spa.readPressure();

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" °C");

    Serial.print("\tPressure: ");
    Serial.print(pressure);
    Serial.println(" hPa");
  }

  delay(10);  // Short delay to avoid overwhelming the sensor
}