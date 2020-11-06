#include <Arduino.h>

#if defined(ESP8266)
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <AsyncElegantOTA.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#endif

#include <FastLED.h>

namespace {

#if defined(ESP8266)
const char* ssid = "VehicleLights";
const char* pass = "12345678";

DNSServer dnsServer;
AsyncWebServer server(80);

const uint8_t ledPin = D1;

const uint8_t pinBackLights = D5;
const uint8_t pinStop = D6;
const uint8_t pinLeft = D7;
const uint8_t pinRight = D8;

bool configureMode = false;
#else
const uint8_t ledPin = 2;

const uint8_t pinBackLights = 10;
const uint8_t pinStop = 9;
const uint8_t pinLeft = 8;
const uint8_t pinRight = 7;
#endif


const uint8_t numLeds = 32;
CRGB leds[numLeds];

const uint8_t backLightBrightness = 100;
const uint8_t stopBrightness = 255;
const uint8_t leftBrightness = 255;
const uint8_t rightBrightness = 255;

uint32_t turnProgress = 0;

CRGB turnPixels[] = {
  CRGB(255, 127, 0),
  CRGB(255, 127, 0),
  CRGB(255, 127, 0),
  CRGB(255, 127, 0),
  CRGB(255, 127, 0),
  CRGB(255, 127, 0),
  CRGB(255, 127, 0),
  CRGB(255, 127, 0),
  CRGB(255, 127, 0),
  CRGB(255, 127, 0),
  CRGB(255, 127, 0),
};
uint8_t turnSize = sizeof(turnPixels) / sizeof(turnPixels[0]);
const uint8_t turnSpeed = 250 / turnSize;

bool backPressed = false;
bool stopPressed = false;
bool leftPressed = false;
bool rightPressed = false;

}

#if defined(ESP8266)
class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    request->redirect(F("/update"));
  }
};
#endif

void setup(void) {
  Serial.begin(115200);
  Serial.println(F("Starting..."));
  FastLED.addLeds<WS2812B, ledPin, GRB>(leds, numLeds);

  pinMode(pinBackLights, INPUT_PULLUP);
  pinMode(pinStop, INPUT_PULLUP);
  pinMode(pinLeft, INPUT_PULLUP);
  pinMode(pinRight, INPUT_PULLUP);

#if defined(ESP8266)
  configureMode = digitalRead(pinBackLights) == HIGH
               && digitalRead(pinStop) == HIGH;

  Serial.println();
  Serial.print(F("Configure mode: "));
  Serial.println(configureMode ? F("ON") : F("OFF"));

  if (configureMode) {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(
      IPAddress(8, 8, 8, 8),
      IPAddress(8, 8, 8, 8),
      IPAddress(255, 255, 255, 0)
    );
    if (!WiFi.softAP(ssid, pass)) {
      Serial.println(F("Error starting AP"));
      ESP.restart();
      return;
    }

    Serial.println(F("AP started"));
    Serial.print(F("AP IP address: "));
    Serial.println(WiFi.softAPIP());

    dnsServer.start(53, "*", WiFi.softAPIP());
    AsyncElegantOTA.begin(&server);
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    server.begin();
    Serial.println(F("Update server started"));

    FastLED.setBrightness(80);
    fill_solid(leds, numLeds, CRGB(0, 0, 255));
    FastLED.show();
  }
#endif
  FastLED.setBrightness(255);
}

void loop(void) {
#if defined(ESP8266)
  if (configureMode) {
    dnsServer.processNextRequest();
    return;
  }
#endif

  const bool newBackPressed = digitalRead(pinBackLights) == HIGH;
  const bool newStopPressed = digitalRead(pinStop) == HIGH;
  const bool newLeftPressed = digitalRead(pinLeft) == HIGH;
  const bool newRightPressed = digitalRead(pinRight) == HIGH;

  if (newLeftPressed != leftPressed ||
      newRightPressed != rightPressed ||
      newStopPressed != stopPressed ||
      newBackPressed != backPressed) {
    FastLED.clear();
  }

  if (newLeftPressed != leftPressed ||
      newRightPressed != rightPressed) {
    turnProgress = 0;
  }

  if (newBackPressed) {
    fill_solid(leds + turnSize, numLeds - (turnSize * 2), CRGB(100, 0, 0));
  }

  if (newStopPressed) {
    if (newLeftPressed) {
      fill_solid(leds + turnSize, numLeds - turnSize, CRGB(255, 0, 0));
    } else if (newRightPressed) {
      fill_solid(leds, numLeds - turnSize, CRGB(255, 0, 0));
    } else {
      fill_solid(leds, numLeds, CRGB(255, 0, 0));
    }
  }

  if (newLeftPressed || newRightPressed) {
    if (turnProgress < turnSize) {
      for (uint8_t i = 0; i < turnProgress + 1; i++) {
        if (newLeftPressed) {
          leds[turnSize - turnProgress - 1 + i] = turnPixels[i];
        } else {
          leds[numLeds - turnSize + i] = turnPixels[turnProgress - i];
        }
      }
      EVERY_N_MILLISECONDS(turnSpeed) {
        turnProgress += 1;
      }
    } else {
      for (uint8_t i = 0; i < turnProgress; i++) {
        if (newLeftPressed) {
          leds[turnSize - turnProgress + i] = turnPixels[i];
        } else {
          leds[numLeds - turnSize + i] = turnPixels[turnProgress - i - 1];
        }
      }
    }
  }

  backPressed = newBackPressed;
  stopPressed = newStopPressed;
  leftPressed = newLeftPressed;
  rightPressed = newRightPressed;

  FastLED.show();
}