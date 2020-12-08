/*
 * TM12864_LCD.h
 *
 *  Created on: 26 Oct 2020
 *      Author: jp112sdl
 */

#ifndef TM12864_LCD_H_
#define TM12864_LCD_H_

enum {LED_ON = HIGH, LED_OFF = LOW };

template<uint8_t BACKLIGHT_RED_PIN, uint8_t BACKLIGHT_GREEN_PIN, uint8_t BACKLIGHT_BLUE_PIN>
class TM12864 : public U8G2 {
private:
  U8G2& _u8g2;
public:
  TM12864(U8G2 &u8g2) : _u8g2(u8g2) { }
  ~TM12864() {}

  uint16_t centerPosition(const char * text, uint16_t width=128) {
    return (width / 2) - (_u8g2.getUTF8Width(text) / 2);
  }

  void initLCD() {
    pinMode(BACKLIGHT_RED_PIN, OUTPUT);
    pinMode(BACKLIGHT_GREEN_PIN, OUTPUT);
    pinMode(BACKLIGHT_BLUE_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_RED_PIN, LOW);
    digitalWrite(BACKLIGHT_GREEN_PIN, LOW);
    digitalWrite(BACKLIGHT_BLUE_PIN, LOW);
    _u8g2.begin();
    _u8g2.setContrast(230);
  }

  void setRED(bool s) {
    digitalWrite(BACKLIGHT_RED_PIN, s == true ? HIGH : LOW);
  }

  void setGREEN(bool s) {
    digitalWrite(BACKLIGHT_GREEN_PIN, s == true ? HIGH : LOW);
  }

  void setBLUE(bool s) {
    digitalWrite(BACKLIGHT_BLUE_PIN, s == true ? HIGH : LOW);
  }

  void setWHITE(bool s) {
    setALL(s);
  }

  void setALL(bool s) {
    setRED(s);
    setGREEN(s);
    setBLUE(s);
  }

};



#endif /* TM12864_LCD_H_ */
