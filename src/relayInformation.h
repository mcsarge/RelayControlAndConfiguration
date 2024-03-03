#include <Arduino.h>


class relayInformation
{
  public:
    String relayName;            //POE
    bool relayStartState = false;     //

    relayInformation();
    relayInformation(int index, int pin, String name, int duration=-1, String path ="", bool startState = false, int buttonPin=-1);
    relayInformation(String rawJson);
    String getShortName();
    String getFixedName();
    String getRelayPath();
    int getRelayPin();
    String configHTML();
    String actionHTML();
    String eventListenerJS();
    bool currentState();
    bool setRelay(int  val);
    bool relayInit();
    String toRawJson();
    int getErrorCondition();
    int getMomentaryDurationInSeconds();
    void setMomentaryDuration(int durationInSeconds);
    bool loop();

  private:
    unsigned long lastSetMillis;
    int momentaryDuration = -1;
    int errorCondition = 0;
    int relayIndex;
    String relayShortName;       //relay1
    String relayFixedName;       //Relay 1
    int relayPin;                //33
    int relayButtonPin = 0; //optional button connected pin
    String relayPath;            //SPIFFs path for relayFile



};
