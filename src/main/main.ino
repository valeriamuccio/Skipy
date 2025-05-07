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

RobotState state = IDLE;

// === Touch Sensor ===
CapacitiveSensor cs = CapacitiveSensor(4, 2);

// === Led Strip ===
const int NUM_LEDS = 12;
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
long distance;
const unsigned long detectionCooldown = 60000;  // 1 minute
unsigned long lastDetectionTime = 0;
unsigned long headTouchStartTime = 0;
unsigned long lastQueueCallTime = 0;

// === HEAD Movement State ===
int headAngles[] = { 0, 15, 30, 45 };
int headStep = 0;
bool headMoving = false;
unsigned long lastHeadMoveTime = 0;
const unsigned long headStepDelay = 500;

// === ARM Movement State ===
int armStep = 0;
unsigned long lastArmMoveTime = 0;
const unsigned long armStepDelay = 200;
bool armMoving = false;
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
  headServo.write(0);

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
        state = GREET_PERSON;
        lastDetectionTime = millis();
        Serial.println(">> Person detected close to the robot");
      }
      break;

    case GREET_PERSON:
      headServo.write(0);
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
      Serial.println(">> Wait head touch");
      long touchValue = 0;
      touchValue = cs.capacitiveSensor(30);
      delay(50);
      if (touchValue > 800) {
        Serial.println(">> Head touched!");
        headServo.write(0);
        //If yes, send event to other modules and animation with led white , then state AWAIT_BALL
        state = AWAIT_BALL;
      } else if (millis() - headTouchStartTime > 40000) {
        Serial.println(">> Timeout, head not touched.");
        state = IDLE;
      };
      Serial.println("exit seek interaction");
      break;
  }


  // === HEAD MOVEMENT HANDLER ===
  if (headMoving && millis() - lastHeadMoveTime >= headStepDelay) {
    headServo.write(headAngles[headStep]);
    lastHeadMoveTime = millis();
    headStep++;
    if (headStep >= sizeof(headAngles) / sizeof(headAngles[0])) {
      headMoving = false;
    }
  }

  // === ARM MOVEMENT HANDLER ===
  if (armMoving && millis() - lastArmMoveTime >= armStepDelay) {
    int angle = armDirectionDown ? 180 : 135;
    if (armStep % 2 == 0) armServoSx.write(angle);
    else armServoDx.write(angle);

    lastArmMoveTime = millis();
    armStep++;

    if (armStep >= 8) {
      armMoving = false;
    } else if (armStep % 2 == 0) {
      armDirectionDown = !armDirectionDown;
    }
  }

  // === LED HANDLER ===
  if (ledAnimating && millis() - lastLedUpdate >= ledUpdateInterval) {
    lastLedUpdate = millis();

    if (ledMode == 0) {
      setStripColor((ledAnimationStep % 2) * 255, (ledAnimationStep % 2) * 255, (ledAnimationStep % 2) * 255);
    } else if (ledMode == 1) {
      if (ledAnimationStep % 2 == 0) {
        setStripColor(0, 255, 0);
      } else {
        setStripColor(0, 0, 255);
      }
    }

    ledAnimationStep++;
    if (ledAnimationStep >= 10) {
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
  int randomTrack = random(1, 4);  //TODO: Change folder to play robot sounds
  mp3.play(1);
  Serial.println("Play track");
  delay(1000);  //TODO: can it be removed??
  startArmMovement();
  startHeadMovement();
}

// === Start HEAD Movement ===
void startHeadMovement() {
  headStep = 0;
  lastHeadMoveTime = millis();
  headMoving = true;
}

// === Start ARM Movement ===
void startArmMovement() {
  armStep = 0;
  lastArmMoveTime = millis();
  armDirectionDown = true;
  armMoving = true;
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
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}
