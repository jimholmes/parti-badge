/*
 *      Copyright 2018 Particle Industries, Inc.
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */
/*
 *      Project: #PartiBadge, 2018 THAT Conference Edition
 *
 *      Description:
 *          This is the firmware for a custom, Photon-based badge PCB that includes:
 *              1. A 1" OLED Screen
 *              2. An SMD Piezo buzzer
 *              3. A SPDT Switch
 *              4. A 5-way joystick
 *              5. An SMD Si7021 temperature and Humidity sensor
 *              6. 4 Tactile LED Buttons in Red, Blue, Green and Yellow/Orange
 *              7. An I2C-Compatible breakout for #BadgeLife add-ons
 *              8. An IR Receiver on the Photon TX pin
 *
 */

#include "Particle.h"
#include "Debounce.h"

// Display includes
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"

// IR Sensor include
#include "IRremoteLearn.h"

#include "images/spark.h"
#include "images/temp.h"
#include "images/humidity.h"
#include "parti-badge.h" // #define pin assignments and other general macros
#include "music/tones.h" // Peizo Sounds
#include "music/roll.h"
#include "WearerInfo/WearerInfo.h"

#include "Adafruit_Si7021.h"
#include "events/events.h"

SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);

PRODUCT_ID(7775);
PRODUCT_VERSION(18);

// Init Display
Adafruit_SSD1306 display(RESET);
void resetDisplayBools();
void showTitle();

// Init IR Receiver
IRrecv irrecv(IR_RECEIVER_PIN);
decode_results irResults;

#include "simonsays/simon.h" // Simon Says Code

String deviceId;
WearerInfo wearerInfo;

// Button Debounce Support
Debounce redButtonADebouncer = Debounce();
Debounce blueButtonBDebouncer = Debounce();
Debounce greenButtonCDebouncer = Debounce();
Debounce yellowButtonDDebouncer = Debounce();

// Debouncers for 5-way tactile switch
Debounce joystickUpDebouncer = Debounce();
Debounce joystickDownDebouncer = Debounce();
Debounce joystickLeftDebouncer = Debounce();
Debounce joystickRightDebouncer = Debounce();
Debounce joystickCenterDebouncer = Debounce();

// Initialize Si7021 sensor
Adafruit_Si7021 envSensor = Adafruit_Si7021();
int currentTemp;
int currentHumidity;

// Timing variables
unsigned long elapsedTime;
unsigned long startTime = 0;
unsigned long previousEnvReading = 0;

// Wearer details
String wearerFirstName;
String wearerLastName;

// Default to display mode, but we'll determine this based on a
// the user holding a button down at startup
int badgeMode = DISPLAY_MODE;

// Display variables
bool displayingTemp = false;
bool displayingLogo = false;
bool displayingTitle = false;
bool displayingWearerDetails = false;
bool displayingAnimations = false;
bool animationsInterrupted = false;
bool displayingEtchASketch = false;
bool playingRoll = false;
bool inCodeMode = false;

#include "keylogger/keylogger.h"
#include "animations/animations.h"

// Display state management
bool titleShown = false;
bool buttonsInitialized = false;
bool checkingInputs = false;
int displayX = display.width()/2;
int displayY = display.height()/2;

void setup() {
  Serial.begin(115200);
  resetDisplayBools();

  // Get the current deviceId
  deviceId = System.deviceID();

  // Initialize Temp and Humidity sensor
  while(! envSensor.begin());

  // Init OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  // Enable IR Receiver
  irrecv.enableIRIn();

  // Show Particle Splashscreen
  showSplashscreen();

  // Show the title screen
  showTitle();

  // Set up cloud variables and functions
  cloudInit();

  rollSetup();

  // Set the Piezo buzzer as an output
  pinMode(BUZZER_PIN, OUTPUT);

  // Init the LED Buttons
  initButtons();

  // Set up Interrupts
  attachInterrupt(BLUE_BUTTON_B, handleInterrupt, CHANGE);
  attachInterrupt(GREEN_BUTTON_C, handleInterrupt, CHANGE);
  attachInterrupt(YELLOW_BUTTON_D, handleInterrupt, CHANGE);

  // Get an initial temp and humidity reading
  getTempAndHumidity();

  //Init Tactile LED Buttons
  initLEDButtons();

  // Play a startup sound on the Piezo
  if (!startupSoundPlayed) playStartup(BUZZER_PIN);

  // Determine if we're in display or game mode
  checkBadgeMode();

  // Connect to the Particle device cloud
  Particle.connect();

  // Fetch badge wearer details from EEPROM
  initWearerDetails();

  // Scroll the title text on the screen
  display.startscrollleft(0x00, 0x0F);
}

void loop() {
  unsigned long currentMillis = millis();

  if (badgeMode == DISPLAY_MODE) {
    redButtonADebouncer.update();
    if (redButtonADebouncer.read() == LOW && ! displayingAnimations && ! animationsInterrupted && ! inCodeMode) {
      resetDisplayBools();

      displayingAnimations = true;
      toggleAllButtons(LOW);
      digitalWrite(RED_LED, HIGH);

      clearScreen();
      cycleAnimations();
    }

    blueButtonBDebouncer.update();
    if (blueButtonBDebouncer.read() == LOW && ! displayingTemp) {
      resetDisplayBools();

      displayingTemp = true;
      toggleAllButtons(LOW);
      digitalWrite(BLUE_LED, HIGH);

      // Show Temp and Humidity on Display
      showTempAndHumidity();
    }

    greenButtonCDebouncer.update();
    if (greenButtonCDebouncer.read() == LOW && ! displayingEtchASketch) {
      resetDisplayBools();

      displayingEtchASketch = true;
      toggleAllButtons(LOW);
      digitalWrite(GREEN_LED, HIGH);

      initEtchASketch();
    }

    yellowButtonDDebouncer.update();
    if (yellowButtonDDebouncer.read() == LOW && ! displayingWearerDetails) {
      resetDisplayBools();
      displayingWearerDetails = true;

      displayWearerDetails();
      toggleAllButtons(LOW);
      digitalWrite(YELLOW_LED, HIGH);
    }

    if (currentMillis - previousEnvReading > TEMP_CHECK_INTERVAL) {
      previousEnvReading = currentMillis;
      getTempAndHumidity();
    }

    if (displayingEtchASketch) {
      etchASketch();
    }

    if (digitalRead(D7) == HIGH || playingRoll) {
      playingRoll = true;

      playRoll(&display);
    }

    // Check the IR Receiver for any signals in the ether
    if (irrecv.decode(&irResults)) {
      irDump(&irResults);
      irrecv.resume(); // Receive the next value
    }

    checkInputSequence();
  } else if (badgeMode == GAME_MODE) {
    configureGame();

    playGame();
  }
}

// Show the Spark on startup
void showSplashscreen() {
  clearScreen();
  display.drawBitmap(0, 0, sparkLogo, 128, 64, 1);
  display.display();
  delay(3000);
}

// Switch to Simon Says mode if the A and B buttons are held down at startup
void checkBadgeMode() {
  redButtonADebouncer.update();
  blueButtonBDebouncer.update();

if (redButtonADebouncer.read() == LOW && blueButtonBDebouncer.read() == LOW) {
  badgeMode = GAME_MODE;
  } else {
    badgeMode = DISPLAY_MODE;
  }
}

// Init our Device Cloud variables and functions
void cloudInit() {
  Particle.variable("wearerFName", wearerFirstName);
  Particle.variable("wearerLName", wearerLastName);

  Particle.variable("currentTemp", currentTemp);
  Particle.variable("currentHu", currentHumidity);

  Particle.function("updateFName", updateFirstNameHandler);
  Particle.function("updateLName", updateLastNameHandler);

  Particle.function("checkTemp", checkTempHandler);
}

// Fetch wearer details from our WearerInfo class
void initWearerDetails() {
  wearerInfo = WearerInfo();

  if (wearerInfo.isSet()) {
    wearerFirstName = wearerInfo.getFirstName();
    wearerLastName = wearerInfo.getLastName();
  }
}

// Show the title text on the display
void showTitle() {
  titleShown = true;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextWrap(true);
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.println();
  display.println();
  display.println(" #PartiBadge v2");
  display.println();
  display.println(" THAT Conference 2018");
  display.display();
}

// Display the wearer's first and last name on the display
void displayWearerDetails() {
  int fnameLength = wearerFirstName.length();
  int lnameLength = wearerLastName.length();
  int longestLength = ((fnameLength > lnameLength) ? fnameLength : lnameLength);

  if (fnameLength > 0 || lnameLength > 0) {
    clearScreen();

    // setTextSize based on largest of two lengths
    // Display is 128 x 64
    // So if the longest of the two names is longer than 10 characters,
    // set the size to 1
    if (longestLength > 10) {
      display.setTextSize(1);
      display.setCursor(0, 20);

    } else {
      display.setTextSize(2);
      display.setCursor(0, 10);
    }

    display.println();

    if (wearerFirstName.length() > 0) {
      display.println(wearerFirstName);
    }
    if (wearerLastName.length() > 0) {
      display.println(wearerLastName);
    }

    display.display();
    display.startscrollleft(0x00, 0x0F);
  }
}

// Init debouncers for all of our inputs
void initButtons() {
  // Init Buttons as Inputs
  redButtonADebouncer.attach(RED_BUTTON_A, INPUT_PULLUP);
  redButtonADebouncer.interval(DEBOUNCE_INTERVAL);
  blueButtonBDebouncer.attach(BLUE_BUTTON_B, INPUT_PULLUP);
  blueButtonBDebouncer.interval(DEBOUNCE_INTERVAL);
  greenButtonCDebouncer.attach(GREEN_BUTTON_C, INPUT_PULLUP);
  greenButtonCDebouncer.interval(DEBOUNCE_INTERVAL);
  yellowButtonDDebouncer.attach(YELLOW_BUTTON_D, INPUT_PULLUP);
  yellowButtonDDebouncer.interval(DEBOUNCE_INTERVAL);

  // Joystick buttons as Inputs
  joystickUpDebouncer.attach(JOYSTICK_UP, INPUT_PULLUP);
  joystickUpDebouncer.interval(DEBOUNCE_INTERVAL);
  joystickDownDebouncer.attach(JOYSTICK_DOWN, INPUT_PULLUP);
  joystickDownDebouncer.interval(DEBOUNCE_INTERVAL);
  joystickLeftDebouncer.attach(JOYSTICK_LEFT, INPUT_PULLUP);
  joystickLeftDebouncer.interval(DEBOUNCE_INTERVAL);
  joystickRightDebouncer.attach(JOYSTICK_RIGHT, INPUT_PULLUP);
  joystickRightDebouncer.interval(DEBOUNCE_INTERVAL);
  joystickCenterDebouncer.attach(JOYSTICK_CENTER, INPUT_PULLUP);
  joystickCenterDebouncer.interval(DEBOUNCE_INTERVAL);
}

void handleInterrupt() {
  Serial.println(displayingAnimations);
  if (displayingAnimations) {
    displayingAnimations = false;
    animationsInterrupted = true;
  }
}

// Set up the tactile LED buttons
void initLEDButtons() {
  buttonsInitialized = true;

  int del = 300;
  int medDel = 500;

  // Init D7
  pinMode(D7, INPUT_PULLDOWN);

  // Init LEDs and Outputs
  pinMode(RED_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);

  digitalWrite(RED_LED, HIGH);
  delay(del);
  digitalWrite(BLUE_LED, HIGH);
  delay(del);
  digitalWrite(GREEN_LED, HIGH);
  delay(del);
  digitalWrite(YELLOW_LED, HIGH);
  delay(del);

  toggleAllButtons(LOW);
  delay(medDel);

  toggleAllButtons(HIGH);
  delay(medDel);

  toggleAllButtons(LOW);
  delay(medDel);

  toggleAllButtons(HIGH);
}

// Show the temperature and humidity on the display
void showTempAndHumidity() {
  clearScreen();

  display.drawBitmap(7, 13, tempImage, 16, 43, 1);
  display.setTextSize(1);
  display.println();
  display.println("         Temp");
  display.setTextSize(2);
  display.print("    ");
  display.print((int)currentTemp);
  display.println("f");
  display.setTextSize(1);
  display.println();
  display.println("       Humidity");
  display.setTextSize(2);
  display.print("    ");
  display.print((int)currentHumidity);
  display.println("%");
  display.drawBitmap(105, 23, humidityImage, 20, 27, 1);
  display.display();
}

// Toggle all the buttons on or off
void toggleAllButtons(int state) {
  digitalWrite(RED_LED, state);
  digitalWrite(BLUE_LED, state);
  digitalWrite(GREEN_LED, state);
  digitalWrite(YELLOW_LED, state);
}

// Reset all display-related state booleans
void resetDisplayBools() {
  displayingTemp = false;
  displayingWearerDetails = false;
  displayingLogo = false;
  displayingTitle = false;
  displayingAnimations = false;
  animationsInterrupted = false;
  displayingEtchASketch = false;
  playingRoll = false;
}

// Get temp and humidity from the sensors
void getTempAndHumidity() {
  int prevTemp = currentTemp;
  int prevHumidity = currentHumidity;

  currentTemp = round((envSensor.readTemperature() * 1.8 + 32.00)*10)/10;
  currentHumidity = round(envSensor.readHumidity()*10)/10;

  // If either has changed and these values are being displayed, update the display
  if (displayingTemp && (prevTemp != currentTemp || prevHumidity != currentHumidity)) {
    showTempAndHumidity();
  }

  fireEnvSensorsEvent(currentTemp, currentHumidity);
}

// Clear the OLED display
void clearScreen() {
  display.stopscroll();
  display.clearDisplay();
  display.display();
  display.setCursor(0, 0);
  display.setTextWrap(true);
}

// Init Etch A Sketch mode on the OLED
void initEtchASketch() {
  clearScreen();
  display.println();
  display.setTextSize(2);
  display.println("  Etch A");
  display.println("  Sketch");
  display.setTextSize(1);
  display.println();
  display.println("Use the joystick...");
  display.display();
  delay(2000);

  clearScreen();
  drawFilledCircle();
}

// Update the display during Etch A Sketch Mode. Draw a new filled circle
// based on the which joystick direction was used.
void etchASketch() {
  int lastY = displayY;
  int lastX = displayX;

  joystickUpDebouncer.update();
  if (joystickUpDebouncer.read() == LOW)
  {
    displayY--;
  }

  joystickDownDebouncer.update();
  if (joystickDownDebouncer.read() == LOW)
  {
    displayY++;
  }

  joystickLeftDebouncer.update();
  if (joystickLeftDebouncer.read() == LOW)
  {
    displayX--;
  }

  joystickRightDebouncer.update();
  if (joystickRightDebouncer.read() == LOW)
  {
    displayX++;
  }

  // Reset the screen
  joystickCenterDebouncer.update();
  if (joystickCenterDebouncer.read() == LOW)
  {
    clearScreen();
    displayX = display.width()/2;
    displayY = display.height()/2;
  }

  if ((lastX != displayX) || (lastY != displayY)) {
    drawFilledCircle();
  }
}

void drawFilledCircle() {
  display.drawCircle(displayX, displayY, 1, WHITE);
  display.drawCircle(displayX, displayY, 2, WHITE);
  display.drawCircle(displayX, displayY, 3, WHITE);
  display.drawCircle(displayX, displayY, 4, WHITE);
  display.drawCircle(displayX, displayY, 5, WHITE);
  display.display();
}

// Decode IR sensor results and fire events if we get certain codes.
void irDump(decode_results *results) {
    int count = results->rawlen;

    if (results->decode_type == NEC) {
      fireIREvent("nec");
      displayIRName("NEC");
    }
    else if (results->decode_type == SONY) {
      fireIREvent("sony");
      displayIRName("Sony");
    }
    else if (results->decode_type == RC5) {
      fireIREvent("rc5");
      displayIRName("RC5");
    }
    else if (results->decode_type == RC6) {
      fireIREvent("rc6");
      displayIRName("RC6");
    }
    else if (results->decode_type == PANASONIC) {
      fireIREvent("panasonic");
      displayIRName("Panasonic");
    }
    else if (results->decode_type == JVC) {
      fireIREvent("jvc");
      displayIRName("JVC");
    }

    if (results->decode_type == DISNEY) {
      fireIREvent("disney");
      displayIRName("Disney");
    }
}

// Display the IR event received on the screen
void displayIRName(String name) {
  clearScreen();
  display.setTextSize(2);
  display.setCursor(0, 24);
  display.printlnf("%s!", name.c_str());
  display.display();
}

// This shall remain undocumented, for it is a secret... what it do?
void checkInputSequence() {
  redButtonADebouncer.update();
  if (redButtonADebouncer.read() == LOW && !checkingInputs)
  {
    checkingInputs = true;
    checkKeyProgress(RED_BUTTON_A, &display);
  }

  blueButtonBDebouncer.update();
  if (blueButtonBDebouncer.read() == LOW && !checkingInputs)
  {
    checkingInputs = true;
    checkKeyProgress(BLUE_BUTTON_B, &display);
  }

  joystickUpDebouncer.update();
  if (joystickUpDebouncer.read() == LOW && !checkingInputs)
  {
    checkingInputs = true;
    checkKeyProgress(JOYSTICK_UP, &display);
  }

  joystickDownDebouncer.update();
  if (joystickDownDebouncer.read() == LOW && !checkingInputs)
  {
    checkingInputs = true;
    checkKeyProgress(JOYSTICK_DOWN, &display);
  }

  joystickLeftDebouncer.update();
  if (joystickLeftDebouncer.read() == LOW && !checkingInputs)
  {
    checkingInputs = true;
    checkKeyProgress(JOYSTICK_LEFT, &display);
  }

  joystickRightDebouncer.update();
  if (joystickRightDebouncer.read() == LOW && !checkingInputs)
  {
    checkingInputs = true;
    checkKeyProgress(JOYSTICK_RIGHT, &display);
  }

  joystickCenterDebouncer.update();
  if (joystickCenterDebouncer.read() == LOW && !checkingInputs)
  {
    checkingInputs = true;
    checkKeyProgress(JOYSTICK_CENTER, &display);
  }

  checkingInputs = false;
}

//Update the first name when called from a cloud function
int updateFirstNameHandler(String data) {
  wearerFirstName = data;
  wearerInfo.setFirstName(wearerFirstName);

  fireNamedEvent();
  if (displayingWearerDetails) {
    displayWearerDetails();
  }
  return 1;
}

//Update the last name when called from a cloud function
int updateLastNameHandler(String data) {
  wearerLastName = data;
  wearerInfo.setLastName(wearerLastName);

  fireNamedEvent();
  if (displayingWearerDetails) {
    displayWearerDetails();
  }
  return 1;
}

// Check the temp and humidity when called from a cloud function
int checkTempHandler(String data) {
  getTempAndHumidity();

  return 1;
}
