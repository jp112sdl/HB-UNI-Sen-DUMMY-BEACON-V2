//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2018-08-30 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
#define MAX_FAKEDEVICE_COUNT  16
#define USE_DISPLAY

#define EI_NOTEXTERNAL
#include <SPI.h>
#ifdef USE_DISPLAY
#include <U8g2lib.h>
#include "TM12864_LCD.h"
#endif
#include <AskSinPP.h>
#include <Register.h>
#include "HB_MultiChannelDevice.h"

#ifdef USE_DISPLAY
#define LCD_CS     28
#define LCD_RST    29
#define LCD_DC     30
#define LCD_BLUE   31
#define LCD_RED    32
#define LCD_GREEN  33
#define CHANNEL_PER_PAGE 8

#define ROTARY_ENC_D_PIN  4
#define ROTARY_ENC_C_PIN  5
#endif

#define CC1101_CS   8 //PB0
#define CC1101_GDO0 6 //PE6 INT6

#define CONFIG_BUTTON_PIN  7 //PE7 INT7
#define LED               22 //PD4

#define PEERS_PER_CHANNEL  4
#define CFG_BYTE_OFFSET    8

#define TOSTRING(x) #x
#define TOSTR(x) TOSTRING(x)

using namespace as;

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
  {0xF3, 0xFE, 0x00},          // Device ID
  "JPBEACON02",                // Device Serial
  {0xF3, 0xFE},                // Device Model
  0x10,                        // Firmware Version
  0x10,                        // Device Type
  {0x01, 0x01}                 // Info Bytes
};

typedef LibSPI<CC1101_CS> SPIType;
typedef Radio<SPIType, CC1101_GDO0> RadioType;
typedef StatusLed<LED> LedType;
typedef AskSin<LedType, NoBattery, RadioType> Hal;
Hal hal;

#ifdef USE_DISPLAY
class LCDDisplayType : public Alarm {
private:
  U8G2_ST7565_64128N_F_4W_HW_SPI display;
  TM12864<LCD_RED, LCD_GREEN, LCD_BLUE> tm12864;
  uint8_t cursorPos;
  uint8_t backlightOnTime;
  uint8_t listOffset;

  uint8_t numdigits(uint32_t i) {
    return (i < 10) ? 1: (uint8_t)(log10((uint32_t)i)) + 1;
  }

public:
  LCDDisplayType () : Alarm(0), display(U8G2_R0, LCD_CS, LCD_DC, LCD_RST), tm12864(display), cursorPos(0), backlightOnTime(4), listOffset(0) {}
  virtual ~LCDDisplayType () {}

  void setBackLightOnTime(uint8_t t) {
    if (backlightOnTime == 0) {
      disableBacklight();
    }

    backlightOnTime = t;

    if (backlightOnTime == 255) {
      enableBacklight();
    }
  }

  void disableBacklight() {
    tm12864.setALL(LED_OFF);
  }

  void enableBacklight() {
    if (backlightOnTime > 0) {
      tm12864.setALL(LED_ON);
      if (backlightOnTime < 255) {
        sysclock.cancel(*this);
        set(seconds2ticks(backlightOnTime));
        sysclock.add(*this);
      }
    } else {
      disableBacklight();
    }
  }

  virtual void trigger (__attribute__ ((unused)) AlarmClock& clock) {
    disableBacklight();
  }

  void moveUp() {
    enableBacklight();
    if (cursorPos > 0) {
      cursorPos--;
    } else {
      if (listOffset > 0) {
        listOffset--;
      }
    }
    update();
  }

  void moveDown() {
    enableBacklight();
    if (cursorPos < (CHANNEL_PER_PAGE - 1)) {
      cursorPos++;
    } else {
      if (listOffset < (MAX_FAKEDEVICE_COUNT - CHANNEL_PER_PAGE)) {
        listOffset++;
      }
    }
    update();
  }

  uint8_t selectedChannel() {
    return listOffset+cursorPos;
  }

  void startup(bool hasMaster) {
    tm12864.initLCD();
    display.setDisplayRotation(U8G2_R2);
    enableBacklight();
    display.setFontMode(false);

    const char * title        PROGMEM = "DUMMY-BEACON V2";
    const char * asksinpp     PROGMEM = "AskSin++ V" ASKSIN_PLUS_PLUS_VERSION;
    const char * compiledMsg  PROGMEM = "compiled on";
    const char * compiledDate PROGMEM = __DATE__ " " __TIME__;
    const char * noMaster     PROGMEM = "-keine Zentrale-";
    const char * numChannel   PROGMEM = TOSTR(MAX_FAKEDEVICE_COUNT) " Ch.";

    display.setFont(u8g2_font_helvB08_tr);
    display.setCursor(tm12864.centerPosition(title), 10);
    display.print(title);

    display.setFont(u8g2_font_helvR08_tr);
    display.setCursor(tm12864.centerPosition(numChannel), 20);
    display.print(numChannel);

    display.setCursor(tm12864.centerPosition(asksinpp), 30);
    display.print(asksinpp);

    display.setFont(u8g2_font_profont10_mf);
    if (hasMaster == false) {
      display.setCursor(tm12864.centerPosition(noMaster), 42);
      display.print(noMaster);
    }

    display.setCursor(tm12864.centerPosition(compiledMsg), 54);
    display.print(compiledMsg);
    display.setCursor(tm12864.centerPosition(compiledDate), 64);
    display.print(compiledDate);
    display.sendBuffer();
  }

  void update() {
    display.clearBuffer();

    display.setFont(u8g2_font_5x8_tr);
    display.setCursor(11,6);
    display.print(" E  ADDR | VAL |  NEXT");
    display.setCursor(82,6);
    display.print('b');
    display.drawLine(0, 7, display.getWidth(), 7);

    for (uint8_t devIdx = 0; devIdx < 8; devIdx++) {
      uint8_t y = ((devIdx+1)*7)+8;
      if (fakeDevice[devIdx + listOffset].Enabled == false)
        display.drawFrame(15, ((devIdx+1)*7)+2, 6, 6);
      else
        display.drawBox(15, ((devIdx+1)*7)+2, 6, 6);

      HMID fid = fakeDevice[devIdx + listOffset].FakeDeviceID;
      display.setCursor(26,y);
      if (fid != HMID::broadcast) {
        if (fid.id0()<10) display.print('0');
        display.print(String(fid.id0(),HEX));
        if (fid.id1()<10) display.print('0');
        display.print(String(fid.id1(),HEX));
        if (fid.id2()<10) display.print('0');
        display.print(String(fid.id2(),HEX));

        display.setCursor(56,y);
        display.print('|');
        display.setCursor(65,y);
        uint8_t statusvalue = fakeDevice[devIdx + listOffset].StatusValue;
        for (uint8_t i = 0; i < 3 - numdigits(statusvalue); i++) { display.print(" "); }
        display.print(statusvalue);

        if (fakeDevice[devIdx + listOffset].Broadcast)
          display.drawBox(82, y-5, 4, 4);

        display.setCursor(86,y);
        display.print('|');
        display.setCursor(92,y);
        uint32_t rest = fakeDevice[devIdx + listOffset].CyclicTimeout - fakeDevice[devIdx + listOffset].CurrentTick;
        for (uint8_t i = 0; i < 7 - numdigits(rest); i++) { display.print(" "); }
        display.print(rest);
      } else {
        display.print("------");
      }

      display.setCursor(4,y);
      uint8_t ch = devIdx + listOffset + 1;
      if (ch < 10) display.print(" ");
      display.print(ch);
    }

    display.setCursor(0,((cursorPos+1)*7)+8);
    display.print(">");
    display.sendBuffer();
  }

} LCDDisplay;
#endif

DEFREGISTER(UReg0, MASTERID_REGS, DREG_TRANSMITTRYMAX, DREG_BACKONTIME)
class DevList0 : public RegList0<UReg0> {
  public:
    DevList0 (uint16_t addr) : RegList0<UReg0>(addr) {}
    void defaults () {
      clear();
      transmitDevTryMax(2);
      backOnTime(4);
    }
};

DEFREGISTER(UReg1, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08)
class UList1 : public RegList1<UReg1> {
  public:
    UList1 (uint32_t addr) : RegList1<UReg1>(addr) {}

    bool ADDRESS_SENDER_HIGH_BYTE(uint8_t v) const { return this->writeRegister(0x01,v); }
    uint8_t ADDRESS_SENDER_HIGH_BYTE() const { return this->readRegister(0x01,0); }
  
    bool ADDRESS_SENDER_MID_BYTE(uint8_t v) const { return this->writeRegister(0x02,v); }
    uint8_t ADDRESS_SENDER_MID_BYTE() const { return this->readRegister(0x02,0); }

    bool ADDRESS_SENDER_LOW_BYTE(uint8_t v) const { return this->writeRegister(0x03,v); }
    uint8_t ADDRESS_SENDER_LOW_BYTE() const { return this->readRegister(0x03,0); }
    
    bool CyclicTimeout (uint32_t value) const {
      return this->writeRegister(0x04, (value >> 16) & 0xff) && this->writeRegister(0x05, (value >> 8) & 0xff) && this->writeRegister(0x06, value & 0xff);
    }
    uint32_t CyclicTimeout () const {
      return ((uint32_t)this->readRegister(0x04, 0) << 16) | ((uint32_t)this->readRegister(0x05, 0) << 8) | (uint32_t)this->readRegister(0x06, 0);
    }
    
    bool Broadcast () const {
      return this->readBit(0x07, 1, true);
    }
    bool Broadcast (bool v) const {
      return this->writeBit(0x07, 1, v);
    }

    bool StatusValue (uint8_t value) const {
      return this->writeRegister(0x08, value & 0xff);
    }
    uint8_t StatusValue () const {
      return this->readRegister(0x08, 0);
    }

    void defaults () {
      clear();
      ADDRESS_SENDER_HIGH_BYTE(0);
      ADDRESS_SENDER_MID_BYTE(0);
      ADDRESS_SENDER_LOW_BYTE(0);
      CyclicTimeout(0);
      //FakeDeviceEnabled(false);
      StatusValue(0);
      Broadcast(true);
    }
};

class FakeChannel : public Channel<Hal, UList1, EmptyList, List4, PEERS_PER_CHANNEL, DevList0>, public Alarm {
  private:
    bool      _last_enabled;
    uint8_t   _status;

  public:

    FakeChannel () : Channel(), Alarm(0), _last_enabled(false), _status(0) {}
    virtual ~FakeChannel () {}

    bool process (__attribute__ ((unused)) const RemoteEventMsg& msg) {
      return false;
    }

    bool process (const ActionSetMsg& msg) {
      setEnabled((msg.value() == 200));
      return false;
    }

    bool process (__attribute__ ((unused)) const ActionCommandMsg& msg) {
      return false;
    }
    
    virtual void trigger (__attribute__ ((unused)) AlarmClock& clock) {
      Alarm::set(seconds2ticks(1));

      uint8_t devIdx = number() - 1;

      HMID fakeDevId = fakeDevice[devIdx].FakeDeviceID;

      if (fakeDevice[devIdx].Enabled == true) {
        if (fakeDevId.valid()) {
          if (fakeDevice[devIdx].CyclicTimeout > 0) {
            fakeDevice[devIdx].CurrentTick++;
            if (fakeDevice[devIdx].CurrentTick >= fakeDevice[devIdx].CyclicTimeout) {
              DPRINT(F("SEND MSG FOR DEV ")); fakeDevId.dump(); DPRINTLN("");
              setStatus(this->getList1().StatusValue());
              device().sendFakeInfoActuatorStatus(fakeDevId, device().nextcount(), *this, fakeDevice[devIdx].Broadcast);
              setStatus(fakeDevice[devIdx].Enabled ? 200 : 0);
              fakeDevice[devIdx].CurrentTick = 0;
              _delay_ms(200);
            }
          }
        } else {
          DPRINTLN(F("Device ID is invalid; disabling"));
          _delay_ms(1000);
          setEnabled(false);
        }
      }

      if (_last_enabled != fakeDevice[devIdx].Enabled) {
        if (fakeDevice[devIdx].Enabled == false)
          setEnabled(false);
        _last_enabled = fakeDevice[devIdx].Enabled;
      }

#ifdef USE_DISPLAY
      if (device().getList0().masterid().valid() == true) LCDDisplay.update();
#endif
      sysclock.add(*this);
    }

    void setEnabled(bool en, bool saveOnly = false) {
      if (number() > 0) {
        HMID fdevID = fakeDevice[number() - 1].FakeDeviceID;
        if (fdevID.valid() == false) en = false;
        //set Enabled in fakeDevice
        fakeDevice[number() - 1].Enabled = en;

        //save Enabled in EEPROM
        StorageConfig sc = device().getConfigArea();
        uint8_t cfgByteOffset = CFG_BYTE_OFFSET + (number() - 1) / 8;
        uint8_t bitPosition = (number() - 1) % 8;
        uint8_t cfgByte = sc.getByte(cfgByteOffset);
        if (en == true) bitSet(cfgByte, bitPosition); else bitClear(cfgByte, bitPosition);
        sc.setByte(cfgByteOffset, cfgByte);

        if (saveOnly == false) {
          //send state to ccu
          setStatus(en ? 200:0);
          changed(true);

          //send first fake state 5 seconds after enabling
          if (en) fakeDevice[number() - 1].CurrentTick = fakeDevice[number() - 1].CyclicTimeout - 5;
        }
        DPRINT("set FakeDeviceEnabled #");DDEC(number()-1);DPRINT(" is  ");DDEC(en);DPRINT(", with CFG BYTE OFFSET ");DDEC(cfgByteOffset);DPRINT(" on bit ");DDECLN(bitPosition);
      }
    }

    bool getEnabled() {
      if (number() > 0) {
        StorageConfig sc = device().getConfigArea();
        uint8_t cfgByteOffset = CFG_BYTE_OFFSET + (number() - 1) / 8;
        uint8_t bitPosition = (number() - 1) % 8;

        bool en = bitRead(sc.getByte(cfgByteOffset), bitPosition);
        DPRINT("get FakeDeviceEnabled #");DDEC(number()-1);DPRINT(" is  ");DDEC(en);DPRINT(", with CFG BYTE OFFSET ");DDEC(cfgByteOffset);DPRINT(" on bit ");DDECLN(bitPosition);
        return en;
      }
      return false;
    }

    void configChanged() {
      HMID fdev = {this->getList1().ADDRESS_SENDER_HIGH_BYTE(), this->getList1().ADDRESS_SENDER_MID_BYTE(), this->getList1().ADDRESS_SENDER_LOW_BYTE()}; 
      uint32_t timeout = this->getList1().CyclicTimeout();
      bool en = getEnabled();
      uint8_t sval = this->getList1().StatusValue();
      bool bcast = this->getList1().Broadcast();
      //DPRINT(F("ch ")); DDEC(number()); DPRINT(F(", FakeDeviceID ")); fdev.dump(); DPRINT(F(", CyclicTimeout ")); DDEC(timeout); DPRINT(F(", Enabled ")); DDEC(en);DPRINT(F(", Broadcast ")); DDEC(bcast); DPRINT(F(", StatusValue ")); DDECLN(sval);
      
      fakeDevice[number() - 1].FakeDeviceID  = fdev;
      fakeDevice[number() - 1].CyclicTimeout = timeout;
      fakeDevice[number() - 1].CurrentTick   = 0;//(timeout > 10) ? timeout - 10 : 0;
      setEnabled(en, true);
      fakeDevice[number() - 1].Broadcast     = bcast;
      fakeDevice[number() - 1].StatusValue   = sval;
    }

    void setup(Device<Hal, DevList0>* dev, uint8_t number, uint16_t addr) {
      Channel::setup(dev,number,addr);
      sysclock.add(*this);
    }

    void setStatus(uint8_t s) { _status = s; }

    uint8_t status () const { return _status; }

    uint8_t flags () const { return 0; }
};


class TDummyDevice : public MultiChannelDevice<Hal, FakeChannel, MAX_FAKEDEVICE_COUNT, DevList0> {
public:
  typedef MultiChannelDevice<Hal, FakeChannel, MAX_FAKEDEVICE_COUNT, DevList0> TSDevice;
  TDummyDevice(const DeviceInfo& info, uint16_t addr) : TSDevice(info, addr) {}
  virtual ~TDummyDevice () {}

  virtual void configChanged () {
    TSDevice::configChanged();
#ifdef USE_DISPLAY
    uint8_t bOn = this->getList0().backOnTime();
    //DPRINT(F("*LCD Backlight Ontime : ")); DDECLN(bOn);
    LCDDisplay.setBackLightOnTime(bOn);
#endif
  }
};
TDummyDevice sdev(devinfo, 0x20);


#ifdef USE_DISPLAY
class ConfBtn : public ConfigButton<TDummyDevice>  {
public:
  ConfBtn (TDummyDevice& i) : ConfigButton(i) {}
  virtual ~ConfBtn () {}

  virtual void state (uint8_t s) {
    uint8_t old = ButtonType::state();
    ButtonType::state(s);
    if (s == ButtonType::pressed) {
      LCDDisplay.enableBacklight();
    }
    else if (s == ButtonType::released) {
      LCDDisplay.enableBacklight();
      uint8_t cp = LCDDisplay.selectedChannel();
      sdev.channel(cp+1).setEnabled(!fakeDevice[cp].Enabled);
    }
    else if( s == ButtonType::longreleased ) {
      LCDDisplay.enableBacklight();
      sdev.startPairing();
    }
    else if( s == ButtonType::longpressed ) {
      if( old == ButtonType::longpressed ) {
        sdev.reset();
      }
      else {
        sdev.led().set(LedStates::key_long);
      }
    }
  }
};
ConfBtn cfgBtn(sdev);
#else
ConfigButton<TDummyDevice> cfgBtn(sdev);
#endif

#ifdef USE_DISPLAY
class RotaryEncoder : public BaseEncoder {
public:
  RotaryEncoder () : BaseEncoder() {};
  virtual ~RotaryEncoder () {};

  void init (uint8_t cpin,uint8_t dpin) {
    BaseEncoder::init(cpin,dpin);
  }
  void process () {
    int8_t dx = read();
    if( dx != 0 ) {
      if (dx > 0) {
        LCDDisplay.moveDown();
      } else {
        LCDDisplay.moveUp();
      }
    }
  }
} enc;
#endif

void setup () {
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
  
  sdev.init(hal);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  sdev.initDone();

  bool hasMaster = (sdev.getList0().masterid().valid() == true);

#ifdef USE_DISPLAY
  LCDDisplay.startup(hasMaster);
  encoderISR(enc, ROTARY_ENC_C_PIN, ROTARY_ENC_D_PIN);
  _delay_ms(2000);
#endif
  if (hasMaster) {
    for (uint8_t i = 0; i < MAX_FAKEDEVICE_COUNT; i++) {
      sdev.channel(i+1).setStatus(fakeDevice[i].Enabled ? 200 : 0);
      sdev.channel(i+1).changed(true);
    }
  }
#ifdef USE_DISPLAY
  LCDDisplay.update();
#endif
}

void loop() {
#ifdef USE_DISPLAY
  enc.process();
#endif
  hal.runready();
  sdev.pollRadio();
}
