#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define NUM_SQUARES 6

#define OLED_SDA 21 
#define OLED_SCL 22
#define OLED_RESET -1

#define FONT font_9

Adafruit_SH1106 lcd(OLED_RESET);  // Sửa lại chỉ truyền RST (I2C)

// Hàm đặt một pixel
void setpx(int16_t x, int16_t y, uint16_t color) {
  lcd.drawPixel(x, y, color);
}

// Font Maker
MakeFont font(&setpx);

// In text
void printLCD(int x, int y, String body) {
  font.print(x, y, body, WHITE, BLACK);
}

// Hiển thị nội dung lên màn
void displayLCD(std::function<void()> runtime) {
  lcd.clearDisplay();
  font.set_font(FONT);
  runtime();
  lcd.display();
}

void clearDisplay() {
  lcd.clearDisplay();
}

// Khởi tạo màn hình
void initLCD() {
  lcd.begin(SH1106_SWITCHCAPVCC, 0x3C);
}
