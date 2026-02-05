#include <Adafruit_GFX.h>    
#include <Adafruit_ST7789.h> 
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <time.h>

// 引脚定义
#define TFT_CS     15  // D8
#define TFT_RST    16  // D0
#define TFT_DC      5  // D1

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// 时间变量
int hh, mm, ss;
int curr_year, curr_month, curr_day, weekday;
bool isClockMode = false;

// 模拟时钟更新的定时器
unsigned long lastTick = 0;

void setup() {
  Serial.begin(115200);

  // 初始化屏幕
  tft.init(240, 320); 
  tft.setRotation(1); 
  tft.fillScreen(ST77XX_BLACK);

  // 显示启动信息
  tft.setCursor(20, 100);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.println("System Starting...");

  // WiFiManager 配网
  WiFiManager wm;
  // wm.resetSettings(); // 如果需要重新配网，取消此行注释
  
  tft.setCursor(20, 130);
  tft.println("Connecting WiFi...");
  
  if (!wm.autoConnect("ESP8266_Clock_AP")) {
    Serial.println("连接失败并超时");
    ESP.restart();
  }

  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(20, 100);
  tft.println("WiFi Connected!");

  // 配置 NTP 时间 (使用阿里云和清华大学服务器)
  configTime(8 * 3600, 0, "ntp.aliyun.com", "ntp.tuna.tsinghua.edu.cn");
  
  // 等待获取到时间
  tft.setCursor(20, 130);
  tft.println("Syncing Time...");
  while (time(nullptr) < 1000000) {
    delay(500);
    Serial.print(".");
  }

  isClockMode = true;
  tft.fillScreen(ST77XX_BLACK);
  drawStaticFrame(); // 第一次绘制边框
}

void loop() {
  if (!isClockMode) return;

  // 每秒更新一次时间变量
  if (millis() - lastTick >= 1000) {
    lastTick = millis();
    
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

    updateTimeDisplay();
  }
}

// 绘制静态 UI 框架（日期、线条）
void drawStaticFrame() {
  tft.fillRect(0, 0, 320, 40, ST77XX_ORANGE);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 12);
  tft.printf("%04d-%02d-%02d  Week %d", curr_year, curr_month, curr_day, weekday);
  
  tft.drawFastHLine(0, 170, 320, ST77XX_WHITE);
}

// 刷新中间的大时间数字
void updateTimeDisplay() {
  // 简单的整分刷新背景防止残影
  if (ss == 0) {
    tft.fillRect(0, 50, 320, 110, ST77XX_BLACK);
    drawStaticFrame();
  }

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(8);
  tft.setCursor(40, 80);
  tft.printf("%02d:%02d", hh, mm);
  
  // 底部显示一个简单的状态，表示当前正在运行
  tft.setTextSize(2);
  tft.setCursor(80, 200);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.printf("System Running: %02ds", ss);
}