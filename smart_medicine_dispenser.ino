#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WebServer.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RTC_DS3231 rtc;
WebServer server(80);

// --- Pin Maps ---
const int SERVO_PIN   = 27;
const int BUZZER_PIN  = 23;
const int BTN_OK_PIN  = 13; 
const int BTN_NXT_PIN = 12; 
const int BTN_UP_PIN  = 14; 

// --- PWM Configurations ---
// PWM_CHANNEL removed — Core v3.x uses pin-based API
const int PWM_FREQ = 50;
const int PWM_RES  = 16;

// --- State Engine Defs ---
enum MenuState { WATCH_MODE, SET_H1, SET_H2, SET_M1, SET_M2, ALARM_ACTIVE };
MenuState appState = WATCH_MODE;

const String STATE_PENDING   = "PENDING";
const String STATE_RUNNING   = "DISPENSING";
const String STATE_COMPLETED = "COMPLETED";

String t1State = STATE_PENDING;
String t2State = STATE_PENDING;

int activeTimerIndex = 0; 
int alarmDig[2][4] = {
  {0, 8, 3, 0},
  {1, 4, 0, 0}
};

int activeAlarmTriggered = 0; 
unsigned long okPressTimer = 0;
bool lastOkState  = HIGH;
bool lastNxtState = HIGH;
bool lastUpState  = HIGH;
const int LONG_PRESS_DELAY = 2000;

int getHour(int timerIdx) { return (alarmDig[timerIdx][0] * 10) + alarmDig[timerIdx][1]; }
int getMin(int timerIdx)  { return (alarmDig[timerIdx][2] * 10) + alarmDig[timerIdx][3]; }

void handleStatus() {
  DateTime now = rtc.now();
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  
  int h1 = getHour(0); int m1 = getMin(0);
  int h2 = getHour(1); int m2 = getMin(1);
  
  String json = "{";
  json += "\"time\":\"" + String(now.hour() < 10 ? "0" : "") + String(now.hour()) + ":" + String(now.minute() < 10 ? "0" : "") + String(now.minute()) + ":" + String(now.second() < 10 ? "0" : "") + String(now.second()) + "\",";
  json += "\"isRinging\":" + String((appState == ALARM_ACTIVE) ? "true" : "false") + ",";
  json += "\"activeAlarm\":" + String(activeAlarmTriggered) + ",";
  json += "\"t1\":\"" + String(h1 < 10 ? "0" : "") + String(h1) + ":" + String(m1 < 10 ? "0" : "") + String(m1) + String(h1 >= 12 ? " PM\"" : " AM\"") + ",";
  json += "\"t2\":\"" + String(h2 < 10 ? "0" : "") + String(h2) + ":" + String(m2 < 10 ? "0" : "") + String(m2) + String(h2 >= 12 ? " PM\"" : " AM\"") + ",";
  json += "\"t1Stat\":\"" + t1State + "\",";
  json += "\"t2Stat\":\"" + t2State + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Med-Reminder Status Monitor</title>";
  html += "<style>body{font-family:-apple-system,sans-serif; text-align:center; padding:20px; background:#f4f6f9; margin:0;}.card{background:white; padding:20px; margin:15px auto; max-width:400px; border-radius:16px; box-shadow:0 4px 12px rgba(0,0,0,0.08); position:relative;}#timeBox{font-size:2.4em; font-weight:bold; color:#007aff;}#statusBanner{color:white; padding:12px; border-radius:20px; font-size:1.1em; margin-bottom:15px; display:block; font-weight:bold;}.system-ok{background:#34c759;}.system-alert{background:#ff3b30;}.badge{display:inline-block; padding:4px 10px; border-radius:12px; font-size:0.85em; font-weight:bold; float:right;}.bg-pending{background:#ff9500; color:white;}.bg-run{background:#ff3b30; color:white;}.bg-done{background:#34c759; color:white;}</style></head><body>";
  html += "<h1>Med-Box Event Tracker</h1><div id='statusBanner' class='system-ok'>🟢 SYSTEM ACTIVE</div>";
  html += "<div class='card'><h2>System Time</h2><div id='timeBox'>00:00:00</div></div>";
  html += "<div class='card'><h2>1st Medication (90&deg;) <span id='b1' class='badge bg-pending'>PENDING</span></h2><h3 id='t1Box'>--:-- --</h3></div>";
  html += "<div class='card'><h2>2nd Medication (180&deg;) <span id='b2' class='badge bg-pending'>PENDING</span></h2><h3 id='t2Box'>--:-- --</h3></div>";
  html += "<script>setInterval(function() { fetch('/status').then(function(res){ return res.json(); }).then(function(data) {";
  html += "  document.getElementById('timeBox').innerText = data.time;";
  html += "  document.getElementById('t1Box').innerText = data.t1;";
  html += "  document.getElementById('t2Box').innerText = data.t2;";
  html += "  const b1 = document.getElementById('b1'); const b2 = document.getElementById('b2'); const banner = document.getElementById('statusBanner');";
  html += "  b1.innerText = data.t1Stat; b1.className = 'badge ' + (data.t1Stat=='PENDING'?'bg-pending':data.t1Stat=='COMPLETED'?'bg-done':'bg-run');";
  html += "  b2.innerText = data.t2Stat; b2.className = 'badge ' + (data.t2Stat=='PENDING'?'bg-pending':data.t2Stat=='COMPLETED'?'bg-done':'bg-run');";
  html += "  if(data.isRinging) { banner.innerText = '🚨 DISPENSING RUNNING'; banner.className = 'statusBanner system-alert'; }";
  html += "  else { banner.innerText = '🟢 SYSTEM ACTIVE'; banner.className = 'statusBanner system-ok'; }";
  html += "}); }, 1000);</script></body></html>";
  server.send(200, "text/html", html);
}

void commandServoPosition(int angleTarget);
void silenceAlarmSequence();
void quickBeep(int durationMs);
void executeCustomDispenserCycle();
void readHardwareInputs();
void renderWatchDisplay(DateTime now);
void renderMenuSettings();
void validateTimerOrder();

void setup() {
  Serial.begin(115200);
  pinMode(BTN_OK_PIN,  INPUT_PULLUP);
  pinMode(BTN_NXT_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN,  INPUT_PULLUP);
  pinMode(BUZZER_PIN,  OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(21, 22);
  Wire.setClock(400000);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  rtc.begin();
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // FIXED: New Core v3.x API — no channel number, just pin
  ledcAttach(SERVO_PIN, PWM_FREQ, PWM_RES);
  commandServoPosition(0);

  WiFi.softAP("MedReminder-PoC", "12345678"); 
  server.on("/", handleRoot);
  server.on("/status", handleStatus); 
  server.begin();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("AP Node Ready");
  display.setCursor(10, 40);
  display.println("URL: 192.168.4.1");
  display.display();
  delay(2000);
}

void loop() {
  DateTime now = rtc.now();
  server.handleClient();
  readHardwareInputs();

  if (now.second() == 0 && appState != ALARM_ACTIVE) {
    int h = now.hour(); int m = now.minute();
    if (h == getHour(0) && m == getMin(0) && t1State == STATE_PENDING) {
      activeAlarmTriggered = 1; appState = ALARM_ACTIVE; t1State = STATE_RUNNING;
    } else if (h == getHour(1) && m == getMin(1) && t2State == STATE_PENDING) {
      activeAlarmTriggered = 2; appState = ALARM_ACTIVE; t2State = STATE_RUNNING;
    }
  }

  if (now.hour() == 0 && now.minute() == 0) {
    t1State = STATE_PENDING; t2State = STATE_PENDING;
  }

  display.clearDisplay();
  if (appState == WATCH_MODE)       renderWatchDisplay(now);
  else if (appState == ALARM_ACTIVE) executeCustomDispenserCycle();
  else                               renderMenuSettings();
  display.display();
  delay(20);
}

void readHardwareInputs() {
  bool currentOk  = (digitalRead(BTN_OK_PIN)  == LOW);
  bool currentNxt = (digitalRead(BTN_NXT_PIN) == LOW);
  bool currentUp  = (digitalRead(BTN_UP_PIN)  == LOW);

  if (currentOk && currentUp && (lastOkState == HIGH || lastUpState == HIGH)) {
    activeTimerIndex = (activeTimerIndex == 0) ? 1 : 0;
    quickBeep(300);
    display.clearDisplay(); display.setTextSize(2); display.setCursor(10, 25);
    display.printf("EDIT T%d", activeTimerIndex + 1); display.display();
    delay(1000);
    lastOkState = LOW; lastUpState = LOW;
    return;
  }

  if (currentOk && !currentUp && lastOkState == HIGH) {
    okPressTimer = millis();
    lastOkState = LOW;
  } 
  else if (!currentOk && lastOkState == LOW) {
    long duration = millis() - okPressTimer;
    if (appState == ALARM_ACTIVE) {
      silenceAlarmSequence();
    } else if (duration >= LONG_PRESS_DELAY) {
      if (appState != WATCH_MODE) {
        validateTimerOrder();
        t1State = STATE_PENDING; t2State = STATE_PENDING;
      }
      appState = (appState == WATCH_MODE) ? SET_H1 : WATCH_MODE;
      quickBeep(200);
    }
    lastOkState = HIGH;
  }

  if (appState != WATCH_MODE && appState != ALARM_ACTIVE) {
    if (currentNxt && !currentOk && lastNxtState == HIGH) {
      if      (appState == SET_H1) appState = SET_H2;
      else if (appState == SET_H2) appState = SET_M1;
      else if (appState == SET_M1) appState = SET_M2;
      else if (appState == SET_M2) appState = SET_H1;
      quickBeep(50); lastNxtState = LOW;
    } 
    else if (!currentNxt) { lastNxtState = HIGH; }

    if (currentUp && !currentOk && lastUpState == HIGH) {
      int idx = (appState == SET_H1)?0 : (appState == SET_H2)?1 : (appState == SET_M1)?2 : 3;
      alarmDig[activeTimerIndex][idx]++;
      if (appState == SET_H1 && alarmDig[activeTimerIndex][idx] > 2) alarmDig[activeTimerIndex][idx] = 0;
      if (appState == SET_H2) {
        if (alarmDig[activeTimerIndex][0] == 2 && alarmDig[activeTimerIndex][idx] > 3) alarmDig[activeTimerIndex][idx] = 0;
        else if (alarmDig[activeTimerIndex][0] < 2 && alarmDig[activeTimerIndex][idx] > 9) alarmDig[activeTimerIndex][idx] = 0;
      }
      if (appState == SET_M1 && alarmDig[activeTimerIndex][idx] > 5) alarmDig[activeTimerIndex][idx] = 0;
      if (appState == SET_M2 && alarmDig[activeTimerIndex][idx] > 9) alarmDig[activeTimerIndex][idx] = 0;
      quickBeep(50); lastUpState = LOW;
    } 
    else if (!currentUp && !currentOk) { lastUpState = HIGH; }
  }
}

void validateTimerOrder() {
  int totalMin1 = (getHour(0) * 60) + getMin(0);
  int totalMin2 = (getHour(1) * 60) + getMin(1);
  if (totalMin2 < totalMin1) {
    digitalWrite(BUZZER_PIN, HIGH);
    display.clearDisplay(); display.setTextSize(2); display.setCursor(10, 15); display.println("TIME ERROR");
    display.display(); delay(2500); digitalWrite(BUZZER_PIN, LOW);
    for (int i = 0; i < 4; i++) alarmDig[1][i] = alarmDig[0][i];
  }
}

void renderWatchDisplay(DateTime now) {
  display.setTextSize(1); display.setCursor(5, 2); display.println("MONITORING TIMERS");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);
  display.setTextSize(2); display.setCursor(18, 18);
  display.printf("%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  char s1 = (t1State == STATE_COMPLETED) ? 'D' : (t1State == STATE_RUNNING) ? 'R' : 'P';
  char s2 = (t2State == STATE_COMPLETED) ? 'D' : (t2State == STATE_RUNNING) ? 'R' : 'P';

  display.setTextSize(1); display.setCursor(5, 42);
  display.printf("T1: %02d:%02d %s [%c]", getHour(0), getMin(0), (getHour(0) >= 12) ? "PM" : "AM", s1);
  display.setCursor(5, 54);
  display.printf("T2: %02d:%02d %s [%c]", getHour(1), getMin(1), (getHour(1) >= 12) ? "PM" : "AM", s2);
}

void renderMenuSettings() {
  int displayHour = getHour(activeTimerIndex);
  display.setTextSize(1); display.setCursor(5, 2); display.printf("SETTING TIMER %d", activeTimerIndex + 1);
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);
  char h1 = (appState == SET_H1 && (millis()/200)%2==0) ? ' ' : '0'+alarmDig[activeTimerIndex][0];
  char h2 = (appState == SET_H2 && (millis()/200)%2==0) ? ' ' : '0'+alarmDig[activeTimerIndex][1];
  char m1 = (appState == SET_M1 && (millis()/200)%2==0) ? ' ' : '0'+alarmDig[activeTimerIndex][2];
  char m2 = (appState == SET_M2 && (millis()/200)%2==0) ? ' ' : '0'+alarmDig[activeTimerIndex][3];
  display.setTextSize(2); display.setCursor(15, 30);
  display.printf("%c%c:%c%c %s", h1, h2, m1, m2, (displayHour >= 12) ? "PM" : "AM");
}

void executeCustomDispenserCycle() {
  display.clearDisplay();
  display.setTextSize(2); display.setCursor(10, 10); display.println("RUNNING");
  display.setCursor(10, 35); display.printf("SWEEP T%d", activeAlarmTriggered);
  display.display();
  digitalWrite(BUZZER_PIN, HIGH);

  if (activeAlarmTriggered == 1) {
    int steps[] = {0, 45, 90};
    for (int i = 0; i < 3; i++) { commandServoPosition(steps[i]); delay(2500); }
    delay(6000);
    int rev[] = {45, 0};
    for (int i = 0; i < 2; i++) { commandServoPosition(rev[i]); delay(1500); }
    t1State = STATE_COMPLETED;
  } else if (activeAlarmTriggered == 2) {
    int steps[] = {0, 45, 90, 135, 180};
    for (int i = 0; i < 5; i++) { commandServoPosition(steps[i]); delay(2500); }
    delay(6000);
    int rev[] = {135, 90, 45, 0};
    for (int i = 0; i < 4; i++) { commandServoPosition(rev[i]); delay(1500); }
    t2State = STATE_COMPLETED;
  }
  silenceAlarmSequence();
}

// FIXED: Use SERVO_PIN directly instead of PWM_CHANNEL
void commandServoPosition(int angleTarget) {
  long dynamicDuty = map(angleTarget, 0, 180, 1638, 7864);
  ledcWrite(SERVO_PIN, dynamicDuty);
}

void quickBeep(int durationMs) {
  digitalWrite(BUZZER_PIN, HIGH); delay(durationMs); digitalWrite(BUZZER_PIN, LOW);
}

void silenceAlarmSequence() {
  digitalWrite(BUZZER_PIN, LOW); commandServoPosition(0); delay(200); appState = WATCH_MODE;
}
