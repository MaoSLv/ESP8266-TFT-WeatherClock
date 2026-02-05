#include <Adafruit_GFX.h>    
#include <Adafruit_ST7789.h> 
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "icons.h"
#include "chinese_fonts.h"

// 引脚定义
#define TFT_CS     15  // D8
#define TFT_RST    16  // D0
#define TFT_DC      5  // D1

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// 状态变量
int hh, mm, ss;
int curr_year, curr_month, curr_day, weekday;
bool isClockMode = false;

// 天气变量
const char* apiKey = "YOUR_API_KEY";
String weather_city = "shenzhen";
int server_code = 99; // 初始为未知状态
int server_temp = 0;

// 定时器
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 300000; // 每5分钟请求一次

void setup() {
  Serial.begin(115200);

  tft.init(240, 320); 
  tft.setRotation(1); 
  tft.fillScreen(ST77XX_BLACK);

  // WiFiManager 配网
  WiFiManager wm;
  tft.setCursor(20, 100);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.println("Connecting WiFi...");

  if (!wm.autoConnect("ESP8266_Clock_AP")) {
    ESP.restart();
  }

  // NTP 时间初始化
  configTime(8 * 3600, 0, "ntp.aliyun.com", "ntp.tuna.tsinghua.edu.cn");
  while (time(nullptr) < 1000000) {
    delay(500);
  }

  isClockMode = true;
  tft.fillScreen(ST77XX_BLACK);
}

void loop() {
  if (!isClockMode) return;

  // 时间刷新逻辑
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  hh = p_tm->tm_hour;
  mm = p_tm->tm_min;
  ss = p_tm->tm_sec;
  curr_year = p_tm->tm_year + 1900;
  curr_month = p_tm->tm_mon + 1;
  curr_day = p_tm->tm_mday;
  int wd = p_tm->tm_wday;
  weekday = (wd == 0) ? 7 : wd;

  // 天气定时更新逻辑
  if (millis() - lastWeatherUpdate >= weatherInterval || lastWeatherUpdate == 0) {
    lastWeatherUpdate = millis();
    updateWeatherFromServer();
  }

  updateDisplay();
  delay(1000); // 每一秒刷新一次
}

void updateWeatherFromServer() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClient client;
  HTTPClient http;
  String url = "http://api.seniverse.com/v3/weather/now.json?key=" + String(apiKey) + "&location=" + weather_city + "&language=zh-Hans&unit=c";
  
  if (http.begin(client, url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      JsonObject nowObj = doc["results"][0]["now"];
      server_temp = nowObj["temperature"].as<int>();
      server_code = nowObj["code"].as<int>();
    }
    http.end();
  }
}

// 核心 UI 绘制函数
void updateDisplay() {
  // 绘制顶部框架
  tft.fillRect(0, 0, 320, 40, ST77XX_ORANGE);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 12);
  tft.printf("%04d-%02d-%02d  Week %d", curr_year, curr_month, curr_day, weekday);

  // 绘制大时间
  tft.setCursor(40, 80);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(8);
  tft.printf("%02d:%02d", hh, mm);

  tft.drawFastHLine(0, 170, 320, ST77XX_WHITE);

  // 绘制天气图标 (根据 server_code 选择图标)
  // 这里简化了逻辑，直接根据几个大类画图
  if (server_code <= 3) {
    tft.drawBitmap(10, 185, icon_sunny, 48, 48, ST77XX_YELLOW);
    tft.drawBitmap(65, 195, qing, 32, 32, ST77XX_YELLOW);
  } else if (server_code <= 8) {
    tft.drawBitmap(10, 185, icon_cloudy, 48, 48, ST77XX_WHITE);
    tft.drawBitmap(65, 195, duo, 32, 32, ST77XX_WHITE);
  } else {
    tft.drawBitmap(10, 185, icon_rain, 48, 48, ST77XX_CYAN);
    tft.drawBitmap(65, 195, yu, 32, 32, ST77XX_CYAN);
  }

  // 绘制温度
  uint16_t tempColor = ST77XX_GREEN;
  if (server_temp > 30) tempColor = ST77XX_RED;
  else if (server_temp < 10) tempColor = ST77XX_CYAN;
  
  tft.setTextColor(tempColor, ST77XX_BLACK);
  tft.setTextSize(4);
  tft.setCursor(120, 195);
  tft.printf("%dC", server_temp);
}