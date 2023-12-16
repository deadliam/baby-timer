#include <Adafruit_NeoPixel.h>
#include <GyverPortal.h>
#include <LittleFS.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include "NTPClient.h"
#include "WiFiUdp.h"
#include <EEPROM.h>

// Graph of food quality
const char* qualityServer = "api.thingspeak.com";
String apiKey = "MLEOS3KZ6FCKOYB3";
WiFiClient qualityClient;

unsigned long previousMillis = 0UL;
unsigned long timeInterval = 1000UL;
time_t currentEpochTime;

GyverPortal ui(&LittleFS);

int spinnerValue = 3;
int minVal = 2;
int maxVal = 6;

#define MAX_LOG_LINES 20
int counter = 0;
String logs[MAX_LOG_LINES] = {};

// void SPINNER(const String& name, float value = 0, float min = NAN, float max = NAN, float step = 1, uint16_t dec = 0, PGM_P st = GP_GREEN, const String& w = "", bool dis = 0) {
GP_SPINNER sp1("sp1", spinnerValue, minVal, maxVal, 1, 0, GP_BLUE, "", 0);
GP_AREA ar("ar", MAX_LOG_LINES, "", "");

#define NUM_LEDS 38
#define DATA_PIN D4
#define BRIGHTNESS 50
#define LED_TYPE NEO_GRB // Adafruit_NeoPixel uses a different naming convention
#define FRAMES_PER_SECOND 120

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, DATA_PIN, LED_TYPE);

const long utcOffsetInSeconds = 19800;
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

GPdate valDate;
GPtime valTime;

int interval = 3;
int currentPixel = 0;
time_t storeEpochTime = 0;
time_t operationEpochTime = 0;

bool isTimerStarted = false;

#define EEPROM_SIZE 512
int eepromTarget;
int eepromIntervalAddress = 0;
int eepromStoreEpochTimeAddress = 1;
int eepromStoreLogsArrayAddress = 2;

// String label1 = "test1";
// String label2 = "test2";

enum foodQuality {GOOD, ORDINARY, BAD};
String foodQualityGood = "😋";
String foodQualityOrdinary = "😐";
String foodQualityBad = "🤔";

String currentFoodQuality;
int currentFoodQualityIntValue = 0;

enum borderColor {RED, BLUE, GREEN};

#define PLOT_SIZE 20
struct PlotData {
  int16_t vals[1][PLOT_SIZE];
  uint32_t unix[PLOT_SIZE];
};
PlotData data;
const char *graphNames[] = {"quality",};

void build() {
  GP.BUILD_BEGIN();

  GP.THEME(GP_DARK);
  //GP.THEME(GP_LIGHT);
  GP.UPDATE("txt1,txt2,sp1");
  GP.TITLE("BABY TIMER");
  GP.HR();

  GP.NAV_TABS("Action,Log,Graph", GP_BLUE_B);

  GP.NAV_BLOCK_BEGIN();
  // M_NAV_BLOCK(
  M_BLOCK_THIN(
    M_BOX(
      GP.LABEL("Elapsed:");
      GP.TIME("time", valTime);
    );
  );
  M_BLOCK_THIN(
    // GP.BUTTON("reset", "Reset", "", GP_BLUE_B, "100", 0, 0);
    GP.BUTTON("good", "😋 Good", "", GP_GREEN_B, "100", 0, 0);
    GP.BUTTON("ordinary", "😐 Ordinary", "", GP_BLUE_B, "100", 0, 0);
    GP.BUTTON("bad", "🤔 Bad", "", GP_RED_B, "100", 0, 0);
  );
  GP.BUTTON("stop", "Stop", "", GP_GRAY, "100", 0, 0);

  M_BOX(
    GP.LABEL("Food interval:");
    GP.BREAK();
    GP.SPINNER(sp1);
  );
  GP.NAV_BLOCK_END();
  // );

  GP.NAV_BLOCK_BEGIN();
    GP.AREA(ar); GP.BREAK();
    // GP.BUTTON("show_log", "Show Log", "", GP_GRAY, "100", 0, 0);
  GP.NAV_BLOCK_END();

  GP.NAV_BLOCK_BEGIN();
    GP.PLOT_STOCK_DARK<1, PLOT_SIZE>("plot1", graphNames, data.unix, data.vals);
  GP.NAV_BLOCK_END();

  // M_BOX(GP.TEXT("txt1", "text1", label1); GP.BREAK(););
  // M_BOX(GP.TEXT("txt2", "text2", label2); GP.BREAK(););
  GP.BUILD_END();
}

void setup() {
  Serial.begin(115200);

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // Uncomment and run it once, if you want to erase all the stored information
  // wifiManager.resetSettings();
  
  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  wifiManager.setHostname("babytimer");

  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the specified name
  // here  "AutoConnectAP"
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("AutoConnectAP");
  // or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();
  
  // if you get here you have connected to the WiFi
  Serial.println("Connected.");

  // Set up mDNS responder:
  // - first argument is the domain name, in this example
  //   the fully-qualified domain name is "esp8266.local"
  // - second argument is the IP address to advertise
  //   we send our IP address on the WiFi network

  if (!MDNS.begin("babytimer")) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  MDNS.addService("http", "tcp", 80);
  
  // ========================================================
  //Init EEPROM
  EEPROM.begin(EEPROM_SIZE);

  EEPROM.get(eepromIntervalAddress, eepromTarget);
  Serial.print("eepromTarget: ");
  Serial.println(eepromTarget);
  if (isnan(eepromTarget) || eepromTarget == 255) {
    eepromTarget = interval;
    Serial.print("Set eepromTarget to: ");
    Serial.println(eepromTarget);

    EEPROM.put(eepromIntervalAddress, eepromTarget);
    EEPROM.commit();
  } else {
    interval = eepromTarget;
    Serial.print("eepromTarget is: ");
    Serial.println(eepromTarget);
  }
  // ========================================================

  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(7200);

  // ========================================================
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  pixelsOff();
  strip.show(); // Initialize all pixels to 'off'

  ui.attachBuild(build);
  ui.attach(action);
  ui.start();
  ui.log.start(30);
  ui.enableOTA();

  // ========================================================
  if (!LittleFS.begin()) Serial.println("FS Error");
  ui.downloadAuto(true);
}

void action() {

  if (ui.update()) {
    // ui.updateString("txt1", label1);
    // ui.updateString("txt2", label2);
    fillLogArea();
    ui.update(ar);
  }

  if (ui.click()) {

    if (ui.click("good")) {
      Serial.println("FOOD QUALITY GOOD");
      resetInterval(GOOD);
    }
    if (ui.click("ordinary")) {
      Serial.println("FOOD QUALITY ORDINARY");
      resetInterval(ORDINARY);
    }
    if (ui.click("bad")) {
      Serial.println("FOOD QUALITY BAD");
      resetInterval(BAD);
    }
    if (ui.click("stop")) {
      Serial.println("STOP TIMER");
      stopTimer();
    }
    if (ui.click(sp1)) {
      Serial.print("Spinner value: ");
      Serial.println(sp1.value);
      interval = sp1.value;
      EEPROM.put(eepromIntervalAddress, interval);
      EEPROM.commit();
    }
  }
}

void loop() {

  ui.tick();
  //------------------------------------

  // UI Update
  // ui.updateDate("date", valDate);

  ui.updateTime("time", valTime);
  ui.update("sp1");
  // ui.updateString("txt1", label1);
  // ui.updateString("txt2", label2);

  MDNS.update();
  timeClient.update();

  unsigned long currentMillis = millis();
    /* The Arduino executes this code once every second
    *  (timeInterval = 1000 (ms) = 1 second).
    */

  if (currentMillis - previousMillis > timeInterval) {
    currentEpochTime = timeClient.getEpochTime();
    updatePixels();
    previousMillis = currentMillis;
  }
  // time_t currentEpochTime = timeClient.getEpochTime();
  // Serial.print("Epoch Time: ");
  // Serial.println(currentEpochTime);

  //Get a time structure
  // struct tm *ptm = gmtime ((time_t *)&currentEpochTime); 
  // int monthDay = ptm->tm_mday;
  // int currentMonth = ptm->tm_mon+1;
  // int currentYear = ptm->tm_year+1900;

  // valDate.set(currentYear, currentMonth, monthDay);
  // valTime.set(timeClient.getHours(), timeClient.getMinutes(), timeClient.getSeconds());
}

void stopTimer() {
  Serial.println("STOP");
  isTimerStarted = false;
  currentPixel = 0;
  pixelsOff();

  char format[] = "hh:mm:ss";
  String elapsedTimeString = getTimeString(currentEpochTime, format);
  String prevValue = logs[counter - 1];
  logs[counter - 1] = prevValue + " - " + elapsedTimeString;
}

void fillLogArea() {
  String textAreaString = "";
  for (int i = 0; i < MAX_LOG_LINES; i++) {
    if (logs[i].length() == 0) {
      continue;
    }
    textAreaString += "#";
    textAreaString += String(i + 1);
    textAreaString += "\t";
    textAreaString += logs[i];
    textAreaString += "\n";
  }
  ar.text = textAreaString;
}

void foodQualityValue(foodQuality quality) {
  // {GOOD, ORDINARY, BAD};
  // 😋 😐 🤔
  if(quality == GOOD)
  {
    currentFoodQuality = foodQualityGood;
    currentFoodQualityIntValue = 1;
  }
  else if(quality == ORDINARY)
  {
    currentFoodQuality = foodQualityOrdinary;
    currentFoodQualityIntValue = 0;
  }
  else if(quality == BAD)
  {
    currentFoodQuality = foodQualityBad;
    currentFoodQualityIntValue = -1;
  }
}

void resetInterval(foodQuality quality) {
  Serial.println("FOOD QUALITY: ");
  Serial.println(quality);
  foodQualityValue(quality);
  postFoodQualityGraph(currentFoodQualityIntValue);

  pixelsOn();
  storeEpochTime = currentEpochTime;
  operationEpochTime = currentEpochTime;
  currentPixel = 0;
  isTimerStarted = true;

  char format[] = "hh:mm:ss";
  String elapsedTimeString = getTimeString(currentEpochTime, format);
  String currentLine = currentFoodQuality + elapsedTimeString;
  logs[counter] = currentLine;
  if (counter < MAX_LOG_LINES) {
    logs[counter + 1] = "---------------";
  }
  counter += 1;

  GPaddInt(currentFoodQualityIntValue, data.vals[0], PLOT_SIZE);
  // Correct time with minus 2 hours
  GPaddUnix(currentEpochTime - 7200, data.unix, PLOT_SIZE);

  // Reset logs line to the first one
  if (counter >= MAX_LOG_LINES) {
    counter = 0;
  }
}

void updatePixels() {
  int currentInterval;
  EEPROM.get(eepromIntervalAddress, currentInterval);
  int pixelDivisionValue = (currentInterval * 3600) / NUM_LEDS;

  if (isTimerStarted) {
    if ((int(currentEpochTime) - int(operationEpochTime)) >= pixelDivisionValue) {
      operationEpochTime += pixelDivisionValue;
      currentPixel += 1;
    }

    Serial.print("currentPixel: ");
    Serial.println(currentPixel);
    if (currentPixel <= NUM_LEDS) {
      setPixel(currentPixel);
    } else if (currentPixel > NUM_LEDS && currentPixel <= NUM_LEDS * 2) {
      setOvertimePixel(currentPixel);
    }

    time_t epochTimeInterval = currentEpochTime - storeEpochTime;
    char format[] = "hh:mm:ss";
    tm tm;
    gmtime_r(&epochTimeInterval, &tm);
    valTime.set(tm.tm_hour, tm.tm_min, tm.tm_sec);
  } else {
    valTime.set(0, 0, 0);
  }
}

void setOvertimePixel(int pixel) {
  if (pixel <= NUM_LEDS * 2) {
    strip.setPixelColor(pixel - NUM_LEDS, strip.Color(80, 0, 20));
    strip.show();
    setPixelBorders();
  }
}

void setPixelBorders() {
  uint32_t color = strip.Color(0, 0, 0);
  if (currentFoodQuality == foodQualityGood) {
    color = strip.Color(0, 100, 0);
  } else if (currentFoodQuality == foodQualityOrdinary) {
    color = strip.Color(0, 0, 100);
  } else if (currentFoodQuality == foodQualityBad) {
    color = strip.Color(100, 0, 0);
  }
  strip.setPixelColor(0, color);
  strip.setPixelColor(NUM_LEDS - 1, color);
  strip.show();
}

void setPixel(int pixel) {
  strip.setPixelColor(pixel, strip.Color(0, 0, 0));
  strip.show();
  setPixelBorders();
}

void pixelsOff() {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
}

void pixelsOn() {
  Serial.println("pixelsOn");
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, 50, 50));
  }
  strip.show();
}

String getTimeString(time_t timeInterval, const char *format) {
  char buf[9];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
           hour(timeInterval), minute(timeInterval), second(timeInterval));
  return String(buf);
}

void postFoodQualityGraph(int point) {
  if (qualityClient.connect(qualityServer,80))     // "184.106.153.149" or api.thingspeak.com
  { 
    String postStr = apiKey;
    postStr +="&field1=";
    postStr += String(point);
    postStr += "\r\n\r\n";

    qualityClient.print("POST /update HTTP/1.1\n");
    qualityClient.print("Host: api.thingspeak.com\n");
    qualityClient.print("Connection: close\n");
    qualityClient.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n");
    qualityClient.print("Content-Type: application/x-www-form-urlencoded\n");
    qualityClient.print("Content-Length: ");
    qualityClient.print(postStr.length());
    qualityClient.print("\n\n");
    qualityClient.print(postStr);
  }
  qualityClient.stop();
}