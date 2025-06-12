void drawTickMark(int x, int y) {
  lcd.drawLine(x, y, x + 4, y + 4, WHITE);
  lcd.drawLine(x + 4, y + 4, x + 10, y - 2, WHITE);
}

void drawCrossMark(int x, int y) {
  lcd.drawLine(x, y, x + 10, y + 10, WHITE);
  lcd.drawLine(x + 10, y, x, y + 10, WHITE);
}

void drawEmptySquares(bool cupboard1, bool cupboard2, bool cupboard3) {
  lcd.clearDisplay();

  int squareWidth = 42;
  int squareHeight = 64;
  int xOffset = 12;
  int yOffset = 24;

  for (int i = 0; i < 3; i++) {
    int x = i * squareWidth;
    int y = 0;

    lcd.drawRect(x, y, squareWidth, squareHeight, WHITE);

    lcd.setTextSize(1);
    lcd.setTextColor(WHITE);
    lcd.setCursor(x + 2, y + 2);
    lcd.print(i + 1);

    int markX = x + xOffset;
    int markY = y + yOffset;

    bool isUnlocked = false;
    if (i == 0) isUnlocked = cupboard1;
    else if (i == 1) isUnlocked = cupboard2;
    else if (i == 2) isUnlocked = cupboard3;

    if (isUnlocked) {
      drawTickMark(markX, markY);
    } else {
      drawCrossMark(markX, markY);
    }
  }

  lcd.display();
}
