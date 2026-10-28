#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ArduinoBLE.h>
#include <LSM6DS3.h>
#include <qrcode.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define MOTOR_PIN 10  // GPIO 10 connected to transistor base
#define BAT_READ_PIN PIN_VBAT
#define BAT_ENABLE_PIN PIN_VBAT_ENABLE
#define CHARGE_SPEED_PIN P0_13
#define BUTTON_PIN 1

// --- CHANGE YOUR GITHUB / TARGET LINK HERE ---
const char* GITHUB_REPO_URL = "https://github.com/PinToCode/Smart-Table-Assistant-";

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
LSM6DS3 myIMU(I2C_MODE, 0x6A);

#include <FluxGarage_RoboEyes.h>
roboEyes roboEyes;  // RoboEyes instance - draws to the same "display" object above

// BLE Setup
BLEService watchService("1805");
BLEService batteryService("180F");
BLEUnsignedCharCharacteristic batteryLevelChar("2A19", BLERead | BLENotify);
BLEFloatCharacteristic batteryVoltageChar("2A1A", BLERead | BLENotify);
BLEStringCharacteristic timeChar("2A2B", BLERead | BLEWrite, 20);
BLEStringCharacteristic notifyChar("2A46", BLERead | BLEWrite, 40);
BLEStringCharacteristic callChar("2A47", BLERead | BLEWrite, 20);
BLEStringCharacteristic dateChar("2A27", BLEWrite | BLERead, 20);
BLEStringCharacteristic weatherChar("2A24", BLERead | BLEWrite, 40);

String currentDate = "Waiting...";
String currentTemp = "--";

// Button variables
unsigned long pressStartTime = 0;
bool isPressing = false;
const int LONG_PRESS_THRESHOLD = 1000;
const int EXIT_TIME_SET_THRESHOLD = 3000; // 3 sec long press to exit Time Set
bool longPressTriggered = false;
bool ignoreThisPress = false;

// Time & System variables
int hours = 0, minutes = 0, seconds = 0;
unsigned long lastTick = 0;
bool timeSet = false;
bool bleConnected = false;
unsigned long lastUpdate = 0;
const long updateInterval = 30000;
int percentage = 0;

// --- DISPLAY MODE VARIABLES ---
enum DisplayMode { MODE_IMU,
                   MODE_ALWAYS_ON,
                   MODE_MANUAL };
DisplayMode currentMode = MODE_IMU;

// --- MAIN MENU (6 ITEMS) ---
// 0: Device Mode, 1: Shake Mode, 2: Haptic, 3: Set Time, 4: Reminders, 5: GitHub QR
bool menuActive = false;
int tempSelection = 0;
unsigned long lastPressTime = 0;
const long autoSelectDelay = 2000;

// --- QR VIEW STATE ---
bool qrMenuActive = false;

// --- DEVICE MODE SUBMENU VARIABLES ---
bool deviceModeMenuActive = false;
int deviceModeSelection = 0; // 0 = IMU Wake, 1 = Always On, 2 = Manual Only
unsigned long deviceModeLastPressTime = 0;
const long deviceModeAutoSelectDelay = 2000;

// --- WATCH FACE VARIABLES ---
enum WatchFace { WATCH_FACE_1,
                 WATCH_FACE_2,
                 WATCH_FACE_3 };
WatchFace currentFace = WATCH_FACE_1;

bool WatchFace_menuActive = false;
int WatchFace_tempSelection = 0;
unsigned long WatchFace_lastPressTime = 0;
const long WatchFace_autoSelectDelay = 2000;

// --- SHAKE TRIGGER & CONFIG SUBMENU VARIABLES ---
bool shakeFeatureEnabled = false;         // Disabled by default on startup
bool shakeMenuActive = false;
int shakeMenuSelection = 0;
unsigned long shakeMenuLastPressTime = 0;
const long shakeMenuAutoSelectDelay = 2000;

unsigned long lastShakeTime = 0;
const unsigned long shakeCooldown = 1500;
bool shakeEyeTriggered = false;
unsigned long shakeEyeStartTime = 0;
const unsigned long shakeEyeDuration = 2500;

// --- HAPTIC / VIBRATION CONFIG SUBMENU VARIABLES ---
bool hapticEnabled = false;               // Disabled by default on startup
bool hapticMenuActive = false;
int hapticMenuSelection = 0;
unsigned long hapticMenuLastPressTime = 0;
const long hapticMenuAutoSelectDelay = 2000;

// --- MANUAL TIME CONFIG STATE (12-HOUR FORMAT WITH AM/PM SELECT) ---
bool timeConfigActive = false;
int timeConfigStep = 0; // 0 = Hours, 1 = Minutes, 2 = AM/PM
int editHours12 = 12;   // 1 to 12
int editMinutes = 0;    // 0 to 59
bool editIsPM = false;  // false = AM, true = PM
unsigned long timeConfigLastPressTime = 0;
const unsigned long timeConfigAutoSwitchDelay = 5000; // 5 seconds wait bar

// --- REMINDER FEATURE VARIABLES (TWO-STAGE) ---
bool remindersEnabled = true;
bool reminderMenuActive = false;
int reminderMenuSelection = 0;
unsigned long reminderMenuLastPressTime = 0;
const long reminderMenuAutoSelectDelay = 2000;
int lastReminderTriggerHour = -1;

bool isShowingReminder = false;
int reminderStage = 0; // 0 = Greeting (10s), 1 = Drink Water / Night msg (15s)
unsigned long reminderStageStartTime = 0;
const unsigned long REMINDER_STAGE1_TIME = 10000; // 10 seconds for greeting
const unsigned long REMINDER_STAGE2_TIME = 15000; // 15 seconds for message
String reminderGreeting = "";
String reminderAction = "";

// Battery & Motion
float movingAvgVolts = 3.7;
bool screenOn = true;
unsigned long lastMoveTime = 0;
const long sleepTimeout = 10000;
float lastX, lastY, lastZ;

// --- ALWAYS-ON EYE ANIMATION STATE ---
bool eyesActive = false;
unsigned long alwaysOnStillStart = 0;
const long eyesIdleDelay = 3000;

// Notification & Call
String msgQueue[3];
int msgCount = 0;
unsigned long msgStartTime = 0;
const long msgVisibleTime = 5000;
bool isShowingMsg = false;
String callerID = "";
bool isCalling = false;

// Vibration Variables (Non-blocking)
unsigned long motorEndTime = 0;
bool isVibrating = false;

// Function Prototypes
void updateBatteryData();
void wakeScreen();
void updateTimeFromString(String input);
void startVibration(int durationMs);
void drawMenu();
void drawDeviceModeMenu();
void drawShakeMenu();
void drawHapticMenu();
void drawReminderMenu();
void drawTimeConfigMenu();
void drawWatchFaceMenu();
void drawQRScreen();
void checkReminders();
void startReminderSequence(String greeting, String action);
void triggerNotification(String message);
void face1();
void face2();
void face3();
void drawSun(int x, int y);
void drawCloud(int x, int y);
void drawRain(int x, int y);
void drawBattery(int x, int y, int percentage);
void recoverI2CBus();
void powerCycleDisplay();

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);

  // Power on display pins
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(4, HIGH);
  digitalWrite(5, HIGH);
  delay(100);

  pinMode(CHARGE_SPEED_PIN, OUTPUT);
  digitalWrite(CHARGE_SPEED_PIN, LOW);

  pinMode(BAT_ENABLE_PIN, OUTPUT);
  digitalWrite(BAT_ENABLE_PIN, LOW);

  recoverI2CBus();
  Wire.begin();
  display.begin(0x3C, true);
  display.setTextWrap(true);

  if (myIMU.begin() != 0) Serial.println("IMU error");

  // RoboEyes setup
  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);
  roboEyes.setWidth(36, 36);
  roboEyes.setHeight(36, 36);
  roboEyes.setBorderradius(8, 8);
  roboEyes.setSpacebetween(10);
  roboEyes.setMood(DEFAULT);
  roboEyes.setPosition(DEFAULT);
  roboEyes.setCuriosity(ON);

  if (BLE.begin()) {
    BLE.setLocalName("XIAO-Watch");
    BLE.setAdvertisedService(watchService);
    watchService.addCharacteristic(timeChar);
    watchService.addCharacteristic(dateChar);
    watchService.addCharacteristic(weatherChar);
    watchService.addCharacteristic(notifyChar);
    watchService.addCharacteristic(callChar);
    BLE.addService(watchService);

    // Setup Battery Service
    batteryService.addCharacteristic(batteryLevelChar);
    batteryService.addCharacteristic(batteryVoltageChar);
    BLE.addService(batteryService);

    BLE.advertise();
  }
  updateBatteryData();

  alwaysOnStillStart = millis();
}

void recoverI2CBus() {
  pinMode(PIN_WIRE_SCL, OUTPUT);
  pinMode(PIN_WIRE_SDA, INPUT);

  for (int i = 0; i < 9; i++) {
    digitalWrite(PIN_WIRE_SCL, LOW);
    delayMicroseconds(5);
    digitalWrite(PIN_WIRE_SCL, HIGH);
    delayMicroseconds(5);
    if (digitalRead(PIN_WIRE_SDA) == HIGH) break;
  }

  pinMode(PIN_WIRE_SDA, OUTPUT);
  digitalWrite(PIN_WIRE_SDA, LOW);
  delayMicroseconds(5);
  digitalWrite(PIN_WIRE_SCL, HIGH);
  delayMicroseconds(5);
  digitalWrite(PIN_WIRE_SDA, HIGH);
  delayMicroseconds(5);

  Wire.end();
  Wire.begin();
}

void powerCycleDisplay() {
  digitalWrite(4, LOW);
  digitalWrite(5, LOW);
  delay(50);
  digitalWrite(4, HIGH);
  digitalWrite(5, HIGH);
  delay(100);
  display.begin(0x3C, true);
  display.setTextWrap(true);
}

void loop() {
  // --- 0. MOTOR OVERSEER (Non-blocking) ---
  if (isVibrating && millis() >= motorEndTime) {
    digitalWrite(MOTOR_PIN, LOW);
    isVibrating = false;
  }

  // --- 1. BUTTON & MENU LOGIC ---
  int buttonState = digitalRead(BUTTON_PIN);

  // 1. Detection of Press (Logic LOW)
  if (buttonState == LOW && !isPressing) {
    pressStartTime = millis();
    isPressing = true;
    longPressTriggered = false;

    // Dismiss active reminder on press
    if (isShowingReminder) {
      isShowingReminder = false;
      ignoreThisPress = true;
      lastMoveTime = millis();
    } else if (eyesActive) {
      eyesActive = false;
      alwaysOnStillStart = millis();
      ignoreThisPress = true;
      startVibration(50);
    } else {
      ignoreThisPress = false;
    }
  }

  // 2. Detection of LONG PRESS WHILE HOLDING
  if (buttonState == LOW && isPressing && !longPressTriggered && !ignoreThisPress) {
    // 3-Second Long Press for Time Setting Exit
    if (timeConfigActive) {
      if (millis() - pressStartTime >= EXIT_TIME_SET_THRESHOLD) {
        if (editHours12 == 12) {
          hours = editIsPM ? 12 : 0;
        } else {
          hours = editIsPM ? (editHours12 + 12) : editHours12;
        }
        minutes = editMinutes;
        seconds = 0;
        timeSet = true;
        lastTick = millis();
        timeConfigActive = false;
        longPressTriggered = true;
        startVibration(250);
        lastMoveTime = millis();
      }
    } else if (!qrMenuActive) {
      // 1-Second Long Press for Watch Face Selection
      if (millis() - pressStartTime >= LONG_PRESS_THRESHOLD) {
        if (screenOn && !WatchFace_menuActive && !menuActive && !deviceModeMenuActive && !shakeMenuActive && !hapticMenuActive && !reminderMenuActive) {
          WatchFace_menuActive = true;
          WatchFace_tempSelection = (int)currentFace;
          startVibration(100);
        } else if (!screenOn) {
          wakeScreen();
        }
        longPressTriggered = true;
        WatchFace_lastPressTime = millis();
      }
    }
  }

  // 3. Detection of Release (For Short Press and Navigation)
  if (buttonState == HIGH && isPressing) {
    unsigned long duration = millis() - pressStartTime;
    isPressing = false;

    if (!longPressTriggered && !ignoreThisPress) {
      if (duration > 50) {
        if (!screenOn) {
          wakeScreen();
          startVibration(50);
        } else if (qrMenuActive) {
          // Single short press exits QR view back to home screen
          qrMenuActive = false;
          startVibration(50);
          lastMoveTime = millis();
        } else {
          // If in Time Config: single press increments currently focused value
          if (timeConfigActive) {
            if (timeConfigStep == 0) {
              editHours12++;
              if (editHours12 > 12) {
                editHours12 = 1;
              }
            } else if (timeConfigStep == 1) {
              editMinutes = (editMinutes + 1) % 60;
            } else if (timeConfigStep == 2) {
              editIsPM = !editIsPM; // Explicit toggle for AM/PM
            }
            timeConfigLastPressTime = millis();
            startVibration(30);
          }
          // If no menus active, enter main Settings menu
          else if (!menuActive && !WatchFace_menuActive && !deviceModeMenuActive && !shakeMenuActive && !hapticMenuActive && !reminderMenuActive) {
            menuActive = true;
            tempSelection = 0;
            startVibration(100);
            lastPressTime = millis();
          } else if (menuActive) {
            // Cycle through 6 options (0: Device Mode, 1: Shake Mode, 2: Haptic, 3: Set Time, 4: Reminders, 5: GitHub QR)
            tempSelection = (tempSelection + 1) % 6;
            startVibration(50);
            lastPressTime = millis();
          } else if (deviceModeMenuActive) {
            deviceModeSelection = (deviceModeSelection + 1) % 3;
            startVibration(50);
            deviceModeLastPressTime = millis();
          } else if (shakeMenuActive) {
            shakeMenuSelection = (shakeMenuSelection + 1) % 2;
            startVibration(50);
            shakeMenuLastPressTime = millis();
          } else if (hapticMenuActive) {
            hapticMenuSelection = (hapticMenuSelection + 1) % 2;
            startVibration(50);
            hapticMenuLastPressTime = millis();
          } else if (reminderMenuActive) {
            reminderMenuSelection = (reminderMenuSelection + 1) % 2;
            startVibration(50);
            reminderMenuLastPressTime = millis();
          } else if (WatchFace_menuActive) {
            WatchFace_tempSelection = (WatchFace_tempSelection + 1) % 3;
            startVibration(50);
            WatchFace_lastPressTime = millis();
          }
        }
      }
    }
    ignoreThisPress = false;
  }

  // Auto-switch between Hour, Minute, and AM/PM in Time Config after 5 seconds idle
  if (timeConfigActive && (millis() - timeConfigLastPressTime >= timeConfigAutoSwitchDelay)) {
    timeConfigStep = (timeConfigStep + 1) % 3; // Steps through 0 (Hours) -> 1 (Minutes) -> 2 (AM/PM)
    timeConfigLastPressTime = millis();
    startVibration(60);
  }

  // Auto-select confirmation for Main Settings Menu
  if (menuActive && (millis() - lastPressTime >= autoSelectDelay)) {
    if (tempSelection == 0) {
      menuActive = false;
      deviceModeMenuActive = true;
      deviceModeSelection = (currentMode == MODE_IMU) ? 0 : (currentMode == MODE_ALWAYS_ON ? 1 : 2);
      deviceModeLastPressTime = millis();
      startVibration(80);
    } else if (tempSelection == 1) {
      menuActive = false;
      shakeMenuActive = true;
      shakeMenuSelection = shakeFeatureEnabled ? 0 : 1;
      shakeMenuLastPressTime = millis();
      startVibration(80);
    } else if (tempSelection == 2) {
      menuActive = false;
      hapticMenuActive = true;
      hapticMenuSelection = hapticEnabled ? 0 : 1;
      hapticMenuLastPressTime = millis();
      startVibration(80);
    } else if (tempSelection == 3) {
      menuActive = false;
      timeConfigActive = true;
      timeConfigStep = 0;

      editIsPM = (hours >= 12);
      editHours12 = hours % 12;
      if (editHours12 == 0) editHours12 = 12;

      editMinutes = minutes;
      timeConfigLastPressTime = millis();
      startVibration(120);
    } else if (tempSelection == 4) {
      menuActive = false;
      reminderMenuActive = true;
      reminderMenuSelection = remindersEnabled ? 0 : 1;
      reminderMenuLastPressTime = millis();
      startVibration(80);
    } else if (tempSelection == 5) {
      menuActive = false;
      qrMenuActive = true;
      startVibration(80);
    }
    lastMoveTime = millis();
  }

  // Auto-select confirmation for Device Mode Submenu
  if (deviceModeMenuActive && (millis() - deviceModeLastPressTime >= deviceModeAutoSelectDelay)) {
    if (deviceModeSelection == 0) currentMode = MODE_IMU;
    else if (deviceModeSelection == 1) currentMode = MODE_ALWAYS_ON;
    else currentMode = MODE_MANUAL;

    deviceModeMenuActive = false;
    eyesActive = false;
    alwaysOnStillStart = millis();
    startVibration(100);
    lastMoveTime = millis();
  }

  // Auto-select confirmation for Shake Mode Submenu
  if (shakeMenuActive && (millis() - shakeMenuLastPressTime >= shakeMenuAutoSelectDelay)) {
    shakeFeatureEnabled = (shakeMenuSelection == 0);
    shakeMenuActive = false;
    startVibration(100);
    lastMoveTime = millis();
  }

  // Auto-select confirmation for Haptic Mode Submenu
  if (hapticMenuActive && (millis() - hapticMenuLastPressTime >= hapticMenuAutoSelectDelay)) {
    hapticEnabled = (hapticMenuSelection == 0);
    hapticMenuActive = false;
    if (hapticEnabled) startVibration(100);
    lastMoveTime = millis();
  }

  // Auto-select confirmation for Reminder Submenu
  if (reminderMenuActive && (millis() - reminderMenuLastPressTime >= reminderMenuAutoSelectDelay)) {
    remindersEnabled = (reminderMenuSelection == 0);
    reminderMenuActive = false;
    startVibration(100);
    lastMoveTime = millis();
  }

  // Auto-select confirmation for Watch Face Menu
  if (WatchFace_menuActive && (millis() - WatchFace_lastPressTime >= WatchFace_autoSelectDelay)) {
    if (WatchFace_tempSelection == 0) currentFace = WATCH_FACE_1;
    else if (WatchFace_tempSelection == 1) currentFace = WATCH_FACE_2;
    else currentFace = WATCH_FACE_3;

    WatchFace_menuActive = false;
    lastMoveTime = millis();
  }

  // --- 2. PERIODIC BATTERY UPDATE ---
  if (millis() - lastUpdate >= updateInterval) {
    lastUpdate = millis();
    updateBatteryData();
  }

  // --- 3. MOTION & DISPLAY POWER LOGIC ---
  if (!menuActive && !WatchFace_menuActive && !deviceModeMenuActive && !shakeMenuActive && !hapticMenuActive && !reminderMenuActive && !timeConfigActive && !qrMenuActive) {

    float x = myIMU.readFloatAccelX();
    float y = myIMU.readFloatAccelY();
    float z = myIMU.readFloatAccelZ();

    float deltaTotal = abs(x - lastX) + abs(y - lastY) + abs(z - lastZ);
    bool motionDetected = (abs(x - lastX) > 0.15 || abs(y - lastY) > 0.15 || abs(z - lastZ) > 0.15);

    lastX = x;
    lastY = y;
    lastZ = z;

    // SHAKE DETECTION
    if (shakeFeatureEnabled && deltaTotal > 1.6 && (millis() - lastShakeTime >= shakeCooldown)) {
      lastShakeTime = millis();
      startVibration(120);

      if (!screenOn) wakeScreen();

      shakeEyeTriggered = true;
      shakeEyeStartTime = millis();
      roboEyes.anim_confused();
    }

    // Shake Eye Expiry Check
    if (shakeEyeTriggered && (millis() - shakeEyeStartTime >= shakeEyeDuration)) {
      shakeEyeTriggered = false;
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT);
    }

    // Display Mode Logic
    if (currentMode == MODE_ALWAYS_ON) {
      if (!screenOn) wakeScreen();

      if (motionDetected) {
        eyesActive = false;
        alwaysOnStillStart = millis();
      } else if (!eyesActive && !isCalling && !isShowingMsg && !isShowingReminder && (millis() - alwaysOnStillStart >= eyesIdleDelay)) {
        eyesActive = true;
      }
    } else if (currentMode == MODE_IMU) {
      if (motionDetected) {
        if (!screenOn) wakeScreen();
        lastMoveTime = millis();
      }
    }

    // Screen sleep timeout (Will never time out when QR is active)
    if (currentMode != MODE_ALWAYS_ON && screenOn && !shakeEyeTriggered && (millis() - lastMoveTime > sleepTimeout) && !isCalling && !isShowingMsg && !isShowingReminder) {
      screenOn = false;
      display.oled_command(SH110X_DISPLAYOFF);
    }
  }

  // --- 4. BLE HANDLING ---
  BLEDevice central = BLE.central();
  if (central) {
    if (!bleConnected) {
      bleConnected = true;
      startVibration(500);
    }
    if (timeChar.written()) updateTimeFromString(timeChar.value());
    if (notifyChar.written()) {
      String val = notifyChar.value();
      val.trim();
      if (val != "CLR" && val != "" && msgCount < 3) {
        triggerNotification(val);
      }
    }
    if (callChar.written()) {
      String val = callChar.value();
      val.trim();
      if (val == "CLR" || val == "") {
        isCalling = false;
        callerID = "";
      } else {
        callerID = val;
        isCalling = true;
        startVibration(3000);
        wakeScreen();
        eyesActive = false;
        alwaysOnStillStart = millis();
      }
    }
    if (dateChar.written()) {
      currentDate = dateChar.value();
    }
    if (weatherChar.written()) {
      String val = weatherChar.value();
      int commaIndex = val.indexOf(',');
      String tempRaw = (commaIndex > 0) ? val.substring(0, commaIndex) : val;
      tempRaw.replace("C", "");
      tempRaw.replace("c", "");
      tempRaw.trim();
      currentTemp = tempRaw;
    }
  } else {
    bleConnected = false;
    isCalling = false;
    callerID = "";
  }

  // --- 5. CLOCK & SCHEDULED REMINDER LOGIC ---
  if (timeSet && millis() - lastTick >= 1000) {
    lastTick += 1000;
    seconds++;
    if (seconds >= 60) {
      seconds = 0;
      minutes++;
    }
    if (minutes >= 60) {
      minutes = 0;
      hours++;
    }
    if (hours >= 24) { hours = 0; }

    checkReminders();
  }

  // --- 6. REMINDER & MESSAGE QUEUE TIMING LOGIC ---
  if (isShowingReminder) {
    if (reminderStage == 0 && (millis() - reminderStageStartTime >= REMINDER_STAGE1_TIME)) {
      reminderStage = 1;
      reminderStageStartTime = millis();
    } else if (reminderStage == 1 && (millis() - reminderStageStartTime >= REMINDER_STAGE2_TIME)) {
      isShowingReminder = false;
      lastMoveTime = millis();
    }
  }

  if (msgCount > 0 && !isShowingMsg) {
    isShowingMsg = true;
    msgStartTime = millis();
  }
  if (isShowingMsg && (millis() - msgStartTime >= msgVisibleTime)) {
    isShowingMsg = false;
    for (int i = 0; i < msgCount - 1; i++) msgQueue[i] = msgQueue[i + 1];
    msgCount--;
  }

  // --- 7. DISPLAY LOGIC ---
  if (screenOn) {
    if (timeConfigActive) {
      display.clearDisplay();
      display.setTextColor(SH110X_WHITE);
      drawTimeConfigMenu();
      display.display();
    } else if (qrMenuActive) {
      display.clearDisplay();
      drawQRScreen();
      display.display();
    } else if (WatchFace_menuActive) {
      display.clearDisplay();
      display.setTextColor(SH110X_WHITE);
      drawWatchFaceMenu();
      display.display();
    } else if (deviceModeMenuActive) {
      display.clearDisplay();
      display.setTextColor(SH110X_WHITE);
      drawDeviceModeMenu();
      display.display();
    } else if (reminderMenuActive) {
      display.clearDisplay();
      display.setTextColor(SH110X_WHITE);
      drawReminderMenu();
      display.display();
    } else if (shakeMenuActive) {
      display.clearDisplay();
      display.setTextColor(SH110X_WHITE);
      drawShakeMenu();
      display.display();
    } else if (hapticMenuActive) {
      display.clearDisplay();
      display.setTextColor(SH110X_WHITE);
      drawHapticMenu();
      display.display();
    } else if (menuActive) {
      display.clearDisplay();
      display.setTextColor(SH110X_WHITE);
      drawMenu();
      display.display();
    } else if (isCalling && callerID != "") {
      display.clearDisplay();
      display.setTextColor(SH110X_WHITE);
      display.setTextSize(1);
      display.setCursor(30, 5);
      display.println("INCOMING CALL");
      display.println("---------------------");
      display.setCursor(0, 30);
      display.setTextSize(2);
      display.println(callerID);
      display.display();
    } else if (isShowingReminder) {
      display.clearDisplay();
      display.setTextColor(SH110X_WHITE);
      display.setTextSize(2);
      display.setCursor(4, 18);
      if (reminderStage == 0) {
        display.println(reminderGreeting);
      } else {
        display.println(reminderAction);
      }
      display.display();
    } else if (isShowingMsg) {
      display.clearDisplay();
      display.setTextColor(SH110X_WHITE);
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("NOTIFICATION");
      display.println("---------------------");
      display.setCursor(0, 20);
      display.setTextSize(1);
      display.println(msgQueue[0]);
      display.display();
    } else if (shakeEyeTriggered || (currentMode == MODE_ALWAYS_ON && eyesActive)) {
      roboEyes.update();
    } else {
      display.clearDisplay();
      display.setTextColor(SH110X_WHITE);
      if (currentFace == WATCH_FACE_1) face1();
      else if (currentFace == WATCH_FACE_2) face2();
      else face3();
      display.display();
    }
  }
}

// --- HELPERS ---

void startReminderSequence(String greeting, String action) {
  reminderGreeting = greeting;
  reminderAction = action;
  reminderStage = 0;
  reminderStageStartTime = millis();
  isShowingReminder = true;
  startVibration(800);
  wakeScreen();
  eyesActive = false;
  alwaysOnStillStart = millis();
}

void triggerNotification(String message) {
  if (msgCount < 3) {
    msgQueue[msgCount] = message;
    msgCount++;
  } else {
    msgQueue[2] = message;
  }
  startVibration(800);
  wakeScreen();
  eyesActive = false;
  alwaysOnStillStart = millis();
}

void checkReminders() {
  if (!remindersEnabled) return;

  if (seconds == 0 && minutes == 0) {
    if (hours == 7 && lastReminderTriggerHour != 7) {
      startReminderSequence("Good\nmorning!", "Drink\nenough\nwater.");
      lastReminderTriggerHour = 7;
    } else if (hours == 12 && lastReminderTriggerHour != 12) {
      startReminderSequence("Good\nafternoon!", "Drink\nenough\nwater.");
      lastReminderTriggerHour = 12;
    } else if (hours == 16 && lastReminderTriggerHour != 16) {
      startReminderSequence("Good\nevening!", "Drink\nenough\nwater.");
      lastReminderTriggerHour = 16;
    } else if (hours == 22 && lastReminderTriggerHour != 22) {
      startReminderSequence("Good\nnight!", "Take rest,\nsweet\ndreams.");
      lastReminderTriggerHour = 22;
    }
  }

  if (minutes > 0) {
    lastReminderTriggerHour = -1;
  }
}

void startVibration(int durationMs) {
  if (!hapticEnabled) return;
  digitalWrite(MOTOR_PIN, HIGH);
  motorEndTime = millis() + durationMs;
  isVibrating = true;
}

void updateBatteryData() {
  analogReadResolution(12);
  int rawADC = analogRead(BAT_READ_PIN);
  float instantVolts = (rawADC * 3.3 / 4095.0) * 3.0;
  movingAvgVolts = (movingAvgVolts * 0.8) + (instantVolts * 0.2);
  percentage = map(constrain(movingAvgVolts * 100, 320, 420), 320, 420, 0, 100);

  if (BLE.connected()) {
    batteryLevelChar.writeValue((byte)percentage);
    batteryVoltageChar.writeValue(movingAvgVolts);
  }

  Serial.print("Real Battery Voltage: ");
  Serial.print(movingAvgVolts);
  Serial.println("V");
}

void wakeScreen() {
  screenOn = true;
  display.oled_command(SH110X_DISPLAYON);
  lastMoveTime = millis();
}

void updateTimeFromString(String input) {
  input.trim();
  if (input.length() >= 8) {
    hours = input.substring(0, 2).toInt();
    minutes = input.substring(3, 5).toInt();
    seconds = input.substring(6, 8).toInt();
    timeSet = true;
    lastTick = millis();
  }
}

void drawTimeConfigMenu() {
  display.setTextSize(1);
  display.setCursor(38, 2);
  display.println("SET TIME");
  display.drawFastHLine(0, 12, 128, SH110X_WHITE);

  bool blinkState = ((millis() / 350) % 2 == 0);

  display.setTextSize(3);
  int startX = 14;
  int startY = 20;

  // Draw 12-Hour
  display.setCursor(startX, startY);
  if (timeConfigStep == 0 && !blinkState) {
    display.print("  ");
  } else {
    if (editHours12 < 10) display.print("0");
    display.print(editHours12);
  }

  display.print(":");

  // Draw Minute
  if (timeConfigStep == 1 && !blinkState) {
    display.print("  ");
  } else {
    if (editMinutes < 10) display.print("0");
    display.print(editMinutes);
  }

  // Draw AM/PM with its own blinking state on step 2
  display.setTextSize(1);
  display.setCursor(startX + 88, startY + 8);
  if (timeConfigStep == 2 && !blinkState) {
    display.print("  ");
  } else {
    display.print(editIsPM ? "PM" : "AM");
  }

  display.setTextSize(1);
  display.setCursor(2, 48);
  display.print("Hold 3s to save/exit");

  int barWidth = map(millis() - timeConfigLastPressTime, 0, timeConfigAutoSwitchDelay, 128, 0);
  display.fillRect(0, 59, barWidth, 4, SH110X_WHITE);
}

void drawWatchFaceMenu() {
  display.setTextSize(1);
  display.setCursor(30, 5);
  display.println("WATCH FACE");
  display.drawFastHLine(0, 15, 128, SH110X_WHITE);

  display.setCursor(15, 25);
  if (WatchFace_tempSelection == 0) display.print("> Face 1");
  else display.print("  Face 1");

  display.setCursor(15, 37);
  if (WatchFace_tempSelection == 1) display.print("> Face 2");
  else display.print("  Face 2");

  display.setCursor(15, 49);
  if (WatchFace_tempSelection == 2) display.print("> Face 3");
  else display.print("  Face 3");

  int barWidth = map(millis() - WatchFace_lastPressTime, 0, WatchFace_autoSelectDelay, 128, 0);
  display.fillRect(0, 60, barWidth, 4, SH110X_WHITE);
}

void drawMenu() {
  display.setTextSize(1);
  display.setCursor(35, 2);
  display.println("SETTINGS");
  display.drawFastHLine(0, 12, 128, SH110X_WHITE);

  const char* menuItems[6] = {
    "Device Mode",
    "Shake Mode",
    "Haptic",
    "Set Time",
    "Reminders",
    "GitHub QR"
  };

  int startIdx = 0;
  if (tempSelection >= 3) {
    startIdx = tempSelection - 3;
    if (startIdx > 2) startIdx = 2; // Keeps window pinned to max items
  }

  for (int i = 0; i < 4; i++) {
    int itemIdx = startIdx + i;
    if (itemIdx >= 6) break;
    int yPos = 16 + (i * 10);
    display.setCursor(10, yPos);
    if (tempSelection == itemIdx) {
      display.print("> ");
    } else {
      display.print("  ");
    }
    display.print(menuItems[itemIdx]);
  }

  int barWidth = map(millis() - lastPressTime, 0, autoSelectDelay, 128, 0);
  display.fillRect(0, 60, barWidth, 4, SH110X_WHITE);
}

void drawQRScreen() {
  int urlLen = strlen(GITHUB_REPO_URL);
  int qrVersion = 3; // Default to version 3 (up to 53 alphanumeric chars)
  if (urlLen <= 25) qrVersion = 2; // Fits up to 32 alphanumeric chars

  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, qrVersion, 0, GITHUB_REPO_URL);

  int pixelScale = 2;
  int qrPixelSize = qrcode.size * pixelScale;
  int quietZone = 2;
  int startX = 4;
  int startY = (SCREEN_HEIGHT - qrPixelSize) / 2;

  // Solid white quiet zone card for instant phone camera scan compatibility
  display.fillRect(startX - quietZone, startY - quietZone, qrPixelSize + (quietZone * 2), qrPixelSize + (quietZone * 2), SH110X_WHITE);

  // Draw dark modules on the white card
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        display.fillRect(startX + (x * pixelScale), startY + (y * pixelScale), pixelScale, pixelScale, SH110X_BLACK);
      }
    }
  }

  // Information & prompt labels on the right
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(72, 8);
  display.print("SCAN ME");
  display.drawFastHLine(72, 18, 54, SH110X_WHITE);

  display.setCursor(72, 24);
  display.print("GitHub");
  display.setCursor(72, 34);
  display.print("Project");

  display.setCursor(72, 50);
  display.print("[Click]");
  display.setCursor(72, 57);
  display.print("to Exit");
}

void drawDeviceModeMenu() {
  display.setTextSize(1);
  display.setCursor(30, 5);
  display.println("DEVICE MODE");
  display.drawFastHLine(0, 15, 128, SH110X_WHITE);

  display.setCursor(15, 22);
  if (deviceModeSelection == 0) display.print("> IMU Wake");
  else display.print("  IMU Wake");

  display.setCursor(15, 34);
  if (deviceModeSelection == 1) display.print("> Always On");
  else display.print("  Always On");

  display.setCursor(15, 46);
  if (deviceModeSelection == 2) display.print("> Manual Only");
  else display.print("  Manual Only");

  int barWidth = map(millis() - deviceModeLastPressTime, 0, deviceModeAutoSelectDelay, 128, 0);
  display.fillRect(0, 60, barWidth, 4, SH110X_WHITE);
}

void drawShakeMenu() {
  display.setTextSize(1);
  display.setCursor(32, 5);
  display.println("SHAKE MODE");
  display.drawFastHLine(0, 15, 128, SH110X_WHITE);

  display.setCursor(20, 26);
  if (shakeMenuSelection == 0) display.print("> Enable");
  else display.print("  Enable");

  display.setCursor(20, 40);
  if (shakeMenuSelection == 1) display.print("> Disable");
  else display.print("  Disable");

  int barWidth = map(millis() - shakeMenuLastPressTime, 0, shakeMenuAutoSelectDelay, 128, 0);
  display.fillRect(0, 60, barWidth, 4, SH110X_WHITE);
}

void drawHapticMenu() {
  display.setTextSize(1);
  display.setCursor(42, 5);
  display.println("HAPTIC");
  display.drawFastHLine(0, 15, 128, SH110X_WHITE);

  display.setCursor(20, 26);
  if (hapticMenuSelection == 0) display.print("> Enable");
  else display.print("  Enable");

  display.setCursor(20, 40);
  if (hapticMenuSelection == 1) display.print("> Disable");
  else display.print("  Disable");

  int barWidth = map(millis() - hapticMenuLastPressTime, 0, hapticMenuAutoSelectDelay, 128, 0);
  display.fillRect(0, 60, barWidth, 4, SH110X_WHITE);
}

void drawReminderMenu() {
  display.setTextSize(1);
  display.setCursor(35, 5);
  display.println("REMINDERS");
  display.drawFastHLine(0, 15, 128, SH110X_WHITE);

  display.setCursor(20, 26);
  if (reminderMenuSelection == 0) display.print("> Enable");
  else display.print("  Enable");

  display.setCursor(20, 40);
  if (reminderMenuSelection == 1) display.print("> Disable");
  else display.print("  Disable");

  int barWidth = map(millis() - reminderMenuLastPressTime, 0, reminderMenuAutoSelectDelay, 128, 0);
  display.fillRect(0, 60, barWidth, 4, SH110X_WHITE);
}

void face1() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(movingAvgVolts, 2);
  display.print("V");
  display.setCursor(85, 0);
  display.print(bleConnected ? "BLE:ON" : "BLE:OFF");

  int displayHours = hours % 12;
  if (displayHours == 0) displayHours = 12;
  String ampm = (hours >= 12) ? "PM" : "AM";

  display.setTextSize(2);
  display.setCursor(5, 25);
  if (displayHours < 10) display.print("0");
  display.print(displayHours);
  display.print(":");
  if (minutes < 10) display.print("0");
  display.print(minutes);
  display.print(":");
  if (seconds < 10) display.print("0");
  display.print(seconds);
  display.setTextSize(1);
  display.print(" ");
  display.print(ampm);

  display.setCursor(0, 55);
  display.println(timeSet ? "SYSTEM ACTIVE" : "WAITING...");

  display.setTextSize(1);
  display.setCursor(105, 55);
  display.print(percentage);
  display.print("%");

  // Battery Bar
  int barW = 18;
  int barH = 7;
  int barX = (128 - barW) / 2;
  display.drawRect(barX, 1, barW, barH, SH110X_WHITE);
  display.drawRect(barX + barW, 3, 1, 3, SH110X_WHITE);
  int fillWidth = map(percentage, 0, 100, 0, barW - 4);
  if (fillWidth > 0) display.fillRect(barX + 2, 3, fillWidth, barH - 4, SH110X_WHITE);
}

void face2() {
  display.setTextSize(1);
  display.setCursor(5, 42);
  display.print(currentDate);

  display.setCursor(5, 54);
  display.print("Temp: ");
  display.print(currentTemp);

  int x = display.getCursorX();
  int y = display.getCursorY();
  display.fillRect(x, y, 2, 2, SH110X_WHITE);
  display.setCursor(x + 5, 54);
  display.print("C");

  int displayHours = hours % 12;
  if (displayHours == 0) displayHours = 12;
  String ampm = (hours >= 12) ? "PM" : "AM";

  display.setTextSize(2);
  display.setCursor(10, 14);
  if (displayHours < 10) display.print("0");
  display.print(displayHours);
  display.print(":");
  if (minutes < 10) display.print("0");
  display.print(minutes);
  display.print(":");
  if (seconds < 10) display.print("0");
  display.print(seconds);
  display.setTextSize(1);
  display.print(" ");
  display.print(ampm);
}

void face3() {
  int weatherType = 0;

  if (weatherType == 0)
    drawSun(16, 12);
  else if (weatherType == 1)
    drawCloud(4, 10);
  else
    drawRain(4, 10);

  drawBattery(2, 26, percentage);

  display.setTextSize(4);
  int displayHours = hours % 12;
  if (displayHours == 0) displayHours = 12;
  int timeX = 40;

  display.setCursor(timeX, 0);
  if (displayHours < 10) display.print("0");
  display.print(displayHours);

  display.setCursor(timeX, 32);
  if (minutes < 10) display.print("0");
  display.print(minutes);

  bool isPM = (hours >= 12);
  display.setTextSize(1);
  int ampmX = 102;

  if (!isPM) {
    display.fillRect(ampmX - 2, 18, 22, 12, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
  } else {
    display.setTextColor(SH110X_WHITE);
  }
  display.setCursor(ampmX, 20);
  display.print("AM");

  if (isPM) {
    display.fillRect(ampmX - 2, 34, 22, 12, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
  } else {
    display.setTextColor(SH110X_WHITE);
  }
  display.setCursor(ampmX, 36);
  display.print("PM");
  display.setTextColor(SH110X_WHITE);
}

void drawSun(int x, int y) {
  display.fillCircle(x, y, 5, SH110X_WHITE);
  display.drawLine(x, y - 9, x, y - 6, SH110X_WHITE);
  display.drawLine(x, y + 6, x, y + 9, SH110X_WHITE);
  display.drawLine(x - 9, y, x - 6, y, SH110X_WHITE);
  display.drawLine(x + 6, y, x + 9, y, SH110X_WHITE);
  display.drawLine(x - 6, y - 6, x - 3, y - 3, SH110X_WHITE);
  display.drawLine(x + 6, y - 6, x + 3, y - 3, SH110X_WHITE);
  display.drawLine(x - 6, y + 6, x - 3, y + 3, SH110X_WHITE);
  display.drawLine(x + 6, y + 6, x + 3, y + 3, SH110X_WHITE);
}

void drawCloud(int x, int y) {
  display.fillCircle(x, y, 4, SH110X_WHITE);
  display.fillCircle(x + 6, y - 2, 5, SH110X_WHITE);
  display.fillCircle(x + 12, y, 4, SH110X_WHITE);
  display.fillRect(x, y, 16, 6, SH110X_WHITE);
}

void drawRain(int x, int y) {
  drawCloud(x, y);
  display.drawLine(x + 3, y + 8, x + 3, y + 12, SH110X_WHITE);
  display.drawLine(x + 8, y + 8, x + 8, y + 12, SH110X_WHITE);
  display.drawLine(x + 13, y + 8, x + 13, y + 12, SH110X_WHITE);
}

void drawBattery(int x, int y, int percentage) {
  display.drawRect(x, y, 26, 14, SH110X_WHITE);
  display.fillRect(x + 26, y + 4, 3, 6, SH110X_WHITE);
  display.fillRect(x + 1, y + 1, 24, 12, SH110X_BLACK);
  int barWidth = map(percentage, 0, 100, 0, 22);
  display.fillRect(x + 2, y + 9, barWidth, 3, SH110X_WHITE);
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(x + 5, y + 2);
  display.print(percentage);
  display.print("%");
}