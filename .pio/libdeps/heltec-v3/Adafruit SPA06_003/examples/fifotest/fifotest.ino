/*!
 * @file fifotest.ino
 *
 * Demonstrates using the SPA06_003 FIFO functionality with I2C interface.
 * This example enables the FIFO, fills it up, then reads all data at once
 * when the FIFO becomes full.
 *
 * The FIFO can store up to 32 pressure and temperature measurements,
 * allowing for burst data collection and reducing I2C traffic.
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

  Serial.println("Adafruit SPA06_003 FIFO Test");

  // Try to initialize!
  if (!spa.begin()) {
    Serial.println("Failed to find SPA06_003 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("SPA06_003 Found!");

  // Configure for FIFO operation
  Serial.println("Configuring sensor for FIFO operation...");

  // Set moderate precision for faster sampling
  spa.setPressureOversampling(SPA06_003_OVERSAMPLE_8);     // 8x oversampling
  spa.setTemperatureOversampling(SPA06_003_OVERSAMPLE_8);  // 8x oversampling

  // Set high measurement rate to fill FIFO quickly
  spa.setPressureMeasureRate(SPA06_003_RATE_32);     // 32 Hz
  spa.setTemperatureMeasureRate(SPA06_003_RATE_32);  // 32 Hz

  // Enable FIFO and FIFO full interrupt
  spa.enableFIFO(true);
  spa.setInterruptSource(true, false,
                         false);  // Enable only FIFO full interrupt

  // Start continuous measurement
  spa.setMeasurementMode(SPA06_003_MEAS_CONTINUOUS_BOTH);

  Serial.println("FIFO enabled and continuous measurement started");
  Serial.println("Waiting for FIFO to fill...");
  Serial.println("FIFO holds up to 32 measurements");
  Serial.println();
}

void loop() {
  // Check if FIFO is full
  if (spa.isFIFOFull()) {
    Serial.println("*** FIFO IS FULL! ***");
    Serial.println("Reading all data from FIFO...");

    uint8_t count = 0;

    // Read all data from FIFO until empty
    uint32_t last_temp_raw =
        0;  // Store last temperature for pressure compensation
    while (!spa.isFIFOEmpty() && count < 64) {  // Safety limit
      // Read raw 24-bit data from FIFO (always from pressure registers)
      uint32_t raw_data = spa.getPressureData();

      // Check LSB to determine measurement type
      bool is_pressure = (raw_data & 0x01) == 1;

      // Clear the LSB for actual measurement calculation and properly handle
      // sign extension
      uint32_t measurement_data = raw_data & 0xFFFFFE;

      // After clearing LSB, we now have a 23-bit signed value
      // Check if bit 22 is set (sign bit of the 23-bit number)
      if (measurement_data & 0x400000) {
        // Sign extend from 23 bits to 32 bits
        measurement_data |= 0xFF800000;
      }

      Serial.print("Sample ");
      Serial.print(count + 1);
      Serial.print(": ");

      if (is_pressure) {
        // This is pressure data - need temperature for compensation
        float pressure = spa.calculatePressure(measurement_data, last_temp_raw);
        Serial.print("Pressure=");
        Serial.print(pressure, 2);
        Serial.println(" hPa");
      } else {
        // This is temperature data
        last_temp_raw = measurement_data;  // Store for pressure compensation
        float temperature = spa.calculateTemperature(measurement_data);
        Serial.print("Temperature=");
        Serial.print(temperature, 2);
        Serial.println(" °C");
      }

      count++;
      delay(10);  // Small delay between reads
    }

    Serial.print("Total samples read from FIFO: ");
    Serial.println(count);
    Serial.println();
    Serial.println("FIFO emptied. Waiting for next fill...");
    Serial.println("-----------------------------------");
    Serial.println();
  }

  // Check FIFO status periodically
  static unsigned long lastStatusCheck = 0;
  if (millis() - lastStatusCheck > 1000) {  // Check every second
    Serial.print("FIFO Status - ");
    Serial.print("Empty: ");
    Serial.print(spa.isFIFOEmpty() ? "Yes" : "No");
    Serial.print(", Full: ");
    Serial.print(spa.isFIFOFull() ? "Yes" : "No");
    Serial.print(", Enabled: ");
    Serial.println(spa.isFIFOEnabled() ? "Yes" : "No");

    lastStatusCheck = millis();
  }

  delay(100);  // Check FIFO status every 100ms
}