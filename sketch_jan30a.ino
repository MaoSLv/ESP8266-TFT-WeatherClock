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

// 静态时间变量
int hh = 18;
int mm = 30;
int ss = 0;
int weekday = 1; // 5代表星期五
bool isClockMode = false;
int curr_year = 2026, curr_month = 1, curr_day = 30;
// API密钥
const char* apiKey = "";
// 城市
String weather_city = "shenzhen";
// 天气代码
int server_code = 0;
// 实时温度
int server_temp = 0;
// 每5分钟更新一次
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 300000;
// 天气绘画标记
bool weatherNeedsRedraw = false;

WiFiManagerParameter custom_city("city", "输入城市拼音 (如: shenzhen)", "shenzhen", 20);

void setup() {
  Serial.begin(115200);
  tft.init(240, 320); 
  tft.setRotation(1); 
  tft.fillScreen(ST77XX_BLACK);

  // 先显示一个加载提示（可选）
  showLoadingScreen();

  WiFiManager wm;
  // wm.resetSettings();
  wm.addParameter(&custom_city);

  wm.setTitle("桌面时钟 - 配网助手");
  wm.setHttpPort(80);


  // 关键点 1: 必须在 autoConnect 之前设置回调
  wm.setAPCallback(configModeCallback);

  // 设置配网超时（秒），如果在 3 分钟内没配好，会重启重试
  wm.setConfigPortalTimeout(180);

  // 关键点 2: autoConnect 会在这里阻塞
  // 如果手机搜不到热点，检查 ESP8266 是否在供电
  if (!wm.autoConnect("ESP8266_CLOCK_AP")) {
    // 如果配网失败（比如超时），重启
    ESP.restart();
  }

  // --- 如果执行到这里，说明已经成功联网 ---
  isClockMode = true;
  tft.fillScreen(ST77XX_BLACK);

  configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org", "time.nist.gov");
  Serial.println("正在同步网络时间...");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) { // 如果获取的时间小于1970年1月1日之后的几个小时，说明还没对上
    delay(500);
    now = time(nullptr);
  }
  Serial.println("\n时间同步成功!");

  weather_city = custom_city.getValue(); // 获取配网输入的城市
  if(weather_city == "") weather_city = "shenzhen"; // 兜底逻辑

  time_t now_setup = time(nullptr);
  struct tm* p_tm_setup = localtime(&now_setup);
  
  // 立即将同步到的网络时间赋值给变量
  curr_year  = p_tm_setup->tm_year + 1900;
  curr_month = p_tm_setup->tm_mon + 1;
  curr_day   = p_tm_setup->tm_mday;
  
  int wd_setup = p_tm_setup->tm_wday;
  weekday = (wd_setup == 0) ? 7 : wd_setup; // 转换周日为7
  
  updateWeatherFromServer(); // 第一次强制获取
  drawStaticFrame();
}

void loop() {
  if (isClockMode) {
    unsigned long currentMillis = millis();

    static unsigned long lastUpdate = 0;
    if (currentMillis - lastUpdate >= 1000) {
      lastUpdate = currentMillis;

      time_t now = time(nullptr);
      struct tm* p_tm = localtime(&now);

      // 更新时间
      hh = p_tm->tm_hour;
      mm = p_tm->tm_min;
      ss = p_tm->tm_sec;

      // 更新日期 (tm_year 是从1900年开始计算的，tm_mon 是0-11)
      curr_year = p_tm->tm_year + 1900;
      curr_month = p_tm->tm_mon + 1;
      curr_day = p_tm->tm_mday;

      // 更新星期 (tm_wday: 0是周日, 1-6是周一到周六)
      int wd = p_tm->tm_wday;
      if (wd == 0) weekday = 7; 
      else weekday = wd;

      updateTimeDisplay();
    }

    if (currentMillis - lastWeatherUpdate >= weatherInterval || lastWeatherUpdate == 0) {
      lastWeatherUpdate = currentMillis;
      updateWeatherFromServer();
    }
  }
}

// 当进入配网模式时的回调函数（用于更新屏幕显示）
void configModeCallback (WiFiManager *myWiFiManager) {
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRoundRect(5, 5, 310, 230, 12, ST77XX_RED);
  
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(3);
  tft.setCursor(62, 40);
  tft.print("CONFIG MODE");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(65, 90);
  tft.println("Connect to WiFi:");
  
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(63, 120);
  // 动态显示热点名称
  tft.println(myWiFiManager->getConfigPortalSSID());

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(100, 160);
  tft.println("Visit IP:");
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(90, 190);
  tft.println("192.168.4.1");
}

// 专门画中文星期的函数
void drawChineseWeek(int x, int y, int day, uint16_t color) {
  tft.fillRect(x + 52, y, 24, 24, ST77XX_BLACK);
  tft.drawBitmap(x, y, xing, 24, 24, color);
  tft.drawBitmap(x + 26, y + 1, qi, 24, 24, color);
  
  const unsigned char* ptr;
  switch(day) {
    case 1: ptr = day_1; break;
    case 2: ptr = day_2; break;
    case 3: ptr = day_3; break;
    case 4: ptr = day_4; break;
    case 5: ptr = day_5; break;
    case 6: ptr = day_6; break;
    default: ptr = day_7; break;
  }
  tft.drawBitmap(x + 52, y + 1, ptr, 24, 24, color);
}

void drawStaticFrame() {
  // 1. 橙色圆角边框
  tft.drawRoundRect(5, 5, 310, 230, 12, ST77XX_ORANGE);
  tft.drawLine(5, 55, 314, 55, ST77XX_ORANGE);
  tft.drawLine(159, 5, 159, 55, ST77XX_ORANGE);
  
  tft.drawLine(5, 175, 314, 175, ST77XX_ORANGE);
  tft.drawLine(159, 175, 159, 234, ST77XX_ORANGE);
  // 2. 顶部日期（英文部分）
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(25, 25);
  // 格式化日期字符串，例如 2026-01-30
  char dateStr[12];
  sprintf(dateStr, "%04d-%02d-%02d", curr_year, curr_month, curr_day);
  tft.print(dateStr);

  // 3. 调用中文显示函数
  drawChineseWeek(200, 19, weekday, ST77XX_WHITE); 
}

void updateTimeDisplay() {
  // 小时:分钟 (使用背景色参数消除闪烁)
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); 
  tft.setCursor(40, 90);
  tft.setTextSize(8); 
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", hh, mm);
  tft.print(timeStr);

  if (weatherNeedsRedraw) {
    tft.fillRect(25, 190, 130, 35, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    // 显示温度：左下角
    uint16_t icon_color;
    if (server_temp >= 30) {
      icon_color = ST77XX_RED;     // 太热了，显示红色
    } else if (server_temp >= 20) {
      icon_color = ST77XX_GREEN;  // 舒适，显示黄色/橙色
    } else {
      icon_color = ST77XX_CYAN;    // 太冷了，显示蓝色
    }
    tft.drawBitmap(25, 190, temperature, 32, 32, icon_color);
    tft.setTextSize(3);
    if (server_temp < 10) {
      tft.setCursor(72, 196);
    } else {
      tft.setCursor(63, 196);
    }
    tft.setTextColor(ST77XX_WHITE);
    tft.printf("%d", server_temp);
    tft.drawBitmap(103, 190, celsius, 32, 32, ST77XX_WHITE);

    tft.fillRect(177, 185, 137, 41, ST77XX_BLACK);
    // 显示天气：右下角
    // int code = server_code;
    if (server_code < 4) {
      // 晴天
      tft.drawBitmap(185, 190, icon_sunny, 32, 32, 0xFC04);
      tft.drawBitmap(226, 193, qing, 24, 24, ST77XX_WHITE);
      tft.drawBitmap(260, 193, tian, 24, 24, ST77XX_WHITE);
    } else if (server_code < 9) {
      // 多云
      tft.drawBitmap(181, 195, icon_cloudy, 32, 22, 0x9CD3);
      tft.drawBitmap(226, 193, duo, 24, 24, ST77XX_WHITE);
      tft.drawBitmap(260, 193, yun, 24, 24, ST77XX_WHITE);
    } else if (server_code == 9) {
      // 多云
      tft.drawBitmap(179, 185, icon_cloudy_sun, 32, 32, 0xFE49);
      tft.drawBitmap(191, 200, icon_cloudy_sky, 32, 22, 0xD77F);
      tft.drawBitmap(236, 193, yin, 24, 24, ST77XX_WHITE);
      tft.drawBitmap(270, 193, tian, 24, 24, ST77XX_WHITE);
    } else if (server_code == 10 || (server_code >= 13 && server_code <= 19)) {
      // 下雨
      tft.drawBitmap(185, 187, icon_rain_cloudy, 32, 22, 0xD77F);
      tft.drawBitmap(185, 204, icon_rain_water, 32, 22, 0x2CFE);
      tft.drawBitmap(228, 195, xia, 24, 24, ST77XX_WHITE);
      tft.drawBitmap(261, 195, yu, 24, 24, ST77XX_WHITE);
    } else if (server_code == 11 || server_code == 12) {
      // 雷阵雨
      tft.drawBitmap(177, 187, icon_rain_cloudy, 32, 22, 0xD77F);
      tft.drawBitmap(184, 208, icon_lightning, 16, 16, 0xFD40);
      tft.drawBitmap(174, 207, icon_raindrops, 16, 16, 0x44DF);
      tft.drawBitmap(195, 207, icon_raindrops, 16, 16, 0x44DF);
      tft.drawBitmap(220, 193, lei, 24, 24, ST77XX_WHITE);
      tft.drawBitmap(246, 193, zhen, 24, 24, ST77XX_WHITE);
      tft.drawBitmap(272, 193, yu, 24, 24, ST77XX_WHITE);
    } else if (server_code <= 25) {
      // 下雪
      tft.drawBitmap(185, 190, icon_snowflakes, 32, 32, 0xAF3F);
      tft.drawBitmap(226, 194, xia, 24, 24, ST77XX_WHITE);
      tft.drawBitmap(260, 194, xue, 24, 24, ST77XX_WHITE);
    } else if (server_code <= 29) {
      // 沙尘
      tft.drawBitmap(183, 190, icon_dust_storms, 32, 32, 0xF462);
      tft.drawBitmap(226, 194, sha, 24, 24, ST77XX_WHITE);
      tft.drawBitmap(260, 194, chen, 24, 24, ST77XX_WHITE);
    } else if (server_code <= 31) {
      tft.drawBitmap(183, 190, icon_haze, 32, 32, 0x9CD3);
      tft.drawBitmap(228, 195, da, 24, 24, ST77XX_WHITE);
      tft.drawBitmap(262, 195, wu, 24, 24, ST77XX_WHITE);
    } else if (server_code <= 36) {
      tft.drawBitmap(183, 190, icon_wind, 32, 32, 0x145F);
      tft.drawBitmap(228, 195, da, 24, 24, ST77XX_WHITE);
      tft.drawBitmap(262, 195, feng, 24, 24, ST77XX_WHITE);
    }
    weatherNeedsRedraw = false;
    Serial.printf("\n天气已重绘");
  }
  
  // 进度条逻辑
  // tft.fillRect(40, 220, ss * 4, 4, ST77XX_CYAN); 
  if (ss == 0) {
    // 秒数为0时，清空进度条区域
    // tft.fillRect(40, 220, 240, 4, 0x3186); 
    // 跨天或跨小时时刷新一下日期区，确保星期更新
    drawStaticFrame(); 
  }
}

void showConfigPage() {
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRoundRect(5, 5, 310, 230, 12, ST77XX_RED);
  
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(3);
  tft.setCursor(40, 40);
  tft.print("WIFI ERROR");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(30, 90);
  tft.println("Connect to AP:");
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(30, 120);
  tft.println("ESP8266_CLOCK");

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(200, 160);
  tft.println("Visit IP:");
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(30, 190);
  tft.println(WiFi.softAPIP().toString());
}

// --- 加载动画 ---
void showLoadingScreen() {
  tft.setCursor(60, 110);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.print("Connecting WiFi...");
}

void updateWeatherFromServer() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;

  // 构建请求 URL
  // language=zh-Hans (中文), unit=c (摄氏度)
  String url = "http://api.seniverse.com/v3/weather/now.json?key=" + String(apiKey) + "&location=" + weather_city + "&language=zh-Hans&unit=c";
  Serial.printf("url: %s", url);
  
  if (http.begin(client, url)) {
    int httpCode = http.GET();
    Serial.printf("\nhttpcode: %d", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      
      // 解析 JSON (建议使用 ArduinoJson Assistant 预估大小)
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        // 先检查 results 是否存在且有元素
        JsonArray results = doc["results"];
        if (results.size() > 0) {
          JsonObject now = results[0]["now"];
          
          // 显式转换字符串为整数
          server_temp = now["temperature"].as<int>();
          server_code = now["code"].as<int>();
          weatherNeedsRedraw = true;
          Serial.printf("天气获取成功！城市：%s, 温度：%d, 代码：%d\n", 
                        doc["results"][0]["location"]["name"].as<const char*>(),
                        server_temp, server_code);
        } else {
          Serial.println("天气数据格式错误或 API 限制");
        }
      }
    } else {
      Serial.printf("HTTP 请求失败，错误码：%s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

// 测试所用天气绘制是否可用
// void updateWeatherFromServer() {
//   static int test_step = 0; // 静态变量，用于记录测试到了第几步
  
//   // 模拟不同的天气数据组合
//   switch(test_step) {
//     case 0: // 场景：晴天，高温
//       server_code = 0;   
//       server_temp = 32; 
//       Serial.println("测试模式：晴天 (0), 32℃");
//       break;
//     case 1: // 场景：多云，舒适
//       server_code = 4;   
//       server_temp = 25; 
//       Serial.println("测试模式：多云 (4), 25℃");
//       break;
//     case 2: // 场景：大雨，补0测试
//       server_code = 14;  
//       server_temp = 9;  
//       Serial.println("测试模式：大雨 (14), 09℃");
//       break;
//     case 3: // 场景：下雪，寒冷
//       server_code = 23;  
//       server_temp = -2; 
//       Serial.println("测试模式：下雪 (23), -2℃");
//       break;
//     case 4: // 场景：大风
//       server_code = 36;
//       server_temp = 18;
//       Serial.println("测试模式：大风 (36), 18℃");
//       break;
//     case 5: // 场景：大雾
//       server_code = 31;
//       server_temp = 18;
//       Serial.println("测试模式：大风 (36), 18℃");
//       break;
//   }

//   // 关键：触发重绘标记
//   weatherNeedsRedraw = true; 

//   // 循环步进
//   test_step++;
//   if (test_step > 5) test_step = 0; 
// }
