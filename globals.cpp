#include "globals.h"

// --- Global instances ---
SensorData sensors;
SystemState state;
RTC_DS3231 rtc;
SHTC3 shtc3;

// --- Constants ---
const int heat = 7;
const int fan = 8;
const int light = 6;
const int pump = 5;

const int EEPROM_LIGHT_MIN_ADDR = 0;
const int EEPROM_LIGHT_HEAT_MIN_ADDR = 4;
const unsigned long MANUAL_TIMEOUT = 10UL * 60UL * 1000UL;
const unsigned long FAN_TEST_DURATION = 10000;
const unsigned int dailyLightQuota = 720;

const float lowTempThreshold = 24.0;
const float highTempThreshold = 26.0;
const float hysteresis = 0.5;

const long serialBaud = 9600;
const long MSEC_PER_MIN = 60000;
const unsigned long pumpLeadTime = MSEC_PER_MIN;

const char dataFilename[] = "DATA.CSV";
const char csvHeader[] = "Y,M,D,H,Min,S,T(C),H(%),Ht,Fn,Lt,Lm,P,LmH";