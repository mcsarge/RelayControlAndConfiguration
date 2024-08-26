/*********
Relay Control and Configuration
This code works with the Lilygo Relay 8 and relay 4
*********/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <Effortless_SPIFFS.h>
#include <esp_log.h>
#include <SensorPCF8563.hpp>
#include <esp_sntp.h>
#include <AceButton.h>
#include <ElegantOTA.h>
#include <ESPAsyncWebServer.h>
#include <LilyGoRelays.hpp>
#include "OTAStuff.h"
#include "secrets.h"
#include "WebStuff.h"
#include "AutoData.h"

static const char* TAG = "RelayControl";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create an Event Source on /events
AsyncEventSource events("/events");

// Search for parameter in HTTP POST request
const char* PARAM_DEVICE_NAME = "name";
const char* PARAM_SSID = "ssid";
const char* PARAM_PASS = "pass";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

//Variables to save values from HTML form
String name;
String ssid;
String pass;
String config;
String automaticControl;

// File paths to save input values permanently
const char* namePath = "/name.txt";
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* configPath = "/relayconfig.json";
const char* automaticPath = "/automatic.txt";


// Timer variables
unsigned long previousMillis = 0;
const long interval = 10 *1000;  // Wait 10 seconds for Wi-Fi connection (milliseconds)

using namespace ace_button;

#define SAVE_DELAY                  2000 // Delay to wait after a save was requested in case we get a bunch quickly

const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";

AceButton boot;
uint8_t state = 0;

LilygoRelays relays = LilygoRelays(LilygoRelays::Lilygo6Relays,1);

#ifdef LILYGORTC
uint32_t lastMillis;
SensorPCF8563 rtc;
#endif

// Timer variables
uint32_t lastSaveRequestTime=-1;

unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;

// Create a eSPIFFS class
#ifndef USE_SERIAL_DEBUG_FOR_eSPIFFS
  // Create fileSystem
  eSPIFFS fileSystem;
#else
  // Create fileSystem with debug output
  eSPIFFS fileSystem(&Serial);  // Optional - allow the methods to print debug
#endif

// IotWebConf
String ChipId = String((uint32_t)ESP.getEfuseMac(), HEX);
// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const String hostName = "Relays-" + ChipId;



AutoData *automaticData;

/*
Initialize the automatic control system. This will turn on and off certain relays based on the
values gathered from the MQTT or if that is unreachable, the Solar Controller itself.
*/
bool AutoControlInitialize(String autoControl){
  if (autoControl = "") {
    automaticData = (AutoData*)malloc(sizeof(AutoData) * relays.numberOfRelays()); 
    for (int i=0; i<relays.numberOfRelays(); i++){
      automaticData[i].enabled = (i % 2==0)?true:false;
      automaticData[i].measure = BATVOLT;
      automaticData[i].value = 24.0;
      automaticData[i].restoreValue = 25.1;
    }
    return true;
  } else {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, autoControl);
    if (err) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(err.c_str());
        return false;
    } else {
        if (!doc.containsKey("numberofRelays")){
            Serial.println("Error, required value numberOfRelays not present aborting.");
            return false;
        }

        //Only read in what is in the doc or what can be configured, whichever is less.
        int jsonCount = doc["numberofRelays"];
        int workingCount = min(relays.numberOfRelays(), jsonCount);
        Serial.println(workingCount);

        automaticData = (AutoData*)malloc(sizeof(AutoData) * workingCount); //allocate

        int index = 0;
        for (JsonObject aData : doc["autoData"].as<JsonArray>()) {
            if (index >= workingCount){
                Serial.println("Break");
                break;
            }
            automaticData[index].enabled = aData["enabled"].as<bool>();
            automaticData[index].measure = aData["measure"].as<AutoMeasure>();
            automaticData[index].value = aData["value"].as<int>();
            automaticData[index].restoreValue = aData["restoreValue"].as<int>();
            index++;
        }
    }
    return true;
  }
}

void AutoControlFree(){
  free(automaticData);
}

String autoMeasureAsRawJson(){
    JsonDocument doc;
    doc["numberofRelays"] = relays.numberOfRelays();

    JsonArray data = doc["autoData"].to<JsonArray>();

    for (int autoDataIndex=0; autoDataIndex < relays.numberOfRelays(); autoDataIndex++){
        JsonObject r = data.add<JsonObject>();
        r["enabled"]=automaticData[autoDataIndex].enabled;
        r["measure"]=automaticData[autoDataIndex].measure;
        r["value"]=automaticData[autoDataIndex].value;
        r["restoreValue"]=automaticData[autoDataIndex].restoreValue;
    }

    String returnString;
    doc.shrinkToFit(); 
    serializeJson(doc,returnString);
    return returnString;
}



#ifdef LILYGORTC
// Callback function (get's called when time adjusts via NTP)
void timeavailable(struct timeval *t)
{
    Serial.println("Got time adjustment from NTP!");
    // printLocalTime();
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("No time available (yet)");
        return;
    }
    rtc.hwClockWrite();
}
#endif

void ButtonHandleEvent(AceButton *n, uint8_t eventType, uint8_t buttonState)
{
    if (eventType != AceButton::kEventPressed) {
        return;
    }
    ESP_LOGD(TAG, "[AceButton][%u]  N:%d E:%u S:%u state:%d\n", millis(), n->getId(), buttonState, buttonState, state);
    switch (state) {
    case 0:
        //control.setAllHigh();
        for (int i=0; i<relays.numberOfRelays();i++)
          relays[i].setRelayStatus(HIGH);
        relays.setGreenLedStatus(HIGH,100,100);
        relays.setRedLedStatus(HIGH,1000,1000);
        if (events.count()>0){
          events.send("1", "allRelays",millis());
        }        
        break;
    case 1:
        //control.setAllLow();
        for (int i=0; i<relays.numberOfRelays();i++)
          relays[i].setRelayStatus(LOW);
        if (events.count()>0){
          events.send("0", "allRelays",millis());
        }
        break;
    case 2:
        // setting single pins
        for (int i=0; i<relays.numberOfRelays();i++){
          relays[i].setRelayStatus(HIGH);
          delay(50);
        }
        break;
    case 3:
        // setting single pins
        for (int i=0; i<relays.numberOfRelays();i++){
          relays[i].setRelayStatus(LOW);
          delay(50);
        }
     break;
    case 4:
        // setting single pins
        ESP_LOGD(TAG, "IP address is: %s", WiFi.localIP().toString());
        Serial.println(autoMeasureAsRawJson());
     break;
    default:
        break;
    }
    state++;
    state %= 5;
}


void findSetRelay(String rname, int  val){
  ESP_LOGD(TAG, "In findSetRelay relay = %s val= %d", rname, val);
  //relays[rname].setRelayStatus(val);
  for (int i=0; i<relays.numberOfRelays(); i++){
    if (rname==relays[i].getRelayFixedShortName()) {
      relays[i].setRelayStatus(val);
    }
  }
}


String processor(const String& replacementString){
  ESP_LOGD(TAG, "In Processor");
  if(replacementString == "NAME"){
    ESP_LOGD(TAG, "NAME - %", name);
    return name;
  }
  else if (replacementString == "SSID"){
    ESP_LOGD(TAG, "SSID - %s", ssid);
    return ssid;
  }
  else if (replacementString == "PASS"){
    ESP_LOGD(TAG, "PASS - %", pass);
    return pass;
  }
  else if (replacementString == "RELAYARRAYCONFIG"){
    ESP_LOGD(TAG, "RELAYARRAYCONFIG");
    String retString =
    String("<div class=\"input-container\">") +
    String(    "<label for=\"name\">Name:</label>") +
    String(    "<input type=\"text\" id=\"name\" name=\"name\" value=\"" + name + "\" maxlength=\"25\">") +
    String("</div><br>");
    for (int i = 0; i<relays.numberOfRelays(); i++) {
      retString += configHTML(relays[i], automaticData[i]);
    }
    return retString;
  }
  else if (replacementString == "RELAYSWITCHES") {
    ESP_LOGD(TAG, "RELAYSWITCHES");
    String retString = "";
    for (int i = 0; i<relays.numberOfRelays(); i++) {
       retString += actionHTML(relays[i]);
    }
    retString +="<button class=\"save-button\" onclick=\"saveStates(this)\">Save States</button>";
    return retString;
  }
  else if (replacementString == "RELAYEVENTLISTENERS") {
    ESP_LOGD(TAG, "RELAYEVENTLISTENERS");
    String retString;
    for (int i = 0; i<relays.numberOfRelays(); i++) {
      retString += eventListenerJS(relays[i]);
    }
    return retString;
  }    
  ESP_LOGD(TAG, "Returning nothing");
  return "";
}

// Initialize WiFi
bool initWiFi() {
  if(ssid=="" || name==""){
    ESP_LOGE(TAG, "Undefined SSID or Device name.");
    return false;
  }

  WiFi.setHostname(hostName.c_str());
  WiFi.mode(WIFI_STA);
  
  WiFi.begin(ssid.c_str(), pass.c_str());
  ESP_LOGD(TAG, "Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while(WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      ESP_LOGE(TAG, "Failed to connect.");
      WiFi.disconnect();
      return false;
    }
  }
  ESP_LOGD(TAG, "IP address is: %s", WiFi.localIP().toString());
  return true;
}

void relayUpdated(int relay, int value){
  if (events.count()>0){
    events.send(String(value).c_str(), relays[relay].getRelayFixedShortName().c_str(),millis());
  }
}



void setup() {
  //Do this ASAP so that the relays will all be turned off before setting to their previous values.
  relays.initialize();
  // Serial port for debugging purposes
  Serial.begin(115200);
  delay(200);

  AutoControlInitialize("");

  //delay(20000); //20 seconds for debugging/
  ESP_LOGD(TAG,"Booted");

  const uint8_t boot_pin = 0;
  pinMode(boot_pin, INPUT_PULLUP);
  boot.init(boot_pin, HIGH, 0);
  ButtonConfig *buttonConfig = boot.getButtonConfig();
  buttonConfig->setEventHandler(ButtonHandleEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setLongPressDelay(5000);

#ifdef LILYGORTC
  pinMode(RTC_IRQ, INPUT_PULLUP);

  bool found=false;
  for (int i=0; i<5; i++){
    if (!rtc.begin(Wire, PCF8563_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        delay(500);
    } else {
        found=true;
        Serial.println("Found PCF8563!");
        break;
    }
  }
  if (!found)
      Serial.println("Failed to find PCF8563 - check your wiring!");

#endif

  // Create a eSPIFFS class
  #ifndef USE_SERIAL_DEBUG_FOR_eSPIFFS
    // Check Flash Size - Always try to incorrperate a check when not debugging to know if you have set the SPIFFS correctly
    if (!fileSystem.checkFlashConfig()) {
      ESP_LOGE(TAG, "Flash size was not correct! Please check your SPIFFS config and try again");
      delay(100000);
      ESP.restart();
    }
  #endif

  // Load values saved in SPIFFS
  fileSystem.openFromFile(namePath, name);
  if (name == "") {
    ESP_LOGD(TAG,"Names was empty");
  }

  fileSystem.openFromFile(ssidPath, ssid);
  if (ssid == "") {
    ESP_LOGD(TAG,"SSID was empty");
  }

  fileSystem.openFromFile(passPath,pass);
  if (pass == ""){
    ESP_LOGD(TAG, "PASS was empty");
  }

  fileSystem.openFromFile(configPath,config);
  if (config == ""){
    Serial.println("Config was empty");
  } else {
    ESP_LOGD(TAG,"Initializing Relays");
    relays.initialize(config);    
  }

  fileSystem.openFromFile(automaticPath,automaticControl);
  if (automaticControl == ""){
    Serial.println("automaticControl was empty");
  } else {
    ESP_LOGD(TAG,"Initializing Automatic Control");
    AutoControlInitialize(automaticControl);    
  }

  relays.setRelayUpdateCallback(relayUpdated);
  
if (!initWiFi()){
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP("RELAYCONTROL", "aabbcc112233");

  IPAddress IP = WiFi.softAPIP();
  ESP_LOGD(TAG,"AP IP address: %s", IP.toString());

}
  ESP_LOGD(TAG, "SSID = %s", ssid);

  for (int i=0; i<relays.numberOfRelays();i++){
    ESP_LOGD(TAG, "%s", relays[i].relayName);
  }

  // Set Authentication Credentials
  ElegantOTA.setAuth("admin", "aabbcc112233"); //page is "/update"
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

#ifdef LILYGORTC
  // set notification call-back function
  sntp_set_time_sync_notification_cb( timeavailable );

  /**
   * A more convenient approach to handle TimeZones with daylightOffset
   * would be to specify a environmnet variable with TimeZone definition including daylight adjustmnet rules.
   * A list of rules for your zone could be obtained from https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
   */
  configTzTime("EST5EDT,M3.2.0,M11.1.0", ntpServer1, ntpServer2);
  //configTzTime("CST-8", ntpServer1, ntpServer2);
#endif

  server.rewrite("/", "/index").setFilter(ON_STA_FILTER);
  server.rewrite("/", "/wifimanager").setFilter(ON_AP_FILTER);
  server.serveStatic("/", SPIFFS, "/");
  
  // Route for root / web page
  server.on("/index", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html", false, processor);
  });
    
  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      ESP_LOGD(TAG, "Client reconnected! Last message ID that it got is: %u", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });

  // Route to configure Relays
  server.on("/relayconfig", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/relayconfig.html", "text/html", false, processor);
  });

  server.on("/relayconfig", HTTP_POST, [](AsyncWebServerRequest *request) {
    ESP_LOGD(TAG, "caught post");
    int params = request->params();
    bool saveIt = false; //did anything change?
    for(int i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isPost()){
        // HTTP POST name value
        ESP_LOGD(TAG, "%s", p->name());
        if (p->name() == PARAM_DEVICE_NAME) {
          name = p->value().c_str();
          ESP_LOGD(TAG, "Name set to: %s", name);
          // Write file to save value
          //writeFile(SPIFFS, namePath, name.c_str());
          fileSystem.saveToFile(namePath, name);            
        }
        // HTTP POST relay value
        for (int i=0; i<relays.numberOfRelays();i ++){
          if (p->name() == relays[i].getRelayFixedShortName()) {
            if (relays[i].relayName != p->value().c_str()){
              saveIt = true;
              relays[i].relayName = p->value();
            }
          }
          if (p->name() == String(relays[i].getRelayFixedShortName()+"-duration")){
            if (relays[i].momentaryDuration != p->value().toInt()){
              saveIt = true;
              relays[i].momentaryDuration = p->value().toInt();
            }
          }
        }
      }
    }
    if (saveIt) {
      ESP_LOGD(TAG, "Requesting Save");
      lastSaveRequestTime = millis();
    }
    request->redirect("/");
  });

  // Send a GET request to <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
  server.on("/relayupdate", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage1;
    String inputMessage2;
    if (request->hasParam("output") && request->hasParam("state")) {
      inputMessage1 = request->getParam("output")->value();
      inputMessage2 = request->getParam("state")->value();
      findSetRelay(inputMessage1, inputMessage2.toInt());
      //Tell the other clients.
      if (events.count()>1){
        events.send(inputMessage2.c_str(), inputMessage1.c_str(), millis());
      }
    } else if (request->hasParam("saverelaystates")) {
      //Save all of the relays
      ESP_LOGD(TAG, "saveRelayState received");
      lastSaveRequestTime = millis(); //request a save
              
    } else {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/wifimanager", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/wifimanager.html", "text/html", false, processor);
  });

  server.on("/wifimanager", HTTP_POST, [](AsyncWebServerRequest *request) {
    int params = request->params();
    for(int i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isPost()){
        // HTTP POST name value
        if (p->name() == PARAM_DEVICE_NAME) {
          name = p->value().c_str();
          ESP_LOGD(TAG, "Name set to: %s", name);
          // Write file to save value
          fileSystem.saveToFile(namePath, name);
        }
        // HTTP POST ssid value
        if (p->name() == PARAM_SSID) {
          ssid = p->value().c_str();
          ESP_LOGD(TAG, "SSID set to: %s", ssid);
          // Write file to save value
          fileSystem.saveToFile(ssidPath, ssid);
        }
        // HTTP POST pass value
        if (p->name() == PARAM_PASS) {
          pass = p->value().c_str();
          ESP_LOGD(TAG, "Password set to: %s", pass);
          // Write file to save value
          fileSystem.saveToFile(passPath, pass);            
        }
        ESP_LOGD(TAG, "POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
    delay(3000);
    request->redirect("/");
    delay(1000);
    ESP.restart();
  });

  server.addHandler(&events);    
  ElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.print("Hostname = "); Serial.println(hostName);

}

void loop() {

  ElegantOTA.loop();
  boot.check();
  relays.loop();

  if (lastSaveRequestTime!=-1 and (lastSaveRequestTime+SAVE_DELAY<millis())) {
    lastSaveRequestTime = -1;
    ESP_LOGD(TAG,"Save was requested");
    config = relays.asRawJson();
    Serial.println("Relays json data:" + config);
    fileSystem.saveToFile(configPath, config);    
  }


#ifdef LILYGORTC
  if (millis() - lastMillis > 600000) {

      lastMillis = millis();

      RTC_DateTime datetime = rtc.getDateTime();
      // Serial.printf(" Year :"); Serial.print(datetime.year);
      // Serial.printf(" Month:"); Serial.print(datetime.month);
      // Serial.printf(" Day :"); Serial.print(datetime.day);
      // Serial.printf(" Hour:"); Serial.print(datetime.hour);
      // Serial.printf(" Minute:"); Serial.print(datetime.minute);
      // Serial.printf(" Sec :"); Serial.println(datetime.second);

  }
#endif

}
