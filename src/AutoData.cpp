#include <Arduino.h>
#include "AutoData.h"
#include <ArduinoJson.h>


AutoMeasure fromString(String theMeasure){
    for  (int i=SOC; i <= IGNORE; i++){
        if (autoMeasureInfo[i].shortName==theMeasure) return autoMeasureInfo[i].autoMeasure;
    }
    return IGNORE;
}

double round2(float value) {
   return (double)((int)(value * 100 + 0.5) / 100.0);
}

String asRawJson(AutoData item){
    JsonDocument doc;
    JsonObject ad = doc["ad"].to<JsonObject>();
    ad["me"] = autoMeasureInfo[item.measure].shortName;
    ad["vl"] = round2(item.value);
    ad["rv"] = round2(item.restoreValue);

    String returnString;
    doc.shrinkToFit(); 
    serializeJson(doc,returnString);
    return returnString;
}

AutoData fromJson(String rawJson){
    //Call the basic initializer, then set the relays and stuff based on the passed in Json String.
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, rawJson);
    AutoData newAd;

    if (err) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(err.c_str());
    } else if (doc.containsKey("ad")){
        JsonObject autodata = doc["ad"];
        newAd.measure = fromString(autodata["me"]);
        newAd.value = autodata["vl"];
        newAd.restoreValue = autodata["rv"];

    } else {
        Serial.println("Error, required value numberOfRelays not present aborting.");
    }

    return newAd;
}
