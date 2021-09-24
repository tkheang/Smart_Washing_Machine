#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <NewPing.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// For debugging in serial monitor
#define DEBUG false

// For communication with Particle Photon
SoftwareSerial particlePhoton(11, 12);

// Ultrasonic Sensor HC-SR04
#define TRIGGER_PIN 9
#define ECHO_PIN 8
NewPing sonar(TRIGGER_PIN, ECHO_PIN);
int capacity = 0;

// Accelerometer/Gyroscope GY-521
const int MPU = 0x68;
float lidAngle = 0.0;
float vibration = 0.0;
float initialAcX;
float initialAcY;
float initialAcZ;

// Servo Motor
#define SERVO_PIN 10
#define BUTTON_PIN 2
Servo servo;
bool startPressed = false;

// Initialize state
int state = 0;

// Vibration detection algorithm
#define TIME_WINDOW 3000 // Vibrations must be present 3 seconds after initial detection for the washer to be running
#define TIME_UNTIL_DONE 300000 // Vibrations must not be present after 5 minutes for the washer to be done
//#define TIME_UNTIL_DONE 10000 // Set to 10 seconds for debugging
long lastIdleTime = 0;
long lastActiveTime = 0;

// Threshold values for sensors
#define MIN_CAPACITY 54
#define OPEN_LID_ANGLE 0.0
#define MIN_VIBRATION 24000.0

void setup() {
  Serial.begin(115200);
  particlePhoton.begin(115200);
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  // Get new sensor values before processing new states
  long now = millis();
  capacity = measureCapacity();
  lidAngle = measureLidAngle();
  vibration = measureVibration();

  // Capture time when washer is active
  if (vibration > MIN_VIBRATION) {
    lastActiveTime = now;
  }

  bool wasRecentlyActive = (now - lastActiveTime) < TIME_WINDOW;

  // Check if button is pressed
  if (digitalRead(BUTTON_PIN) == HIGH && startPressed == false && state == 0) {
    digitalWrite(LED_BUILTIN, HIGH);
    startPressed = true;
    servo.attach(SERVO_PIN);
    servo.write(100);
    delay(500);
    servo.write(0);
    delay(500);
    servo.detach();
  }
  if (digitalRead(BUTTON_PIN) == LOW && startPressed == true && state == 1) {
    digitalWrite(LED_BUILTIN, LOW);
    startPressed = false;
    servo.attach(SERVO_PIN);
    servo.write(100);
    delay(500);
    servo.write(0);
    delay(500);
    servo.detach();
    state = 0;
  }

  /* 
  State 0: Ready
  State 1: Washing
  State 2: Paused
  State 3: Finished
  */
  switch (state) {
    case 0:
      // Go to state 1 only if the washer has clothes, the lid is closed, and the start button is pressed, otherwise stay in state 0
      if (capacity < MIN_CAPACITY && startPressed == true) {
        state = 1;
        break;
      }
      else {
        state = 0;
        break;
      }
    case 1:
      // Go to state 2 if the lid is opened, otherwise stay in state 1
      if (lidAngle > OPEN_LID_ANGLE) {
        state = 2;
        break;
      }
      else {
        state = 1;
      }
      // Check for vibrations
      // Stay in state 1 if vibrations are detected after 3 seconds
      if (wasRecentlyActive) {
        state = 1;
        break;
      }
      // If washer was idle long enough, go to state 3
      else if (now > (lastActiveTime + TIME_UNTIL_DONE)) {
        state = 3;
        break;
      }
    case 2:
      // Go to state 1 if the lid is closed, otherwise stay in state 2
      if (lidAngle < OPEN_LID_ANGLE) {
        state = 1;
        break; 
      }
      else {
        state = 2;
        break;
      }
    case 3:
      // Go to state 0 if the washer is empty, otherwise stay in state 3
      if (capacity >= MIN_CAPACITY) {
        state = 0;
        startPressed = false;
        break;
      }
      else {
        state = 3;
        break;
      }
    default: 
      state = 0;
      startPressed = false;
      break;
  }

  // Send current state to Particle Photon
  particlePhoton.print(state); particlePhoton.print(',');
  particlePhoton.print(capacity); particlePhoton.print(',');
  particlePhoton.print(lidAngle); particlePhoton.print(',');
  particlePhoton.println(vibration);
  
  if (DEBUG) {
    Serial.print(state); Serial.print(',');
    Serial.print(capacity); Serial.print(',');
    Serial.print(lidAngle); Serial.print(',');
    Serial.println(vibration);
  }
  
  delay(1000);
}

int measureCapacity() {
  int d;
  d = sonar.ping_cm();
  return d;
}

float measureLidAngle() {
  float a;
  int16_t AcX, AcY, AcZ;
  float Xg, Yg, Zg;
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 12, true);
  AcX = Wire.read() << 8 | Wire.read();
  AcY = Wire.read() << 8 | Wire.read();
  AcZ = Wire.read() << 8 | Wire.read();
  Xg = AcX / 16384.0;
  Yg = AcY / 16384.0;
  Zg = AcZ / 16384.0;
  a = atan(Xg/sqrt((Yg * Yg) + (Zg * Zg))) * 57.2958;
  return a;
}

float measureVibration() {
  int16_t AcX, AcY, AcZ;
  float Xg, Yg, Zg;
  float total = 0;
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 12, true);
  AcX = Wire.read() << 8 | Wire.read();
  AcY = Wire.read() << 8 | Wire.read();
  AcZ = Wire.read() << 8 | Wire.read();
  total += fabs(AcX - initialAcX);
  total += fabs(AcY - initialAcX);
  total += fabs(AcZ - initialAcX);
  return total;
}
