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
#include "OTAStuff.h"
#include "secrets.h"
#include "WebStuff.h"
#include <LilyGoRelays.hpp>

static const char* TAG = "RelayControl";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create an Event Source on /events
AsyncEventSource events("/events");

// Search for parameter in HTTP POST request
const char* PARAM_DEVICE_NAME = "name";
const char* PARAM_SSID = "ssid";
const char* PARAM_PASS = "pass";

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "8080";
char blynk_token[34] = "YOUR_BLYNK_TOKEN";

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

// File paths to save input values permanently
const char* namePath = "/name.txt";
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* relayPath = "/relays.txt";
const char* configPath = "/config.json";

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10 *1000;  // Wait 10 seconds for Wi-Fi connection (milliseconds)

using namespace ace_button;

#define SAVE_DELAY                  2000 // Delay to wait after a save was requested in case we get a bunch quickly

const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";

AceButton boot;
uint8_t state = 0;

LilygoRelays relays = LilygoRelays(LilygoRelays::Lilygo6Relays,3);

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
    Serial.printf("[AceButton][%u]  N:%d E:%u S:%u state:%d\n", millis(), n->getId(), buttonState, buttonState, state);
    switch (state) {
    case 0:
        //control.setAllHigh();
        for (int i=0; i<relays.numberOfRelays();i++)
          relays[i].setRelayStatus(HIGH);
        relays.setLEDStatus(HIGH,LilygoRelays::GreenLED,100,100);
        relays.setLEDStatus(HIGH,LilygoRelays::RedLED,1000,1000);
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
          delay(250);
        }
        break;
    case 3:
        // setting single pins
        for (int i=0; i<relays.numberOfRelays();i++){
          relays[i].setRelayStatus(LOW);
          delay(250);
        }
     break;
    default:
        break;
    }
    state++;
    state %= 4;
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
    ESP_LOGD(TAG, "NAME");
    return name;
  }
  else if (replacementString == "SSID"){
    ESP_LOGD(TAG, "SSID");
    return ssid;
  }
  else if (replacementString == "PASS"){
    ESP_LOGD(TAG, "PASS");
    return pass;
  }
  else if (replacementString == "RELAYARRAYCONFIG"){
    ESP_LOGD(TAG, "RELAYARRAYCONFIG");
    String retString =
    String("<div class=\"input-container\">") +
    String(    "<label for=\"input1\">Name:</label>") +
    String(    "<input type=\"text\" id=\"name\" name=\"name\" value=\"" + name + "\" maxlength=\"25\">") +
    String("</div><br>");
    for (int i = 0; i<relays.numberOfRelays(); i++) {
      retString += configHTML(relays[i]);
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
  //Serial.print("Relay Updated ");
  //Serial.print(relay);
  //Serial.print(" value=");
  //Serial.println(value);
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
  //delay(20000); //20 seconds for debugging/
  Serial.println("Booted");

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
    Serial.println("Names was empty");
//    name = "Relay Control";
//    fileSystem.saveToFile(namePath,name);
  }

  fileSystem.openFromFile(ssidPath, ssid);
  if (ssid == "") {
    Serial.println("SSID was empty");
//    ssid = SECRET_SSID;
//    fileSystem.saveToFile(ssidPath,ssid);
  }

  fileSystem.openFromFile(passPath,pass);
  if (pass == ""){
    Serial.println("PASS was empty");
//   pass=SECRET_PASS;
//    fileSystem.saveToFile(passPath,pass);
  }

  fileSystem.openFromFile(configPath,config);
  if (config == ""){
    Serial.println("Config was empty");
  } else {
    Serial.println("Initializing Relays");
    relays.initialize(config);    
  }

  relays.setRelayUpdateCallback(relayUpdated);
  


if (!initWiFi()){
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP("RELAYCONTROL", "aabbcc112233");

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

}

  ESP_LOGD(TAG, "%s", ssid);
  //ESP_LOGD(TAG, "%s", pass);

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
          //ESP_LOGD(TAG, "Name set to: %s", name);
          Serial.println("Name = " + name);
          // Write file to save value
          fileSystem.saveToFile(namePath, name);
        }
        // HTTP POST ssid value
        if (p->name() == PARAM_SSID) {
          ssid = p->value().c_str();
          Serial.println("SSID = " + ssid);
          //ESP_LOGD(TAG, "SSID set to: %s", ssid);
          // Write file to save value
          fileSystem.saveToFile(ssidPath, ssid);
        }
        // HTTP POST pass value
        if (p->name() == PARAM_PASS) {
          pass = p->value().c_str();
          Serial.println("Pass = " + pass);
          //ESP_LOGD(TAG, "Password set to: %s", pass);
          // Write file to save value
          fileSystem.saveToFile(passPath, pass);            
        }
        //ESP_LOGD(TAG, "POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
    request->send(200, "text/plain", "Done. ESP will restart, connect to your router on your network");
    delay(6000);
    request->redirect("/");
    delay(1000);
    ESP.restart();
  });

  server.addHandler(&events);    
  ElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
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
