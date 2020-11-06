#include <Arduino.h>

#if defined(ESP8266)
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#endif

#include <FastLED.h>

namespace {

#if defined(ESP8266)
const char* ssid = "VehicleLights";
const char* pass = "12345678";

size_t updateSize = 0;

const char upload_html[] PROGMEM = \
"<form method='POST' enctype='multipart/form-data'>\n"\
"<h1>Vehicle Lights Updater</h1>\n"\
"<input id='file' type='file' name='update' onchange='sub(this)' style=display:none>\n"\
"<label id='file-input' for='file'>Click to choose file</label>\n"\
"<input id='btn' type='submit' class=btn value='Upload' disabled>\n"\
"</form>\n"\
"<script>\n"\
"function sub(obj) {\n"\
"    var fileName = obj.value.split('\\\\');\n"\
"    console.log(fileName);\n"\
"    document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];\n"\
"    document.getElementById('btn').disabled = false;\n"\
"};\n"\
"</script>\n"\
"<style>\n"\
"#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}\n"\
"input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}\n"\
"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:center;display:block;cursor:pointer}\n"\
"form{background:#fff;max-width:50%;margin:75px auto;padding:10px;border-radius:5px;text-align:center}\n"\
".btn{background:#3498db;color:#fff;cursor:pointer}\n"\
".btn:disabled{background:#98342b;color:#fff;cursor:pointer}\n"\
"</style>\n";

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
void updateRequestHandler(AsyncWebServerRequest *request)
{
    Serial.println(F("updateRequestHandler"));
    request->send(200);
    ESP.restart();
}

void drawProgress(size_t progress)
{
    double pcs;
    if (updateSize == 0) {
        pcs = 0.5;
    } else {
        pcs = static_cast<double>(progress) / updateSize;
    }
    fill_solid(leds, numLeds * pcs, CRGB(0, 40, 127));
}

void updateHandler(uint8_t *data, size_t len, size_t index, size_t total, bool final)
{
    if (index == 0) {
        FastLED.clear();
        Serial.println(F("Update started!"));
        Serial.printf_P(PSTR("Total size: %zu\n"), total);
        
        updateSize = total;
        Update.runAsync(true);
        if (!Update.begin(total, U_FLASH)) {
            Serial.print(F("Upload begin error: "));
            Update.printError(Serial);
            fill_solid(leds, numLeds, CRGB(255, 0, 0));
            return;
        }
    }
    drawProgress(index + len);
    if (Update.write(data, len) != len) {
        Serial.print(F("Upload write error: "));
        Update.printError(Serial);
            fill_solid(leds, numLeds, CRGB(255, 0, 0));
        return;
    }
    if (final) {
        if (!Update.end(true)) {
            Serial.print(F("Upload end error: "));
            Update.printError(Serial);
            fill_solid(leds, numLeds, CRGB(255, 0, 0));
            return;
        }
        Serial.printf_P(PSTR("Update Success: %zd\nRebooting...\n"), index + len);
        fill_solid(leds, numLeds, CRGB(0, 255, 0));
        return;
    }
    ESP.wdtFeed();
    FastLED.show();
}

void updateBodyHandler(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    updateHandler(data, len, index, total, index + len == total);
}

void updateFileHandler(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
{
    updateHandler(data, len, index, request->contentLength(), final);
}
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

    server.on(PSTR("/update"), HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", upload_html);
    });
    server.on(PSTR("/update"), HTTP_POST, updateRequestHandler, updateFileHandler, updateBodyHandler);
    server.onNotFound([](AsyncWebServerRequest *request){
      request->redirect(F("/update"));
    });
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