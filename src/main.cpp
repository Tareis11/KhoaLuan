// ======================= INCLUDE LIBRARIES =========================
#include <Arduino.h>           // Thư viện ESP32 hệ thống
#include <WiFi.h>              // Quản lý Wi-Fi cho ESP32
#include <HTTPClient.h>        // Gửi yêu cầu HTTP
#include <ArduinoJson.h>       // Phân tích và tạo JSON
#include <Adafruit_Thermal.h>  // Thư viện điều khiển máy in nhiệt Adafruit
#include <Adafruit_SH1106.h>   // OLED SH1106
#include "FontMaker.h"         // Thư viện tạo font tùy chỉnh
#include <DHT.h>               // Thư viện cảm biến nhiệt độ độ ẩm DHT11
#include "freertos/FreeRTOS.h" // FreeRTOS cho ESP32
#include "freertos/task.h"     // Quản lý task trong FreeRTOS

#include "lcd_build.h"    // Quản lý màn hình LCD
#include "wifi_build.h"   // Quản lý kết nối WiFi
#include "drawer_build.h" // Điều khiển tủ khóa

// ======================= WIFI MANAGEMENT VARIABLES =================
unsigned long lastWiFiReconnect = 0;
const unsigned long WIFI_RECONNECT_INTERVAL = 30000; // 30 giây
bool wifiConnectionStable = false;
int wifiReconnectAttempts = 0;
const int MAX_WIFI_RECONNECT_ATTEMPTS = 5;

// ======================= PIN & HARDWARE CONFIG =====================
// --- CẤU HÌNH CỔNG CHO ĐẦU ĐỌC GM65 ---
#define GM65_RX 3       // RX của GM65 nối ESP32 TX16
#define GM65_TX 1       // TX của GM65 nối ESP32 RX17
HardwareSerial GM65(2); // Tạo đối tượng Serial thứ 2 cho GM65

// --- CẤU HÌNH CHÂN CHO MÁY IN NHIỆT ---
#define PRINTER_RX_PIN 16           // RX máy in (ESP TX3)
#define PRINTER_TX_PIN 17           // TX máy in (ESP RX1)
Adafruit_Thermal printer(&Serial1); // Máy in nhiệt sử dụng Serial1

// --- CẤU HÌNH GM65 ---
String gm65Buffer = "";
String lastProcessedCode = "";
unsigned long lastScanTime = 0;
bool codeAlreadyProcessed = false;

const unsigned long RESET_DELAY = 3000; // Reset mã sau 5 giây không đọc

// --- CẤU HÌNH NÚT NHẤN ---
#define BUTTON_PIN 18 // Nút nhấn dùng chân GPIO18

// --- CẤU HÌNH CÁC CHÂN ĐIỀU KHIỂN KHÓA ---
#define LOCK_1 14
#define LOCK_2 27
#define LOCK_3 26

#define BUZZER 25 // Chân điều khiển loa cảnh báo

// --- CẢM BIẾN NHIỆT ĐỘ DHT11 ---
#define DHTPIN 33         // DHT11 nối chân GPIO33
#define DHTTYPE DHT11     // Loại cảm biến là DHT11
DHT dht(DHTPIN, DHTTYPE); // Khởi tạo đối tượng DHT
#define DEBOUNCE_DELAY 50 // Thời gian chống rung nút nhấn (50ms)
unsigned long lastDebounceTime = 0;
bool lastButtonState = HIGH;
bool buttonState = HIGH;

// ======================= TASK HANDLES ==============================
TaskHandle_t StatusTaskHandle;
TaskHandle_t TempTaskHandle;
TaskHandle_t PrintTaskHandle;

// ======================= STRUCTS & DATA ============================
// --- HÀM LẤY CODE TỦ TỪ SERVER ---
// --- HÀM LẤY THÔNG TIN TỦ TỪ SERVER (TÍCH HỢP) ---
struct LockerInfo
{
  String code;
  int number;
  bool isValid;
  bool isNetworkError;
};

// ======================= WIFI & SERVER FUNCTIONS ===================
bool isWiFiStable()
{
  return (WiFi.status() == WL_CONNECTED && WiFi.RSSI() > -80);
}
// Kiểm tra kết nối wifi có ổn định không

void SureWiFiConnection()
{
  if (WiFi.status() != WL_CONNECTED || !isWiFiStable())
  {
    wifiReconnectAttempts++;

    if (wifiReconnectAttempts > MAX_WIFI_RECONNECT_ATTEMPTS)
    {
      ESP.restart(); // Restart nếu thử quá nhiều lần
    }

    WiFi.disconnect();
    delay(1000);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, passwordWiFi);

    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 60) // 30 giây timeout
    {
      delay(500);
      timeout++;

      // Reset WiFi sau mỗi 10 lần thử
      if (timeout % 20 == 0)
      {
        WiFi.disconnect();
        delay(1000);
        WiFi.begin(ssid, passwordWiFi);
      }
    }

    // Kiểm tra kết nối thành công
    if (WiFi.status() == WL_CONNECTED && isWiFiStable())
    {
      wifiConnectionStable = true;
      wifiReconnectAttempts = 0; // Reset counter khi kết nối thành công
      lastWiFiReconnect = millis();
    }
    else
    {
      wifiConnectionStable = false;
      // Nếu vẫn không kết nối được sau nhiều lần, restart ESP32
      if (wifiReconnectAttempts >= MAX_WIFI_RECONNECT_ATTEMPTS)
      {
        ESP.restart();
      }
    }
  }
  else
  {
    wifiConnectionStable = true;
    wifiReconnectAttempts = 0;
  }
}

LockerInfo getLockerInfo()
{
  LockerInfo result = {"", -1, false, false};

  // Kiểm tra kết nối WiFi trước khi gọi API
  if (WiFi.status() != WL_CONNECTED)
  {
    SureWiFiConnection();
    if (WiFi.status() != WL_CONNECTED)
    {
      result.isNetworkError = true;
      return result;
    }
  }

  HTTPClient http;
  http.begin("https://www.lephonganhtri.id.vn/api/request");
  http.setTimeout(10000); // Tăng timeout lên 10 giây
  int httpCode = http.GET();

  if (httpCode == 200)
  {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    if (!err && doc["locker"])
    {
      result.code = doc["locker"]["code"].as<String>();
      result.number = doc["locker"]["number"];
      result.isValid = true;
    }
  }
  else if (httpCode == 404)
  {
    result.isValid = false;
    result.isNetworkError = false;
  }
  else
  {
    result.isNetworkError = true;
  }
  http.end();
  return result;
}

// ======================= LOCKER STATUS FUNCTIONS ===================
bool c1 = true, c2 = true, c3 = true;

void statusLockerCode()
{
  // Kiểm tra kết nối WiFi trước khi gọi API
  if (WiFi.status() != WL_CONNECTED)
  {
    SureWiFiConnection();
    if (WiFi.status() != WL_CONNECTED)
    {
      return; // Bỏ qua nếu không có kết nối
    }
  }

  HTTPClient http;
  http.begin("https://www.lephonganhtri.id.vn/api/lockers");
  http.setTimeout(8000); // Tăng timeout
  int httpCode = http.GET();
  if (httpCode == 200)
  {
    JsonDocument doc;
    if (!deserializeJson(doc, http.getString()))
    {
      for (JsonObject locker : doc["lockers"].as<JsonArray>())
      {
        int num = locker["number"];
        bool isLocked = locker["isLocked"];
        bool isUnHire = locker["code"].as<String>().isEmpty();
        int times = locker["times"].as<int>();

        // Đóng/mở khóa vật lý
        if (isLocked)
        {
          if (num == 1)
            digitalWrite(LOCK_1, LOW);
          if (num == 2)
            digitalWrite(LOCK_2, LOW);
          if (num == 3)
            digitalWrite(LOCK_3, LOW);
        }
        else
        {
          if (num == 1)
            digitalWrite(LOCK_1, HIGH);
          if (num == 2)
            digitalWrite(LOCK_2, HIGH);
          if (num == 3)
            digitalWrite(LOCK_3, HIGH);
        }

        // Cập nhật trạng thái cờ
        if (num == 1)
          c1 = isUnHire;
        else if (num == 2)
          c2 = isUnHire;
        else if (num == 3)
          c3 = isUnHire;
      }

      drawEmptySquares(c1, c2, c3); // Vẽ lại giao diện tủ trên OLED
    }
  }
  http.end();
}

// ======================= LOCKER CONTROL FUNCTIONS ==================
bool openLockerByCode(const String &code)
{
  SureWiFiConnection(); // Đảm bảo kết nối WiFi
  int retry = 0;
  const int maxRetry = 3;
  int httpCode = 0;

  while (retry < maxRetry)
  {
    HTTPClient http;
    http.begin("https://www.lephonganhtri.id.vn/api/unlock");
    http.addHeader("Content-Type", "application/json");
    JsonDocument doc;
    doc["code"] = code;
    String body;
    serializeJson(doc, body);
    httpCode = http.POST(body);

    if (httpCode == 200)
    {
      JsonDocument responseDoc;
      DeserializationError err = deserializeJson(responseDoc, http.getString());
      if (!err && responseDoc["locker"])
      {
        int lockerNumber = responseDoc["locker"]["number"];
        bool isLocked = responseDoc["locker"]["isLocked"];
        int times = responseDoc["locker"]["times"];

        if (!isLocked)
        {
          if (lockerNumber == 1)
            digitalWrite(LOCK_1, HIGH);
          else if (lockerNumber == 2)
            digitalWrite(LOCK_2, HIGH);
          else if (lockerNumber == 3)
            digitalWrite(LOCK_3, HIGH);
          lcd.clearDisplay();
          if (times == 0)
          {
            printLCD(10, 0, "Cám ơn đã sử dụng");
            printLCD(25, 16, String("Tủ Số: ") + lockerNumber);
            printLCD(20, 32, "Sẽ Tự Đóng");
            printLCD(20, 48, "Sau 10 Giây");
            lcd.display();
            delay(3000);
          }
          else
          {
            printLCD(10, 0, "Mở tủ thành công");
            printLCD(25, 16, String("Tủ Số: ") + lockerNumber);
            printLCD(20, 32, "Sẽ Tự Đóng");
            printLCD(20, 48, "Sau 10 Giây");
            lcd.display();
            delay(3000);
          }
          // statusLockerCode();
          http.end();
          return true;
        }
      }
      http.end();
      break; // Nếu 200 nhưng không đúng dữ liệu, không retry nữa
    }
    else if (httpCode == 404)
    {
      // Mã code không hợp lệ
      lcd.clearDisplay();
      printLCD(12, 16, "Mã không hợp lệ");
      printLCD(12, 32, "Vui lòng quét lại");
      lcd.display();
      delay(3000);
      http.end();
      return false;
    }
    else
    {
      http.end();
      retry++;
      delay(500); // Đợi một chút rồi thử lại
    }
  }

  // Lỗi mạng hoặc server không phản hồi
  lcd.clearDisplay();
  printLCD(12, 16, "Lỗi mạng!");
  printLCD(0, 32, "Vui lòng thử lại");
  lcd.display();
  delay(3000);
  return false;
}

// ======================= TASKS =====================================
void statusTask(void *param)
{
  vTaskDelay(2000); // Delay ban đầu
  while (true)
  {
    // Kiểm tra kết nối WiFi trước khi gọi API
    if (WiFi.status() == WL_CONNECTED)
      statusLockerCode();
    else
    {
      SureWiFiConnection(); // Thử kết nối lại WiFi
    }
    vTaskDelay(5000); // Tăng interval lên 5 giây để giảm tải
  }
}

// ======================= PRINTER & QR FUNCTIONS ====================
void printQRCode(const char *data, int lockerNumber)
{

  printer.justify('C'); // Căn giữa
  printer.setSize('S'); // Font size vừa
  printer.println("TU SO: " + String(lockerNumber));
  printer.feed(1);

  int len = strlen(data);
  printer.write(0x1D);
  printer.write('(');
  printer.write('k');
  printer.write(4);
  printer.write(0);
  printer.write(0x31);
  printer.write(0x41);
  printer.write(2);
  printer.write(0);
  printer.write(0x1D);
  printer.write('(');
  printer.write('k');
  printer.write(3);
  printer.write(0);
  printer.write(0x31);
  printer.write(0x43);
  printer.write(4);
  printer.write(0x1D);
  printer.write('(');
  printer.write('k');
  printer.write(3);
  printer.write(0);
  printer.write(0x31);
  printer.write(0x45);
  printer.write(48);
  printer.write(0x1D);
  printer.write('(');
  printer.write('k');
  printer.write(len + 3);
  printer.write(0);
  printer.write(0x31);
  printer.write(0x50);
  printer.write(0x30);
  printer.print(data);
  printer.write(0x1D);
  printer.write('(');
  printer.write('k');
  printer.write(3);
  printer.write(0);
  printer.write(0x31);
  printer.write(0x51);
  printer.write(0x30);
}

// ======================= BUTTON HANDLER ============================
void handleButtonQR()
{
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState)
    lastDebounceTime = millis();

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY && reading != buttonState)
  {
    buttonState = reading;
    if (buttonState == LOW)
    {
      lcd.clearDisplay();
      printLCD(15, 20, "Đang Xử Lý...");
      lcd.display();

      int retry = 0;
      const int maxRetry = 10;
      LockerInfo lockerInfo;
      bool networkError = false;
      while (retry < maxRetry)
      {
        lockerInfo = getLockerInfo();
        if (lockerInfo.isValid && !lockerInfo.code.isEmpty())
        {
          networkError = false;
          break;
        }
        else if (lockerInfo.isNetworkError)
        {
          networkError = true;
          retry++;
          delay(500);
        }
        else
        {
          // Hết tủ thật sự
          networkError = false;
          break;
        }
      }
      if (networkError)
      {
        lcd.clearDisplay();
        printLCD(12, 16, "Lỗi mạng!");
        printLCD(0, 32, "Vui lòng thử lại");
        lcd.display();
        delay(2000);
        // statusLockerCode();
        lastButtonState = reading;
        return;
      }

      if (lockerInfo.isValid && !lockerInfo.code.isEmpty())
      {
        lcd.clearDisplay();
        printLCD(20, 0, "Đang In QR...");
        lcd.display();

        printer.justify('C');
        printQRCode(lockerInfo.code.c_str(), lockerInfo.number);
        printer.feed(11);

        lcd.clearDisplay();
        printLCD(15, 0, "Bạn Đã Thuê");
        printLCD(20, 20, String("Tủ Số: ") + lockerInfo.number);
        printLCD(15, 40, "Thành Công!");
        lcd.display();
        vTaskDelay(5000);
      }
      else
      {
        lcd.clearDisplay();
        printLCD(12, 0, "Hết Tủ Trống");
        printLCD(0, 16, "Vui Lòng Thử Lại");
        printLCD(0, 32, "Sau !");
        lcd.display();
        delay(1000);
        // statusLockerCode();
      }
    }
  }
  lastButtonState = reading;
}

// ======================= GM65 SCANNER HANDLER ======================
void handleGM65Scanner()
{
  unsigned long currentTime = millis();

  // Tự reset trạng thái nếu không còn mã trước đầu đọc sau 5s
  if (!lastProcessedCode.isEmpty() && (currentTime - lastScanTime > RESET_DELAY))
  {
    lastProcessedCode = "";
    codeAlreadyProcessed = false;
  }

  while (GM65.available())
  {
    char c = GM65.read();
    lastScanTime = currentTime;

    if (c == '\r' || c == '\n')
    {
      gm65Buffer.trim();
      if (gm65Buffer.length() > 0)
      {
        bool isSameCode = (gm65Buffer == lastProcessedCode);

        // Chỉ xử lý nếu mã mới hoặc mã này chưa từng xử lý
        if (!isSameCode || !codeAlreadyProcessed)
        {
          digitalWrite(BUZZER, HIGH);
          delay(100);
          digitalWrite(BUZZER, LOW);

          lcd.clearDisplay();
          printLCD(15, 20, "Đang Xử Lý...");
          lcd.display();
          openLockerByCode(gm65Buffer);

          // Ghi nhớ mã đã xử lý
          lastProcessedCode = gm65Buffer;
          codeAlreadyProcessed = true;
        }

        gm65Buffer = "";
      }
    }
    else
    {
      gm65Buffer += c;
    }
  }

  // Tự reset buffer nếu không đọc đủ mã sau 1 giây
  if (gm65Buffer.length() > 0 && currentTime - lastScanTime > 1000)
  {
    gm65Buffer = "";
  }
}

// ======================= TEMPERATURE FUNCTIONS =====================
void sendTemperatureToServer(float temperature)
{
  // Kiểm tra kết nối WiFi trước khi gọi API
  if (WiFi.status() != WL_CONNECTED)
  {
    SureWiFiConnection();
    if (WiFi.status() != WL_CONNECTED)
    {
      return; // Bỏ qua nếu không có kết nối
    }
  }

  HTTPClient http;
  http.begin("https://www.lephonganhtri.id.vn/api/temperature");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000); // Tăng timeout

  JsonDocument doc;
  doc["temperature"] = temperature;
  String body;
  serializeJson(doc, body);

  int httpCode = http.POST(body);
  // Serial.printf("Temp HTTP Code: %d\n", httpCode);
  http.end();
}

void temperatureTask(void *param)
{
  while (true)
  {
    float temp = dht.readTemperature();
    if (!isnan(temp))
    {
      // Serial.printf("Temperature: %.1f C\n", temp);
      sendTemperatureToServer(temp);

      if (temp > 40.0)
      {
        for (int i = 0; i < 3; i++)
        {
          digitalWrite(BUZZER, HIGH);
          vTaskDelay(2000);
          digitalWrite(BUZZER, LOW);
          vTaskDelay(2000);
        }
      }
    }

    vTaskDelay(4000); // Tăng interval lên 10 giây để giảm tải
  }
}

// ======================= PRINT QUEUE POLLING =======================
void pollPrintQueue(void *param)
{
  while (true)
  {
    // Kiểm tra kết nối WiFi trước khi gọi API
    if (WiFi.status() != WL_CONNECTED)
    {
      SureWiFiConnection();
      if (WiFi.status() != WL_CONNECTED)
      {
        vTaskDelay(5000); // Đợi 5 giây rồi thử lại
        continue;
      }
    }

    HTTPClient http;
    http.begin("https://www.lephonganhtri.id.vn/api/print_qr");
    http.setTimeout(8000); // Tăng timeout
    int httpCode = http.GET();
    if (httpCode == 200)
    {
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, http.getString());
      if (!err)
      {
        String code = doc["code"] | "";
        bool printed = doc["printed"] | false;
        int lockerNumber = doc["locker_id"];

        if (printed && code.length() > 0 && code != "null")
        {
          lcd.clearDisplay();
          printLCD(15, 20, "Đang In QR...");
          printLCD(15, 40, "Cho tủ Số: " + String(lockerNumber));
          lcd.display();
          // Serial.printf("[pollPrintQueue] Print QR: %s\n", code.c_str());
          printer.justify('C');
          printQRCode(code.c_str(), lockerNumber);
          printer.feed(11);
        }
      }
    }
    http.end();
    vTaskDelay(5000); // Tăng interval lên 5 giây để giảm tải
  }
}

// ======================= SETUP & LOOP ==============================
void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(false);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  initLCD();
  if (!connectToWiFi())
  {
    delay(5000);
    ESP.restart();
  }

  Serial1.begin(9600, SERIAL_8N1, PRINTER_RX_PIN, PRINTER_TX_PIN);
  GM65.begin(9600, SERIAL_8N1, GM65_RX, GM65_TX);
  dht.begin();

  pinMode(BUTTON_PIN, INPUT_PULLUP); // Nút nhấn ở trạng thái HIGH mặc định
  pinMode(LOCK_1, OUTPUT);
  pinMode(LOCK_2, OUTPUT);
  pinMode(LOCK_3, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(LOCK_1, LOW);
  digitalWrite(LOCK_2, LOW);
  digitalWrite(LOCK_3, LOW);
  digitalWrite(BUZZER, LOW);

  statusLockerCode(); // Cập nhật trạng thái ban đầu

  // Tăng stack size cho các task để tránh overflow
  xTaskCreatePinnedToCore(statusTask, "StatusTask", 10240, NULL, 1, &StatusTaskHandle, 1);
  xTaskCreatePinnedToCore(temperatureTask, "TempTask", 10240, NULL, 1, &TempTaskHandle, 1);
  xTaskCreatePinnedToCore(pollPrintQueue, "PrintTask", 10240, NULL, 1, &PrintTaskHandle, 1);
}

// --- VÒNG LẶP CHÍNH ---
void loop()
{
  // Kiểm tra kết nối WiFi định kỳ
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000)
  { // Kiểm tra mỗi 30 giây
    if (WiFi.status() != WL_CONNECTED)
    {
      SureWiFiConnection();
    }
    lastWiFiCheck = millis();
  }

  handleButtonQR();
  handleGM65Scanner();
  delay(50);
}