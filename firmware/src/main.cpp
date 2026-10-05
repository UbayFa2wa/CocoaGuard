#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ======================================================
// COCOAGUARD
// FINAL COMPETITION VIRTUAL PROTOTYPE
// XIAO ESP32-S3 + WOKWI
// ======================================================
//
// IMPORTANT:
// Current competition implementation uses adjustable
// virtual inputs to represent field sensors.
//
// Proposed physical implementation:
// - DHT22 -> Temperature + Humidity
// - Soil Moisture Sensor
// - Leaf Wetness Sensor
// - Rain Sensor
//
// ======================================================


// ======================================================
// PIN MAPPING
// ======================================================
//
// D0  = GPIO1   -> Temperature virtual input
// D1  = GPIO2   -> Soil Moisture virtual input
// D2  = GPIO3   -> Leaf Wetness virtual input
// D3  = GPIO4   -> Rain Intensity virtual input
// D4  = GPIO5   -> OLED SDA
// D5  = GPIO6   -> OLED SCL
// D6  = GPIO43  -> Green LED
// D7  = GPIO44  -> Yellow LED
// D8  = GPIO7   -> Red LED
// D9  = GPIO8   -> Buzzer
// D10 = GPIO9   -> Humidity virtual input
//
// ======================================================

constexpr int TEMP_PIN = 1;          // D0
constexpr int SOIL_PIN = 2;          // D1
constexpr int LEAF_WET_PIN = 3;      // D2
constexpr int RAIN_PIN = 4;          // D3

constexpr int OLED_SDA = 5;          // D4
constexpr int OLED_SCL = 6;          // D5

constexpr int LED_GREEN = 43;        // D6
constexpr int LED_YELLOW = 44;       // D7
constexpr int LED_RED = 7;           // D8

constexpr int BUZZER_PIN = 8;        // D9

constexpr int HUMIDITY_PIN = 9;      // D10


// ======================================================
// OLED
// ======================================================

constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);


// ======================================================
// BUZZER
// ======================================================

constexpr int BUZZER_FREQUENCY = 1800;
constexpr int BUZZER_RESOLUTION = 8;

#if ESP_ARDUINO_VERSION_MAJOR < 3
constexpr int BUZZER_CHANNEL = 0;
#endif


// ======================================================
// RISK LEVEL
// ======================================================

enum RiskLevel {
  LOW_RISK,
  MODERATE_RISK,
  HIGH_RISK
};


// ======================================================
// SENSOR DATA
// ======================================================

struct Reading {
  float temperature;
  float humidity;
  float soil;
  float leafWetness;
  float rain;
  float risk;
  RiskLevel level;
};


// ======================================================
// HELPER FUNCTIONS
// ======================================================

float clamp100(float value) {

  if (value < 0.0f) {
    return 0.0f;
  }

  if (value > 100.0f) {
    return 100.0f;
  }

  return value;
}


float normalizeAnalog(int rawValue) {

  return clamp100(
    (rawValue / 4095.0f) * 100.0f
  );
}


float mapFloat(
  float value,
  float inMin,
  float inMax,
  float outMin,
  float outMax
) {

  return
    (value - inMin)
    * (outMax - outMin)
    / (inMax - inMin)
    + outMin;
}


// ======================================================
// VIRTUAL TEMPERATURE SENSOR
// ======================================================
//
// Maps ADC:
// 0%   -> 10°C
// 100% -> 45°C
//
// ======================================================

float readVirtualTemperature() {

  int raw =
    analogRead(
      TEMP_PIN
    );

  float temperature =
    mapFloat(
      raw,
      0,
      4095,
      10.0f,
      45.0f
    );

  return temperature;
}


// ======================================================
// VIRTUAL HUMIDITY SENSOR
// ======================================================
//
// Maps ADC:
// 0%   -> 20% RH
// 100% -> 100% RH
//
// ======================================================

float readVirtualHumidity() {

  int raw =
    analogRead(
      HUMIDITY_PIN
    );

  float humidity =
    mapFloat(
      raw,
      0,
      4095,
      20.0f,
      100.0f
    );

  return clamp100(
    humidity
  );
}


// ======================================================
// COCOAGUARD ENVIRONMENTAL RISK ENGINE
// ======================================================
//
// Prototype environmental model.
//
// LOW      < 35
// MODERATE 35 - 69.9
// HIGH     >= 70
//
// NOT a laboratory disease diagnosis.
//
// ======================================================

float calculateRisk(
  float temperature,
  float humidity,
  float soil,
  float leafWetness,
  float rain
) {

  // ----------------------------------
  // HUMIDITY RISK
  // ----------------------------------

  float humidityRisk =
    clamp100(
      (
        (humidity - 60.0f)
        / 35.0f
      )
      * 100.0f
    );


  // ----------------------------------
  // TEMPERATURE RISK
  // ----------------------------------

  float temperatureRisk = 0.0f;


  if (temperature < 20.0f) {

    temperatureRisk =
      clamp100(
        (
          (20.0f - temperature)
          / 8.0f
        )
        * 100.0f
      );
  }


  else if (temperature > 32.0f) {

    temperatureRisk =
      clamp100(
        (
          (temperature - 32.0f)
          / 10.0f
        )
        * 100.0f
      );
  }


  // ----------------------------------
  // SOIL RISK
  // ----------------------------------

  float soilRisk = 0.0f;


  if (soil < 25.0f) {

    soilRisk =
      clamp100(
        (
          (25.0f - soil)
          / 25.0f
        )
        * 100.0f
      );
  }


  else if (soil > 80.0f) {

    soilRisk =
      clamp100(
        (
          (soil - 80.0f)
          / 20.0f
        )
        * 100.0f
      );
  }


  // ----------------------------------
  // WEIGHTED RISK
  // ----------------------------------

  float risk =

      (0.30f * leafWetness)

    + (0.25f * humidityRisk)

    + (0.20f * rain)

    + (0.15f * soilRisk)

    + (0.10f * temperatureRisk);


  // ----------------------------------
  // CRITICAL OVERRIDE
  // ----------------------------------

  if (
    leafWetness >= 90.0f
    &&
    humidity >= 90.0f
  ) {

    risk = max(
      risk,
      85.0f
    );
  }


  if (
    rain >= 95.0f
    &&
    leafWetness >= 85.0f
  ) {

    risk = max(
      risk,
      85.0f
    );
  }


  return clamp100(
    risk
  );
}


// ======================================================
// CLASSIFY RISK
// ======================================================

RiskLevel classifyRisk(float risk) {

  if (risk < 35.0f) {
    return LOW_RISK;
  }

  if (risk < 70.0f) {
    return MODERATE_RISK;
  }

  return HIGH_RISK;
}


// ======================================================
// RISK TEXT
// ======================================================

const char* levelText(
  RiskLevel level
) {

  switch (level) {

    case LOW_RISK:
      return "LOW";

    case MODERATE_RISK:
      return "MODERATE";

    case HIGH_RISK:
      return "HIGH";

    default:
      return "UNKNOWN";
  }
}


// ======================================================
// BUZZER SETUP
// ======================================================

void setupBuzzer() {

#if ESP_ARDUINO_VERSION_MAJOR >= 3

  ledcAttach(
    BUZZER_PIN,
    BUZZER_FREQUENCY,
    BUZZER_RESOLUTION
  );

  ledcWriteTone(
    BUZZER_PIN,
    0
  );

#else

  ledcSetup(
    BUZZER_CHANNEL,
    BUZZER_FREQUENCY,
    BUZZER_RESOLUTION
  );

  ledcAttachPin(
    BUZZER_PIN,
    BUZZER_CHANNEL
  );

  ledcWriteTone(
    BUZZER_CHANNEL,
    0
  );

#endif
}


// ======================================================
// BUZZER ON
// ======================================================

void buzzerOn() {

#if ESP_ARDUINO_VERSION_MAJOR >= 3

  ledcWriteTone(
    BUZZER_PIN,
    BUZZER_FREQUENCY
  );

#else

  ledcWriteTone(
    BUZZER_CHANNEL,
    BUZZER_FREQUENCY
  );

#endif
}


// ======================================================
// BUZZER OFF
// ======================================================

void buzzerOff() {

#if ESP_ARDUINO_VERSION_MAJOR >= 3

  ledcWriteTone(
    BUZZER_PIN,
    0
  );

#else

  ledcWriteTone(
    BUZZER_CHANNEL,
    0
  );

#endif
}


// ======================================================
// OUTPUT CONTROL
// ======================================================

void updateOutputs(
  RiskLevel level
) {

  // LOW

  if (level == LOW_RISK) {

    digitalWrite(
      LED_GREEN,
      HIGH
    );

    digitalWrite(
      LED_YELLOW,
      LOW
    );

    digitalWrite(
      LED_RED,
      LOW
    );

    buzzerOff();
  }


  // MODERATE

  else if (
    level == MODERATE_RISK
  ) {

    digitalWrite(
      LED_GREEN,
      LOW
    );

    digitalWrite(
      LED_YELLOW,
      HIGH
    );

    digitalWrite(
      LED_RED,
      LOW
    );

    buzzerOff();
  }


  // HIGH

  else {

    digitalWrite(
      LED_GREEN,
      LOW
    );

    digitalWrite(
      LED_YELLOW,
      LOW
    );

    digitalWrite(
      LED_RED,
      HIGH
    );

    buzzerOn();
  }
}


// ======================================================
// OLED
// ======================================================

void updateOLED(
  const Reading& reading
) {

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(
    1
  );

  display.setCursor(
    0,
    0
  );

  display.println(
    "COCOAGUARD"
  );


  display.setCursor(
    0,
    12
  );


  display.printf(
    "T:%.1fC H:%.0f%%\n",
    reading.temperature,
    reading.humidity
  );


  display.printf(
    "Soil:%.0f%% Leaf:%.0f%%\n",
    reading.soil,
    reading.leafWetness
  );


  display.printf(
    "Rain:%.0f%% Risk:%.0f\n",
    reading.rain,
    reading.risk
  );


  display.setTextSize(
    2
  );

  display.setCursor(
    0,
    48
  );

  display.print(
    levelText(
      reading.level
    )
  );


  display.display();
}


// ======================================================
// SEND DATA TO DASHBOARD
// ======================================================

void sendDashboardData(
  const Reading& reading
) {

  Serial.printf(

    "CGJSON:{"

    "\"temperature\":%.1f,"

    "\"humidity\":%.1f,"

    "\"soil\":%.1f,"

    "\"leaf_wetness\":%.1f,"

    "\"rain\":%.1f,"

    "\"risk\":%.1f,"

    "\"status\":\"%s\""

    "}\r\n",

    reading.temperature,

    reading.humidity,

    reading.soil,

    reading.leafWetness,

    reading.rain,

    reading.risk,

    levelText(
      reading.level
    )
  );
}


// ======================================================
// TERMINAL
// ======================================================

void printTerminal(
  const Reading& reading
) {

  Serial.print(
    "\r\n"
  );

  Serial.print(
    "================================\r\n"
  );

  Serial.print(
    "        COCOAGUARD LIVE\r\n"
  );

  Serial.print(
    "================================\r\n"
  );


  Serial.printf(
    "Temperature : %6.1f C\r\n",
    reading.temperature
  );


  Serial.printf(
    "Humidity    : %6.1f %%\r\n",
    reading.humidity
  );


  Serial.printf(
    "Soil        : %6.1f %%\r\n",
    reading.soil
  );


  Serial.printf(
    "Leaf Wet    : %6.1f %%\r\n",
    reading.leafWetness
  );


  Serial.printf(
    "Rain        : %6.1f %%\r\n",
    reading.rain
  );


  Serial.print(
    "--------------------------------\r\n"
  );


  Serial.printf(
    "Risk Score  : %6.1f / 100\r\n",
    reading.risk
  );


  Serial.printf(
    "Status      : %s\r\n",
    levelText(
      reading.level
    )
  );


  if (
    reading.level == HIGH_RISK
  ) {

    Serial.print(
      "Alert       : CRITICAL WARNING\r\n"
    );
  }


  else if (
    reading.level == MODERATE_RISK
  ) {

    Serial.print(
      "Alert       : MONITOR CONDITION\r\n"
    );
  }


  else {

    Serial.print(
      "Alert       : NORMAL\r\n"
    );
  }


  Serial.print(
    "Mode        : VIRTUAL SENSOR SIMULATION\r\n"
  );


  Serial.print(
    "================================\r\n"
  );
}


// ======================================================
// READ ALL VIRTUAL SENSOR INPUTS
// ======================================================

Reading takeReading() {

  Reading reading {};


  // Temperature

  reading.temperature =
    readVirtualTemperature();


  // Humidity

  reading.humidity =
    readVirtualHumidity();


  // Soil moisture

  reading.soil =
    normalizeAnalog(
      analogRead(
        SOIL_PIN
      )
    );


  // Leaf wetness

  reading.leafWetness =
    normalizeAnalog(
      analogRead(
        LEAF_WET_PIN
      )
    );


  // Rain intensity

  reading.rain =
    normalizeAnalog(
      analogRead(
        RAIN_PIN
      )
    );


  // Risk

  reading.risk =
    calculateRisk(
      reading.temperature,
      reading.humidity,
      reading.soil,
      reading.leafWetness,
      reading.rain
    );


  // Classification

  reading.level =
    classifyRisk(
      reading.risk
    );


  return reading;
}


// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(
    115200
  );

  delay(
    500
  );


  Serial.print(
    "\r\n"
  );

  Serial.print(
    "================================\r\n"
  );

  Serial.print(
    "      COCOAGUARD STARTING\r\n"
  );

  Serial.print(
    "================================\r\n"
  );


  // LEDs

  pinMode(
    LED_GREEN,
    OUTPUT
  );

  pinMode(
    LED_YELLOW,
    OUTPUT
  );

  pinMode(
    LED_RED,
    OUTPUT
  );


  digitalWrite(
    LED_GREEN,
    LOW
  );

  digitalWrite(
    LED_YELLOW,
    LOW
  );

  digitalWrite(
    LED_RED,
    LOW
  );


  // Buzzer

  setupBuzzer();


  // ADC

  analogReadResolution(
    12
  );


  // OLED

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );


  bool oledReady =
    display.begin(
      SSD1306_SWITCHCAPVCC,
      0x3C
    );


  if (!oledReady) {

    Serial.print(
      "OLED        : FAILED\r\n"
    );
  }


  else {

    Serial.print(
      "OLED        : READY\r\n"
    );


    display.clearDisplay();

    display.setTextColor(
      SSD1306_WHITE
    );

    display.setTextSize(
      2
    );

    display.setCursor(
      0,
      8
    );

    display.println(
      "COCOA"
    );

    display.println(
      "GUARD"
    );

    display.display();

    delay(
      1000
    );
  }


  Serial.print(
    "Sensor Mode : VIRTUAL INPUTS\r\n"
  );

  Serial.print(
    "System      : READY\r\n"
  );

  Serial.print(
    "================================\r\n"
  );
}


// ======================================================
// LOOP
// ======================================================

void loop() {

  Reading reading =
    takeReading();


  updateOutputs(
    reading.level
  );


  updateOLED(
    reading
  );


  sendDashboardData(
    reading
  );


  printTerminal(
    reading
  );


  delay(
    1500
  );
}