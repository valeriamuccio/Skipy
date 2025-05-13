#include <Servo.h>
#include <SoftwareSerial.h>
#include <SR04.h>
#include <DFRobotDFPlayerMini.h>
#include <Adafruit_NeoPixel.h>
#include <CapacitiveSensor.h>
#include <Wire.h>

// === Pin Definitions ===
#define CAPACITIVE_SENSOR_RECEIVE_PIN 2
#define ECHO_PIN 3
#define TRIG_PIN 4
#define BUTTON_PIN 5
#define CAPACITIVE_SENSOR_SEND_PIN 6
#define HEAD_SERVO_PIN 8
#define DX_SERVO_PIN 9
#define SX_SERVO_PIN 10
#define RX_PIN 11
#define TX_PIN 12
#define LED_STRIP_PIN 13

// === Communication I2C ===
#define MY_ADDR 3

// === State Machine ===
enum RobotState {
  IDLE,
  GREET_PERSON,
  SEEK_INTERACTION,
  AWAIT_BALL,
  CELEBRATE,
  CALL_NEXT
};

enum ArmMotorsActions {
  NONE_ARM,
  HELLO
};

enum HeadMotorsActions {
  NONE_HEAD,
  PET
};

RobotState state = IDLE;
ArmMotorsActions armAction = NONE_ARM;
HeadMotorsActions headAction = NONE_HEAD;

// === Touch Sensor ===
CapacitiveSensor cs = CapacitiveSensor(4, 2);

// === Led Strip ===
const int NUM_LEDS = 9;
Adafruit_NeoPixel strip(NUM_LEDS, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);

// === Ultrasonic Sensor ===
const int DISTANCE_THRESHOLD = 5;  //TODO: Change to 50 later
SR04 sr04 = SR04(ECHO_PIN, TRIG_PIN);

// === MP3 Player ===
DFRobotDFPlayerMini mp3;
SoftwareSerial mp3Serial(RX_PIN, TX_PIN);

// === Servo Motors ===
Servo armServoSx;
Servo armServoDx;
Servo headServo;

// === Queue handling variables ===
bool buttonPressed = false;
bool headTouched = false;
int currentTrack = 0;
int totalTracks = 10;  // Total number of tracks in the folder (adjust as needed)
int currentQueuePos = 0;
int headTouchAttempt = 0;
long distance;
const unsigned long detectionCooldown = 60000;  // 1 minute
unsigned long lastDetectionTime = 0;
unsigned long headTouchStartTime = 0;
unsigned long lastQueueCallTime = 0;

// === HEAD Movement State ===
int headAngles[] = { 0, 0, 0, 5, 10, 15, 20, 25, 30 };
int headStep = 0;
unsigned long lastHeadMoveTime = 0;
const unsigned long headStepDelay = 80;

// === ARM Movement State ===
int armStep = 0;
unsigned long lastArmMoveTime = 0;
const unsigned long armStepDelay = 50;
bool armDirectionDown = true;

// === LED Animation State ===
unsigned long lastLedUpdate = 0;
const unsigned long ledUpdateInterval = 200;  // ms tra i cambi
int ledAnimationStep = 0;
bool ledAnimating = false;
int ledMode = 0;  // 0 = white ON/OFF, 1 = green/blue

// === Setup ===
void setup() {
  Serial.begin(9600);
  mp3Serial.begin(9600);

  Wire.begin(MY_ADDR);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);

  armServoDx.attach(DX_SERVO_PIN);
  armServoDx.write(90);
  armServoSx.attach(SX_SERVO_PIN);
  armServoSx.write(90);
  headServo.attach(HEAD_SERVO_PIN);
  headServo.write(10);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RX_PIN, INPUT);
  pinMode(TX_PIN, OUTPUT);

  if (!mp3.begin(mp3Serial)) {
    Serial.println("Unable to begin MP3 player. Check connections.");
    while (true)
      ;  // Stop everything
  }
  mp3.setTimeOut(500);
  mp3.volume(30);
  mp3.EQ(DFPLAYER_EQ_NORMAL);

  strip.begin();
  strip.show();
  Serial.println("MP3 player ready.");
}

// === Main Loop ===
void loop() {
  checkIfButtonPressed();

  switch (state) {
    case IDLE:
      distance = sr04.Distance();
      if (currentQueuePos != currentTrack || millis() - lastQueueCallTime > 50000) {
        state = CALL_NEXT;
        lastQueueCallTime = millis();
        Serial.println(">> Current queue pos != current track or expired");
      } else if (distance > 0 && distance < DISTANCE_THRESHOLD) {  //&& millis() - lastDetectionTime > detectionCooldown) {  //if last time detectione expired
        headServo.write(10);
        state = GREET_PERSON;
        lastDetectionTime = millis();
        Serial.println(">> Person detected close to the robot");
      }
      break;

    case GREET_PERSON:
      headServo.write(10);
      Serial.println(">> Greeting person");
      performGreetings();
      state = SEEK_INTERACTION;
      headTouchStartTime = millis();
      break;

    case AWAIT_BALL:
      //check message for the ball
      Serial.println(state);
      delay(1000);
      Serial.println(">> Waiting for ball message");
      state = CELEBRATE;
      break;

    case CELEBRATE:
      Serial.println(">> Celebrating new ball!");
      delay(1000);
      startLedAnimation(1);
      state = IDLE;
      break;


    case CALL_NEXT:
      Serial.println(">> Calling next number in queue");
      // Play the music and perform the servo action
      //mp3.playFolder(2, currentTrack); // Folder 01, Track 001.mp3
      mp3.play(1);
      delay(1000);  // Or wait for track to finish (can be improved) REMOVE??
      if (currentTrack > totalTracks) {
        currentTrack = 0;  // Reset to the first track
      }
      currentTrack = currentQueuePos;
      startLedAnimation(2);
      Serial.println(currentTrack, currentQueuePos);
      state = IDLE;
      break;


    case SEEK_INTERACTION:
      if (headAction != NONE_HEAD || armAction != NONE_HEAD) break;
      Serial.println(">> Wait head touch");
      long touchValue = 0;
      touchValue = cs.capacitiveSensor(30);
      Serial.println(touchValue);
      delay(80);
      if (touchValue > 400) {
        Serial.println(">> Head touched!");
        headServo.write(25);
        delay(50);
        headServo.write(15);
        delay(50);
        headServo.write(10);

        //If yes, send event to other modules and animation with led white , then state AWAIT_BALL
        state = AWAIT_BALL;
      }else if (headTouchAttempt > 5) {
        Serial.println(">> Timeout, head not touched.");
        state = IDLE;
      }
      else if (millis() - headTouchStartTime > 3000) {
        headServo.write(25);
        armServoDx.write(180);
        delay(100);
        headServo.write(30);
        armServoDx.write(170);
        delay(100);
        headServo.write(25);
        armServoDx.write(180);
        delay(100);
         headServo.write(25);
        armServoDx.write(180);
        delay(100);
        headServo.write(30);
        armServoDx.write(170);
        delay(100);
        headServo.write(25);
        armServoDx.write(180);
        delay(100);
        headTouchStartTime = millis();
        headTouchAttempt++;
        break;
      };
      headTouchAttempt = 0;
      Serial.println("exit seek interaction");
      break;
  }


  // === HEAD MOVEMENT HANDLER ===
  if (headAction != NONE_HEAD && millis() - lastHeadMoveTime >= headStepDelay) {
    if (headAction == PET) { headPettingAction(); }
  }

  // === ARM MOVEMENT HANDLER ===
  if (armAction != NONE_ARM && millis() - lastArmMoveTime >= armStepDelay) {
    if (armAction == HELLO) { sayHelloAction(); }
  }

  // === LED HANDLER ===
  if (ledAnimating && millis() - lastLedUpdate >= ledUpdateInterval) {
    Serial.println("Led");
    lastLedUpdate = millis();
    setStripColor(255, 255, 255);

    ledAnimationStep++;
    if (ledAnimationStep >= 10) {
      setStripColor(0, 0, 0);
      headServo.write(10);
      ledAnimating = false;
    }
  }
}

// === Check input ===
void checkIfButtonPressed() {
  buttonPressed = digitalRead(BUTTON_PIN) == LOW;
  delay(50);
  if (buttonPressed) {
    currentQueuePos++;
  }
}

// === Actions ===
void performGreetings() {
  Serial.println("Start greetings...");
  mp3.play(3);
  Serial.println("Play track");
  delay(1000);  //TODO: can it be removed??
  startArmMovement();
}

// === Start HEAD Movement ===
void startHeadMovement() {
  headStep = 0;
  lastHeadMoveTime = millis();
  headAction = PET;
}

// === Start ARM Movement ===
void startArmMovement() {
  armStep = 0;
  lastArmMoveTime = millis();
  armDirectionDown = true;
  armAction = HELLO;
}

// === Start LED Animation ===
void startLedAnimation(int mode) {
  ledMode = mode;
  ledAnimationStep = 0;
  ledAnimating = true;
  lastLedUpdate = millis();
}

// === COMMUNICATION ===
void onReceive() {
  Serial.println("onReceive");
}

void onRequest() {
  Serial.println("onRequest");
}


// === UTILS ===
void setStripColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));  // bianco
  }
  strip.show();
}

void sayHelloAction() {
  int angle = armDirectionDown ? 180 : 145;
  if (armStep % 2 == 0) armServoSx.write(angle);
  else armServoDx.write(angle);

  lastArmMoveTime = millis();
  armStep++;

  if (armStep >= 12) {
    armAction = NONE_ARM;
    startHeadMovement();
  } else if (armStep % 2 == 0) {
    armDirectionDown = !armDirectionDown;
  }
}

void headPettingAction() {
  headServo.write(headAngles[headStep]);
  lastHeadMoveTime = millis();
  headStep++;
  if (headStep >= sizeof(headAngles) / sizeof(headAngles[0])) {
    headAction = NONE_HEAD;
  }
}
