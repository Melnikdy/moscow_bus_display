#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <U8g2_for_Adafruit_GFX.h>

// TFT display pins
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
U8G2_FOR_ADAFRUIT_GFX u8g2;

// Wi-Fi credentials
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "PASSWORD";

// Московский транспорт stop_id
const String stops[] = {
  "44b7adfc-cba1-4c5a-b3e1-2d26c87092fc", // первая остановка
  "e7d7bf44-a378-47b6-99fc-6fea1d24b2c7"  // вторая остановка
};

const char* stopNames[] = {
  "Зарядье",
  "ул. Балчуг"
};

// Colors
#define BACKGROUND ILI9341_WHITE
#define TEXT_COLOR ILI9341_BLACK
#define HEADER_COLOR ILI9341_BLUE
#define BUS_COLOR ILI9341_RED
#define TIME_COLOR ILI9341_DARKGREEN

struct BusInfo {
  String number;
  int minutes;
};

// ---------------------------------------------------
void setup() {
  Serial.begin(115200);

  // TFT setup
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(BACKGROUND);

  u8g2.begin(tft);
  u8g2.setFontMode(1);
  u8g2.setForegroundColor(TEXT_COLOR);
  u8g2.setBackgroundColor(BACKGROUND);
  
  // Wi-Fi connect
  WiFi.begin(ssid, password);
  u8g2.setFont(u8g2_font_8x13_t_cyrillic);
  u8g2.setCursor(10, 15);
  u8g2.print("Подключение WiFi...");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    tft.fillScreen(BACKGROUND);
  } else {
    tft.fillScreen(BACKGROUND);
    u8g2.setCursor(10, 15);
    u8g2.print("Ошибка сети");
    while(1) delay(1000);
  }
}

// ---------------------------------------------------
String getStopData(const String& stopId) {
  WiFiClientSecure client;
  client.setInsecure();  

  HTTPClient https;
  String url = "https://moscowtransport.app/api/stop_v2/" + stopId;

  Serial.println("URL: " + url);

  https.begin(client, url);

  https.addHeader("User-Agent", 
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");
  https.addHeader("Accept", "application/json, text/plain, */*");
  https.addHeader("Accept-Language", "ru,en;q=0.9");
  https.addHeader("Referer", "https://moscowapp.mos.ru/");
  https.addHeader("Origin", "https://moscowapp.mos.ru");
  https.addHeader("Connection", "keep-alive");
  https.addHeader("Cache-Control", "no-cache");

  int code = https.GET();
  Serial.println("HTTP: " + String(code));

  if (code != 200) {
    Serial.println("Error: " + String(code));
    https.end();
    return "";
  }

  String payload = https.getString();
  https.end();
  return payload;
}

// ---------------------------------------------------
void parseStopData(const String& jsonData, BusInfo buses[], int& count) {
  count = 0;
  if (jsonData.length() == 0) return;

  StaticJsonDocument<32768> doc;
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) {
    Serial.println("JSON parse error");
    return;
  }

  JsonArray routes = doc["routePath"].as<JsonArray>();
  for (JsonObject route : routes) {
    if (count >= 4) break;

    String number = route["number"].as<String>();
    JsonArray forecasts = route["externalForecast"].as<JsonArray>();
    if (forecasts.size() > 0) {
      int minutes = forecasts[0]["time"].as<int>() / 60; // время в минутах
      buses[count].number = number;
      buses[count].minutes = minutes;
      count++;
    }
  }
}
void drawVerticalSeparator(int x) {
    tft.fillRect(x, 0, 4, tft.height(), HEADER_COLOR);  
}

void drawHorizontalSeparator(int y) {
    tft.fillRect(0, y, tft.width(), 3, HEADER_COLOR);
}
// ---------------------------------------------------
void displayBusInfo(int stopIndex, const BusInfo buses[], int count) {
  int x = stopIndex * 160;
  int y = 0;
  u8g2.setFont(u8g2_font_9x15_t_cyrillic);
  tft.fillRect(x, y, 160, 30, HEADER_COLOR);
  u8g2.setForegroundColor(ILI9341_WHITE);
  u8g2.setBackgroundColor(HEADER_COLOR);
  u8g2.setCursor(x + 2, y + 20);
  u8g2.print(stopNames[stopIndex]);
  drawVerticalSeparator(150);
  y += 40;
  u8g2.setFont(u8g2_font_10x20_t_cyrillic);
  if (count == 0) {
    u8g2.setForegroundColor(TEXT_COLOR);
    u8g2.setBackgroundColor(BACKGROUND);
    u8g2.setCursor(x + 2, y + 20);
    u8g2.print("Нет данных");
    return;
  }

  for (int i = 0; i < count; i++) {
    int lineY = y + (i * 52);
    drawHorizontalSeparator(lineY+42);
    u8g2.setFont(u8g2_font_inr24_t_cyrillic);
    u8g2.setCursor(x + 10, lineY + 20);
    u8g2.setForegroundColor(BUS_COLOR);
    u8g2.setBackgroundColor(BACKGROUND);
    u8g2.print(buses[i].number);
    u8g2.setFont(u8g2_font_10x20_t_cyrillic);
    u8g2.setForegroundColor(TIME_COLOR);
    u8g2.setBackgroundColor(BACKGROUND);
    u8g2.setCursor(x + 10, lineY + 36);
    u8g2.print("через ");
    u8g2.print(buses[i].minutes);
    u8g2.print(" мин");
  }
}

// ---------------------------------------------------
void updateDisplay() {
  tft.fillScreen(BACKGROUND);

  for (int i = 0; i < 2; i++) {
    BusInfo buses[4];
    int busCount = 0;

    Serial.printf("\n=== Запрос stop %d ===\n", i);
    String data = getStopData(stops[i]);
    parseStopData(data, buses, busCount);
    displayBusInfo(i, buses, busCount);
    delay(1000);
  }
}

// ---------------------------------------------------
void loop() {
  updateDisplay();
  for (int i = 60; i > 0; i--) {
    tft.fillRect(300, 37, 30, -7, BACKGROUND);
    u8g2.setFont(u8g2_font_5x7_t_cyrillic);
    u8g2.setForegroundColor(TEXT_COLOR);
    u8g2.setCursor(300, 37);
    u8g2.print(i);
    u8g2.print(" c");
    delay(1000);
  }
}