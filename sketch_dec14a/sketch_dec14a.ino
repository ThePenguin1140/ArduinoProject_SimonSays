/*
  Started: 6-19-2007
  Spark Fun Electronics
  Nathan Seidle

  Simon Says is a memory game. Start the game by pressing one of the four buttons. When a button lights up,
  press the button, repeating the sequence. The sequence will get longer and longer. The game is won after
  13 rounds.

  This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  Simon Says game originally written in C for the PIC16F88.
  Ported for the ATmega168, then ATmega328, then Arduino 1.0.
  Fixes and cleanup by Joshua Neal <joshua[at]trochotron.com>

  Generates random sequence, plays music, and displays button lights.

  Simon tones from Wikipedia
  - A (red, upper left) - 440Hz - 2.272ms - 1.136ms pulse
  - a (green, upper right, an octave higher than A) - 880Hz - 1.136ms,
  0.568ms pulse
  - D (blue, lower left, a perfect fourth higher than the upper left)
  587.33Hz - 1.702ms - 0.851ms pulse
  - G (yellow, lower right, a perfect fourth higher than the lower left) -
  784Hz - 1.276ms - 0.638ms pulse

  The tones are close, but probably off a bit, but they sound all right.

  The old version of SparkFun simon used an ATmega8. An ATmega8 ships
  with a default internal 1MHz oscillator.  You will need to set the
  internal fuses to operate at the correct external 16MHz oscillator.

  Original Fuses:
  avrdude -p atmega8 -P lpt1 -c stk200 -U lfuse:w:0xE1:m -U hfuse:w:0xD9:m

  Command to set to fuses to use external 16MHz:
  avrdude -p atmega8 -P lpt1 -c stk200 -U lfuse:w:0xEE:m -U hfuse:w:0xC9:m

  The current version of Simon uses the ATmega328. The external osciallator
  was removed to reduce component count.  This version of simon relies on the
  internal default 1MHz osciallator. Do not set the external fuses.
*/

#include "pitches.h" // Used for the MODE_BEEGEES, for playing the melody on the buzzer!
#include <SPI.h> // File System
#include <SD.h>  // SD Card 
#include <LiquidCrystal.h>  //Screen

#define CHOICE_OFF      0 //Used to control LEDs
#define CHOICE_NONE     0 //Used to check buttons
#define CHOICE_RED  (1 << 0)
#define CHOICE_GREEN  (1 << 1)
#define CHOICE_BLUE (1 << 2)
#define CHOICE_YELLOW (1 << 3)

#define LED_RED     5
#define LED_GREEN   9
#define LED_BLUE    7
#define LED_YELLOW  2

// Button pin definitions
#define BUTTON_RED    6
#define BUTTON_GREEN  A4
#define BUTTON_BLUE   8
#define BUTTON_YELLOW 3

#define RS 0
#define E 1
#define D4 A0
#define D5 A3
#define D6 A2
#define D7 A1

// Buzzer pin definitions
#define BUZZER  A5


// Define game parameters
#define ENTRY_TIME_LIMIT   3000 //Amount of time to press a button before game times out. 3000ms = 3 sec

// Game state variables
byte gameBoard[32]; //Contains the combination of buttons as we advance
byte gameRound = 0; //Counts the number of succesful rounds the player has made it through
char displayContent[2][17];
int scores[10];
LiquidCrystal lcd(RS, E, D4, D5, D6, D7);
int score = 0;
int highScore = 0;

void setup() {
  //Setup hardware inputs/outputs. These pins are defined in the hardware_versions header file

  //Enable pull ups on inputs
  pinMode(BUTTON_RED, INPUT);
  pinMode(BUTTON_GREEN, INPUT);
  pinMode(BUTTON_BLUE, INPUT);
  pinMode(BUTTON_YELLOW, INPUT);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);

  pinMode(BUZZER, OUTPUT);

//  Serial.begin(9600);

//  Serial.print("Initializing SD card communications...");
  if ( !SD.begin(4) ) {
//    Serial.println("FAILED");
    return;
  }
//  Serial.println("DONE");

  //configure ethernet shield
//  Serial.print("Disable Ethernet...");
  pinMode(10, OUTPUT);
//  Serial.print("...");
  digitalWrite(10, HIGH);
//  Serial.println("DONE");

  Serial.print("Creating new Log File...");
  File logFile = SD.open("log.txt", FILE_READ);
//  Serial.print("...");
  if (logFile) {
    int count = 0;
    char c;
    String curScore = "";
    while ( logFile.available() ) {
      c = logFile.read();
      if ( c == ',' ) {
        scores[count]  = curScore.toInt();
        curScore = "";
        count++;
      } else {
        curScore += c;
      }
    }
//    Serial.println("DONE");
    logFile.close();
  } 

  if( scores[0] ){
    highScore = scores[0]; 
  }

  lcd.begin(16, 2);
  lcd.setCursor(0,0);
  lcd.print("Test");
  
//  Serial.print("Initializing Display...");
  for ( int i = 0; i < 2; i++) {
    for ( int n = 0; n < 17; n++) {
      displayContent[i][n] = '-';
    }
  }
  
  push("HIGHSCORE");
  push(String(highScore));
//  Serial.println("DONE");
//  Serial.println("SETUP COMPLETE");

  //read SD card
  play_winner(); // After setup is complete, say hello to the world
}

void loop()
{
  attractMode(); // Blink lights while waiting for user to press a button

  // Indicate the start of game play
  setLEDs(CHOICE_RED | CHOICE_GREEN | CHOICE_BLUE | CHOICE_YELLOW); // Turn all LEDs on
  delay(1000);
  setLEDs(CHOICE_OFF); // Turn off LEDs
  delay(250);

  if (play_memory() == true)
    play_winner(); // Player won, play winner tones
  else {
    int tmp = score;
    int cur = 0;
    for(int i = 0; i < 10; i++ ){
      cur = scores[i];
      if( tmp > cur ){
        scores[i] = tmp;
        tmp = cur;
      }
    }

    SD.remove("log.txt");
    File logFile2 = SD.open("log.txt", FILE_WRITE);
    if( logFile2 ){
      for( int i = 0; i < 10; i++ ) {
        logFile2.println( String(scores[i]) + ",");
      }
      logFile2.close();
    }
   
    play_loser(); // Player lost, play loser tones
  }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//The following functions are related to game play only

// Play the regular memory game
// Returns 0 if player loses, or 1 if player wins
boolean play_memory(void)
{
  randomSeed(millis()); // Seed the random generator with random amount of millis()

  gameRound = 0;
  score = 0;

  while (1) {
    push("Score: " + String(score));
    push("HighScore: " + String(highScore));
    add_to_moves(); // Add a button to the current moves, then play them back

    playMoves(); // Play back the current game board

    // Then require the player to repeat the sequence.
    for (byte currentMove = 0 ; currentMove < gameRound ; currentMove++)
    {
      byte choice = wait_for_button(); // See what button the user presses

      if (choice == 0) return false; // If wait timed out, player loses

      if (choice != gameBoard[currentMove]) return false; // If the choice is incorect, player loses
    }
    score++;
    if( score > highScore ) {
      highScore = score;
    }
    delay(1000); // Player was correct, delay before playing moves
  }

  return true; // Player made it through all the rounds to win!
}

void push(String text) {
  String(displayContent[1]).toCharArray( displayContent[0], 17 );
  //FIXME is buffering strings that are long enough
  //maybe try turning down to like 14
  //if ( sizeof(text)/sizeof(char)<16 ) {
  //  text = bufferString(text, '*');
  //}
  text.toCharArray( displayContent[1], 17 );

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(displayContent[0]);
  lcd.setCursor(0, 1);
  lcd.write(displayContent[1]);
}

// Plays the current contents of the game moves
void playMoves(void)
{
  for (byte currentMove = 0 ; currentMove < gameRound ; currentMove++)
  {
    toner(gameBoard[currentMove], 150);

    // Wait some amount of time between button playback
    // Shorten this to make game harder
    delay(150); // 150 works well. 75 gets fast.
  }
}

// Adds a new random button to the game sequence, by sampling the timer
void add_to_moves(void)
{
  byte newButton = random(0, 4); //min (included), max (exluded)

  // We have to convert this number, 0 to 3, to CHOICEs
  if (newButton == 0) newButton = CHOICE_RED;
  else if (newButton == 1) newButton = CHOICE_GREEN;
  else if (newButton == 2) newButton = CHOICE_BLUE;
  else if (newButton == 3) newButton = CHOICE_YELLOW;

  gameBoard[gameRound++] = newButton; // Add this new button to the game array
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//The following functions control the hardware

// Lights a given LEDs
// Pass in a byte that is made up from CHOICE_RED, CHOICE_YELLOW, etc
void setLEDs(byte leds)
{
  if ((leds & CHOICE_RED) != 0)
    digitalWrite(LED_RED, HIGH);
  else
    digitalWrite(LED_RED, LOW);

  if ((leds & CHOICE_GREEN) != 0)
    digitalWrite(LED_GREEN, HIGH);
  else
    digitalWrite(LED_GREEN, LOW);

  if ((leds & CHOICE_BLUE) != 0)
    digitalWrite(LED_BLUE, HIGH);
  else
    digitalWrite(LED_BLUE, LOW);

  if ((leds & CHOICE_YELLOW) != 0)
    digitalWrite(LED_YELLOW, HIGH);
  else
    digitalWrite(LED_YELLOW, LOW);
}

// Wait for a button to be pressed.
// Returns one of LED colors (LED_RED, etc.) if successful, 0 if timed out
byte wait_for_button(void)
{
  long startTime = millis(); // Remember the time we started the this loop

  while ( (millis() - startTime) < ENTRY_TIME_LIMIT) // Loop until too much time has passed
  {
    byte button = checkButton();

    if (button != CHOICE_NONE)
    {
      toner(button, 150); // Play the button the user just pressed

      while (checkButton() != CHOICE_NONE) ; // Now let's wait for user to release button

      delay(10); // This helps with debouncing and accidental double taps

      return button;
    }

  }

  return CHOICE_NONE; // If we get here, we've timed out!
}

// Returns a '1' bit in the position corresponding to CHOICE_RED, CHOICE_GREEN, etc.
byte checkButton(void)
{
//  Serial.println("R:" + String(digitalRead(BUTTON_RED)));
//  Serial.println("G:" + String(digitalRead(BUTTON_GREEN)));
//  Serial.println("B:" + String(digitalRead(BUTTON_BLUE)));
//  Serial.println("Y:" + String(digitalRead(BUTTON_YELLOW)));

  if (digitalRead(BUTTON_RED) == 1) return (CHOICE_RED);
  else if (digitalRead(BUTTON_GREEN) == 1) return (CHOICE_GREEN);
  else if (digitalRead(BUTTON_BLUE) == 1) return (CHOICE_BLUE);
  else if (digitalRead(BUTTON_YELLOW) == 1) return (CHOICE_YELLOW);

  return (CHOICE_NONE); // If no button is pressed, return none
}

// Light an LED and play tone
// Red, upper left:     440Hz - 2.272ms - 1.136ms pulse
// Green, upper right:  880Hz - 1.136ms - 0.568ms pulse
// Blue, lower left:    587.33Hz - 1.702ms - 0.851ms pulse
// Yellow, lower right: 784Hz - 1.276ms - 0.638ms pulse
void toner(byte which, int buzz_length_ms)
{
  setLEDs(which); //Turn on a given LED

  //Play the sound associated with the given LED
  switch (which)
  {
    case CHOICE_RED:
      buzz_sound(buzz_length_ms, 1136);
      break;
    case CHOICE_GREEN:
      buzz_sound(buzz_length_ms, 568);
      break;
    case CHOICE_BLUE:
      buzz_sound(buzz_length_ms, 851);
      break;
    case CHOICE_YELLOW:
      buzz_sound(buzz_length_ms, 638);
      break;
  }

  setLEDs(CHOICE_OFF); // Turn off all LEDs
}

// Toggle buzzer every buzz_delay_us, for a duration of buzz_length_ms.
void buzz_sound(int buzz_length_ms, int buzz_delay_us)
{
  // Convert total play time from milliseconds to microseconds
  long buzz_length_us = buzz_length_ms * (long)1000;

  // Loop until the remaining play time is less than a single buzz_delay_us
  while (buzz_length_us > (buzz_delay_us * 2))
  {
    buzz_length_us -= buzz_delay_us * 2; //Decrease the remaining play time

    // Toggle the buzzer at various speeds
    digitalWrite(BUZZER, HIGH);
    delayMicroseconds(buzz_delay_us);

    digitalWrite(BUZZER, LOW);
    delayMicroseconds(buzz_delay_us);
  }
}

// Play the winner sound and lights
void play_winner(void)
{
  setLEDs(CHOICE_GREEN | CHOICE_BLUE);
  winner_sound();
  setLEDs(CHOICE_RED | CHOICE_YELLOW);
  winner_sound();
  setLEDs(CHOICE_GREEN | CHOICE_BLUE);
  winner_sound();
  setLEDs(CHOICE_RED | CHOICE_YELLOW);
  winner_sound();
}

// Play the winner sound
// This is just a unique (annoying) sound we came up with, there is no magic to it
void winner_sound(void)
{
  // Toggle the buzzer at various speeds
  for (byte x = 250 ; x > 70 ; x--)
  {
    for (byte y = 0 ; y < 3 ; y++)
    {
      digitalWrite(BUZZER, HIGH);
      delayMicroseconds(x);

      digitalWrite(BUZZER, LOW);
      delayMicroseconds(x);
    }
  }
}

// Play the loser sound/lights
void play_loser(void)
{
  setLEDs(CHOICE_RED | CHOICE_GREEN);
  buzz_sound(255, 1500);

  setLEDs(CHOICE_BLUE | CHOICE_YELLOW);
  buzz_sound(255, 1500);

  setLEDs(CHOICE_RED | CHOICE_GREEN);
  buzz_sound(255, 1500);

  setLEDs(CHOICE_BLUE | CHOICE_YELLOW);
  buzz_sound(255, 1500);
}

// Show an "attract mode" display while waiting for user to press button.
void attractMode(void)
{
  while (1)
  {
    setLEDs(CHOICE_RED);
    delay(100);
    if (checkButton() != CHOICE_NONE) return;

    setLEDs(CHOICE_BLUE);
    delay(100);
    if (checkButton() != CHOICE_NONE) return;

    setLEDs(CHOICE_GREEN);
    delay(100);
    if (checkButton() != CHOICE_NONE) return;

    setLEDs(CHOICE_YELLOW);
    delay(100);
    if (checkButton() != CHOICE_NONE) return;
  }
}
