#include <Arduino.h>
#include <esp_log.h>
#include <ArduinoJson.h>
#include "relayInformation.h"

static const char* TAG = "RelayInformation";

relayInformation::relayInformation(){
      errorCondition = -1;
      relayIndex = 99;
      relayShortName = "relay" + String(relayIndex);
      relayFixedName = "Relay " + String(relayIndex);
      relayPin = -1;
      relayName = "Error";
      relayStartState = false;
      relayButtonPin = -1;
      if (relayPath == "")
          relayPath = "/" + relayShortName;
      momentaryDuration = -1; //-1 = off
};

relayInformation::relayInformation(int index, int pin, String name, int duration, String path, bool startState, int buttonPin){
    relayIndex = index;
    relayShortName = "relay" + String(relayIndex);
    relayFixedName = "Relay " + String(relayIndex);
    relayPin = pin;
    relayName = name;
    relayStartState = startState;
    relayButtonPin = buttonPin;
    relayPath = path;
    if (relayPath == "")
        relayPath = "/" + relayShortName;
    momentaryDuration = duration;
    if (momentaryDuration>0)
      relayStartState = false;
    errorCondition = 0;
};

relayInformation::relayInformation(String rawJson){

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, rawJson);
    if (err) {
      ESP_LOGE(TAG, "deserializeJson() failed: %s", err.c_str());
      errorCondition = -1;
    } else {

      relayIndex = doc["index"];
      relayShortName = "relay" + String(relayIndex);
      relayFixedName = "Relay " + String(relayIndex);
      relayPin = doc["pin"];
      relayName = doc["name"].as<String>();
      relayStartState = doc["startState"].as<bool>();;
      relayButtonPin = doc["buttonPin"];
      relayPath = doc["path"].as<String>();;
      if (relayPath == "")
          relayPath = "/" + relayShortName;
      momentaryDuration = doc["duration"];
      if (momentaryDuration>0)
        relayStartState = false;
      errorCondition = 0;
    }
}

int relayInformation::getErrorCondition(){
  return errorCondition;
};

String relayInformation::toRawJson(){
  JsonDocument doc;
  doc["index"]=relayIndex;
  doc["pin"] = relayPin;
  doc["name"] = relayName;
  doc["buttonPin"] = relayButtonPin;
  doc["path"] = relayPath;
  doc["duration"] = momentaryDuration;
  if (momentaryDuration>0)
    relayStartState = false;
  else
    relayStartState = currentState();

  doc["startState"] = relayStartState;

  String returnString;
  serializeJson(doc,returnString);

  return returnString;
}

bool relayInformation::currentState(){
  return digitalRead(relayPin);
}

String isChecked(int pin){
  return digitalRead(pin)?"checked":"";  
}

int relayInformation::getRelayPin(){
    return relayPin;
}
String relayInformation::getShortName(){
    return relayShortName;
}

String relayInformation::getFixedName(){
    return relayFixedName;
}

String relayInformation::getRelayPath(){
    return relayPath;
}

int relayInformation::getMomentaryDurationInSeconds(){
  return momentaryDuration;
}

void relayInformation::setMomentaryDuration(int durationInSeconds){
  momentaryDuration = durationInSeconds;
}

String relayInformation::configHTML(){
  return 
      String("<div class=\"relay-section\">")
    + String("<h3>" + relayFixedName + "</h3>")
    + String(   "<div class=\"relay-item\">")
    + String(     "<label for=\"" + relayShortName + "\">Name:</label>")
    + String(     "<input type=\"text\" id=\""+ relayShortName + "\" name=\"" + relayShortName + "\" value=\"" + relayName + "\" maxlength=\"25\">")
    + String(   "</div>")
    + String(   "<div class=\"relay-item\">")
    + String(     "<label for=\"" + relayShortName + "-duration\">Duration:</label>")
    + String(     "<input type=\"number\" id=\"" + relayShortName + "-duration\" name=\"" + relayShortName + "-duration\" value=\"" + momentaryDuration + "\" maxlength=\"2\">")
    + String(   "</div>")
    + String( "</div>");
}

String relayInformation::actionHTML(){
  return 
    String("<div class=\"relay-item\">") +
    String(   "<label class=\"switch\">") +
    String(     "<input type=\"checkbox\" id=\"" + relayShortName + "\" onchange=\"toggleCheckbox(this)\"" + isChecked(relayPin) + ">") +
    String(     "<span class=\"slider\"></span>") +
    String(   "</label>") +
    String(   "<span class=\"relay-name\">" + relayName + "</span>") +
    String( "</div>");
};

String relayInformation::eventListenerJS(){
   return "source.addEventListener('" + relayShortName + "', function(e) {" +
          "console.log(\"" + relayShortName + "\", e.data);" +
          "document.getElementById(\"" + relayShortName + "\").checked = (e.data?.toLowerCase()?.trim()==\"1\");"
          "}, false);";
};

bool relayInformation::setRelay(int  val){
  ESP_LOGD(TAG, "In relayInformation::setRelay. relayFixedName = %s  relayName = %s  value to set = %d",relayFixedName, relayName, val);
  int currentVal = digitalRead(relayPin);
  if (val != currentVal) {
    digitalWrite(relayPin, val);
    //If you are setting this to ON and it is momentary, set the time.
    if ((val==1) && (momentaryDuration>0)){
      lastSetMillis = millis();
    }
    return true;
  } else {
    return false;
  }
}

bool relayInformation::relayInit(){
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, relayStartState);
    return true;
}
 
bool relayInformation::loop(){
  //Is this a momentary one?
  if (momentaryDuration > 0){
    //Is it now on and the time expired?
    if(currentState() && (lastSetMillis + (momentaryDuration*1000)) < millis()){
      ESP_LOGD(TAG,"Momentary time expired for %s", relayName);
      //Turn it off
      setRelay(false);
      return true; //an action has been taken
    }
  }
  return false; //No action was taken
}
