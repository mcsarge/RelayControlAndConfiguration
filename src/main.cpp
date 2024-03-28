/*********
Relay Control and Configuration
This code works with the Lilygo Relay 8 and relay 4
*********/

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <Effortless_SPIFFS.h>
#include <esp_log.h>
#include <SensorPCF8563.hpp>
#include <esp_sntp.h>
#include <AceButton.h>
#include "secrets.h"
#include "LilygoRelays.hpp"
#include <ElegantOTA.h>

static const char* TAG = "RelayControl";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create an Event Source on /events
AsyncEventSource events("/events");

// Search for parameter in HTTP POST request
const char* PARAM_DEVICE_NAME = "name";
const char* PARAM_SSID = "ssid";
const char* PARAM_PASS = "pass";


//Variables to save values from HTML form
String name;
String ssid;
String pass;

// File paths to save input values permanently
const char* namePath = "/name.txt";
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* relayPath = "/relays.txt";

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

using namespace ace_button;

#define I2C_SDA                     16
#define I2C_SCL                     17
#define RTC_IRQ                     15

#define SAVE_DELAY                  2000 // Delay to wait after a save was requested in case we get a bunch quickly

SensorPCF8563 rtc;
uint32_t lastMillis;
uint32_t lastSaveRequestTime=-1;

unsigned long ota_progress_millis = 0;

const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";

AceButton boot;
uint8_t state = 0;

LilygoRelays relays = LilygoRelays(LilygoRelays::Lilygo6Relays,2);


// Create a eSPIFFS class
#ifndef USE_SERIAL_DEBUG_FOR_eSPIFFS
  // Create fileSystem
  eSPIFFS fileSystem;
#else
  // Create fileSystem with debug output
  eSPIFFS fileSystem(&Serial);  // Optional - allow the methods to print debug
#endif

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


void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}


String configHTML(int index){
  return 
      String("<div class=\"relay-section\">")
    + String("<h3>" + relays[index].getRelayFixedName() + "</h3>")
    + String(   "<div class=\"relay-item\">")
    + String(     "<label for=\"" + relays[index].getRelayFixedShortName() + "\">Name:</label>")
    + String(     "<input type=\"text\" id=\""+ relays[index].getRelayFixedShortName() + "\" name=\"" + relays[index].getRelayFixedShortName() + "\" value=\"" + relays[index].relayName + "\" maxlength=\"25\">")
    + String(   "</div>")
    + String(   "<div class=\"relay-item\">")
    + String(     "<label for=\"" + relays[index].getRelayFixedShortName() + "-duration\">Duration:</label>")
    + String(     "<input type=\"number\" id=\"" + relays[index].getRelayFixedShortName() + "-duration\" name=\"" + relays[index].getRelayFixedShortName() + "-duration\" value=\"" + relays[index].momentaryDuration + "\" maxlength=\"4\">")
    + String(   "</div>")
    + String( "</div>");
}

String actionHTML(int index){
  return 
    String("<div class=\"relay-item\">") +
    String(   "<label class=\"switch\">") +
    String(     "<input type=\"checkbox\" id=\"" + relays[index].getRelayFixedShortName() + "\" onchange=\"toggleCheckbox(this)\" " + String(relays[index].getRelayStatus()==1?"checked":"") + ">") +
    String(     "<span class=\"slider\"></span>") +
    String(   "</label>") +
    String(   "<span class=\"relay-name\">" + relays[index].relayName + "</span>") +
    String( "</div>");
};

String eventListenerJS(int index){
   return "source.addEventListener('" + relays[index].getRelayFixedShortName() + "', function(e) {" +
          "console.log(\"" + relays[index].getRelayFixedShortName() + "\", e.data);" +
          "document.getElementById(\"" + relays[index].getRelayFixedShortName() + "\").checked = (e.data?.toLowerCase()?.trim()==\"1\");}, false);";
};

String processor(const String& replacementString){
  ESP_LOGD(TAG, "In Processor");
  if(replacementString == "NAME"){
    ESP_LOGD(TAG, "NAME");
    return name;
  }
  else if (replacementString == "RELAYARRAYCONFIG"){
    ESP_LOGD(TAG, "RELAYARRAYCONFIG");
    String retString =
    String("<div class=\"input-container\">") +
    String(    "<label for=\"input1\">Name:</label>") +
    String(    "<input type=\"text\" id=\"name\" name=\"name\" value=\"" + name + "\" maxlength=\"25\">") +
    String("</div><br>");
    for (int i = 0; i<relays.numberOfRelays(); i++) {
      retString += configHTML(i);
    }
    return retString;
  }
  else if (replacementString == "RELAYSWITCHES") {
    ESP_LOGD(TAG, "RELAYSWITCHES");
    String retString = "";
    for (int i = 0; i<relays.numberOfRelays(); i++) {
       retString += actionHTML(i);
    }
    retString +="<button class=\"save-button\" onclick=\"saveStates(this)\">Save States</button>";
    return retString;
  }
  else if (replacementString == "RELAYEVENTLISTENERS") {
    ESP_LOGD(TAG, "RELAYEVENTLISTENERS");
    String retString;
    for (int i = 0; i<relays.numberOfRelays(); i++) {
      retString += eventListenerJS(i);
    }
    return retString;
  }
    
  ESP_LOGD(TAG, "Returning nothing");
  return "";
}

void findSetRelay(String rname, int  val){
  ESP_LOGD(TAG, "In findSetRelay relay = %s val= %d", rname, val);
  for (int i=0; i<relays.numberOfRelays(); i++){
    if (rname==relays[i].getRelayFixedShortName()) {
      relays[i].setRelayStatus(val);
    }
  }
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


#ifdef RTC_IRQ
  pinMode(RTC_IRQ, INPUT_PULLUP);
#endif

  // Serial port for debugging purposes
  Serial.begin(115200);
  delay(200);
  Serial.println("Booted");

  relays.initialize();
  Serial.println(relays.numberOfLEDs());
  relays.setRelayUpdateCallback(relayUpdated);
  
  const uint8_t boot_pin = 0;
  pinMode(boot_pin, INPUT_PULLUP);
  boot.init(boot_pin, HIGH, 0);
  ButtonConfig *buttonConfig = boot.getButtonConfig();
  buttonConfig->setEventHandler(ButtonHandleEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setLongPressDelay(5000);

  for (int i=0; i<5; i++){
    if (!rtc.begin(Wire, PCF8563_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        Serial.println("Failed to find PCF8563 - check your wiring!");
        delay(1000);
    } else {
        Serial.println("Found PCF8563!");
        break;
    }
  }


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
    name = "Relay Control";
    fileSystem.saveToFile(namePath,name);
  }
  fileSystem.openFromFile(ssidPath, ssid);
  if (ssid == "") {
    ssid = SECRET_SSID;
    fileSystem.saveToFile(ssidPath,ssid);
  }
  fileSystem.openFromFile(passPath,pass);
  if (pass == ""){
    pass=SECRET_PASS;
    fileSystem.saveToFile(passPath,pass);
  }

  //Read in the names, and if they are not there, use the defaults already stored in the relay object
  // String rawJson;
  // relayInformation tempRelay;
  // for (int i=0; i<relays.numberOfRelays();i++){
  //   rawJson="";
  //   fileSystem.openFromFile(relays[i].getRelayPath().c_str(),rawJson);
  //   if (rawJson!=""){
  //     Serial.println(rawJson);
  //     relayInformation tempRelay = relays[i];
  //     relays[i] = relayInformation(rawJson);
  //     if (relays[i].getErrorCondition()==-1){
  //       ESP_LOGE(TAG, "relay from json data failed");
  //       relays[i] = tempRelay;
  //     }
  //   } 
  // }  

  // for (int i=0; i<relays.numberOfRelays();i++){
  //   relays[i].relayInit();
  // }

  ESP_LOGD(TAG, "%s", ssid);
  //ESP_LOGD(TAG, "%s", pass);
  for (int i=0; i<relays.numberOfRelays();i++){
    ESP_LOGD(TAG, "%s", relays[i].relayName);
  }

     // Set Authentication Credentials
    ElegantOTA.setAuth("admin", "aabbcc112233");
    // ElegantOTA callbacks
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);
    


  if(initWiFi()) {

    // set notification call-back function
    sntp_set_time_sync_notification_cb( timeavailable );

    /**
     * A more convenient approach to handle TimeZones with daylightOffset
     * would be to specify a environmnet variable with TimeZone definition including daylight adjustmnet rules.
     * A list of rules for your zone could be obtained from https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
     */
    configTzTime("EST5EDT,M3.2.0,M11.1.0", ntpServer1, ntpServer2);
    //configTzTime("CST-8", ntpServer1, ntpServer2);


    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/index.html", "text/html", false, processor);
    });
    server.serveStatic("/", SPIFFS, "/");
    
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
        for (int i=0; i<relays.numberOfRelays(); i++){
          relays[i].startState = relays[i].getRelayStatus();
        }
        lastSaveRequestTime = millis(); //request a save
                
      } else {
        inputMessage1 = "No message sent";
        inputMessage2 = "No message sent";
      }
      request->send(200, "text/plain", "OK");
    });

    ElegantOTA.begin(&server);    // Start ElegantOTA

    server.addHandler(&events);    
    server.begin();
  
  } else {
    // Connect to Wi-Fi network with SSID and password
    ESP_LOGD(TAG, "Setting AP (Access Point)");
    // NULL sets an open Access Point
    if (name != "")
      WiFi.softAP(name.c_str(), NULL);
    else
      WiFi.softAP("ESP-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    ESP_LOGD(TAG, "AP IP address: %s", IP); 

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/wifimanager.html", "text/html");
    });
    
    server.serveStatic("/", SPIFFS, "/");
    
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
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
          //ESP_LOGD(TAG, "POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router on your network");
      delay(3000);
      ESP.restart();
    });

    ElegantOTA.begin(&server);    // Start ElegantOTA
    server.begin();
  }
}

// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;

void loop() {
  
  ElegantOTA.loop();
  boot.check();
  relays.loop();

  if (lastSaveRequestTime!=-1 and (lastSaveRequestTime+SAVE_DELAY<millis())) {
    lastSaveRequestTime = -1;
    ESP_LOGD(TAG,"Save was requested");
    Serial.println("Relays json data:" +  relays.asRawJson());
  }

  if (millis() - lastMillis > 3000) {

      lastMillis = millis();

      RTC_DateTime datetime = rtc.getDateTime();
      // Serial.printf(" Year :"); Serial.print(datetime.year);
      // Serial.printf(" Month:"); Serial.print(datetime.month);
      // Serial.printf(" Day :"); Serial.print(datetime.day);
      // Serial.printf(" Hour:"); Serial.print(datetime.hour);
      // Serial.printf(" Minute:"); Serial.print(datetime.minute);
      // Serial.printf(" Sec :"); Serial.println(datetime.second);

  }
}
