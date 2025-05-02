#include <Servo.h>
#include <SoftwareSerial.h>
#include <SR04.h>
#include <DFRobotDFPlayerMini.h>

// === Pin Definitions ===
#define TRIG_PIN 4
#define ECHO_PIN 2
#define BUTTON_PIN 5
#define RX_PIN 11
#define TX_PIN 12
#define SX_SERVO_PIN 9
#define HEAD_SERVO_PIN 8

// === Ultrasonic Sensor ===
SR04 sr04 = SR04(ECHO_PIN, TRIG_PIN); //sensore distanza
const int DISTANCE_THRESHOLD = 5; // Change to 100 later

// === MP3 Player ===
SoftwareSerial mp3Serial(RX_PIN, TX_PIN);
DFRobotDFPlayerMini mp3;

// === Servo Motors ===
Servo armServo;
Servo headServo;

// === State Machine ===
enum RobotState {
  IDLE,
  WAVE,
  BUTTON_MUSIC
};

RobotState state = IDLE;

// === Variables ===
bool buttonPressed = false;
int currentTrack = 1;  // Track counter for sequential play
int totalTracks = 10;   // Total number of tracks in the folder (adjust as needed)
long distance;

// === Setup ===
void setup() {
  Serial.begin(9600);
  mp3Serial.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RX_PIN, INPUT);
  pinMode(TX_PIN, OUTPUT);

  if (!mp3.begin(mp3Serial)) {
    Serial.println("Unable to begin MP3 player. Check connections.");
    while (true); // Stop everything
  }

  mp3.setTimeOut(500);
  mp3.volume(30); // Set volume (0-30)
  mp3.EQ(DFPLAYER_EQ_NORMAL);
  Serial.println("MP3 player ready.");
}

// === Main Loop ===
void loop() {
  distance = sr04.Distance();
  buttonPressed = digitalRead(BUTTON_PIN) == LOW;
  delay(50); 
  switch (state) {
    case IDLE:
      if (buttonPressed) {
        state = BUTTON_MUSIC;
        Serial.println(">> Button pressed. Switching to BUTTON_MUSIC state");
      } else if (distance > 0 && distance < DISTANCE_THRESHOLD) {
        state = WAVE;
        Serial.println(">> Object detected. Switching to WAVE state");
      }
      break;

    case WAVE:
      performWave();
      state = IDLE;
      break;

    case BUTTON_MUSIC:
      // Play the music and perform the servo action
      //mp3.playFolder(2, currentTrack); // Folder 01, Track 001.mp3
      delay(2000); // Or wait for track to finish (can be improved)
      currentTrack++;
      if (currentTrack > totalTracks) {
        currentTrack = 1; // Reset to the first track
      }
      state = IDLE;
      break;
  }

  delay(100);
}

// === Actions ===
void performWave() {
  Serial.println("Waving...");
  int randomTrack = random(1, 4); 
  mp3.play(1);
  Serial.println("Play random track");
  delay(1000);
  armServo.attach(SX_SERVO_PIN);
  headServo.attach(HEAD_SERVO_PIN);
  armServo.write(90);
  headServo.write(90);
  delay(1000);

  for (int i = 0; i < 3; i++) {
    armServo.write(0);
    delay(300);
    armServo.write(180);
    delay(300);
  }
  armServo.write(90);
  delay(500);
  headServo.write(180);
  delay(1000);

  armServo.detach();
  headServo.detach();

}
