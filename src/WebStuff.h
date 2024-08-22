#include <Arduino.h>
#include "LilygoRelays.hpp"
#include "AutoData.h"

String configHTML(LilygoRelays::lilygoRelay relay, AutoData relayAutoData);
String actionHTML(LilygoRelays::lilygoRelay relay);
String eventListenerJS(LilygoRelays::lilygoRelay relay);
