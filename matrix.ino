#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GFX.h>

Adafruit_7segment matrix = Adafruit_7segment();

void matrixInit() {
  matrix.begin(0x70);
}

void matrixBrightness(int level) {
  matrix.setBrightness(level);
}

void matrixDisplay(int hh, int mm) {

  bool dots = true;
  matrix.drawColon(dots);

  if (hh < 10) {
    matrix.writeDigitNum(0, 0, dots);
    matrix.writeDigitNum(1, hh, dots);
  } else if (hh < 20) {
    matrix.writeDigitNum(0, 1, dots);
    matrix.writeDigitNum(1, hh - 10, dots);
  } else {
    matrix.writeDigitNum(0, 2, dots);
    matrix.writeDigitNum(1, hh - 20, dots);
  }

  if (mm < 10) {
    matrix.writeDigitNum(3, 0, dots);
    matrix.writeDigitNum(4, mm, dots);
  } else if (mm < 20) {
    matrix.writeDigitNum(3, 1, dots);
    matrix.writeDigitNum(4, mm - 10, dots);
  } else if (mm < 30) {
    matrix.writeDigitNum(3, 2, dots);
    matrix.writeDigitNum(4, mm - 20, dots);
  } else if (mm < 40) {
    matrix.writeDigitNum(3, 3, dots);
    matrix.writeDigitNum(4, mm - 30, dots);
  } else if (mm < 50) {
    matrix.writeDigitNum(3, 4, dots);
    matrix.writeDigitNum(4, mm - 40, dots);
  } else {
    matrix.writeDigitNum(3, 5, dots);
    matrix.writeDigitNum(4, mm - 50, dots);
  }

  matrix.writeDisplay();

}

