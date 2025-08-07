#include <Arduino.h>
#include <LilyGoRelays.hpp>
#include "WebStuff.h"
#include "AutoData.h"

String allMeasureStrings(AutoMeasure select){
  String retString;
  for (int i=SOC; i<=IGNORE; i++){
    retString = "<option value=\"" + autoMeasureInfo[(AutoMeasure)i].shortName + "\" ";
    retString += (select==(AutoMeasure)i?"selected":"");
    retString += ">" + autoMeasureInfo[(AutoMeasure)i].longName + "</option>";
  }
  return retString;
}

String configHTML(LilygoRelays::lilygoRelay relay, AutoData relayAutoData){  
  return 
      String("<div class=\"relay-section\">")
    + String("<h3>" + relay.getRelayFixedName() + "</h3>")
    + String(   "<div class=\"relay-item\">")
    + String(     "<label for=\"" + relay.getRelayFixedShortName() + "\">Name:</label>")
    + String(     "<input type=\"text\" id=\""+ relay.getRelayFixedShortName() + "\" name=\"" + relay.getRelayFixedShortName() + "\" value=\"" + relay.relayName + "\" maxlength=\"25\">")
    + String(   "</div>")
    + String(   "<div class=\"relay-item\">")
    + String(     "<label for=\"" + relay.getRelayFixedShortName() + "-duration\">Duration:</label>")
    + String(     "<input type=\"number\" id=\"" + relay.getRelayFixedShortName() + "-duration\" name=\"" + relay.getRelayFixedShortName() + "-duration\" value=\"" + relay.momentaryDuration + "\" maxlength=\"4\">")
    + String(   "</div>")
    + String(   "<div class=\"relay-item\">")
    + String(     "<label for=\"" + relay.getRelayFixedShortName() + "-measure\">Measure:</label>")
    + String(     "<select id=\"" + relay.getRelayFixedShortName() + "-measure\" name=\"" + relay.getRelayFixedShortName() + "-measure\">")
    +             allMeasureStrings(relayAutoData.measure)
    + String(     "</select>")
    + String(   "</div>")
    + String(   "<div class=\"relay-item\">")
    + String(     "<label for=\"" + relay.getRelayFixedShortName() + "-value\">Value:</label>")
    + String(     "<input type=\"number\" id=\"" + relay.getRelayFixedShortName() + "-value\" name=\"" + relay.getRelayFixedShortName() + "-value\" step=\"any\" value=\"" + relayAutoData.value + "\">")
    + String(     "<label for=\"" + relay.getRelayFixedShortName() + "-restorevalue\">Res. Value:</label>")
    + String(     "<input type=\"number\" id=\"" + relay.getRelayFixedShortName() + "-restorevalue\" name=\"" + relay.getRelayFixedShortName() + "-restorevalue\" step=\"any\" value=\"" + relayAutoData.restoreValue + "\">")
    + String(   "</div>")
    + String("</div>");
}

String actionHTML(LilygoRelays::lilygoRelay relay){
  return 
    String("<div class=\"relay-item\">") +
    String(   "<label class=\"switch\">") +
    String(     "<input type=\"checkbox\" id=\"" + relay.getRelayFixedShortName() + "\" onchange=\"toggleCheckbox(this)\" " + String(relay.getRelayStatus()==1?"checked":"") + ">") +
    String(     "<span class=\"slider\"></span>") +
    String(   "</label>") +
    String(   "<span class=\"relay-name\">" + relay.relayName + "</span>") +
    String( "</div>");
};

String eventListenerJS(LilygoRelays::lilygoRelay relay){
   return "source.addEventListener('" + relay.getRelayFixedShortName() + "', function(e) {" +
          "console.log(\"" + relay.getRelayFixedShortName() + "\", e.data);" +
          "document.getElementById(\"" + relay.getRelayFixedShortName() + "\").checked = (e.data?.toLowerCase()?.trim()==\"1\");}, false);";
};