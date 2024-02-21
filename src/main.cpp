/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-wi-fi-manager-asyncwebserver/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
//#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Effortless_SPIFFS.h>
#include "relayInformation.h"

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
const int ledPin = WIFI_STATUS_PIN;
// Stores LED state
String ledState;

relayInformation relays[4]={
  relayInformation(1, RELAY1_PIN, "Relay 1", "/relay1"),
  relayInformation(2, RELAY2_PIN, "Relay 2", "/relay2"),
  relayInformation(3, RELAY3_PIN, "Relay 3", "/relay3"),
  relayInformation(4, RELAY4_PIN, "Relay 4", "/relay4")
};


// Create a eSPIFFS class
#ifndef USE_SERIAL_DEBUG_FOR_eSPIFFS
  // Create fileSystem
  eSPIFFS fileSystem;
#else
  // Create fileSystem with debug output
  eSPIFFS fileSystem(&Serial);  // Optional - allow the methods to print debug
#endif



String processor(const String& var){
  Serial.println(var);
  if(var == "NAME"){
     return name;
  }
  else if (var == "RELAYARRAYCONFIG"){
    String retString =
       "<label for=\"name\">Name </label><input type=\"text\" id =\"name\" name=\"name\"   value=\"" + name + "\"><br>";
    for (int i = 0; i<4; i++) {
       retString += relays[i].configHTML();
    }
    Serial.println(retString);
    return retString;
  }
  else if (var == "RELAYSWITCHES") {
    String retString = "";
       //"<p class=\"card-title\"><i class=\"fa-solid fa-plug\"></i>" + name + "</p><p>";
    for (int i = 0; i<4; i++) {
       retString += relays[i].actionHTML();
    }
    retString += "</p>";
    Serial.println(retString);
    return retString;
  }
  else if (var == "RELAYEVENTLISTENERS") {
    String retString;
    for (int i = 0; i<4; i++) {
      retString += relays[i].eventListenerJS();
    }
    Serial.println(retString);
    return retString;
  }
  
  return "";
}

void findSetRelay(String rname, int  val){
  Serial.println("In findStRelay. relay =" + rname + " value=" + val);
  for (int i=0; i<4; i++){
    if (rname==relays[i].getShortName()) {
      relays[i].setRelay(val);
    }
  }
}


// // Initialize SPIFFS
// void initSPIFFS() {
//   if (!SPIFFS.begin(true)) {
//     Serial.println("An error has occurred while mounting SPIFFS");
//   }
//   Serial.println("SPIFFS mounted successfully");
// }

// // Read File from SPIFFS
// String readFile(fs::FS &fs, const char * path, String defaultValue){
//   Serial.printf("Reading file: %s\r\n", path);

//   File file = fs.open(path);
//   if(!file || file.isDirectory()){
//     Serial.println("- failed to open file for reading");
//     return defaultValue;
//   }
  
//   String fileContent;
//   while(file.available()){
//     fileContent = file.readStringUntil('\n');
//     break;     
//   }
//   return fileContent;
// }
// String readFile(fs::FS &fs, const char * path){
//   return readFile(fs, path, String());
// }

// // Write file to SPIFFS
// void writeFile(fs::FS &fs, const char * path, const char * message){
//   Serial.printf("Writing file: %s\r\n", path);

//   File file = fs.open(path, FILE_WRITE);
//   if(!file){
//     Serial.println("- failed to open file for writing");
//     return;
//   }
//   if(file.print(message)){
//     Serial.println("- file written");
//   } else {
//     Serial.println("- frite failed");
//   }
// }

// Initialize WiFi
bool initWiFi() {
  if(ssid=="" || name==""){
    Serial.println("Undefined SSID or Device name.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while(WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.println(WiFi.localIP());
  return true;
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Create a eSPIFFS class
  #ifndef USE_SERIAL_DEBUG_FOR_eSPIFFS
    // Check Flash Size - Always try to incorrperate a check when not debugging to know if you have set the SPIFFS correctly
    if (!fileSystem.checkFlashConfig()) {
      Serial.println("Flash size was not correct! Please check your SPIFFS config and try again");
      delay(100000);
      ESP.restart();
    }
  #endif


  //initSPIFFS();

  // Set GPIO 2 as an OUTPUT
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  for (int i=0; i<4;i++){
    relays[i].relayInit();
  }

  // Load values saved in SPIFFS
  fileSystem.openFromFile(namePath, name);
  fileSystem.openFromFile(ssidPath, ssid);
  if (ssid == "")
    ssid = "SARGENET";
  fileSystem.openFromFile(passPath,pass);
  if (pass == "")
    pass="11223344556677889900aabbcc";

  //Read in the names, and if they are not there, use the defaults already stored in the relay object
  String tempName;
  for (int i=0; i<4;i++){
    fileSystem.openFromFile(relays[i].getRelayPath().c_str(),tempName);
    if (tempName!="")
      relays[i].relayName = tempName; 
  }  

  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(relays[0].relayName);
  Serial.println(relays[1].relayName);
  Serial.println(relays[2].relayName);
  Serial.println(relays[3].relayName);
  
  if(initWiFi()) {
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/index.html", "text/html", false, processor);
    });
    server.serveStatic("/", SPIFFS, "/");
    
    // Handle Web Server Events
    events.onConnect([](AsyncEventSourceClient *client){
      if(client->lastId()){
        Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
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
      Serial.print("caught post");
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST name value
          Serial.println(p->name());
          if (p->name() == PARAM_DEVICE_NAME) {
            name = p->value().c_str();
            Serial.print("Name set to: ");
            Serial.println(name);
            // Write file to save value
            //writeFile(SPIFFS, namePath, name.c_str());
            fileSystem.saveToFile(namePath, name);            
          }
          // HTTP POST relay value
          for (int i=0; i<4;i ++){
            if (p->name() == relays[i].getShortName()) {
              relays[i].relayName = p->value().c_str();
              Serial.print(relays[i].getFixedName() + " set to: ");
              Serial.println(relays[i].relayName);
              // Write file to save value
              //writeFile(SPIFFS, relays[i].getRelayPath().c_str(), relays[i].relayName.c_str());
              fileSystem.saveToFile(relays[i].getRelayPath().c_str(), relays[i].relayName);
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

      }
      else {
        inputMessage1 = "No message sent";
        inputMessage2 = "No message sent";
      }
      Serial.print("Relay: ");
      Serial.print(inputMessage1);
      Serial.print(" - Set to: ");
      Serial.println(inputMessage2);
      request->send(200, "text/plain", "OK");
  });

    server.addHandler(&events);    
    server.begin();
  }
  else {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    if (name != "")
      WiFi.softAP(name.c_str(), NULL);
    else
      WiFi.softAP("ESP-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP); 

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
            Serial.print("Name set to: ");
            Serial.println(name);
            // Write file to save value
            //writeFile(SPIFFS, namePath, name.c_str());
            fileSystem.saveToFile(namePath, name);
          }
          // HTTP POST ssid value
          if (p->name() == PARAM_SSID) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            //writeFile(SPIFFS, ssidPath, ssid.c_str());
            fileSystem.saveToFile(ssidPath, ssid);
          }
          // HTTP POST pass value
          if (p->name() == PARAM_PASS) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            //writeFile(SPIFFS, passPath, pass.c_str());
            fileSystem.saveToFile(passPath, pass);            
          }
          //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
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
 if ((millis() - lastTime) > timerDelay) {
  //is anybody receiving events?
  if (events.count()>0){

      bool nowState = digitalRead(relays[0].getRelayPin());
      if (nowState) {
        Serial.println("Relay 1 off");
        relays[0].setRelay(!nowState);
        // Send Event to the Web Client(s)
        events.send(String(!nowState).c_str(), relays[0].getShortName().c_str(),millis());
      }
  }
    
    lastTime = millis();
  }
}
