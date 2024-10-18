#ifndef MODBUSSTUFF_H
#define MODBUSSTUFF_H

#include <Arduino.h>
#include <WiFi.h>
#include "ChargeControllerInfo.h"
#include <esp32ModbusTCP.h>

#define MODBUS_READ_TIMEOUT 300000               //5 minutes in ms. Clear the modbus read
#define WATCHDOG_TIMER 600000                    //time in ms to trigger the watchdog
#define MAX_MODBUS_READ_ATTEMPTS 3               //maximum number of tries per gather cycle.
#define DEFAULT_GATHER_RATE 120000               //300,000 = 5 minutes, 60,000 = 1 minute, 120,000 = 2 minutes
#define INITIAL_MODBUS_COLLECTION_DELAY 15000    //15,000 15 seconds

/**
 * Thsi data structure contains the values that can be used for automatically controlling the relays.
 * If additional measures are needed, create an entry here and then set the value in the code where
 * the other values are being set. The measures then can be used in the code to automatically control
 * the relays.
 */
struct chargerDataForRelayControl
{
   unsigned long gatherMillis = 0;
   time_t timeDataWasGathered = 0;
   int SOC = 0;
   double BatVoltage = -1;
   double BatCurrent = -1;
   double PVVoltage = -1;
   double PVCurrent = -1;
};

void init_watchdog();
void feed_watchdog();

bool setupModbus(String ip_addr, String port_number, WiFiClass _wifi);
bool gatherModbusData();
void printModbusData();

chargerDataForRelayControl getChargerData();

#endif