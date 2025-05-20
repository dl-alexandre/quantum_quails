#ifndef GLOBALS_H
#define GLOBALS_H

#include <RTClib.h>
#include <SparkFun_SHTC3.h>

// --- Sensor values ---
struct SensorData {
  float temperature = 0.0;
  float humidity = 0.0;
};

// --- System state and control ---
struct SystemState {
  bool heating         = false;
  bool cooling         = false;
  bool pumping         = false;
  bool lighting        = false;
  bool lightHeat       = false;
  bool manualOverride  = false;
  bool testFanActive   = false;

  int heat        = LOW;
  int fan         = LOW;
  int light       = LOW;
  int pump        = LOW;

  unsigned int lightOnMinutes   = 0;
  unsigned int lightHeatMinutes = 0;

  unsigned long lastManualCommandTime = 0;
  unsigned long coolingTriggerMillis  = 0;
  unsigned long testFanStart          = 0;

  int previousLightState  = LOW;
  int lastLoggedDay = -1;
};

// --- Global instances ---
extern SensorData sensors;
extern SystemState state;
extern RTC_DS3231 rtc;
extern SHTC3 shtc3;

// --- Constants ---
extern const unsigned long MANUAL_TIMEOUT;
extern const unsigned long FAN_TEST_DURATION;
extern const unsigned int dailyLightQuota;
extern const int EEPROM_LIGHT_MIN_ADDR;
extern const int EEPROM_LIGHT_HEAT_MIN_ADDR;
extern const float lowTempThreshold;
extern const float highTempThreshold;
extern const float hysteresis;
extern const long serialBaud;
extern const long MSEC_PER_MIN;
extern const unsigned long pumpLeadTime;
extern const char dataFilename[];
extern const char csvHeader[];

// --- Pin mappings ---
extern const int heat, fan, light, pump;

#endif