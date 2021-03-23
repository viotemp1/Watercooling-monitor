#include <MicroView.h>
#include <TimeAlarms.h>
#include <Adafruit_AMG88xx.h>
#include <avr/wdt.h>
#include <EEPROM.h>

#define buzzerPin 5
#define AMG_ROWS 8
#define AMG_COLS 8


Adafruit_AMG88xx amg;
AlarmId buzzer_AlarmId;

struct mySettings_struct {
  bool e_initialized;
  bool debug;
  bool buzzer_on;
  bool invert_res;
  uint8_t max_error;
};

mySettings_struct mySettings = {
  true, //EEPROM initialized
  true, //debug
  false, // buzzer on
  true, //invert result
  20, // max error percent
};

float amg_pixels[AMG_COLS * AMG_ROWS];
float amg_cal_value = 0;
float amg_avg_value = 0;
float temp_diff = 0;
uint16_t displayPixelWidth, displayPixelHeight;
bool serial_available = false;
String serialInputString = "";
bool serialComplete = false;
bool calibration_in_progress = false;
float ambient_temp = 30;

void myPrint(String text, bool new_line = true, bool force = false) {
  if (mySettings.debug && serial_available && !force ) {
    if (new_line) {
      Serial.println(text);
    }
    else {
      Serial.print(text);
    }
  }
  if (force && serial_available) {
    if (new_line) {
      Serial.println(text);
    }
    else {
      Serial.print(text);
    }
  }
}

void write_Settings(mySettings_struct mySettings_var) {
  EEPROM.put(0, mySettings_var);
  EEPROM.get(0, mySettings);
  myPrint("mySettings: e_initialized:" + String(mySettings.e_initialized) + "/debug:" + String(mySettings.debug) + \
          "/buzzer_on:" + String(mySettings.buzzer_on) + "/invert_res:" + String(mySettings.invert_res) + \
          "/max_error:" + String(mySettings.max_error));
}


void substract_ambient(float *pixels) {
  ambient_temp = amg.readThermistor();
  for (uint8_t i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++) {
    pixels[i] = pixels[i] - ambient_temp;
  }
}

float temp_diff_fn (float vsensorAverage, float vsensorCalAverage) {
  float result = 0;
  if (mySettings.invert_res) {
    result = float(vsensorAverage) - float(vsensorCalAverage);
  }
  else {
    result = float(vsensorCalAverage) - float(vsensorAverage);
  }
  return result;
}

void check_error(float vtemp_diff) {
  if (vtemp_diff <- mySettings.max_error) {
    // ((abs(vtemp_diff) > 100 + mySettings.max_error) ||
    Alarm.enable(buzzer_AlarmId);
  }
  else {
    Alarm.disable(buzzer_AlarmId);
  }
}
float average(float *pixels, int count_pixels) {
  float result = 0;
  for (uint8_t i = 0; i < count_pixels; i++) {
    result += pixels[i];
  }
  result /= count_pixels;
  return result;
}

float calibrate_fn(uint8_t no_calibrations = 10) {
  float result = 0;
  calibration_in_progress = true;
  for (uint8_t i = 0; i < no_calibrations; i++) {
    amg.readPixels(amg_pixels);
    substract_ambient(amg_pixels);
    result += average(amg_pixels, AMG88xx_PIXEL_ARRAY_SIZE);
    Alarm.delay(10);
  }
  result /= no_calibrations;
  calibration_in_progress = false;
  return result;
}

void reboot() {
  wdt_disable();
  wdt_enable(WDTO_15MS);
  while (1) {}
}

void buzzer_Alarm_fn() {
  digitalWrite(buzzerPin, HIGH);
  Alarm.delay(500);
  digitalWrite(buzzerPin, LOW);
}

void myTimer() {
  if (!calibration_in_progress) {
    amg.readPixels(amg_pixels);
    substract_ambient(amg_pixels);
    amg_avg_value = average(amg_pixels, AMG88xx_PIXEL_ARRAY_SIZE);
    temp_diff = temp_diff_fn(amg_avg_value, amg_cal_value);

    if (mySettings.buzzer_on) {
      check_error(temp_diff);
    }

    myPrint("sensorAverage: " + String(amg_avg_value) + "/" + String(amg_cal_value) + "/" + String(temp_diff) + " - ambient: " + String(ambient_temp));

    uView.clear(PAGE);
    for (int i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++) {
      uint8_t colorIndex = 0;
      if (amg_pixels[i] >= amg_cal_value) {
        colorIndex = 1;
      }
      uView.rectFill(displayPixelWidth * (i % 8), displayPixelHeight * floor(i / 8),
                     displayPixelWidth, displayPixelHeight, colorIndex, NORM);
    }
    uView.display();
    if ( serialComplete ) {
      myPrint(String(temp_diff), true, true);
      serialComplete = false;
      serialInputString = "";
    }
  }

  wdt_reset();
}

void serialEvent() {
  if (serial_available) {
    while (Serial.available()) {
      char inChar = (char)Serial.read();
      serialInputString += inChar;
      if (inChar == '\n') {
        serialComplete = true;
      }
    }
  }
}

void setup() {
  Serial.begin(115200, SERIAL_8N1);
  if (Serial) {
    serial_available = true;
  }
  if (!amg.begin()) {
    Serial.println("Could not find a valid AMG88xx sensor, check wiring!");
    while (1) {
      delay(1);
    }
  }

  EEPROM.get(0, mySettings);
  if (String(mySettings.e_initialized) == "255") {
    myPrint("EEPROM not initialized. Writing delault values");
    mySettings = {
      true, //EEPROM initialized
      true, //debug
      false, // buzzer on
      true, //invert result
      15, // max error percent
    };
    write_Settings(mySettings);
  }

  myPrint("Started");

  if (mySettings.buzzer_on) {
    pinMode(buzzerPin, OUTPUT);
    buzzer_AlarmId = Alarm.timerRepeat(1, buzzer_Alarm_fn);
    Alarm.delay(500);
    Alarm.disable(buzzer_AlarmId);
    buzzer_Alarm_fn();
  }

  displayPixelWidth =  uView.getLCDWidth() / 8;
  displayPixelHeight = uView.getLCDHeight() / 8;
  uView.begin();
  uView.clear(ALL);
  uView.clear(PAGE);
  uView.setCursor(20, 20);
  uView.print("Init...");
  uView.display();

  amg_cal_value = calibrate_fn(10);

  uView.clear(PAGE);
  uView.setCursor(5, 0);
  uView.print("Cal. done");
  uView.setCursor(12, 20);
  uView.print(String(amg_cal_value));
  uView.setCursor(15, 40);
  uView.print("*by vio*");
  uView.display();
  Alarm.delay(5000);
  uView.clear(PAGE);

  wdt_disable();
  Alarm.timerRepeat(1, myTimer);
  wdt_reset();
  wdt_enable(WDTO_8S);
}

void loop() {

  if (Serial) {
    serial_available = true;
  }
  else {
    serial_available = false;
  }
  if (serialComplete && serialInputString[0] == 'r') { //read
    myPrint("read");
    myTimer();
  }
  else if (serialComplete && serialInputString[0] == 'p') { //print
    myTimer();
    myPrint("sensorAverage: " + String(amg_avg_value) + "/" + String(amg_cal_value) + " - wl: " + String(temp_diff) + " - ambient: " + String(ambient_temp), true, true);
  }
  else if (serialComplete && serialInputString[0] == 's') { //reboot
    reboot();
  }
  else if (serialComplete && serialInputString[0] == 'c') { //calibrare
    amg_cal_value = calibrate_fn(10);
    myPrint("Calibration: " + String(amg_cal_value), true, true);
    myTimer();
  }
  else if (serialComplete && serialInputString[0] == 'i') { //invert
    mySettings.invert_res = !mySettings.invert_res;
    write_Settings(mySettings);
    myTimer();
  }
  else if (serialComplete && serialInputString[0] == 'b') { //buzzer
    mySettings.buzzer_on = !mySettings.buzzer_on;
    if (mySettings.buzzer_on) {
      Alarm.enable(buzzer_AlarmId);
    }
    else {
      Alarm.disable(buzzer_AlarmId);
    }
    write_Settings(mySettings);
    myTimer();
    reboot();
  }
  else if (serialComplete && serialInputString[0] == 'd') { //debug
    mySettings.debug = !mySettings.debug;
    write_Settings(mySettings);
    myTimer();
  }
  else if (serialComplete && serialInputString[0] == 'e') { //error
    String serialInputString1 = serialInputString;
    serialInputString1.replace("e", "");
    mySettings.max_error = serialInputString1.toInt();
    write_Settings(mySettings);
    myTimer();
  }
  else if (serialComplete && serialInputString[0] == 'q') { //query
    myPrint("r: read / c: calibrate / i: invert / b: buzzer / d: debug / e: error / s: reboot / p: print", true, true);
    myPrint("mySettings: e_initialized:" + String(mySettings.e_initialized) + "/debug:" + String(mySettings.debug) + \
            "/buzzer_on:" + String(mySettings.buzzer_on) + "/invert_res:" + String(mySettings.invert_res) + \
            "/max_error:" + String(mySettings.max_error), true, true);
    myTimer();
  }

  Alarm.delay(100);
}
