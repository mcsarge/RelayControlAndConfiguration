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
#include "relayInformation.h"
#include "secrets.h"

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

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

// Set LED GPIO
const int ledPin = LED_PIN;

// Stores LED state
String ledState;

relayInformation relays[RELAYS] ={
  relayInformation(1, RELAY1_PIN, "Relay 1", 2),
  relayInformation(2, RELAY2_PIN, "Relay 2"),
  relayInformation(3, RELAY3_PIN, "Relay 3"),
#if RELAYS<=4
  relayInformation(4, RELAY4_PIN, "Relay 4")
#else
  relayInformation(4, RELAY4_PIN, "Relay 4"),
  relayInformation(5, RELAY5_PIN, "Relay 5"),
  relayInformation(6, RELAY6_PIN, "Relay 6"),
  relayInformation(7, RELAY7_PIN, "Relay 7"),
  relayInformation(8, RELAY8_PIN, "Relay 8")
#endif
};


// Create a eSPIFFS class
#ifndef USE_SERIAL_DEBUG_FOR_eSPIFFS
  // Create fileSystem
  eSPIFFS fileSystem;
#else
  // Create fileSystem with debug output
  eSPIFFS fileSystem(&Serial);  // Optional - allow the methods to print debug
#endif



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

//      "<label for=\"name\">Name</label><input type=\"text\" id =\"name\" name=\"name\"   value=\"" + name + "\"><br>";
    for (int i = 0; i<RELAYS; i++) {
      retString += relays[i].configHTML();
    }
    return retString;
  }
  else if (replacementString == "RELAYSWITCHES") {
    ESP_LOGD(TAG, "RELAYSWITCHES");
    String retString = "";
    for (int i = 0; i<RELAYS; i++) {
       retString += relays[i].actionHTML();
    }
    retString +="<button class=\"save-button\" onclick=\"saveStates(this)\">Save States</button>";
    return retString;
  }
  else if (replacementString == "RELAYEVENTLISTENERS") {
    ESP_LOGD(TAG, "RELAYEVENTLISTENERS");
    String retString;
    for (int i = 0; i<RELAYS; i++) {
      retString += relays[i].eventListenerJS();
    }
    return retString;
  }
    
  ESP_LOGD(TAG, "Returning nothing");
  return "";
}

void findSetRelay(String rname, int  val){
  ESP_LOGD(TAG, "In findSetRelay relay = %s val= %d", rname, val);
  for (int i=0; i<RELAYS; i++){
    if (rname==relays[i].getShortName()) {
      relays[i].setRelay(val);
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

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Create a eSPIFFS class
  #ifndef USE_SERIAL_DEBUG_FOR_eSPIFFS
    // Check Flash Size - Always try to incorrperate a check when not debugging to know if you have set the SPIFFS correctly
    if (!fileSystem.checkFlashConfig()) {
      ESP_LOGE(TAG, "Flash size was not correct! Please check your SPIFFS config and try again");
      delay(100000);
      ESP.restart();
    }
  #endif


  // Set LED as an OUTPUT
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);


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
  String rawJson;
  relayInformation tempRelay;
  for (int i=0; i<RELAYS;i++){
    rawJson="";
    fileSystem.openFromFile(relays[i].getRelayPath().c_str(),rawJson);
    if (rawJson!=""){
      Serial.println(rawJson);
      relayInformation tempRelay = relays[i];
      relays[i] = relayInformation(rawJson);
      if (relays[i].getErrorCondition()==-1){
        ESP_LOGE(TAG, "relay from json data failed");
        relays[i] = tempRelay;
      }
    } 
  }  

  for (int i=0; i<RELAYS;i++){
    relays[i].relayInit();
  }

  ESP_LOGD(TAG, "%s", ssid);
  //ESP_LOGD(TAG, "%s", pass);
  for (int i=0; i<RELAYS;i++){
    ESP_LOGD(TAG, "%s", relays[i].relayName);
  }

  if(initWiFi()) {
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
          Serial.println(p->name());
          for (int i=0; i<RELAYS;i ++){
            bool saveIt = false;
            if (p->name() == relays[i].getShortName()) {
              if (relays[i].relayName != p->value().c_str()){
                saveIt = true;
                relays[i].relayName = p->value().c_str();
              }
            }
            if (p->name() == String(relays[i].getShortName()+"-duration")){
              if (relays[i].getMomentaryDurationInSeconds() != p->value().toInt()){
                saveIt = true;
                relays[i].setMomentaryDuration(p->value().toInt());
              }
            }
            if (saveIt) {
                ESP_LOGD(TAG, "Writing file for %s", relays[i].getFixedName());
                Serial.println("Relay :" +  relays[i].getFixedName() + " json data: " + relays[i].toRawJson());
                fileSystem.saveToFile(relays[i].getRelayPath().c_str(), relays[i].toRawJson().c_str());
            }
          }
        }
      }
      request->redirect("/");
    });

    // Send a GET request to <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
      String inputMessage1;
      String inputMessage2;
      // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
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
        for (int i=0;i<RELAYS; i++)
          fileSystem.saveToFile(relays[i].getRelayPath().c_str(), relays[i].toRawJson().c_str());            
                
      } else {
        inputMessage1 = "No message sent";
        inputMessage2 = "No message sent";
      }
      ESP_LOGD(TAG, "Relay: %s - set to: %s",inputMessage1, inputMessage2);
      request->send(200, "text/plain", "OK");
    });

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
    server.begin();
  }
}

// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;

void loop() {
  for (int i=0; i<RELAYS; i++){
    if (relays[i].loop()){
      if (events.count()>0){
        events.send(String(relays[i].currentState()).c_str(), relays[i].getShortName().c_str(),millis());
      }
    }
  }
}
