/*
  Basic test sketch for SPA06_003 Digital Pressure Sensor

  This sketch initializes the sensor and verifies communication.

  Written by Limor 'ladyada' Fried with assistance from Claude Code
  for Adafruit Industries.
  MIT license, check license.txt for more information
*/

#include <Adafruit_SPA06_003.h>

Adafruit_SPA06_003 spa;

void printMeasureRate(spa06_003_rate_t rate) {
  switch (rate) {
    case SPA06_003_RATE_1:
      Serial.println(F("1 measurements per second"));
      break;
    case SPA06_003_RATE_2:
      Serial.println(F("2 measurements per second"));
      break;
    case SPA06_003_RATE_4:
      Serial.println(F("4 measurements per second"));
      break;
    case SPA06_003_RATE_8:
      Serial.println(F("8 measurements per second"));
      break;
    case SPA06_003_RATE_16:
      Serial.println(F("16 measurements per second"));
      break;
    case SPA06_003_RATE_32:
      Serial.println(F("32 measurements per second"));
      break;
    case SPA06_003_RATE_64:
      Serial.println(F("64 measurements per second"));
      break;
    case SPA06_003_RATE_128:
      Serial.println(F("128 measurements per second"));
      break;
    case SPA06_003_RATE_25_16:
      Serial.println(F("25/16 samples per second"));
      break;
    case SPA06_003_RATE_25_8:
      Serial.println(F("25/8 samples per second"));
      break;
    case SPA06_003_RATE_25_4:
      Serial.println(F("25/4 samples per second"));
      break;
    case SPA06_003_RATE_25_2:
      Serial.println(F("25/2 samples per second"));
      break;
    case SPA06_003_RATE_25:
      Serial.println(F("25 samples per second"));
      break;
    case SPA06_003_RATE_50:
      Serial.println(F("50 samples per second"));
      break;
    case SPA06_003_RATE_100:
      Serial.println(F("100 samples per second"));
      break;
    case SPA06_003_RATE_200:
      Serial.println(F("200 samples per second"));
      break;
    default:
      Serial.println(F("Unknown rate"));
      break;
  }
}

void printOversampling(spa06_003_oversample_t prc) {
  switch (prc) {
    case SPA06_003_OVERSAMPLE_1:
      Serial.println(F("Single"));
      break;
    case SPA06_003_OVERSAMPLE_2:
      Serial.println(F("2 times"));
      break;
    case SPA06_003_OVERSAMPLE_4:
      Serial.println(F("4 times"));
      break;
    case SPA06_003_OVERSAMPLE_8:
      Serial.println(F("8 times"));
      break;
    case SPA06_003_OVERSAMPLE_16:
      Serial.println(F("16 times"));
      break;
    case SPA06_003_OVERSAMPLE_32:
      Serial.println(F("32 times"));
      break;
    case SPA06_003_OVERSAMPLE_64:
      Serial.println(F("64 times"));
      break;
    case SPA06_003_OVERSAMPLE_128:
      Serial.println(F("128 times"));
      break;
    default:
      Serial.println(F("Unknown"));
      break;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  Serial.println("SPA06_003 test!");

  if (!spa.begin()) {
    Serial.println("Could not find a valid SPA06_003 sensor, check wiring!");
    while (1)
      delay(10);
  }

  Serial.println(F("SPA06_003 sensor found and initialized!"));

  Serial.println(F("Setting measurement mode to continuous both..."));
  spa.setMeasurementMode(SPA06_003_MEAS_CONTINUOUS_BOTH);

  Serial.print(F("Current measurement mode: "));
  spa06_003_meas_mode_t current_mode = spa.getMeasurementMode();

  switch (current_mode) {
    case SPA06_003_MEAS_IDLE:
      Serial.println(F("Idle"));
      break;
    case SPA06_003_MEAS_PRESSURE:
      Serial.println(F("Pressure (Command Mode)"));
      break;
    case SPA06_003_MEAS_TEMPERATURE:
      Serial.println(F("Temperature (Command Mode)"));
      break;
    case SPA06_003_MEAS_CONTINUOUS_PRESSURE:
      Serial.println(F("Continuous pressure"));
      break;
    case SPA06_003_MEAS_CONTINUOUS_TEMPERATURE:
      Serial.println(F("Continuous temperature"));
      break;
    case SPA06_003_MEAS_CONTINUOUS_BOTH:
      Serial.println(F("Continuous both"));
      break;
    default:
      Serial.println(F("Unknown"));
      break;
  }

  spa.enableFIFO(false);

  spa.setInterruptPolarity(SPA06_003_INT_ACTIVE_HIGH);

  spa.setInterruptSource(false /*fifo*/, true /*temp_ready*/,
                         true /*pres_ready*/);

  spa.setTemperatureOversampling(SPA06_003_OVERSAMPLE_8);

  spa06_003_oversample_t temp_prc = spa.getTemperatureOversampling();
  Serial.print(F("Current temperature oversampling: "));
  printOversampling(temp_prc);

  spa.setTemperatureMeasureRate(SPA06_003_RATE_64);

  spa06_003_rate_t temp_rate = spa.getTemperatureMeasureRate();
  Serial.print(F("Current temperature measurement rate: "));
  printMeasureRate(temp_rate);

  spa.setPressureMeasureRate(SPA06_003_RATE_128);

  spa06_003_rate_t current_rate = spa.getPressureMeasureRate();
  Serial.print(F("Current pressure measurement rate: "));
  printMeasureRate(current_rate);

  spa.setPressureOversampling(SPA06_003_OVERSAMPLE_8);

  spa06_003_oversample_t current_prc = spa.getPressureOversampling();
  Serial.print(F("Current pressure oversampling: "));
  printOversampling(current_prc);
}

void loop() {
  if (spa.isTempDataReady()) {
    Serial.print(F("Temperature: "));
    Serial.print(spa.readTemperature());
    Serial.print(F("°C"));
  }

  if (spa.isPresDataReady()) {
    Serial.print(F(", Pressure: "));
    Serial.print(spa.readPressure());
    Serial.print(F(" hPa"));
  }

  uint8_t status_flags = spa.getStatusFlags();
  Serial.print(F(", Status flags: 0x"));
  Serial.print(status_flags, HEX);
  Serial.print(F(" ["));

  if (status_flags & SPA06_003_INT_FIFO_FULL) {
    Serial.print(F("FIFO_FULL "));
  }
  if (status_flags & SPA06_003_INT_TMP_RDY) {
    Serial.print(F("TMP_RDY "));
  }
  if (status_flags & SPA06_003_INT_PRS_RDY) {
    Serial.print(F("PRS_RDY "));
  }
  if (status_flags == 0) {
    Serial.print(F("NONE"));
  }

  Serial.println(F("]"));

  Serial.print(F(", FIFO: "));
  if (spa.isFIFOEnabled()) {
    Serial.print(F("enabled "));
    if (spa.isFIFOEmpty()) {
      Serial.print(F("(empty)"));
    } else if (spa.isFIFOFull()) {
      Serial.print(F("(full)"));
    } else {
      Serial.print(F("(partial)"));
    }
  } else {
    Serial.print(F("disabled"));
  }
  Serial.println();

  delay(1000);
}