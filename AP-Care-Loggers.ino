#include <M5StickCPlus.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include "WebServer.h"
#include <Preferences.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "newAuth.h"
#include "time.h"
#include "esp_wifi.h"

/** Version selection
 *
 *  @note select only one
 */
//#define _BED//AP1-M
//#define _HALLWAY//AP4-M
#define _DOOR //AP2-M
//#define _KITCHEN//AP6-M

/** PIR sensor selection
 *
 *  @note if the HAT version of the sensor is used, uncomment the directive, leave commented otherwise
 */
#define _PIR_HAT

/** BME680 available directive
 * 
 *  @note if BME680 sensor is connected, uncomment the directive, leave commented otherwise 
 */
#define _HAS_BME

#define ADVERTISEMENT_INTERVAL  5//s

//Diff thresholds
#define DIFF_K  0.05//*100%
#define SMOKE_DIFF_K DIFF_K * 3

#ifdef _HALLWAY
#define _HALLWAY_SLEEP
#endif

#ifdef _HALLWAY
#ifdef _PIR_HAT
#define PIR_PIN      36
#else
#define PIR_PIN      33
#endif
#endif

#ifdef _DOOR
#include <VL53L0X.h>
VL53L0X tof;
RTC_DATA_ATTR bool mHold = false;
RTC_DATA_ATTR bool wasMotion = false;
RTC_DATA_ATTR long motionTime = 0;
RTC_DATA_ATTR bool stuckMute = false;
RTC_DATA_ATTR long stuckMuteTime = 0;

uint16_t calDist = 0;

#define STUCK_TIME 600000//ms
#define STUCK_MUTE 20000//ms
#endif

#ifdef _BED
//#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#define PIR_PIN      36
#define FSR_PIN      26

#ifdef _HAS_BME
Adafruit_BME680 bme;
#endif
#endif

#ifdef _KITCHEN
//#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#define PIR_PIN      36
#define LIGHT_PIN    0

#ifdef _HAS_BME
Adafruit_BME680 bme;
#endif
#endif

#define cTime        10000
#define cSleepTime   10

#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP  60

#define PIN_NUM_TO_MASK(PIN_NUM) ((uint64_t)((1 << (PIN_NUM - 31))) << 31)

#ifdef _HALLWAY
typedef struct data{
  bool pir;
  float bat;
}data_t;

data_t data;
data_t olddata;

void event(data_t data);
#endif

#ifdef _DOOR
typedef struct data{
  bool motion;
  uint16_t dist;
  float bat;
}data_t;

data_t data;
data_t olddata;

void event(data_t data);
#endif

#ifdef _BED
typedef struct data{
  bool pir; 
  uint16_t fsr; 
  float temp; 
  float hum; 
  float smoke; 
  float bat;
}data_t;

data_t data;
data_t olddata;

void event(data_t data);
#endif

#ifdef _KITCHEN
typedef struct data{
  bool pir; 
  uint16_t light; 
  float temp; 
  float hum; 
  float smoke; 
  float bat;
}data_t;

data_t data;
data_t olddata;

void event(data_t data);
#endif

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

const char* serverName = "http://192.168.2.7:1880/niceBridge";

char payload[1024];
long cStart;

authHandler auth;

const IPAddress apIP(192, 168, 4, 1);
char apSSID[20] = "niceAP-";

#ifdef _BED
char tag[5] = "1-M-";
#endif

#ifdef _HALLWAY
char tag[5] = "4-M-";
#endif

#ifdef _DOOR
char tag[5] = "2-M-";
#endif

#ifdef _KITCHEN
char tag[5] = "6-M-";
#endif

boolean settingMode;
String ssidList;
String wifi_ssid;
String wifi_password;

bool needConnect = true;

// DNSServer dnsServer;
WebServer webServer(80);

// wifi config store
Preferences preferences;

HTTPClient http;

bool diffCheck(uint8_t val, uint8_t oldVal, float K);
bool diffCheckF(float val, float oldVal, float K);
void diffEvent(data_t data, data_t olddata);

unsigned long lastAdv;

void wifiConfig(){
  preferences.begin("wifi-config");
  delay(10);
    if (restoreConfig()) {
        if (checkConnection()) { 
        settingMode = false;
        startWebServer();
        return;
        }
    }
    settingMode = true;
    setupMode();

    while (needConnect){
        if (settingMode) {
        }
        webServer.handleClient();
    }
}

void forceWifiConfig(){
  preferences.begin("wifi-config");
  delay(10);
    if (false) {
        if (checkConnection()) { 
        settingMode = false;
        startWebServer();
        return;
        }
    }
    settingMode = true;
    setupMode();

    needConnect = true;

    while (needConnect){
        if (settingMode) {
        }
        webServer.handleClient();
    }
}

void setup(){
    M5.begin();
    M5.Axp.SetChargeCurrent(100);
    //M5.Axp.ScreenBreath(0);
    //Wire.begin();
    //Serial init
    Serial.begin(115200);
    //Wire.begin(0, 26);
    sprintf(apSSID + strlen(apSSID), "%s", tag);
    uint8_t index = strlen(apSSID);
    apSSID[index++] = SN[8];
    apSSID[index++] = '-';
    apSSID[index++] = SN[14];
    apSSID[index++] = SN[15];

    #ifdef _DOOR
    Wire.begin();

    tof.setTimeout(500);
    if (!tof.init())
    {
        Serial.println("Failed to detect and initialize ToF sensor!");
        while (1) {}
    }
    tof.startContinuous(100);

    calDist = tof.readRangeContinuousMillimeters();
    #endif

    #ifdef _BED
    pinMode(FSR_PIN, INPUT);
    pinMode(PIR_PIN, INPUT);

    #ifdef _HAS_BME
    Wire1.begin(32, 33);
    bme.begin();
    #endif
    #endif

    #ifdef _KITCHEN
    pinMode(PIR_PIN, INPUT);
    pinMode(LIGHT_PIN, INPUT);

    #ifdef _HAS_BME
    Wire.begin(32, 33);
    bme.begin();
    #endif
    #endif

    #ifdef _HALLWAY
    pinMode(PIR_PIN, INPUT);
    #endif

    #ifdef _ALT_BED
    pinMode(FSR_PIN, INPUT);
    pinMode(PIR_PIN, INPUT);
    pinMode(LIGHT_PIN, INPUT);
    #endif

    wifiConfig();

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  ArduinoOTA
  .onStart([]() {
  String type;
  if (ArduinoOTA.getCommand() == U_FLASH)
    type = "sketch";
  else // U_SPIFFS
    type = "filesystem";

  // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
  Serial.println("Start updating " + type);
  })
    
  .onEnd([]() {
  Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
  Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  M5.Lcd.setCursor(0, 1);
  M5.Lcd.printf("Progress: %u%%     \r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
  Serial.printf("Error[%u]: ", error);
  if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
  else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
  else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
  else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
  else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}//setup

void loop(){
  data.bat = M5.Axp.GetBatVoltage();
  #ifdef _HALLWAY
  data.pir = digitalRead(PIR_PIN);
  #endif

  #ifdef _DOOR
  data.dist = tof.readRangeContinuousMillimeters();
  #endif

  #ifdef _BED
  data.pir = digitalRead(PIR_PIN);
  data.fsr = analogRead(FSR_PIN);

  #ifdef _HAS_BME
  data.temp = bme.readTemperature();
  data.hum = bme.readHumidity();
  data.smoke = bme.readGas();
  #else
  data.temp = 0.0;
  data.hum = 0.0;
  data.smoke = 0.0;
  #endif
  #endif

  #ifdef _KITCHEN
  data.pir = digitalRead(PIR_PIN);
  data.light = 4096 - analogRead(LIGHT_PIN);

  #ifdef _HAS_BME
  data.temp = bme.readTemperature();
  data.hum = bme.readHumidity();
  data.smoke = bme.readGas();
  #else
  data.temp = 0.0;
  data.hum = 0.0;
  data.smoke = 0.0;
  #endif
  #endif

  if ((millis() - lastAdv) >= (ADVERTISEMENT_INTERVAL * 60 * 1000)) {
    event(data);
    lastAdv = millis();
  } else {
    diffEvent(data, olddata);
  }

  M5.update();

  if (M5.BtnA.wasPressed()) {
    Serial.println("Blink Motherfucker!");
    OTAhandler();
  }

  if (M5.BtnB.wasPressed()) {
    Serial.println("starting force wifi config");
    forceWifiConfig();
  }

  #ifdef _HALLWAY_SLEEP
  esp_sleep_enable_ext1_wakeup(PIN_NUM_TO_MASK(PIR_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  M5.Axp.ScreenBreath(0);
  esp_deep_sleep_start();
  #endif

  olddata = data;
  delay(ADVERTISEMENT_INTERVAL * 1000);
}//loop

#ifdef _HALLWAY
void event(data_t data){
    wifi_ap_record_t stats;
    esp_wifi_sta_get_ap_info(&stats);

    struct tm mytime;
    getLocalTime(&mytime);
    time_t epoch = mktime(&mytime);
    Serial.printf("Timestamp: %ld", (long)epoch);
    
    sprintf(&payload[0], "{\
    \"sn\": \"%s\",\
    \"kid\": \"%s\",\
    \"body\":\
    [{\"LoggerName\": \"SysInfo\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"bat_v\",\"Value\": %f }, { \"Name\": \"wifi_rssi\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}, \
    { \"LoggerName\": \"PIR\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"motion\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}], \
    \"devId\": \"%s\",\
    \"includeTS\" : 0, \
    \"plen\": 2 }", SN, kid, (long)epoch, data.bat, stats.rssi, (long)epoch, data.pir, myId);

    http.begin(serverName);

    http.addHeader("Content-Type", "application/json");

    int ret = http.POST(payload);
    //kontrola responsu
    if(ret != 200){
      Serial.printf("ret: %d\r\n", ret);
    } else {
      Serial.println("OK");
    }

    //koniec
    http.end();
}

void diffEvent(data_t data, data_t olddata) {
  struct tm mytime;
    getLocalTime(&mytime);
    time_t epoch = mktime(&mytime);
    Serial.printf("Timestamp: %ld", (long)epoch);
    wifi_ap_record_t stats;
    esp_wifi_sta_get_ap_info(&stats);

    sprintf(&payload[0], "\
    {\
    \"sn\": \"%s\",\
    \"kid\": \"%s\",\
    \"devId\": \"%s\",\
    \"includeTS\" : 0, \
    \"body\": [", SN, kid, myId);

    uint8_t plen = 0;
    char buffer[256];

    if (data.pir != olddata.pir) {
      plen++;
      sprintf(buffer, "{\"LoggerName\": \"PIR\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"motion\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}", (long)epoch, data.pir);
      strcat(payload, buffer);
    }

    if (plen) {
      strcat(payload, ", ");
      plen++;
      sprintf(buffer, "{ \"LoggerName\": \"SysInfo\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"bat_v\",\"Value\": %f }, { \"Name\": \"wifi_rssi\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}], ", (long)epoch, data.bat, stats.rssi);
    }

    sprintf(buffer, "\"plen\": %d }", plen);
    strcat(payload, buffer);

    if (plen) {
      http.begin(serverName);

      http.addHeader("Content-Type", "application/json");

      int ret = http.POST(payload);
      //kontrola responsu
      if(ret != 200){
        Serial.printf("ret: %d", ret);
      } else {
        Serial.println("OK");
      }

      //koniec
      http.end();
    }
}
#endif

#ifdef _DOOR
void event(uint16_t dist, float bat){
    data.motion = (data.dist < (calDist - 100)) ? true : false;
    struct tm mytime;
    getLocalTime(&mytime);
    time_t epoch = mktime(&mytime);
    Serial.printf("Timestamp: %ld", (long)epoch);
    wifi_ap_record_t stats;
    esp_wifi_sta_get_ap_info(&stats);
    sprintf(&payload[0], "{\
    \"sn\": \"%s\",\
    \"kid\": \"%s\",\
    \"body\":\
    [{\"LoggerName\": \"ToF\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"motion\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}, \
    { \"LoggerName\": \"SysInfo\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"bat_v\",\"Value\": %f }, { \"Name\": \"wifi_rssi\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}], \
    \"devId\": \"%s\",\
    \"includeTS\" : 0, \
    \"plen\": 2 }", SN, kid, (long)epoch, data.motion, (long)epoch, data.bat, stats.rssi, myId);
    Serial.print(payload);
    char myjwt[410] = "Bearer "; 
    size_t jlen;
    auth.createJWT((uint8_t*)myjwt + strlen(myjwt), sizeof(myjwt) - strlen(myjwt), &jlen, epoch);
    Serial.printf("auth: %s\r\n", myjwt);

    http.begin(serverName);

    http.addHeader("Content-Type", "application/json");

    int ret = http.POST(payload);
    //kontrola responsu
    if(ret != 200){
      Serial.printf("ret: %d", ret);
    } else {
      Serial.println("OK");
    }

    //koniec
    http.end();
}

void diffEvent(data_t data, data_t olddata) {
    data.motion = (data.dist < (calDist - 100)) ? true : false;
    bool stuck = false;

    if (!mHold) {
      if (stuckMute) {
        if ((millis() - stuckMuteTime) >= STUCK_MUTE) {
          stuckMute = false;
        }
      }

      if (data.motion && (!stuckMute)) {
        if (!wasMotion) {
          wasMotion = true;
          motionTime = millis();
          Serial.println("mot");
        }else{
          wasMotion = false;
          stuckMute = true;
          stuckMuteTime = millis();
        }
      }

      if (wasMotion && ((millis() - motionTime) >= STUCK_TIME)) {
        stuck = true;
        wasMotion = false;
      }
    }

    mHold = data.motion;

    Serial.printf("m %d, was: %d, mt %ld, t %ld", data.motion, wasMotion, motionTime, millis());

    struct tm mytime;
    getLocalTime(&mytime);
    time_t epoch = mktime(&mytime);
    Serial.printf("Timestamp: %ld", (long)epoch);
    wifi_ap_record_t stats;
    esp_wifi_sta_get_ap_info(&stats);

    sprintf(&payload[0], "\
    {\
    \"sn\": \"%s\",\
    \"kid\": \"%s\",\
    \"devId\": \"%s\",\
    \"includeTS\" : 0, \
    \"body\": [", SN, kid, myId);

    uint8_t plen = 0;
    char buffer[256];

    if (data.motion != olddata.motion) {
      plen++;
      sprintf(buffer, "{\"LoggerName\": \"ToF\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"motion\",\"Value\": %d }, { \"Name\": \"stuck\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}", (long)epoch, data.motion, stuck);
      strcat(payload, buffer);
    }

    if (stuck) {
      if (plen) strcat(payload, ", ");
      plen++;
      sprintf(buffer, "{\"LoggerName\": \"ToF\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"stuck\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}", (long)epoch, stuck);
      strcat(payload, buffer);
    }

    if (plen) {
      strcat(payload, ", ");
      plen++;
      sprintf(buffer, "{ \"LoggerName\": \"SysInfo\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"bat_v\",\"Value\": %f }, { \"Name\": \"wifi_rssi\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}], ", (long)epoch, data.bat, stats.rssi);
    }

    sprintf(buffer, "\"plen\": %d }", plen);
    strcat(payload, buffer);

    if (plen) {
      http.begin(serverName);

      http.addHeader("Content-Type", "application/json");

      int ret = http.POST(payload);
      //kontrola responsu
      if(ret != 200){
        Serial.printf("ret: %d", ret);
      } else {
        Serial.println("OK");
      }

      //koniec
      http.end();
    }
}
#endif

#ifdef _BED
void event(data_t data){
    struct tm mytime;
    getLocalTime(&mytime);
    time_t epoch = mktime(&mytime);
    Serial.printf("Timestamp: %ld", (long)epoch);
    wifi_ap_record_t stats;
    esp_wifi_sta_get_ap_info(&stats);
    sprintf(&payload[0], "\
    {\
    \"sn\": \"%s\",\
    \"kid\": \"%s\",\
    \"body\":\
    [{\"LoggerName\": \"PIR\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"motion\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}, \
    { \"LoggerName\": \"FSR\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"pressure\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}, \
    { \"LoggerName\": \"BME680\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"temperature\",\"Value\": %f }, { \"Name\": \"humidity\",\"Value\": %f }, { \"Name\": \"smoke\",\"Value\": %f }], \"ServiceData\": [], \"DebugData\": []}, \
    { \"LoggerName\": \"SysInfo\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"bat_v\",\"Value\": %f }, { \"Name\": \"wifi_rssi\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}], \
    \"devId\": \"%s\",\
    \"includeTS\" : 0, \
    \"plen\": 4 }", SN, kid, 
    (long)epoch, data.pir, (long)epoch, data.fsr, (long)epoch, data.temp, data.hum, data.smoke, (long)epoch, data.bat, stats.rssi, myId);
    Serial.print(payload);
    char myjwt[410] = "Bearer ";
    size_t jlen;
    auth.createJWT((uint8_t*)myjwt + strlen(myjwt), sizeof(myjwt) - strlen(myjwt), &jlen, epoch);
    Serial.printf("auth: %s\r\n", myjwt);

    http.begin(serverName);

    http.addHeader("Content-Type", "application/json");

    int ret = http.POST(payload);
    //kontrola responsu
    if(ret != 200){
      Serial.printf("ret: %d", ret);
    } else {
      Serial.println("OK");
    }

    //koniec
    http.end();
}

void diffEvent(data_t data, data_t olddata) {
  struct tm mytime;
    getLocalTime(&mytime);
    time_t epoch = mktime(&mytime);
    Serial.printf("Timestamp: %ld", (long)epoch);
    wifi_ap_record_t stats;
    esp_wifi_sta_get_ap_info(&stats);

    sprintf(&payload[0], "\
    {\
    \"sn\": \"%s\",\
    \"kid\": \"%s\",\
    \"devId\": \"%s\",\
    \"includeTS\" : 0, \
    \"body\": [", SN, kid, myId);

    uint8_t plen = 0;
    char buffer[256];

    if (data.pir != olddata.pir) {
      plen++;
      sprintf(buffer, "{\"LoggerName\": \"PIR\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"motion\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}", (long)epoch, data.pir);
      strcat(payload, buffer);
    }

    if (diffCheck(data.fsr, olddata.fsr, DIFF_K)) {
      if (plen) strcat(payload, ", ");
      plen++;
      sprintf(buffer, "{ \"LoggerName\": \"FSR\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"pressure\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}", (long)epoch, data.fsr);
      strcat(payload, buffer);
    }

    if (diffCheckF(data.smoke, olddata.smoke, SMOKE_DIFF_K)) {
      if (plen) strcat(payload, ", ");
      plen++;
      sprintf(buffer, "{ \"LoggerName\": \"BME680\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"temperature\",\"Value\": %f }, { \"Name\": \"humidity\",\"Value\": %f }, { \"Name\": \"smoke\",\"Value\": %f }], \"ServiceData\": [], \"DebugData\": []}", (long)epoch, data.temp, data.hum, data.smoke);
      strcat(payload, buffer);
    }

    if (plen) {
      strcat(payload, ", ");
      plen++;
      sprintf(buffer, "{ \"LoggerName\": \"SysInfo\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"bat_v\",\"Value\": %f }, { \"Name\": \"wifi_rssi\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}], ", (long)epoch, data.bat, stats.rssi);
    }

    sprintf(buffer, "\"plen\": %d }", plen);
    strcat(payload, buffer);

    if (plen) {
      http.begin(serverName);

      http.addHeader("Content-Type", "application/json");

      int ret = http.POST(payload);
      //kontrola responsu
      if(ret != 200){
        Serial.printf("ret: %d", ret);
      } else {
        Serial.println("OK");
      }

      //koniec
      http.end();
    }
}
#endif

#ifdef _KITCHEN
void event(data_t data){
    struct tm mytime;
    getLocalTime(&mytime);
    time_t epoch = mktime(&mytime);
    Serial.printf("Timestamp: %ld", (long)epoch);
    wifi_ap_record_t stats;
    esp_wifi_sta_get_ap_info(&stats);
    sprintf(&payload[0], "\
    {\
    \"sn\": \"%s\",\
    \"kid\": \"%s\",\
    \"body\":\
    [{\"LoggerName\": \"PIR\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"motion\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}, \
    { \"LoggerName\": \"BME680\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"temperature\",\"Value\": %f }, { \"Name\": \"humidity\",\"Value\": %f }, { \"Name\": \"smoke\",\"Value\": %f }], \"ServiceData\": [], \"DebugData\": []}, \
    { \"LoggerName\": \"SysInfo\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"bat_v\",\"Value\": %f }, { \"Name\": \"wifi_rssi\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}], \
    \"devId\": \"%s\",\
    \"includeTS\" : 0, \
    \"plen\": 2 }", SN, kid, 
    (long)epoch, data.pir, (long)epoch, data.temp, data.hum, data.smoke, (long)epoch, data.bat, stats.rssi, myId);
    Serial.print(payload);

    http.begin(serverName);

    http.addHeader("Content-Type", "application/json");

    int ret = http.POST(payload);
    //kontrola responsu
    if(ret != 200){
      Serial.printf("ret: %d", ret);
    } else {
      Serial.println("OK");
    }

    //koniec
    http.end();
}

void diffEvent(data_t data, data_t olddata) {
    struct tm mytime;
    getLocalTime(&mytime);
    time_t epoch = mktime(&mytime);
    Serial.printf("Timestamp: %ld", (long)epoch);
    wifi_ap_record_t stats;
    esp_wifi_sta_get_ap_info(&stats);

    sprintf(&payload[0], "\
    {\
    \"sn\": \"%s\",\
    \"kid\": \"%s\",\
    \"devId\": \"%s\",\
    \"includeTS\" : 0, \
    \"body\": [", SN, kid, myId);

    uint8_t plen = 0;
    char buffer[256];

    if (data.pir != olddata.pir) {
      plen++;
      sprintf(buffer, "{\"LoggerName\": \"PIR\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"motion\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}", (long)epoch, data.pir);
      strcat(payload, buffer);
    }

    if (diffCheckF(data.smoke, olddata.smoke, SMOKE_DIFF_K)) {
      if (plen) strcat(payload, ", ");
      plen++;
      sprintf(buffer, "{ \"LoggerName\": \"BME680\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"temperature\",\"Value\": %f }, { \"Name\": \"humidity\",\"Value\": %f }, { \"Name\": \"smoke\",\"Value\": %f }], \"ServiceData\": [], \"DebugData\": []}", (long)epoch, data.temp, data.hum, data.smoke);
      strcat(payload, buffer);
    }

    if (plen) {
      strcat(payload, ", ");
      plen++;
      sprintf(buffer, "{ \"LoggerName\": \"SysInfo\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"bat_v\",\"Value\": %f }, { \"Name\": \"wifi_rssi\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": []}], ", (long)epoch, data.bat, stats.rssi);
    }

    sprintf(buffer, "\"plen\": %d }", plen);
    strcat(payload, buffer);

    if (plen) {
      http.begin(serverName);

      http.addHeader("Content-Type", "application/json");

      int ret = http.POST(payload);
      //kontrola responsu
      if(ret != 200){
        Serial.printf("ret: %d", ret);
      } else {
        Serial.println("OK");
      }

      //koniec
      http.end();
    }
}
#endif

boolean restoreConfig() {
  wifi_ssid = preferences.getString("WIFI_SSID");
  wifi_password = preferences.getString("WIFI_PASSWD");
  Serial.print("WIFI-SSID: ");
  //M5.Lcd.print("WIFI-SSID: ");
  Serial.println(wifi_ssid);
  //M5.Lcd.println(wifi_ssid);
  Serial.print("WIFI-PASSWD: ");
  //M5.Lcd.print("WIFI-PASSWD: ");
  Serial.println(wifi_password);
  //M5.Lcd.println(wifi_password);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

  if(wifi_ssid.length() > 0) {
    return true;
} else {
    return false;
  }
}

boolean checkConnection() {
  int count = 0;
  Serial.print("Waiting for Wi-Fi connection");
  M5.Lcd.println("Waiting for Wi-Fi connection");
  while ( count < 50 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      //M5.Lcd.println();
      Serial.println("Connected!");
      M5.Lcd.println("Connected!");
      needConnect = false;
      return (true);
    }
    delay(500);
    Serial.print(".");
    //M5.Lcd.print(".");
    count++;
  }
  Serial.println("Timed out.");
  //M5.Lcd.println("Timed out.");
  return false;
}

void startWebServer() {
  if (settingMode) {
    Serial.print("Starting Web Server at ");
    //M5.Lcd.print("Starting Web Server at ");
    Serial.println(WiFi.softAPIP());
    //M5.Lcd.println(WiFi.softAPIP());
    webServer.on("/settings", []() {
      String s = "<h1>Wi-Fi Settings</h1><p>Please enter your password by selecting the SSID.</p>";
      s += "<form method=\"get\" action=\"setap\"><label>SSID: </label><select name=\"ssid\">";
      s += ssidList;
      s += "</select><br>Password: <input name=\"pass\" length=64 type=\"password\"><input type=\"submit\"></form>";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
    });
    webServer.on("/setap", []() {
      //String ssid = urlDecode(webServer.arg("ssid"));
      String ssid = webServer.arg("ssid");
      Serial.print("SSID: ");
      //M5.Lcd.print("SSID: ");
      Serial.println(ssid);
      //M5.Lcd.println(ssid);
      //String pass = urlDecode(webServer.arg("pass"));
      String pass = webServer.arg("pass");
      Serial.print("Password: ");
      //M5.Lcd.print("Password: ");
      Serial.println(pass);
      //M5.Lcd.println(pass);
      Serial.println("Writing SSID to EEPROM...");
      //M5.Lcd.println("Writing SSID to EEPROM...");

      // Store wifi config
      Serial.println("Writing Password to nvr...");
      //M5.Lcd.println("Writing Password to nvr...");
      preferences.putString("WIFI_SSID", ssid);
      preferences.putString("WIFI_PASSWD", pass);

      Serial.println("Write nvr done!");
      //M5.Lcd.println("Write nvr done!");
      String s = "<h1>Setup complete.</h1><p>device will be connected to \"";
      s += ssid;
      s += "\" after the restart.";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
      delay(3000);
      ESP.restart();
    });
    webServer.onNotFound([]() {
      String s = "<h1>AP mode</h1><p><a href=\"/settings\">Wi-Fi Settings</a></p>";
      webServer.send(200, "text/html", makePage("AP mode", s));
    });
  }
  else {
    Serial.print("Starting Web Server at ");
    //M5.Lcd.print("Starting Web Server at ");
    Serial.println(WiFi.localIP());
    //M5.Lcd.println(WiFi.localIP());
    webServer.on("/", []() {
      String s = "<h1>STA mode</h1><p><a href=\"/reset\">Reset Wi-Fi Settings</a></p>";
      webServer.send(200, "text/html", makePage("STA mode", s));
    });
    webServer.on("/reset", []() {
      // reset the wifi config
      preferences.remove("WIFI_SSID");
      preferences.remove("WIFI_PASSWD");
      String s = "<h1>Wi-Fi settings was reset.</h1><p>Please reset device.</p>";
      webServer.send(200, "text/html", makePage("Reset Wi-Fi Settings", s));
      delay(3000);
      ESP.restart();
    });
  }
  webServer.begin();
}

void setupMode() {
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  delay(100);
  Serial.println("");
  //M5.Lcd.println("");
  for (int i = 0; i < n; ++i) {
    ssidList += "<option value=\"";
    ssidList += WiFi.SSID(i);
    ssidList += "\">";
    ssidList += WiFi.SSID(i);
    ssidList += "</option>";
  }
  delay(100);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSSID);
  WiFi.mode(WIFI_MODE_AP);
  // WiFi.softAPConfig(IPAddress local_ip, IPAddress gateway, IPAddress subnet);
  // WiFi.softAP(const char* ssid, const char* passphrase = NULL, int channel = 1, int ssid_hidden = 0);
  // dnsServer.start(53, "*", apIP);
  startWebServer();
  Serial.print("Starting Access Point at \"");
  //M5.Lcd.print("Starting Access Point at \"");
  Serial.print(apSSID);
  //M5.Lcd.print(apSSID);
  Serial.println("\"");
  //M5.Lcd.println("\"");
  M5.Lcd.println("No WiFi Connection!");
}

String makePage(String title, String contents) {
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += contents;
  s += "</body></html>";
  return s;
}

void OTAhandler(){
  M5.Axp.ScreenBreath(255);
  M5.Lcd.setCursor(0, 1);
  M5.Lcd.println("Preparing OTA");
  delay(5000);
  M5.update();
  M5.Lcd.setCursor(0, 1);
  M5.Lcd.println("             ");
  M5.Lcd.setCursor(0, 1);
  M5.Lcd.printf("Waiting for\nFW");

  while (true) {
    ArduinoOTA.handle();

    if(M5.BtnA.wasPressed()) {
      Serial.printf("End OTA\r\n");
      M5.Lcd.setCursor(0, 1);
      M5.Lcd.println("           ");
      M5.Axp.ScreenBreath(0);
      return;
    }
    M5.update();
  }
}

bool diffCheck(uint8_t val, uint8_t oldVal, float K) {
  return (((val - oldVal) > (DIFF_K * val)) || ((oldVal - val) > (DIFF_K * val)));
}

bool diffCheckF(float val, float oldVal, float K) {
  return (((val - oldVal) > (DIFF_K * val)) || ((oldVal - val) > (DIFF_K * val)));
}