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

// --- CẤU HÌNH CỔNG CHO ĐẦU ĐỌC GM65 ---
#define GM65_RX 16      // RX của GM65 nối ESP32 TX16
#define GM65_TX 17      // TX của GM65 nối ESP32 RX17
HardwareSerial GM65(2); // Tạo đối tượng Serial thứ 2 cho GM65

// --- CẤU HÌNH CHÂN CHO MÁY IN NHIỆT ---
#define PRINTER_RX_PIN 3            // RX máy in (ESP TX3)
#define PRINTER_TX_PIN 1            // TX máy in (ESP RX1)
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

// --- HANDLE CỦA TASK ---
TaskHandle_t StatusTaskHandle;
TaskHandle_t TempTaskHandle;
TaskHandle_t PrintTaskHandle;

// --- HÀM LẤY CODE TỦ TỪ SERVER ---
// --- HÀM LẤY THÔNG TIN TỦ TỪ SERVER (TÍCH HỢP) ---
struct LockerInfo
{
  String code;
  int number;
  bool isValid;
};

void SureWiFiConnection()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.begin(ssid, passwordWiFi);
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) // 10 giây timeout
    {
      delay(500);
      timeout++;
    }
  }
}

LockerInfo getLockerInfo()
{
  LockerInfo result = {"", -1, false};
  SureWiFiConnection(); // Đảm bảo kết nối WiFi

  HTTPClient http;
  http.begin("https://www.lephonganhtri.id.vn/api/request");
  http.setTimeout(5000);
  int httpCode = http.GET();

  if (httpCode == 200)
  {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    if (!err && doc["locker"])
    {
      result.code = doc["locker"]["code"].as<String>();
      result.number = doc["locker"]["number"];
      result.isValid = true;
    }
  }
  http.end();
  return result;
}

// --- CỜ ĐỂ KIỂM TRA SỰ THAY ĐỔI TRẠNG THÁI TỦ ---
bool c1 = true, c2 = true, c3 = true;

// --- CẬP NHẬT TRẠNG THÁI TỦ TỪ SERVER ---
void statusLockerCode()
{
  HTTPClient http;
  http.begin("https://www.lephonganhtri.id.vn/api/lockers");
  int httpCode = http.GET();
  if (httpCode == 200)
  {
    StaticJsonDocument<1024> doc;
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

// --- GỬI YÊU CẦU MỞ TỦ BẰNG MÃ CODE ---
bool openLockerByCode(const String &code)
{
  if (WiFi.status() != WL_CONNECTED)
    return false;

  HTTPClient http;
  http.begin("https://www.lephonganhtri.id.vn/api/unlock");
  http.addHeader("Content-Type", "application/json");
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["code"] = code;
  String body;
  serializeJson(jsonDoc, body);
  int httpCode = http.POST(body);
  if (httpCode == 200)
  {
    StaticJsonDocument<256> responseDoc;
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

        statusLockerCode();
        http.end();
        return true;
      }
    }
  }
  lcd.clearDisplay();
  printLCD(12, 16, "Mã không hợp lệ");
  printLCD(12, 32, "Vui lòng quét lại");
  lcd.display();
  delay(3000);
  http.end();
  return false;
}
// --- TASK KIỂM TRA TRẠNG THÁI TỦ ---
void statusTask(void *param)
{
  vTaskDelay(2000 / portTICK_PERIOD_MS); // Delay ban đầu
  while (true)
  {
    if (WiFi.status() == WL_CONNECTED)
      statusLockerCode();
    vTaskDelay(2000 / portTICK_PERIOD_MS); // Cập nhật mỗi 2 giây
  }
}

// --- HÀM IN MÃ QR CODE ---
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

// --- XỬ LÝ NÚT NHẤN ĐỂ IN MÃ QR ---
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
      if (WiFi.status() != WL_CONNECTED)
      {
        lcd.clearDisplay();
        printLCD(10, 0, "Tạm thời mất kết nối");
        printLCD(5, 20, "Vui Lòng Thử Lại");
        lcd.display();
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        return;
      }
      lcd.clearDisplay();
      printLCD(15, 20, "Đang Xử Lý...");
      lcd.display();
      LockerInfo lockerInfo = getLockerInfo();

      if (lockerInfo.isValid && !lockerInfo.code.isEmpty())
      {
        // In QR code
        lcd.clearDisplay();
        printLCD(20, 0, "Đang In QR...");
        lcd.display();

        printer.justify('C');
        printQRCode(lockerInfo.code.c_str(), lockerInfo.number);
        printer.feed(11);

        // Hiển thị kết quả
        lcd.clearDisplay();
        printLCD(15, 0, "Bạn Đã Thuê");
        printLCD(20, 20, String("Tủ Số: ") + lockerInfo.number);
        printLCD(15, 40, "Thành Công!");
        lcd.display();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
      }
      else
      {
        lcd.clearDisplay();
        printLCD(12, 0, "Hết Tủ Trống");
        printLCD(0, 16, "Vui Lòng Thử Lại");
        printLCD(0, 32, "Sau !");
        lcd.display();
        delay(1000);
        statusLockerCode();
      }
    }
  }
  lastButtonState = reading;
}
// --- XỬ LÝ ĐẦU ĐỌC GM65 ---

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

    if (c == '\r' || c == '\n')
    {
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

          // lastScanTime = millis(); // luôn cập nhật thời gian đọc cuối
        }
        // else
        // {
        //   unsigned long elapsed = currentTime - lastScanTime;
        //   if (elapsed > RESET_DELAY)
        //     elapsed = RESET_DELAY; // tránh âm hoặc lớn bất thường
        //   unsigned long remaining = (RESET_DELAY - elapsed) / 1000 + 1;

        //   lcd.clearDisplay();
        //   printLCD(10, 20, "Thao tác đã thực hiện");
        //   printLCD(10, 40, "Thử lại sau " + String(remaining) + " giây");
        //   lcd.display();
        //   digitalWrite(BUZZER, HIGH);
        //   delay(50);
        //   digitalWrite(BUZZER, LOW);
        // }
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

// --- GỬI NHIỆT ĐỘ LÊN SERVER ---
void sendTemperatureToServer(float temperature)
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  HTTPClient http;
  http.begin("https://www.lephonganhtri.id.vn/api/temperature");
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<128> doc;
  doc["temperature"] = temperature;
  String body;
  serializeJson(doc, body);

  int httpCode = http.POST(body);
  // Serial.printf("Temp HTTP Code: %d\n", httpCode);
  http.end();
}

// --- TASK GỬI NHIỆT ĐỘ ĐỊNH KỲ VÀ CẢNH BÁO ---
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
          vTaskDelay(2000 / portTICK_PERIOD_MS);
          digitalWrite(BUZZER, LOW);
          vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
      }
    }
    // else {
    //   Serial.println("Failed to read from DHT11.");
    // }
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
}

void pollPrintQueue(void *param)
{
  while (true)
  {
    HTTPClient http;
    http.begin("https://www.lephonganhtri.id.vn/api/print_qr");
    int httpCode = http.GET();
    if (httpCode == 200)
    {
      StaticJsonDocument<512> doc;
      DeserializationError err = deserializeJson(doc, http.getString());
      if (!err)
      {
        String code = doc["code"] | "";
        bool printed = doc["printed"] | false;
        int lockerNumber = doc["locker_id"];

        if (printed && code.length() > 0 && code != "null")
        {
          // Serial.printf("[pollPrintQueue] Print QR: %s\n", code.c_str());
          printer.justify('C');
          printQRCode(code.c_str(), lockerNumber);
          printer.feed(11);
        }
      }
    }
    http.end();
    vTaskDelay(3000 / portTICK_PERIOD_MS); // Kiểm tra mỗi 3 giây
  }
}

// --- SETUP HỆ THỐNG ---
void setup()
{
  Serial.begin(115200);
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

  xTaskCreatePinnedToCore(statusTask, "StatusTask", 8192, NULL, 1, &StatusTaskHandle, 1);
  xTaskCreatePinnedToCore(temperatureTask, "TempTask", 8192, NULL, 1, &TempTaskHandle, 1);
  xTaskCreatePinnedToCore(pollPrintQueue, "PrintTask", 8192, NULL, 1, &PrintTaskHandle, 1);
}

// --- VÒNG LẶP CHÍNH ---
void loop()
{

  if (WiFi.status() != WL_CONNECTED && !connectToWiFi())
  {
    delay(5000);
    return;
  }
  handleButtonQR();    // Kiểm tra nút nhấn in QR
  handleGM65Scanner(); // Kiểm tra đầu đọc mã
  delay(10);
}