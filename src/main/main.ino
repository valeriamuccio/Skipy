#include <Servo.h>
#include <SoftwareSerial.h>
#include <SR04.h>
#include <DFRobotDFPlayerMini.h>
#include <Adafruit_NeoPixel.h>
#include <CapacitiveSensor.h>

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

// === Variables ===
bool buttonPressed = false;
int currentTrack = 0;  // Track counter for sequential play
int totalTracks = 10;  // Total number of tracks in the folder (adjust as needed)
int currentQueuePos = 0;
long distance;
unsigned long lastDetectionTime = 0;
const unsigned long detectionCooldown = 60000;  // 1 minute
unsigned long headTouchStartTime = 0;
bool headTouched = false;

// === Setup ===
void setup() {
  Serial.begin(9600);
  mp3Serial.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RX_PIN, INPUT);
  pinMode(TX_PIN, OUTPUT);

  if (!mp3.begin(mp3Serial)) {
    Serial.println("Unable to begin MP3 player. Check connections.");
    while (true)
      ;  // Stop everything
  }

  mp3.setTimeOut(500);  //TODO : should it be increased?
  mp3.volume(30);       // Set volume (0-30)
  mp3.EQ(DFPLAYER_EQ_NORMAL);
  strip.begin();
  strip.show();
  Serial.println("MP3 player ready.");
}

// === Main Loop ===
void loop() {
  buttonPressed = digitalRead(BUTTON_PIN) == LOW;
  delay(50);
  if (buttonPressed) {
    currentQueuePos++;
  }

  switch (state) {
    case IDLE:
      distance = sr04.Distance();
      if (currentQueuePos != currentTrack) {
        state = CALL_NEXT;
        Serial.println(">> Current queue pos != current track");
      } else if (distance > 0 && distance < DISTANCE_THRESHOLD) {  //&& millis() - lastDetectionTime > detectionCooldown) {  //if last time detectione expired
        state = GREET_PERSON;
        lastDetectionTime = millis();
        Serial.println(">> Person detected close to the robot");
      }
      break;

    case GREET_PERSON:
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
      //reproduce sounds
      //color led
      Serial.println(">> Celebrating new ball!");
      // for (int i = 0; i < 5; i++) {
      //   setStripColor(255, 0, 0);  // Red
      //   delay(200);
      //   setStripColor(0, 255, 0);  // Green
      //   delay(200);
      //   setStripColor(0, 0, 255);  // Blu
      //   delay(200);
      //   setStripColor(0, 0, 0);  // Off
      //   delay(200);
      // }
      state = IDLE;
      //reset position head and arm.
      break;


    case CALL_NEXT:
      Serial.println(">> Calling next number in queue");
      // Play the music and perform the servo action
      //mp3.playFolder(2, currentTrack); // Folder 01, Track 001.mp3
      delay(1000);  // Or wait for track to finish (can be improved) REMOVE??
      currentTrack++;
      if (currentTrack > totalTracks) {
        currentTrack = 1;  // Reset to the first track
      }
      currentQueuePos = currentTrack;
      state = IDLE;
      break;


    case SEEK_INTERACTION:
      //check if head has been touched
      Serial.println(">> Wait head touch");
      long touchValue = 0;
      touchValue = cs.capacitiveSensor(30);
      if (touchValue > 800) {
        Serial.println(">> Head touched!");
        //If yes, send event to other modules and animation with led white , then state AWAIT_BALL
        state = AWAIT_BALL;
        delay(50);
      } else if (millis() - headTouchStartTime > 40000) {
        Serial.println(">> Timeout, head not touched.");
        state = IDLE;
      };
      Serial.println("exit seek interaction");
      break;
  }
  delay(100);
}

// === Actions ===
void performGreetings() {
  Serial.println("Start greetings...");
  int randomTrack = random(1, 4);  //TODO: Change folder to play robot sounds
  mp3.play(1);
  Serial.println("Play track");
  delay(1000);  //TODO: can it be removed??

  armServoDx.attach(DX_SERVO_PIN);  //TODO: Still required?? It was to solve issue when sending commands to MP3
  armServoSx.attach(SX_SERVO_PIN);
  headServo.attach(HEAD_SERVO_PIN);

  armServoDx.write(180);
  headServo.write(0);
  delay(1000);
  armServoDx.write(90);
  headServo.write(45);
  delay(1000);
  armServoDx.write(180);
  headServo.write(0);
  delay(1000);

  armServoDx.detach();
  armServoSx.detach();
  headServo.detach();
}

void setStripColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}
