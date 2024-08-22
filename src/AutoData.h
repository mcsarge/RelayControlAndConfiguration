#include <Arduino.h>

#ifndef AUTOMEASURE_H
#define AUTOMEASURE_H

enum AutoMeasure {
    SOC=0, BATVOLT=1, BATCURRENT=2, PVVOLT=3, PVCURRENT=4 };

const String autoMeasureInfo[][2] {
    {"SOC","SOC"}, 
    {"BATVOLT","Battery Voltage"}, 
    {"BATCURRENT","Battery Current"}, 
    {"PVVOLT","PV Voltage"}, 
    {"PVCURRENT","PV Current"}};

//Structure to contain the Automated features for each Relay.
struct AutoData
{
   bool enabled = false;
   AutoMeasure measure = SOC; //Which measure to test
   float value = 75; //if (relayState = ON and reading >= Value) then relayState reamins On, else relayState = Off
   float restoreValue = 85; //If (relayState = Off AND reading >= restoreValue) then relayState = ON, else relayState remains OFF
};
#endif
