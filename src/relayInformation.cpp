#include <Arduino.h>
#include "relayInformation.h"

relayInformation::relayInformation(int index, int pin, String name, String path, bool startState, int buttonPin){
    relayShortName = "relay" + String(index);
    relayFixedName = "Relay " + String(index);
    relayPin = pin;
    relayName = name;
    relayStartState = startState;
    relayButtonPinParam = buttonPin;
    relayPath = path;
    if (relayPath == "")
        relayPath = "/" + relayShortName;
};

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

String relayInformation::configHTML(){
    return "<label for=\"" + relayShortName + "\">" + relayFixedName + "</label><input type=\"text\" id =\"" + relayShortName + "\" name=\"" + relayShortName + "\" value=\"" + relayName + "\"><br>";
}

String relayInformation::actionHTML(){
   return "<h4>" + relayName + "</h4><label class=\"switch\" for=\"" + relayShortName + "\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"" + relayShortName + "\"" + isChecked(relayPin) + "><div class=\"slider round\"></div></label>";
};

String relayInformation::eventListenerJS(){
   return "source.addEventListener('" + relayShortName + "', function(e) {" +
          "console.log(\"" + relayShortName + "\", e.data);" +
          "document.getElementById(\"" + relayShortName + "\").checked = (e.data?.toLowerCase()?.trim()==\"1\");"
          "}, false);";
};

bool relayInformation::setRelay(int  val){
  Serial.println("In setRelay. relayFixedName = " + relayFixedName + " relayName = " + relayName + " value to set =" + val);
  int currentVal = digitalRead(relayPin);
  if (val != currentVal) {
    digitalWrite(relayPin, val);
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
