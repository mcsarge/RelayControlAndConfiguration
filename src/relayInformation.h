#include <Arduino.h>

class relayInformation
{
  private:
    String relayShortName;       //relay1
    String relayFixedName;       //Relay 1
    int relayPin;                //33
    int relayButtonPinParam = 0; //optional button connected pin
    String relayPath;            //SPIFFs path for relayFile

  public:
    String relayName;            //POE
    bool relayStartState = false;     //

    relayInformation(int index, int pin, String name, String path ="", bool startState = false, int buttonPin=-1);
    String getShortName();
    String getFixedName();
    String getRelayPath();
    int getRelayPin();
    String configHTML();
    String actionHTML();
    String eventListenerJS();
    bool setRelay(int  val);
    bool relayInit();
};
