/*!
 * @file spi_test.ino
 *
 * Demonstrates using the SPA06_003 sensor with SPI interface.
 * This example shows both hardware and software SPI configurations.
 *
 * The begin() function automatically configures the sensor for:
 * - Highest precision (128x oversampling)
 * - Highest sample rate (200 Hz)
 * - Continuous measurement mode
 * - Interrupt-based data ready detection
 *
 * Designed specifically to work with the Adafruit SPA06_003 Breakout
 * ----> https://www.adafruit.com/products/xxxx
 *
 * These sensors support both I2C and SPI interfaces.
 * For SPI connections:
 * - CS   -> pin 10 (or any digital pin)
 * - MOSI -> pin 11
 * - MISO -> pin 12
 * - SCK  -> pin 13
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code
 * for Adafruit Industries.
 * MIT license, check license.txt for more information
 * All text above must be included in any redistribution
 */

#include <Adafruit_SPA06_003.h>

Adafruit_SPA06_003 spa;

// SPI pin definitions
#define SPA_CS_PIN 10

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);  // will pause Zero, Leonardo, etc until serial console opens

  Serial.println("Adafruit SPA06_003 SPI Test");

  // Try to initialize the sensor using hardware SPI
  // CS pin = 10, default SPI instance
  if (!spa.begin(SPA_CS_PIN, &SPI)) {
    Serial.println("Failed to find SPA06_003 chip via hardware SPI");
    Serial.println("Check wiring and try software SPI example");
    while (1) {
      delay(10);
    }
  }
  Serial.println("SPA06_003 Found via Hardware SPI!");

  // Alternative: Software SPI (uncomment to use instead of hardware SPI)
  // Default ATmega328 pins: MOSI=11, MISO=12, SCK=13, CS=10
  // if (!spa.begin(10, 11, 12, 13)) {
  //   Serial.println("Failed to find SPA06_003 chip via software SPI");
  //   while (1) { delay(10); }
  // }
  // Serial.println("SPA06_003 Found via Software SPI!");

  Serial.println("Starting continuous measurement...");
  Serial.println("Temperature and pressure readings:");
  Serial.println("Format: Temp (°C) | Pressure (hPa)");
  Serial.println("-----------------------------------");
}

void loop() {
  // Check if new temperature data is available
  if (spa.isTempDataReady()) {
    float temperature = spa.readTemperature();
    Serial.print(temperature, 2);
    Serial.print(" °C");
  }

  // Check if new pressure data is available
  if (spa.isPresDataReady()) {
    float pressure = spa.readPressure();
    Serial.print(" | ");
    Serial.print(pressure, 2);
    Serial.println(" hPa");
  }

  delay(100);  // Small delay between checks
}