/*
 * Alarm: Monitors a voltage source and triggers an alarm if it's greater that a value
 *        for more than a configured percent of a configured time.
 * 
 * See the readme for documentation (https://github.com/zippy/sound_alarm/blob/master/README.md)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#define DEBUG    1
#include <Debug.h>
#include <Wire.h>
#include <Bounce.h>
#include <LiquidCrystal.h>
#include <IRremote.h>

const int potPin = 1;
const int buttonPin = 4;
const int irPin = 3;
const int soundPin = 0;

#define SETUP_STATE 0
#define NORMAL_STATE 1
#define ALARM_STATE 2
#define AUTOLEVEL_STATE 3
#define CALIBRATE_RANGE_STATE 4
#define GET_PARAM_STATE 5

#define SECOND 1000

int state = NORMAL_STATE;
unsigned long timeRef;
int ll; // variable to hold that last level displayed

Debug debug;
LiquidCrystal lcd(13, 8, 9, 10, 11, 12);

class Alarm {
public:
  int threshold;
  int level;
  int period;
  int reset_period;
  int range_high; // lowest voltage value expected from digitalRead of soundPin
  int range_low; // highest voltage value expected from digitalRead of soundPin
  int range;
  unsigned long counts_above;
  unsigned long counts_below;
  unsigned long val;
  int alarm_percent;
  unsigned int alarms;
  IRsend irsend;
  boolean auto_adjust;
  void setThreshold(int t);
  void setRange(int h, int l);
  void autoAdjustRange(int val);
  Alarm(void);
};

Alarm::Alarm() {
  auto_adjust = true;
  setRange(800,0);
  setThreshold(400);
  period = SECOND*1;
  reset_period = SECOND*10;
  alarm_percent = 40;
}

void Alarm::setThreshold(int t) {
  threshold = t;
  level = map(t, range_low, range_high, 0, 15);
}

void Alarm::setRange(int h, int l) {
  range_high = h;
  range_low = l;
  range = h-l;
  setThreshold(threshold); // do this to re-map the quantized level to the new range.
}

void Alarm::autoAdjustRange(int val) {
  if (auto_adjust) {
    if (val > range_high) { // we are experiencing something above the usual range, auto adjust the range.
      setRange(val,range_low);
    }
    if (val < range_low) { // we are experiencing something above the usual range, auto adjust the range.
      setRange(range_high,val);
    }
  }
}

// Instantiate the alarm object
Alarm a;


// ***************************************
#define VIEW 1
#define SET 2
const int setup_params = 7;
#define LEVEL_PARAM 0
#define PERIOD_PARAM 1
#define RESET_PARAM 2
#define PERCENT_PARAM 3
#define AUTO_LEVEL_PARAM 4
#define AUTO_ADJUST_PARAM 5
#define CALIBRATE_RANGE_PARAM 6


class Param {
  public:
   virtual void set() {setGetParamState();};
   virtual int updateValue(int v) {};
   virtual void display(int type);
};

class AutoLevelParam : public Param {
  public:
  void set() {setAutolevelState();};
  void display(int ctx) {
    Serial.print("Level-A: ");Serial.println(a.threshold);
    lcd.setCursor(0,1);
    lcd.print("Level-A: ");lcd.print(a.threshold);lcd.print("    ");
    };
};

class CalibrateRangeParam : public Param {
  public:
  void set() {setCalRangeState();};
  void display(int ctx) {
    Serial.print("Calibrate range: "); Serial.print("L=");  Serial.print(a.range_low);Serial.print(" H=");  Serial.println(a.range_high);
    lcd.setCursor(0,1);
    lcd.print("Calibrate range: ");lcd.print("L=");  lcd.print(a.range_low);lcd.print(" H=");  lcd.print(a.range_high);lcd.print("    ");
    };
};

class AutoAdjustParam : public Param {
  public:
  void display(int ctx) {
    Serial.print("Auto range: ");Serial.println(a.auto_adjust ? "On" : "Off");
    lcd.setCursor(0,1);
    lcd.print("Auto range: ");lcd.print((char *) (a.auto_adjust ? "On" : "Off"));lcd.print("    ");
  };
  void set() {a.auto_adjust = !a.auto_adjust; display(VIEW);setSetupState(AUTO_ADJUST_PARAM);};
};

class LevelParam : public Param {
  public:
  void display(int ctx) {
    
    Serial.print("Level-M: ");Serial.println(a.threshold);
    if (ctx == SET) {
      displayLevelForce(0);
    }
    lcd.setCursor(0,1);
    lcd.print("Level-M: ");lcd.print(a.threshold);lcd.print("    ");
  };
  int updateValue(int val) {a.setThreshold(map(val,0,1022,a.range_low,a.range_high));return a.threshold;};
};

class PeriodParam : public Param {
  public:
  void display(int ctx) {
    int p;
    p =  a.period/1000;
    Serial.print("Period: ");Serial.println(p);
    lcd.setCursor(0,1);
    lcd.print("Period: ");lcd.print(p);lcd.print("      ");
  };
  int updateValue(int val) {a.period = map(val,0,1023,1000,30000);return a.period;};
};

class ResetParam : public Param {
  public:
  void display(int ctx) {
    int p;
    p =  a.reset_period/1000;
    Serial.print("Alarm Reset: ");Serial.println(p);
    lcd.setCursor(0,1);
    lcd.print("Alarm Reset: ");lcd.print(p);lcd.print("    ");
  };
  int updateValue(int val) {a.reset_period = map(val,0,1023,1000,30000);return a.reset_period;};
};

class PercentParam : public Param {
  public:
  void display(int ctx) {
    Serial.print("Percent: ");Serial.println(a.alarm_percent);
    lcd.setCursor(0,1);
    lcd.print("Percent: ");lcd.print(a.alarm_percent);lcd.print("      ");
  };
  int updateValue(int val) {a.alarm_percent = map(val,0,1023,0,100);return a.alarm_percent;};
};

LevelParam level_p;
PeriodParam period_p;
ResetParam reset_p;
PercentParam percent_p;
AutoLevelParam auto_level_p;
AutoAdjustParam auto_adjust_p;
CalibrateRangeParam calibrate_range_p;
Param* params[] = {&level_p,&period_p,&reset_p,&percent_p,&auto_level_p,&auto_adjust_p,&calibrate_range_p};

//*******************************

// Instantiate a Bounce object with a 5 millisecond debounce time
Bounce bouncer = Bounce( buttonPin,5 ); 

byte block[8] = {0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f};
byte space[8] = {0x1f,0x0,0x0,0x0,0x0,0x0,0x0,0x1f};
byte bar[8] = {	0x1f,0x4,0x4,0x4,0x4,0x4,0x4,0x1f};
byte bari[8] = {	0x1f,0x1b,0x1b,0x1b,0x1b,0x1b,0x1b,0x1f};
byte lefte[8] = {0x1f,0x10,0x10,0x10,0x10,0x10,0x10,0x1f};
byte righte[8] = {0x1f,0x1,0x1,0x1,0x1,0x1,0x1,0x1f};

void setup() {
  lcd.createChar(1,block);
  lcd.createChar(2,space);
  lcd.createChar(3,bar);
  lcd.createChar(4,bari);
  lcd.createChar(5,lefte);
  lcd.createChar(6,righte);
  lcd.begin(16, 2);
  lcd.print("starting up...");
  pinMode(potPin, INPUT);
  pinMode(buttonPin, INPUT);
  digitalWrite(buttonPin, HIGH); // connect internal pull-up
  pinMode(soundPin, INPUT);
  setNormalState();
  debug.setup();
}

boolean wait_for_rising_edge;

// Our loop just has us execute the current state function except if there was a long press 
// where we don't do anything until the button is released (i.e. we wait for the rising edge)
void loop() {
  bouncer.update();
  if (wait_for_rising_edge) {
    if (bouncer.risingEdge()) {
      wait_for_rising_edge = false;
    }
  }
  else {
    switch (state) {
      case NORMAL_STATE: doNormalState();break;
      case SETUP_STATE: doSetupState();break;
      case ALARM_STATE: doAlarmState();break;
      case AUTOLEVEL_STATE: doAutoLevelState();break;
      case CALIBRATE_RANGE_STATE: doCalRangeState();break;
      case GET_PARAM_STATE: doGetParamState();break;
    }
  }
}

//************************************************************
// Normal mode functions

void setNormalState() {
  lcd.clear();
  timeRef = millis();
  ll = -1;
  if (state == ALARM_STATE) {
    alarmOff();
  }
  lcd.setCursor(0,1);
  lcd.print("Alarms: ");
  lcd.print(a.alarms);
  setState(NORMAL_STATE);
}

void doNormalState() {
  if (longPress()) {
    setSetupState(LEVEL_PARAM);
  }
  else {
    unsigned long tr = millis();
    if ((timeRef + a.period) > tr) {
      int s = analogRead(soundPin);
//debug.msg("VAL = ",s);
      a.autoAdjustRange(s);
      displayLevel(s);      
      a.val += s;
      if (s > a.threshold) {
        a.counts_above++;
      }
      else {
        a.counts_below++;
      }
    }
    else {
      timeRef = tr;
      debug.msg("above = ",a.counts_above);
      debug.msg("below = ",a.counts_below);
      unsigned int total = (a.counts_below+a.counts_above);
      float p = map(a.counts_above, 0, total, 0, 100);
      debug.msgf("percent above = ", p);
      debug.msgf("average = ", (a.val+1.0)/total);
      if (p >= a.alarm_percent) {
        setAlarmState();
      }
      a.counts_above = 0;
      a.counts_below = 0;
      a.val = 0;
    }
  }
}

//************************************************************
// Setup Mode functions
unsigned long pv;
int count;

void initializeParamPot() {
  pv = 0;
  count = 0;
}

#define INITIALIZING 0
#define CHANGED 1
#define SAME 2

int readParamPot(unsigned int &cur_val)
{
  unsigned int p = analogRead(potPin);
  boolean value_changed = false;
  if (count < 1000) {
    pv += p;
    count++;
    cur_val = 0;
    return INITIALIZING;
  }
  else {
    unsigned int avg = pv/count;
    debug.msg("pv:",pv);
    debug.msg("p:",p);
    debug.msg("count:",count);
    debug.msg("avg:",avg);

    if (p != avg) {
      value_changed = true;
    }
    cur_val = avg;
    count = count/2;
    pv = pv/2;
  }
  return value_changed ? CHANGED : SAME;
}


int setup_param;
void setSetupState(int p) {
  setup_param = p;
  lcd.clear();
  displaySetupParam();
  setState(SETUP_STATE);
}

void displaySetupParam() {
  lcd.setCursor(0,0);
  for (int i=0; i<setup_params; i++) {
    lcd.print(i == setup_param ? '+' : '-');
  }
  lcd.setCursor(0,1);
  params[setup_param]->display(VIEW);
}

void doSetupState() {
  if (shortPress()) {
    lcd.clear();
    Serial.println("--Setting--");
    lcd_println("--Setting--");
    params[setup_param]->set();
  }
  else if (longPress()) {
    setNormalState();
  }
  else {
    unsigned int val;
    int result = readParamPot(val);
    if (result == CHANGED) {
      setup_param = map(val, 0, 1023, 0, setup_params);
      displaySetupParam();
    }
  }
}


//************************************************************
// Get param mode functions

void setGetParamState() {
  setState(GET_PARAM_STATE);
  initializeParamPot();
}

void doGetParamState() {
  if (shortPress()) {
    setSetupState(setup_param);
  }
  else {
    unsigned int val;
    int result = readParamPot(val);
    if (result != INITIALIZING) {
      params[setup_param]->updateValue(val);
      if (result == CHANGED) {
        debug.msg("val:",val);
        params[setup_param]->display(SET);
      }
    }
  }
}


//************************************************************
// Autolevel mode functions

void setAutolevelState() {
  setState(AUTOLEVEL_STATE);
//  a.setRange(0,1000); // set opposite range
  a.setThreshold(0); // set level to null
  ll = -1;
}

void doAutoLevelState() {
  if (shortPress()) {
    setSetupState(setup_param);
  }
  else {
    int s = analogRead(soundPin);
    if (s < a.range_low) {
      a.setRange(a.range_high,s);
    }
    if (s > a.threshold) {
      if (s > a.range_high) {
        a.setRange(s,a.range_low);
      }
      a.setThreshold(s);
    }
    displayLevel(s);
  }
}


//************************************************************
// Calibrate range mode functions

void setCalRangeState() {
  setState(CALIBRATE_RANGE_STATE);
  a.setRange(0,1000); // set inverted range
  ll = -1;
}

void doCalRangeState() {
  if (shortPress()) {
    setSetupState(setup_param);
  }
  else {
    int s = analogRead(soundPin);
    if (s < a.range_low) {
      a.setRange(a.range_high,s);
    }
    if (s > a.range_high) {
      a.setRange(s,a.range_low);
      lcd.setCursor(0,1);
      lcd.print("High: ");
      lcd.print(a.range_high);
      lcd.setCursor(0,0);
    }
    displayLevel(s);
  }
}

//************************************************************
// Alarm mode functions

void setAlarmState() {
  lcd.clear();
  setState(ALARM_STATE);
  timeRef = millis();
  a.alarms++;
  alarmOn();
}

void doAlarmState() {
  if (shortPress()) {
    setNormalState();
  }
  else {
    unsigned long tr = millis();
    if (timeRef + a.reset_period <= tr) {
      setNormalState();
    }
    else {
    lcd.setCursor(0, 1);
    lcd.print((a.reset_period - (tr-timeRef))/SECOND);lcd.print("     ");
    }
  }
}

#define IR_ON 0xffb04f
#define IR_OFF 0xfff807
#define IR_FLASH 0xffb24d
#define IR_STROBE 0xFF00FF
#define IR_FADE 0xFF58A7
#define IF_SMOOTH 0xFF30CF

void alarmOn(){
  debug.msg("ALARM!!!");
  lcd.print("Alarm Activated");
  a.irsend.sendNEC(IR_ON, 32);
  delay(40);
  a.irsend.sendNEC(IR_ON, 32);
  delay(40);
  a.irsend.sendNEC(IR_FLASH, 32);
  delay(40);
  a.irsend.sendNEC(IR_FLASH, 32);
}

void alarmOff(){
  debug.msg("Alarm off.");
  a.irsend.sendNEC(IR_OFF, 32);
  delay(40);
  a.irsend.sendNEC(IR_OFF, 32);
  delay(40);

}

//************************************************************
// Functions to switch operational modes

void setState(int new_state) {
//  debug.msg("NEW STATE:", new_state);
  state = new_state;
}

//************************************************************
// Functions to handle the buttons

boolean shortPress() {
  return bouncer.risingEdge();
}

boolean longPress() {
  boolean down = bouncer.read() == LOW;
  if (down && (bouncer.duration() > 1000)) {
    wait_for_rising_edge = true;
  }
  return wait_for_rising_edge;
}


//************************************************************
// LCD display functions

void displayLevelForce(int val) {
  ll = -1;
  displayLevel(val);
}

void displayLevel(int val) {
  int level = map(val, a.range_low, a.range_high, 1, 16);
  if (level != ll) {
    drawLevel(level,val);
    ll = level;
  }
}

char level_meter[17];
char lcd_level_meter[17];

void drawLevel(int level,int val) {
  level--;
  lcd.setCursor(0, 0);

  for (int i=0; i<level; i++) {
    level_meter[i] = '#';
    lcd_level_meter[i] = 1;
    //    lcd.write((uint8_t)0);
//    Serial.print("#");
  }
  for (int i=level; i<16; i++) {
    level_meter[i] = '_';
    lcd_level_meter[i] = 2;
    //    lcd.write(" ");
//    Serial.print("_");
  }
  level_meter[a.level] = '|';
  lcd_level_meter[a.level] = (level > a.level) ? 4 : 3;  
  level_meter[16] = 0;
  lcd_level_meter[16] = 0;
  if (lcd_level_meter[15] == 2) {
    lcd_level_meter[15] = 6;
  }
  if (lcd_level_meter[0] == 2) {
    lcd_level_meter[0] = 5;
  }

  Serial.println(level_meter);
  Serial.print("Low:");  Serial.print(a.range_low);
  Serial.print(" Hi:");  Serial.print(a.range_high);
  Serial.print(" L:");  Serial.print(a.level);
  Serial.print(" Cur:");   Serial.println(val);
  lcd.print(lcd_level_meter);
}

void lcd_println(char *s) {
  lcd.print(s);
  lcd.setCursor(0, 1);
}

void lcd_println(int v) {
  lcd.print(v);
  lcd.setCursor(0, 1);
}

//************************************************************
// IR functions

