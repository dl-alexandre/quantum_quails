#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <RTClib.h>
#include "SparkFun_SHTC3.h"
#include <TaskScheduler.h>
#include <EEPROM.h>
#include "SerialCommands.h"
#include "globals.h"

#define CS_PIN 10
#define LED_PIN 4

Scheduler runner;

enum InitStage { INIT_SERIAL, INIT_RTC, INIT_SD, INIT_SHTC3, INIT_LOGFILE, INIT_DONE };
InitStage initStage = INIT_SERIAL;
unsigned long lastInitAttempt = 0;
bool initFailed = false;

void resetDailyLightTimerIfNewDay(DateTime now) {
  if (now.day() != state.lastLoggedDay) {
    state.lightOnMinutes = 0;
    state.lastLoggedDay = now.day();
  }
}

void readSensorsAndControl() {
  DateTime now = rtc.now();
  resetDailyLightTimerIfNewDay(now);

  if (!state.manualOverride) {
    if (shtc3.update() == SHTC3_Status_Nominal) {
      sensors.humidity = shtc3.toPercent();
      sensors.temperature = shtc3.toDegC();
    } else {
      Serial.println(F("❌ Failed to read SHTC3 sensor."));
      return;
    }

    Serial.print(F("🌡️ Controlling with Temp: "));
    Serial.println(sensors.temperature);

    if (sensors.temperature < lowTempThreshold - hysteresis) {
      state.heating = true;
      state.cooling = false;
    } else if (sensors.temperature > highTempThreshold + hysteresis) {
      state.heating = false;
      state.cooling = true;
    }

    static bool coolingPrev = false;
    if (state.cooling && !coolingPrev) {
      state.coolingTriggerMillis = millis();
    }
    coolingPrev = state.cooling;

    // Activate pump slightly before fan
    if (state.cooling && (millis() - state.coolingTriggerMillis >= pumpLeadTime)) {
      state.pumping = true;
    } else {
      state.pumping = false;
    }

    state.pump = state.pumping ? HIGH : LOW;
    state.heat = state.heating ? HIGH : LOW;
    state.fan = state.cooling ? HIGH : LOW;

    if (state.heating) {
    state.lighting = true;
    state.lightHeat = true;
  } else {
    state.lightHeat = false;
    state.lighting = (!state.cooling && state.lightOnMinutes < dailyLightQuota);
  }

  state.light = state.lighting ? HIGH : LOW;

    digitalWrite(heat, state.heat);
    digitalWrite(fan, state.fan);
    digitalWrite(pump, state.pump);

    if (state.light != state.previousLightState) {
      digitalWrite(light, state.light);
      state.previousLightState = state.light;
    }

    Serial.print(F("🔥 Heat: ")); Serial.println(state.heat);
    Serial.print(F("🌀 Fan: ")); Serial.println(state.fan);
    Serial.print(F("💡 Light: ")); Serial.println(state.light);
    Serial.print(F("⏱️ Light Minutes: ")); Serial.println(state.lightOnMinutes);
  }
}

bool enableLogging = true;

void logData() {
  Serial.println(F("📝 logData() called"));
  DateTime now = rtc.now();

  if (state.light == HIGH && !state.lightHeat) {
    state.lightOnMinutes++;
  }
  EEPROM.put(EEPROM_LIGHT_MIN_ADDR, state.lightOnMinutes);

  if (state.light == HIGH && state.lightHeat) {
    state.lightHeatMinutes++;
  }
  EEPROM.put(EEPROM_LIGHT_HEAT_MIN_ADDR, state.lightHeatMinutes);

  if (!enableLogging) return;

  File dataFile = SD.open(dataFilename, FILE_WRITE);
  if (dataFile) {
    Serial.println(F("📝 Logging values:"));
    Serial.print(F("Temp: ")); Serial.println(sensors.temperature);
    Serial.print(F("Humidity: ")); Serial.println(sensors.humidity);
    Serial.print(F("Heat: ")); Serial.println(state.heat);
    Serial.print(F("Fan: ")); Serial.println(state.fan);
    Serial.print(F("Light: ")); Serial.println(state.light);
    Serial.print(F("LightMinutes: ")); Serial.println(state.lightOnMinutes);
    Serial.print(F("Pump: ")); Serial.println(state.pump);
    Serial.print(F("LightHeatMinutes: ")); Serial.println(state.lightHeatMinutes);
    dataFile.print(now.year()); dataFile.print(",");
    dataFile.print(now.month()); dataFile.print(",");
    dataFile.print(now.day()); dataFile.print(",");
    dataFile.print(now.hour()); dataFile.print(",");
    dataFile.print(now.minute()); dataFile.print(",");
    dataFile.print(now.second()); dataFile.print(",");
    dataFile.print(sensors.temperature); dataFile.print(",");
    dataFile.print(sensors.humidity); dataFile.print(",");
    dataFile.print(state.heat); dataFile.print(",");
    dataFile.print(state.fan); dataFile.print(",");
    dataFile.print(state.light); dataFile.print(",");
    dataFile.print(state.lightOnMinutes); dataFile.print(",");
    dataFile.print(state.pump); dataFile.print(",");
    dataFile.println(state.lightHeatMinutes);
    dataFile.close();
    Serial.println(F("✅ Data logged."));
  } else {
    Serial.println(F("❌ Failed to open log file"));
  }
}

Task taskSensors(MSEC_PER_MIN, TASK_FOREVER, &readSensorsAndControl);
Task taskLogger(10 * MSEC_PER_MIN, TASK_FOREVER, &logData);

void setup() {
  Serial.begin(serialBaud);
  pinMode(heat, OUTPUT);
  pinMode(fan, OUTPUT);
  pinMode(light, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(pump, OUTPUT);
  digitalWrite(heat, LOW);
  digitalWrite(fan, LOW);
  digitalWrite(light, LOW);
  digitalWrite(pump, LOW);
  EEPROM.get(EEPROM_LIGHT_MIN_ADDR, state.lightOnMinutes);
  EEPROM.get(EEPROM_LIGHT_HEAT_MIN_ADDR, state.lightHeatMinutes);
}

void loop() {
  unsigned long nowMillis = millis();

  if (initFailed) {
    digitalWrite(LED_PIN, (nowMillis / 500) % 2);
    return;
  }

  if (initStage != INIT_DONE && nowMillis - lastInitAttempt > 250) {
    lastInitAttempt = nowMillis;

    switch (initStage) {
      case INIT_SERIAL:
        Serial.println(F("🔧 Starting system..."));
        Wire.begin();
        initStage = INIT_RTC;
        break;

      case INIT_RTC:
        Serial.println(F("🔎 Initializing RTC..."));
        if (rtc.begin()) {
          Serial.println(F("✅ RTC OK"));
          initStage = INIT_SD;
        } else {
          Serial.println(F("❌ RTC failed"));
          break;
        }
        break;

      case INIT_SD:
        Serial.println(F("🔎 Initializing SD..."));
        if (SD.begin(CS_PIN)) {
          Serial.println(F("✅ SD OK"));
          initStage = INIT_SHTC3;
        } else {
          Serial.println(F("❌ SD failed"));
          break;
        }
        break;

      case INIT_SHTC3:
        Serial.println(F("🔎 Initializing SHTC3..."));
        if (shtc3.begin() == SHTC3_Status_Nominal) {
          Serial.println(F("✅ SHTC3 OK"));
          initStage = INIT_LOGFILE;
        } else {
          Serial.println(F("❌ SHTC3 failed"));
          break;
        }
        break;

      case INIT_LOGFILE:
        if (!SD.exists(dataFilename)) {
          File f = SD.open(dataFilename, FILE_WRITE);
          if (f) {
            f.println(csvHeader);
            f.close();
            Serial.println(F("✅ Created CSV file."));
          } else {
            Serial.println(F("❌ Failed to create CSV file"));
            break;
          }
        } else {
          Serial.println(F("ℹ️ Log file exists."));
        }

        runner.init();
        runner.addTask(taskSensors);
        runner.addTask(taskLogger);
        taskSensors.enable();
        taskLogger.enable();

        DateTime now = rtc.now();
        Serial.print(F("🕒 RTC Now: "));
        Serial.print(now.year()); Serial.print("-");
        Serial.print(now.month()); Serial.print("-");
        Serial.print(now.day()); Serial.print(" ");
        Serial.print(now.hour()); Serial.print(":");
        Serial.print(now.minute()); Serial.print(":");
        Serial.println(now.second());

        Serial.println(F("✅ System ready."));
        initStage = INIT_DONE;
        break;

      default: break;
    }
  }

  if (state.testFanActive && millis() - state.testFanStart >= FAN_TEST_DURATION) {
    digitalWrite(fan, LOW);
    state.testFanActive = false;
    Serial.println(F("🧪 Fan test ended"));
  }

  if (initStage == INIT_DONE) {
    runner.execute();
    handleSerialCommands();
    if (state.manualOverride && (millis() - state.lastManualCommandTime > MANUAL_TIMEOUT)) {
      state.manualOverride = false;
      Serial.println(F("⏳ Manual override timed out — returning to AUTONOMOUS mode"));
    }
  }
}