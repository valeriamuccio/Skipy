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
const uint8_t MY_ADDR = 0b011;

// —— Commands ——
//GENERAL
const uint8_t MASTER_ADDR = 0b001;
const uint8_t START_SYS_CMD = 0b00001;  //M → L,C,A
const uint8_t STOP_SYS_CMD = 0b11111;   //M → L,C,A

const uint8_t ASK_READY_CMD = 0b00011;   //M → L,C,A
const uint8_t TELL_READY_CMD = 0b11100;  //M ← L,C,A

//COMMUNICATION
const uint8_t START_INTERACTION_CMD = 0b01000;    //M → C
const uint8_t STARTED_INTERACTION_CMD = 0b00100;  //M ← C
const uint8_t ENDED_INTERACTION_CMD = 0b01111;    //M ← C
const uint8_t NOTIFY_HEADTOUCH_CMD = 0b01001;     //M ← C !!!
const uint8_t NOTIFY_LUCKYBALL_CMD = 0b01010;     //M → C !!!
const uint8_t NOTIFY_NORMALBALL_CMD = 0b01011;    //M → C !!!

// === States ===
enum RobotState {
  IDLE,
  INITIAL_INTERACTION,
  GREET_PERSON,
  SEEK_INTERACTION,
  AWAIT_BALL,
  CELEBRATE,
  CALL_NEXT,
  END_INTERACTION
};

enum ArmMotorsActions {
  NONE_ARM,
  HELLO,
  SMALL_SHAKE
};

enum HeadMotorsActions {
  NONE_HEAD,
  PET,
  RESET_POSITION,
  SMALL_SHAKE_HEAD
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

boolean hasMsg = false;

// === MP3 Player ===
DFRobotDFPlayerMini mp3;
SoftwareSerial mp3Serial(RX_PIN, TX_PIN);

// === Servo Motors ===
Servo armServoSx;
Servo armServoDx;
Servo headServo;

// === Queue handling variables ===
const int totalTracks = 10;  // Total number of tracks in the folder (adjust as needed)
bool buttonPressed = false;
int currentTrack = 0;
int currentQueuePos = 0;
int headTouchAttempt = 0;
const unsigned long detectionCooldown = 60000;  // 1 minute
unsigned long lastDetectionTime = 0;
unsigned long headTouchStartTime = 0;
unsigned long lastQueueCallTime = 0;

// === HEAD Movement State ===
int currentHeadAngle = 0;
int headStep = 0;
unsigned long lastHeadMoveTime = 0;
const unsigned long headStepDelay = 80;

// === ARM Movement State ===
int armStep = 0;
unsigned long lastArmMoveTime = 0;
const unsigned long armStepDelay = 80;
bool armDirectionDown = true;

// === LED Animation State ===
unsigned long lastLedUpdate = 0;
const unsigned long ledUpdateInterval = 200;
int ledAnimationStep = 0;
bool ledAnimating = false;

// === Setup ===
void setup() {
  Serial.begin(9600);
  //mp3Serial.begin(9600);

  Wire.begin(3);
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

  strip.begin();
  strip.show();

  // if (!mp3.begin(mp3Serial)) {
  //   Serial.println("Unable to begin MP3 player. Check connections and reset robot.");  //LED ROSSI
  //   setStripColor(255, 0, 0);
  //   strip.show();

  //   while (true)
  //     ;
  // }
  // mp3.setTimeOut(500);
  // mp3.volume(30);
  // mp3.EQ(DFPLAYER_EQ_NORMAL);

  Serial.println("Robot ready.");
}

// === Main Loop ===
void loop() {
  checkIfButtonPressed();

  switch (state) {
    case IDLE:
      //nothing
      break;
    case INITIAL_INTERACTION:
      if (currentQueuePos != currentTrack || millis() - lastQueueCallTime > 50000) {
        state = CALL_NEXT;
        lastQueueCallTime = millis();
        Serial.println(">> Current queue pos != current track or expired");
      } else {
        long distance = sr04.Distance();
        if (true) {
          state = GREET_PERSON;
          lastDetectionTime = millis();
          Serial.println(">> Person detected close to the robot");
        }
      }
      break;
    case GREET_PERSON:
      currentHeadAngle = 10;  //default position
      headServo.write(currentHeadAngle);
      Serial.println(">> Greeting person");
      performGreetingsAction();
      state = SEEK_INTERACTION;
      headTouchStartTime = millis();
      break;

    case AWAIT_BALL:
      Serial.println(state);
      Serial.println(">> Waiting for ball message");
      //state = CELEBRATE;
      break;

    case CELEBRATE:
      Serial.println(">> Celebrating new ball!");
      delay(1000);
      startHeadMovement(RESET_POSITION);
      startLedAnimation();
      state = END_INTERACTION;
      hasMsg = true;
      break;


    case CALL_NEXT:
      Serial.println(">> Calling next number in queue");
      // Play the music and perform the servo action
      //mp3.playFolder(2, currentTrack); // Folder 01, Track 001.mp3
      // mp3.playFolder(1, 1);
      delay(1000);  // Or wait for track to finish (can be improved) REMOVE??
      if (currentTrack > totalTracks) {
        currentTrack = 0;  // Reset to the first track
      }
      currentTrack = currentQueuePos;
      startLedAnimation();
      Serial.println(currentTrack, currentQueuePos);
      state = IDLE;
      break;

    case END_INTERACTION:
      //manda messaggio, aspetta, dopo passa a IDLE
      state = IDLE;
      break;


    case SEEK_INTERACTION:
      if (headAction != NONE_HEAD || armAction != NONE_HEAD) break;
      Serial.println(">> Wait head touch");
      long touchValue = 0;
      touchValue = cs.capacitiveSensor(30);
      Serial.println(touchValue);
      delay(80);
      if (true) {
        Serial.println(">> Head touched!");
        //If yes, send event to other modules and animation with led white , then state AWAIT_BALL
        hasMsg = true;
        state = AWAIT_BALL;
      } else if (headTouchAttempt > 5) {
        Serial.println(">> Timeout, head not touched.");
        state = END_INTERACTION;
      } else if (millis() - headTouchStartTime > 3000) {
        indicateHeadAction();
        headTouchAttempt++;
        headTouchStartTime = millis();
        break;
      };
      headTouchAttempt = 0;
      Serial.println("exit seek interaction");
      break;
  }


  // === HEAD MOVEMENT HANDLER ===
  if (headAction != NONE_HEAD && millis() - lastHeadMoveTime >= headStepDelay) {
    if (headAction == PET) { headPettingAction(); }
    if (headAction == RESET_POSITION) { resetHeadPositionAction(); }
    if (headAction == SMALL_SHAKE_HEAD) { headSmallShakeAction(); }
  }

  // === ARM MOVEMENT HANDLER ===
  if (armAction != NONE_ARM && millis() - lastArmMoveTime >= armStepDelay) {
    if (armAction == HELLO) { sayHelloAction(); }
    if (armAction == SMALL_SHAKE) { smallShakeAction(); }
  }

  // === LED HANDLER ===
  if (ledAnimating && millis() - lastLedUpdate >= ledUpdateInterval) {
    Serial.println("Led");
    ledAnimationAction();
  }
}

// === Check input ===
void checkIfButtonPressed() {
  buttonPressed = digitalRead(BUTTON_PIN) == LOW;
  delay(50);
  if (buttonPressed) {
    // mp3.playFolder(1, 5);
    delay(80);
    Serial.println(currentQueuePos);
    currentQueuePos++;
  }
}

// === Actions ===
void performGreetingsAction() {
  Serial.println("Start greetings...");
  //mp3.playFolder(1, 3);
  delay(80);
  Serial.println("Play track");
  startArmMovement(HELLO);
}

void sayHelloAction() {
  int angle = armDirectionDown ? 180 : 145;
  if (armStep % 2 == 0) armServoSx.write(angle);
  else armServoDx.write(angle);

  lastArmMoveTime = millis();
  armStep++;

  if (armStep >= 12) {
    armAction = NONE_ARM;
    startHeadMovement(PET);
  } else if (armStep % 2 == 0) {
    armDirectionDown = !armDirectionDown;
  }
}

void smallShakeAction() {
  int angle = armDirectionDown ? 180 : 170;
  if (armStep % 2 == 0) armServoSx.write(angle);
  else armServoDx.write(angle);

  lastArmMoveTime = millis();
  armStep++;

  if (armStep >= 6) {
    armAction = NONE_ARM;
  } else if (armStep % 2 == 0) {
    armDirectionDown = !armDirectionDown;
  }
}

void headPettingAction() {
  currentHeadAngle = currentHeadAngle + 5;
  headServo.write(currentHeadAngle);
  lastHeadMoveTime = millis();
  if (currentHeadAngle == 35) {
    headAction = NONE_HEAD;
  }
}

void resetHeadPositionAction() {
  currentHeadAngle = currentHeadAngle - 5;
  headServo.write(currentHeadAngle);
  lastHeadMoveTime = millis();
  if (currentHeadAngle == 0) {
    headAction = NONE_HEAD;
  }
}

void headSmallShakeAction() {
  currentHeadAngle = currentHeadAngle != 25 ? 25 : 30;
  headServo.write(currentHeadAngle);
  lastHeadMoveTime = millis();
  headStep++;
  if (headStep > 6) {
    headAction = NONE_HEAD;
  }
}

void indicateHeadAction() {
  startArmMovement(SMALL_SHAKE);
  startHeadMovement(SMALL_SHAKE_HEAD);
}

void ledAnimationAction() {
  lastLedUpdate = millis();
  setStripColor(0, 255, 0);
  ledAnimationStep++;
  if (ledAnimationStep >= 15) {
    setStripColor(0, 0, 0);
    ledAnimating = false;
  }
}

// === HEAD Movement ===
void startHeadMovement(HeadMotorsActions action) {
  //lastHeadMoveTime = millis();
  headAction = action;
  headStep = 0;
}

// === ARM Movement ===
void startArmMovement(ArmMotorsActions action) {
  armStep = 0;
  //lastArmMoveTime = millis();
  armDirectionDown = true;
  armAction = action;
}

// === Start LED Animation ===
void startLedAnimation() {
  ledAnimationStep = 0;
  ledAnimating = true;
  //lastLedUpdate = millis();
}

// === COMMUNICATION ===
void onReceive(int) {
  Serial.println("onReceive");
  if (Wire.available()) {
    uint8_t v = Wire.read();
    uint8_t dest = v >> 5;
    uint8_t cmd = v & 0x1F;
    String bits = byteToBitString(v);
    Serial.print(" (dest=");
    Serial.print(dest);
    Serial.print("): ");
    Serial.println(bits);

    handle_message(v);
  }
}

void onRequest() {
  //If HeadTouched --> send it during AWAIT_BALL
  if (state == AWAIT_BALL && hasMsg) {
    uint8_t msg = (MASTER_ADDR << 5) | NOTIFY_HEADTOUCH_CMD;
    Wire.write(msg);
    Serial.println("Inviato msg");
    Serial.println(msg);
    hasMsg = false;
  }
  if ((state == END_INTERACTION || CELEBRATE) && hasMsg) {
    uint8_t msg = (MASTER_ADDR << 5) | ENDED_INTERACTION_CMD;
    Wire.write(msg);
    Serial.println("Inviato msg");
    Serial.println(msg);
    hasMsg = false;
  }
}


// === UTILS ===
void setStripColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

String byteToBitString(uint8_t v) {
  String r;
  for (int8_t i = 7; i >= 0; i--) r += char('0' + ((v >> i) & 1));
  return r;
}

// — New function: split, match, act —
void handle_message(uint8_t raw) {
  uint8_t dest = raw >> 5;
  uint8_t cmd = raw & 0x1F;
  // only handle messages addressed to me:
  if (dest != MY_ADDR) return;

  switch (cmd) {
    case START_INTERACTION_CMD:
      //It's possible to start interation
      Serial.println(">>> START_INTERACTION_CMD received");
      state = INITIAL_INTERACTION;
      break;

    case NOTIFY_LUCKYBALL_CMD:
      // Lucky ball has been extracted by actuation2
      // Forward the message to communication
      Serial.println(">>> DETECTED_LUCKYBALL_CMD received");
      state = CELEBRATE;
      //SET state select music and so on
      break;

    case NOTIFY_NORMALBALL_CMD:
      // Normal ball has been extracted by actuation2
      // Forward the message to communication
      Serial.println(">>> DETECTED_NORMALBALL_CMD received");
      state = CELEBRATE;
      //SET state select music and so on
      break;

    default:
      Serial.print("Ignored message");
      Serial.println(cmd);
      break;
  }
}
