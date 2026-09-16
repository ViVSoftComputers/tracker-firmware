/*!
 * @file sensorapi.ino
 *
 * Demonstrates using the SPA06_003 sensor with Adafruit's Unified Sensor
 * interface. This example shows how to get sensor information and read values
 * using the standardized Adafruit Sensor API, which allows easy integration
 * with other sensor libraries and data logging systems.
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
 * These sensors use I2C to communicate, 2 pins are required to interface.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code
 * for Adafruit Industries.
 * MIT license, check license.txt for more information
 * All text above must be included in any redistribution
 */

#include <Adafruit_SPA06_003.h>
#include <Adafruit_Sensor.h>

Adafruit_SPA06_003 spa;

// Get references to the individual sensor objects
Adafruit_Sensor *spa_temp;
Adafruit_Sensor *spa_pressure;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);  // will pause Zero, Leonardo, etc until serial console opens

  Serial.println("Adafruit SPA06_003 Unified Sensor API Test");

  // Try to initialize the sensor
  if (!spa.begin()) {
    Serial.println("Failed to find SPA06_003 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("SPA06_003 Found!");

  // Get sensor objects for temperature and pressure
  spa_temp = spa.getTemperatureSensor();
  spa_pressure = spa.getPressureSensor();

  // Print sensor details
  printSensorDetails();
}

void loop() {
  // Read temperature using unified sensor interface
  sensors_event_t temp_event;
  if (spa_temp->getEvent(&temp_event)) {
    Serial.print("Temperature: ");
    Serial.print(temp_event.temperature);
    Serial.print(" °C");
  }

  // Read pressure using unified sensor interface
  sensors_event_t pressure_event;
  if (spa_pressure->getEvent(&pressure_event)) {
    Serial.print("\tPressure: ");
    Serial.print(pressure_event.pressure);
    Serial.print(" hPa");

    Serial.print("\tTimestamp: ");
    Serial.println(pressure_event.timestamp);
  }

  delay(500);  // Wait 500ms between readings
}

void printSensorDetails() {
  Serial.println("------------------------------------");

  // Print temperature sensor details
  sensor_t sensor;
  spa_temp->getSensor(&sensor);
  Serial.println("Temperature Sensor:");
  Serial.print("  Sensor Type: ");
  Serial.println(sensor.name);
  Serial.print("  Driver Ver:  ");
  Serial.println(sensor.version);
  Serial.print("  Unique ID:   ");
  Serial.println(sensor.sensor_id);
  Serial.print("  Max Value:   ");
  Serial.print(sensor.max_value);
  Serial.println(" °C");
  Serial.print("  Min Value:   ");
  Serial.print(sensor.min_value);
  Serial.println(" °C");
  Serial.print("  Resolution:  ");
  Serial.print(sensor.resolution);
  Serial.println(" °C");
  Serial.println("");

  // Print pressure sensor details
  spa_pressure->getSensor(&sensor);
  Serial.println("Pressure Sensor:");
  Serial.print("  Sensor Type: ");
  Serial.println(sensor.name);
  Serial.print("  Driver Ver:  ");
  Serial.println(sensor.version);
  Serial.print("  Unique ID:   ");
  Serial.println(sensor.sensor_id);
  Serial.print("  Max Value:   ");
  Serial.print(sensor.max_value);
  Serial.println(" hPa");
  Serial.print("  Min Value:   ");
  Serial.print(sensor.min_value);
  Serial.println(" hPa");
  Serial.print("  Resolution:  ");
  Serial.print(sensor.resolution);
  Serial.println(" hPa");
  Serial.println("");

  Serial.println("------------------------------------");
  Serial.println("");
}