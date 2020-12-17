#include <M5StickC.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include "WebServer.h"
#include <Preferences.h>
//#include "DHT12.h"
#include <Wire.h>
#include "newAuth.h"
#include "time.h"

#define _HALLWAY
//#define _DOOR
//#define _BED
//#define _Kitchen

#define _PIR_HAT

#ifdef _HALLWAY
#ifdef _PIR_HAT
#define PIR_PIN      36
#else
#define PIR_PIN      33
#endif

#ifdef _DOOR
#include <VL53L0X.h>

VL53L0X tof;
#endif

#ifdef _BED
#include <Adafruit_BME680.h>
#define PIR_PIN      33
#define FSR_PIN      32

Adafruit_BME680 bme;
#endif

#define LIGHT_PIN    23

#define cTime        10000
#define cSleepTime   10

#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP  60

#ifdef _HALLWAY
void event(bool pir, float bat);
#endif

#ifdef _DOOR
void event(uint16_t dist, float bat);
#endif

#ifdef _BED
void event(bool pir, uint16_t fsr, float temp, float hum, float smoke, float bat);
#endif

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

//const char* ssid = "fei-iot";
//const char* pass = "F+e-i.feb575";

//RTC_DATA_ATTR int bootCount = 0;
//RTC_DATA_ATTR int cFails = 0;
//RTC_DATA_ATTR long timestamp = 1607962136;

const char* serverName = "https://fei.edu.r-das.sk:51415/api/v1/Auth";

char payload[800];
long cStart;

authHandler auth;

const IPAddress apIP(192, 168, 4, 1);
const char* apSSID = "nice-AP";
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

void setup(){
    M5.begin();
    M5.Axp.ScreenBreath(0);
    //Wire.begin();
    //Serial init
    Serial.begin(115200);
    //Wire.begin(0, 26);

    #ifdef _DOOR
    Wire.begin();

    tof.setTimeout(500);
    if (!tof.init())
    {
        Serial.println("Failed to detect and initialize ToF sensor!");
        while (1) {}
    }
    tof.startContinuous(100);
    #endif

    #ifdef _BED
    pinMode(FSR_PIN, INPUT);
    pinMode(PIR_PIN, INPUT);

    Wire.begin();
    bme.begin();
    #endif

    #ifdef _KITCHEN
    Wire.begin(32, 33);
    #endif

    #ifdef _BED
    Wire.begin(32, 33);
    #endif
    /*
    WiFi.begin(ssid, pass);
    while(WiFi.status() != WL_CONNECTED){
        Serial.print(".");
    }
    Serial.println();
    */

    wifiConfig();

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    /*
    while ((WiFi.status() != WL_CONNECTED)&&((millis() - start) <= cTime)) {
        delay(100);
        Serial.print(".");
    }//while
    Serial.println();
    
    if (WiFi.status() != WL_CONNECTED) {
        ++cFails;
        esp_sleep_enable_timer_wakeup(cFails * cSleepTime * uS_TO_S_FACTOR);
        esp_deep_sleep_start();
    }//if (WiFi.status() != WL_CONNECTED)
    */

   /*
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
    */

   #ifdef _HALLWAY
   pinMode(PIR_PIN, INPUT);
   #endif
}//setup

void loop(){
  float bat = M5.Axp.GetBatVoltage();

  #ifdef _HALLWAY
  bool pir = digitalRead(PIR_PIN);
  Serial.printf("Pir: %d\r\nBat: %.2f\r\n", pir, bat);
  event(pir, bat);
  #endif

  #ifdef _DOOR
  uint16_t dist = tof.readRangeContinuousMillimeters();
  Serial.printf("dist: %d\r\nBat: %.2f\r\n", dist, bat);
  event(dist, bat);
  #endif

  #ifdef _BED
  bool pir = digitalRead(PIR_PIN);
  uint16_t fsr = analogRead(FSR_PIN);

  float dummy = 0;
  Serial.printf("Pir: %d\r\nFSR: %d\r\n", pir, fsr);
  event(pir, fsr, dummy, dummy,)
  #endif
  
  delay(1000);
}//loop

#ifdef _HALLWAY
void event(bool pir, float bat){
    //sprintf(&payload[0], "{ \"T\": %.1f, \"H\": %.1f }", t, h);
    //timestamp += (long)(millis() / 1000) + 60;
    struct tm mytime;
    getLocalTime(&mytime);
    time_t epoch = mktime(&mytime);
    Serial.printf("Timestamp: %ld", (long)epoch);
    sprintf(&payload[0], "[ { \"LoggerName\": \"PIR\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"Motion\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": [], \"DeviceId\": \"%s\" }, { \"LoggerName\": \"Battery\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"Voltage\",\"Value\": %f }], \"ServiceData\": [], \"DebugData\": [], \"DeviceId\": \"%s\" } ]", (long)epoch, pir, myId, (long)epoch, bat, myId);
    Serial.print(payload);
    char myjwt[400];
    char hjwt[410] = "Bearer ";
    size_t jlen;
    auth.createJWT((uint8_t*)myjwt, sizeof(myjwt), &jlen, epoch);
    strcat(hjwt, myjwt);
    Serial.printf("auth: %s\r\n", hjwt);

    http.begin(serverName);

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", hjwt);

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
#endif

#ifdef _DOOR
void event(uint16_t dist, float bat){
    //sprintf(&payload[0], "{ \"T\": %.1f, \"H\": %.1f }", t, h);
    //timestamp += (long)(millis() / 1000) + 60;
    bool motion = (dist < 8000) ? true : false;
    struct tm mytime;
    getLocalTime(&mytime);
    time_t epoch = mktime(&mytime);
    Serial.printf("Timestamp: %ld", (long)epoch);
    sprintf(&payload[0], "[ { \"LoggerName\": \"ToF\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"Motion\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": [], \"DeviceId\": \"%s\" }, { \"LoggerName\": \"Battery\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"Voltage\",\"Value\": %f }], \"ServiceData\": [], \"DebugData\": [], \"DeviceId\": \"%s\" } ]", (long)epoch, motion, myId, (long)epoch, bat, myId);
    Serial.print(payload);
    char myjwt[400];
    char hjwt[410] = "Bearer ";
    size_t jlen;
    auth.createJWT((uint8_t*)myjwt, sizeof(myjwt), &jlen, epoch);
    strcat(hjwt, myjwt);
    Serial.printf("auth: %s\r\n", hjwt);

    http.begin(serverName);

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", hjwt);

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
#endif

#ifdef _BED
void event(bool pir, uint16_t fsr, float temp, float hum, float smoke, float bat){
    struct tm mytime;
    getLocalTime(&mytime);
    time_t epoch = mktime(&mytime);
    Serial.printf("Timestamp: %ld", (long)epoch);
    sprintf(&payload[0], "\
    [ { \"LoggerName\": \"PIR\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"Motion\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": [], \"DeviceId\": \"%s\" }, \
    { \"LoggerName\": \"FSR\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"Pressure\",\"Value\": %d }], \"ServiceData\": [], \"DebugData\": [], \"DeviceId\": \"%s\" }, \
    { \"LoggerName\": \"BME680\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"Temperature\",\"Value\": %f }, { \"Name\": \"Humidity\",\"Value\": %f }, { \"Name\": \"Smoke\",\"Value\": %f }], \"ServiceData\": [], \"DebugData\": [], \"DeviceId\": \"%s\" }, \
    { \"LoggerName\": \"Battery\", \"Timestamp\": %ld, \"MeasuredData\": [{ \"Name\": \"Voltage\",\"Value\": %f }], \"ServiceData\": [], \"DebugData\": [], \"DeviceId\": \"%s\" } ]", 
    (long)epoch, pir, myId, (long)epoch, fsr, myId, (long)epoch, temp, myId, (long)epoch, hum, myId, (long)epoch, smoke, myId, (long)epoch, bat, myId);
    Serial.print(payload);
    char myjwt[400];
    char hjwt[410] = "Bearer ";
    size_t jlen;
    auth.createJWT((uint8_t*)myjwt, sizeof(myjwt), &jlen, epoch);
    strcat(hjwt, myjwt);
    Serial.printf("auth: %s\r\n", hjwt);

    http.begin(serverName);

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", hjwt);

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
#endif

boolean restoreConfig() {
  wifi_ssid = preferences.getString("WIFI_SSID");
  wifi_password = preferences.getString("WIFI_PASSWD");
  Serial.print("WIFI-SSID: ");
  M5.Lcd.print("WIFI-SSID: ");
  Serial.println(wifi_ssid);
  M5.Lcd.println(wifi_ssid);
  Serial.print("WIFI-PASSWD: ");
  M5.Lcd.print("WIFI-PASSWD: ");
  Serial.println(wifi_password);
  M5.Lcd.println(wifi_password);
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
  M5.Lcd.print("Waiting for Wi-Fi connection");
  while ( count < 30 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      M5.Lcd.println();
      Serial.println("Connected!");
      M5.Lcd.println("Connected!");
      needConnect = false;
      return (true);
    }
    delay(500);
    Serial.print(".");
    M5.Lcd.print(".");
    count++;
  }
  Serial.println("Timed out.");
  M5.Lcd.println("Timed out.");
  return false;
}

void startWebServer() {
  if (settingMode) {
    Serial.print("Starting Web Server at ");
    M5.Lcd.print("Starting Web Server at ");
    Serial.println(WiFi.softAPIP());
    M5.Lcd.println(WiFi.softAPIP());
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
      M5.Lcd.print("SSID: ");
      Serial.println(ssid);
      M5.Lcd.println(ssid);
      //String pass = urlDecode(webServer.arg("pass"));
      String pass = webServer.arg("pass");
      Serial.print("Password: ");
      M5.Lcd.print("Password: ");
      Serial.println(pass);
      M5.Lcd.println(pass);
      Serial.println("Writing SSID to EEPROM...");
      M5.Lcd.println("Writing SSID to EEPROM...");

      // Store wifi config
      Serial.println("Writing Password to nvr...");
      M5.Lcd.println("Writing Password to nvr...");
      preferences.putString("WIFI_SSID", ssid);
      preferences.putString("WIFI_PASSWD", pass);

      Serial.println("Write nvr done!");
      M5.Lcd.println("Write nvr done!");
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
    M5.Lcd.print("Starting Web Server at ");
    Serial.println(WiFi.localIP());
    M5.Lcd.println(WiFi.localIP());
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
  M5.Lcd.println("");
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
  M5.Lcd.print("Starting Access Point at \"");
  Serial.print(apSSID);
  M5.Lcd.print(apSSID);
  Serial.println("\"");
  M5.Lcd.println("\"");
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

String urlDecode(String input) {
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}
