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
#include <esp32ModbusTCP.h>
#include "Log.h"
#include "AsyncMqttClient.h"
#include "ChargeControllerInfo.h"
#include "OTAStuff.h"
#include "ModbusStuff.h"
#include "secrets.h"
#include "WebStuff.h"
#include "AutoData.h"




// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create an Event Source on /events
AsyncEventSource events("/events");

// Search for parameter in HTTP POST request
const char* PARAM_DEVICE_NAME = "name";
const char* PARAM_SSID = "ssid";
const char* PARAM_PASS = "pass";

const char* PARAM_CLASSIC_NAME = "classicname";
const char* PARAM_CLASSIC_IP = "classicip";
const char* PARAM_CLASSIC_PORT = "classicport";

const char* DEFAULT_CLASSIC_IP = "192.168.0.225";
const char* DEFAULT_CLASSIC_PORT = "502";
const char* DEFAULT_CLASSIC_NAME = "Classic";

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
String classicname;
String classicip;
String classicport;
String config;
String automaticControl;

// File paths to save input values permanently
const char* namePath = "/name.txt";
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* classicnamePath = "/classicname.txt";
const char* classicipPath = "/classicip.txt";
const char* classicportPath = "/classicport.txt";
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
uint32_t wifiNeedsSave=-1;

unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;
unsigned long wifiReconnectPreviousMillis = millis();


bool modbusGood = false;
chargerData cd;


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
Initialize the array that stores the information for the automatic control system. 
This will turn on and off certain relays based on the values gathered from the Solar Controller
*/
bool AutoControlAllocate(int numberOfRelays){
  automaticData = (AutoData*)malloc(sizeof(AutoData) * numberOfRelays); 
  return true;
}

void AutoControlFree(){
  free(automaticData);
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
        ESP_LOGD(TAG, "IP address is: %s", WiFi.localIP().toString());
        Serial.println(relays[0].getUserData());
        Serial.println(relays[1].getUserData());

        fileSystem.openFromFile(configPath,config);
        if (config == ""){
          Serial.println("Config was empty");
        } else {
          ESP_LOGD(TAG,"Initializing Relays");
          relays.initialize(config);

          //now initialize each AutoData from the UserData field
          for (int i=0; i<relays.numberOfRelays();i++){
            Serial.println(relays[i].getUserData());
            automaticData[i] = fromJson(relays[i].getUserData());
          }
        }
        Serial.println(relays[0].getUserData());
        Serial.println(relays[1].getUserData());




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

String measureText(){
  String retString = "Charger and Battery status will be displayed here.";
  time_t now;
  time(&now);
  if (cd.BatVoltage != -1 
      & millis()-cd.gatherMillis < (DEFAULT_GATHER_RATE*2)) { 
    retString = "SOC: " + String((int)cd.SOC);
    retString += "; Bat. Volts: " + String(cd.BatVoltage,2);
    retString += "V; Bat. Curr: " + String(cd.BatCurrent,2);
    retString += "A; PV Volts: " + String(cd.PVVoltage,2);
    retString += "V; PV Curr: " + String(cd.PVCurrent,2);
    retString += "A";
    
    struct tm *timeinfo;
    timeinfo = localtime(&cd.gatherTime);
    if(timeinfo->tm_year > (2016 - 1900)){ //is the time good?
      Serial.println(timeinfo, "%A, %B %d %Y %H:%M:%S");
      char str[20];
      strftime(str, sizeof str, "%H:%M:%S", timeinfo); 
      retString += "; at " + String(str);
    } else {
      Serial.println("No time available (yet)");
    }
  }
  return retString;
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
  else if (replacementString == "CLASSICNAME"){
    ESP_LOGD(TAG, "CLASSICNAME - %", classicname);
    return classicname;
  }
  else if (replacementString == "CLASSICIP"){
    ESP_LOGD(TAG, "CLASSICIP - %", classicip);
    return classicip;
  }
  else if (replacementString == "CLASSICPORT"){
    ESP_LOGD(TAG, "CLASSICPORT - %", classicport);
    return classicport;
  }
  else if (replacementString == "CHARGERINFORMATION"){
    String mt = measureText();
    ESP_LOGD(TAG, "CHARGERINFORMATION - %", mt);
    return mt;
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


void measuresUpdated(){
  if (events.count()>0 & (millis() - (cd.gatherMillis) < DEFAULT_GATHER_RATE*2)){
    events.send(measureText().c_str(), "chargedata", millis());
  }
}


bool autoAdjustSingleRelay(double currentVal, bool currentState, double val, double resVal){

  bool retVal = false;  
  bool opposite = false;  //is this a 'normal' where resVal is greater than val or opposite?

  if (resVal < val) {
    opposite = true;
  }

  String info = "autoAdjustSingleRelay: currentVal="; 
  info += String(currentVal);
  info += " currentState="; 
  info += currentState?"on val=":"off val="; 
  info += String(val); 
  info += " resVal="; 
  info += String(resVal); 
  info += opposite?" opposite=true":" opposite=false";

  Serial.println(info);

  if (currentState){ //If the relay is ON
    retVal = (opposite?!(currentVal >= val):(currentVal >= val));
  } else { //the relay is currently off
    retVal = (opposite?!(currentVal >= resVal):(currentVal >= resVal));
  }
  Serial.print("Returning ");
  Serial.println(retVal?"true":"false");
  return retVal;
}

/*
Process the settings found in autoData against the data collected from the solar charger and 
change the state of any relays that need to be switched based on the charger data.
Note calling setRelayStatus will only chage the relay if the value is different and it will handle
sending out the event to update the screen etc.
*/
void doAutoControl(){
  double theCurrentValue;
  //Make sure that the data that was received is recent
  if (millis() - cd.gatherMillis <= DEFAULT_GATHER_RATE){
    for (int i=0; i< relays.numberOfRelays(); i++){
      //Only run the check if the measure is not "IGNORE"
      if (automaticData[i].measure != IGNORE){
        switch (automaticData[i].measure) {
        case SOC:
          theCurrentValue = cd.SOC;
          break;
        case BATVOLT:
          theCurrentValue = cd.BatVoltage;
          break;
        case BATCURRENT:
          theCurrentValue = cd.BatCurrent;
          break;
        case PVVOLT:
          theCurrentValue = cd.PVVoltage;
          break;
        case PVCURRENT:
          theCurrentValue = cd.PVCurrent;
          break;
        default:
          theCurrentValue = cd.SOC;
          break;
        }

        Serial.println("Checking relay = " + String(i));
        relays[i].setRelayStatus(
          autoAdjustSingleRelay(
            theCurrentValue, 
            relays[i].getRelayStatus(),
            automaticData[i].value,
            automaticData[i].restoreValue));
      }
    }
  }
}





void setup() {
  //Do this ASAP so that the relays will all be turned off before setting to their previous values.
  relays.initialize();
  // Serial port for debugging purposes
  Serial.begin(115200);
  delay(200);

  AutoControlAllocate(relays.numberOfRelays());

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

  fileSystem.openFromFile(classicnamePath, classicname);
  if (classicname == ""){
    ESP_LOGD(TAG, "Classic Name was empty, setting default");
    classicname = DEFAULT_CLASSIC_NAME;
  }

  fileSystem.openFromFile(classicipPath,classicip);
  if (classicip == ""){
    ESP_LOGD(TAG, "ClassicIP was empty, setting default");
    classicip = DEFAULT_CLASSIC_IP;
  }

  fileSystem.openFromFile(classicportPath,classicport);
  if (classicport == ""){
    ESP_LOGD(TAG, "ClassicPort was empty, setting default");
    classicport = DEFAULT_CLASSIC_PORT;
  }

  fileSystem.openFromFile(configPath,config);
  if (config == ""){
    Serial.println("Config was empty");
  } else {
    ESP_LOGD(TAG,"Initializing Relays");
    relays.initialize(config);

    //now initialize each AutoData from the UserData field
    for (int i=0; i<relays.numberOfRelays();i++){
      Serial.println(relays[i].getUserData());
      automaticData[i] = fromJson(relays[i].getUserData());
    }
  }

  relays.setRelayUpdateCallback(relayUpdated);
  
if (!initWiFi()){
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(hostName, "aabbcc112233");

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
    int params = request->params();
    bool saveIt = false; //did anything change?
    
    for(int i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isPost()){
        // HTTP POST name value
        ESP_LOGD(TAG, "%s", p->name());
        if (p->name() == PARAM_DEVICE_NAME) {
          name = p->value();
          saveIt = true;
        }
        // HTTP POST relay value
        for (int i=0; i<relays.numberOfRelays();i ++){
          if (p->name() == relays[i].getRelayFixedShortName()) {
            if (relays[i].relayName != p->value()){
              saveIt = true;
              relays[i].relayName = p->value();
            }
          }
          if (p->name() == relays[i].getRelayFixedShortName()+"-duration"){
            if (relays[i].momentaryDuration != p->value().toInt()){
              saveIt = true;
              relays[i].momentaryDuration = p->value().toInt();
            }
          }

          if (p->name() == relays[i].getRelayFixedShortName()+"-measure"){
            if(autoMeasureInfo[automaticData[i].measure].shortName != p->value()){
              saveIt = true;
              automaticData[i].measure = fromString(p->value());
            }
          }

          if (p->name() == relays[i].getRelayFixedShortName()+"-value"){
            if (abs(automaticData[i].value-p->value().toFloat())>0.01){
              saveIt = true;
              automaticData[i].value = p->value().toFloat();
            }
          }

          if (p->name() == relays[i].getRelayFixedShortName()+"-restorevalue"){
            if (abs(automaticData[i].restoreValue-p->value().toFloat())>0.01){
              saveIt = true;
              automaticData[i].restoreValue = p->value().toFloat();
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
          wifiNeedsSave = millis();
        }
        // HTTP POST ssid value
        if (p->name() == PARAM_SSID) {
          ssid = p->value().c_str();
          wifiNeedsSave = millis();
        }
        // HTTP POST pass value
        if (p->name() == PARAM_PASS) {
          pass = p->value().c_str();
          wifiNeedsSave = millis();
        }
        // HTTP POST classicname value
        if (p->name() == PARAM_CLASSIC_NAME) {
          classicname = p->value().c_str();
          wifiNeedsSave = millis();
        }
        // HTTP POST classicname value
        if (p->name() == PARAM_CLASSIC_IP) {
          classicip = p->value().c_str();
          wifiNeedsSave = millis();
        }
        // HTTP POST classicname value
        if (p->name() == PARAM_CLASSIC_PORT) {
          classicport = p->value().c_str();
          wifiNeedsSave = millis();
        }
      }
    }
    request->redirect("/");
  });

  server.addHandler(&events);    
  ElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.print("Hostname = "); Serial.println(hostName);
  
  modbusGood = false;
  int i = 0;
  while (!modbusGood and i++<10) {
    modbusGood = setupModbus(classicip, classicport, WiFi);
    if (!modbusGood)
    {
      Serial.println("Modbus setup failed");
      delay(1000);
    } else {
      Serial.println("Modbus setup success");
    }
  }

  //Initialize the watchdog that can reset the module if thing go wrong.
	init_watchdog();
}



void loop() {

  feed_watchdog();

  ElegantOTA.loop();
  boot.check();
  relays.loop();

  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED) && (millis() - wifiReconnectPreviousMillis >= (1000*60))) { //check every minute
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    wifiReconnectPreviousMillis = millis();
  }

  if (WiFi.status() == WL_CONNECTED){
    if (modbusGood){
      if (gatherModbusData()){
        //got modbus data, process it.
        printModbusData();
        cd = getChargerData();
        doAutoControl();
        measuresUpdated();
      }
    }
  }

  if (lastSaveRequestTime!=-1 and (lastSaveRequestTime+SAVE_DELAY<millis())) {
    lastSaveRequestTime = -1;
    ESP_LOGD(TAG,"Save was requested");

    //store all the autoData with each relay in the UserData section
    for (int i=0; i<relays.numberOfRelays(); i++){
      relays[i].setUserData(asRawJson(automaticData[i]));
    }
    //Save the name of the device.
    fileSystem.saveToFile(namePath, name);            

    config = relays.asRawJson();
    Serial.println("Relays json data:" + config);
    fileSystem.saveToFile(configPath, config);
  }

  if (wifiNeedsSave!=-1 and (wifiNeedsSave+SAVE_DELAY<millis())) {
    wifiNeedsSave = -1;
    ESP_LOGD(TAG,"WiFi configuration Save was requested");
    fileSystem.saveToFile(namePath, name);
    fileSystem.saveToFile(ssidPath, ssid);
    fileSystem.saveToFile(passPath, pass);            
    fileSystem.saveToFile(classicnamePath, classicname);            
    fileSystem.saveToFile(classicipPath, classicip);            
    fileSystem.saveToFile(classicportPath, classicport);   
    delay(3000); //wait 3 seconds, then restart.
    ESP.restart();
         
  }



#ifdef LILYGORTC
  if (millis() - lastMillis > (1000*60*10)) { //10 minutes
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
