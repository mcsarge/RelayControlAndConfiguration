#include <Arduino.h>
#include "LilyGoRelays.hpp"
#include "AutoData.h"

String configHTML(LilygoRelays::lilygoRelay relay, AutoData relayAutoData);
String actionHTML(LilygoRelays::lilygoRelay relay);
String eventListenerJS(LilygoRelays::lilygoRelay relay);
