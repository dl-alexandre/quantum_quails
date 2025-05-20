#ifndef SERIAL_COMMANDS_H
#define SERIAL_COMMANDS_H

#include <Arduino.h>
#include <EEPROM.h>
#include <RTClib.h>
#include "globals.h"

// Functions from main file
extern void printDataFile();
extern void printLastLines(const char* filename, int numLines);

void printDataFile() {
  File dataFile = SD.open(dataFilename, FILE_READ);

  if (dataFile) {
    Serial.println(F("📄 --- Contents of DATA.CSV ---"));
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close();
    Serial.println(F("📄 --- End of File ---"));
  } else {
    Serial.println(F("❌ Failed to open DATA.CSV"));
  }
}

void printLastLines(const char* filename, int numLines) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.println(F("❌ Failed to open file"));
    return;
  }

  String lines[numLines];
  int count = 0;
  DateTime today = rtc.now();

  while (file.available()) {
    String line = file.readStringUntil('\n');

    // Check if line matches today's date
    if (line.startsWith(String(today.year()) + "," + String(today.month()) + "," + String(today.day()))) {
      lines[count % numLines] = line;
      count++;
    }
  }

  file.close();

  Serial.println(F("📄 --- Filtered TAIL (Today) ---"));
  int start = count > numLines ? count - numLines : 0;
  for (int i = 0; i < numLines && i < count; i++) {
    int index = (start + i) % numLines;
    Serial.println(lines[index]);
  }
  Serial.println(F("📄 --- End ---"));
}

void parseTailCommand(String cmd) {
  int spaceIndex = cmd.indexOf(' ');
  if (spaceIndex == -1) {
    Serial.println(F("⚠️ Usage: TAIL <number>"));
    return;
  }

  int numLines = cmd.substring(spaceIndex + 1).toInt();
  if (numLines <= 0) {
    Serial.println(F("⚠️ Invalid line count"));
    return;
  }

  printLastLines(dataFilename, numLines);
}

void applyOutputs() {
  digitalWrite(heat, state.heat);
  digitalWrite(fan, state.fan);
  digitalWrite(pump, state.pump);
  digitalWrite(light, state.light);
}

void handleSerialCommands() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  state.lastManualCommandTime = millis();

  if (cmd == "M ON") {
    state.manualOverride = true;
    Serial.println(F("🔧 Manual override ENABLED"));
    return;
  } else if (cmd == "M OFF") {
    state.manualOverride = false;
    Serial.println(F("🔄 Returned to AUTONOMOUS mode"));
    return;
  } else if (cmd == "STAT") {
    Serial.print(F("🌡️ Temp: ")); Serial.print(sensors.temperature);
    Serial.print(F(" C, 💧 Humidity: ")); Serial.print(sensors.humidity);
    Serial.print(F(" %, 🔥 Heat: ")); Serial.print(state.heat);
    Serial.print(F(", 🌀 Fan: ")); Serial.print(state.fan);
    Serial.print(F(", 💡 Light: ")); Serial.print(state.light);
    Serial.print(F(", ⏱️ Lm: ")); Serial.print(state.lightOnMinutes);
    Serial.print(F(", MODE: ")); Serial.println(state.manualOverride);
    return;
  } else if (cmd == "READ") {
    printDataFile();
    return;
  } else if (cmd.startsWith("TAIL")) {
    int spaceIndex = cmd.indexOf(' ');
    if (spaceIndex != -1) {
      int numLines = cmd.substring(spaceIndex + 1).toInt();
      if (numLines > 0) {
        printLastLines(dataFilename, numLines);
      } else {
        Serial.println(F("⚠️ Invalid line count"));
      }
    } else {
      Serial.println(F("⚠️ Usage: TAIL <number>"));
    }
    return;
  } else if (cmd == "DUMP EEPROM") {
    unsigned int lm = 0, lmh = 0;
    EEPROM.get(EEPROM_LIGHT_MIN_ADDR, lm);
    EEPROM.get(EEPROM_LIGHT_HEAT_MIN_ADDR, lmh);
    Serial.print(F("📦 EEPROM LightMinutes: ")); Serial.println(lm);
    Serial.print(F("📦 EEPROM LightHeatMinutes: ")); Serial.println(lmh);
    return;
  } else if (cmd == "HELP") {
    Serial.println(F("📖 Available Commands:"));
    Serial.println(F(" - M ON / M OFF"));
    Serial.println(F(" - STAT / READ / TAIL <n>"));
    Serial.println(F(" - H ON/OFF / F ON/OFF / L ON/OFF / P ON/OFF"));
    Serial.println(F(" - DUMP EEPROM"));
    Serial.println(F(" - CAL TEMP <offset>"));
    Serial.println(F(" - TEST FAN 10s"));
    return;
  }

  // --- Manual override-only commands ---
  if (!state.manualOverride) return;

  if (cmd == "H ON") {
    state.heating = true;
    state.cooling = false;
    Serial.println(F("🛠️ HEAT overridden ON"));
  } else if (cmd == "H OFF") {
    state.heating = false;
    Serial.println(F("🛠️ HEAT overridden OFF"));
  } else if (cmd == "F ON") {
    state.cooling = true;
    state.coolingTriggerMillis = millis();
    Serial.println(F("🛠️ FAN overridden ON"));
  } else if (cmd == "F OFF") {
    state.cooling = false;
    Serial.println(F("🛠️ FAN overridden OFF"));
  } else if (cmd == "L ON") {
    state.lighting = true;
    state.lightHeat = false;
    state.light = HIGH;
    digitalWrite(light, HIGH);
    Serial.println(F("🛠️ LIGHT overridden ON"));
  } else if (cmd == "L OFF") {
    state.lighting = false;
    state.lightHeat = false;
    state.light = LOW;
    digitalWrite(light, LOW);
    Serial.println(F("🛠️ LIGHT overridden OFF"));
  } else if (cmd == "P ON") {
    state.pumping = true;
    Serial.println(F("🛠️ PUMP overridden ON"));
  } else if (cmd == "P OFF") {
    state.pumping = false;
    Serial.println(F("🛠️ PUMP overridden OFF"));
  } else if (cmd == "TEST FAN 10s") {
    digitalWrite(fan, HIGH);
    state.testFanActive = true;
    state.testFanStart = millis();
    Serial.println(F("🧪 Fan test started (10s)"));
  } else {
    Serial.println(F("❓ Unknown command"));
  }

  // Apply digital output states
  state.heat = state.heating ? HIGH : LOW;
  state.fan = state.cooling ? HIGH : LOW;
  state.pump = state.pumping ? HIGH : LOW;

  applyOutputs();

  // Clear remaining input buffer
  while (Serial.available()) Serial.read();
}

#endif