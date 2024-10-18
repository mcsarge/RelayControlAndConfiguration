/**
 * Author:      Matthew C. Sargent
 * Created:     10/18/2024
 * Description: This is intended to group the data structures and code needed to implement the automatic control of the relays
 * based on the data gathered from the Midnite Classic solar controller. Currently, the only parameters that are collected are 
 * listed in the enum "AutoMeasure" but this could be modified to include other measures. If you modify the possible
 * automatic measures, you will need to modify the chargerDataForRelayControl that is gathered by the ModbusStuff code.
 * 
 * (c) Copyright by Matthew C. Sargent.
 **/

#include <Arduino.h>

#ifndef AUTOMEASURE_H
#define AUTOMEASURE_H

enum AutoMeasure {
    SOC=0, BATVOLT=1, BATCURRENT=2, PVVOLT=3, PVCURRENT=4, IGNORE=5 };

struct AutoMeasureMatrixItem
{
    AutoMeasure autoMeasure;
    String shortName;
    String longName;
};

const AutoMeasureMatrixItem autoMeasureInfo[] {
    {SOC, "SOC","SOC"}, 
    {BATVOLT, "BATVOLT","Battery Voltage"}, 
    {BATCURRENT, "BATCURRENT","Battery Current"}, 
    {PVVOLT,"PVVOLT","PV Voltage"}, 
    {PVCURRENT, "PVCURRENT","PV Current"},
    {IGNORE, "IGNORE", "Ignore"}};

//Structure to contain the Automated features for each Relay.
struct AutoData
{
   AutoMeasure measure = IGNORE; //Which measure to test
   double value = 0; //if (relayState = ON and reading >= Value) then relayState reamins On, else relayState = Off
   double restoreValue = 0; //If (relayState = Off AND reading >= restoreValue) then relayState = ON, else relayState remains OFF
};

AutoData* AutoControlAllocate(int numberOfRelays);
AutoMeasure fromString(String theMeasure);
String asRawJson(AutoData item);
AutoData fromJson(String rawJson);
bool autoAdjustSingleRelay(double currentVal, bool currentState, AutoData thisAutoData);

#endif
