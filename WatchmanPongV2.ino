/**************************************
  Pong game for Sony Watchman

  - adjustable difficulty
  - adjustable max score
  - adjustable paddle smoothing
  - adjustable paddle sensitivity
  - auto attract mode on game end
  - skill level select on startup
  - paddle reverse control
  
  Tavis Hord
  © Sideburn Studios - August 2025
***************************************/

#include <avr/pgmspace.h>
#include <TVout.h>
#include <video_gen.h>
#include <Controllers.h>

#define DEBUG false

#define W 135
#define H 98
#define MODE_SWITCH 10            //  D10 switch for start / stop (attract mode)

#define MAX_SCORE 11              // Max score to be reached
#define DEFAULT_FAULT_PERCENT 65  // Default Ai difficulty level, higher numbers are easier
#define EASY_FAULT_PERCENT 100    // Easy mode fault percent
#define PADDLE_SMOOTHING 8        // Increase for more paddle smoothing (8=current, 16=very smooth, 6=more responsive)
#define PADDLE_SENSITIVITY 2.5    // Adjusts touchyness of the paddle input from the tuner POT
#define PADDLE_WIDTH 2            // Width of paddles in pixels
#define PADDLE_HEIGHT 16          // Height of paddles in pixels

#define PADDLE_REVERSE false       // Set to true to reverse paddle direction (for older 1986 model FD-10A )

// Paddle calibration values
#define NORMAL_MIN_READING 775
#define NORMAL_MAX_READING 850
#define REVERSE_MIN_READING 900
#define REVERSE_MAX_READING 930


// Musical note frequencies (in Hz)
#define NOTE_C2  65.41
#define NOTE_C3  130.81
#define NOTE_G3  196.00
#define NOTE_C4  261.63
#define NOTE_G4  392.00
#define NOTE_C5  523.25

boolean gameEnded = true;           // Startup default.
                                    // False = game starts on power up (if the switch is set to start). 
                                    // True = always boots in attract mode.
// Skill level variables
boolean easyMode = false;         
int currentFaultPercent = DEFAULT_FAULT_PERCENT;

// Pong variables
int ballx, bally;
char dx;
char dy;
boolean ballServing = false;        // Track if ball is waiting to serve
byte paddleAy = 44;                 // Computer paddle (left side)
byte paddleBy = 44;                 // Human paddle (right side)
byte paddleAx = 2;                  // Computer paddle X position
byte paddleBx = W - 10;             // Human paddle X position
byte paddleWidth = PADDLE_WIDTH;    // Use define instead of hardcoded value
byte paddleLength = PADDLE_HEIGHT;  // Use define instead of hardcoded value
byte score = 0;                     // Computer score (left)
byte score2 = 0;                    // Player score (right)
boolean attractMode = false;        // Track if in attract mode
boolean lastSwitchState = HIGH;     // Assume HIGH = attract off at startup

// Rally speed system variables
byte rallyCount = 0;                // Track consecutive hits without a miss
byte speedLevel = 0;                // Current speed level (0-3)
byte frameSkip = 0;                 // Frame counter for speed control
byte maxSpeedLevel = 3;             // Maximum speed level

// Computer AI variables
float aiPaddleTarget = 44.0;        // Target position for smooth AI movement
float aiPaddleFloat = 44.0;         // Floating point position for smooth movement
float maxAiSpeed = 2.5;             // Maximum pixels per frame the AI can move
float aiMomentum = 0.0;             // Track movement momentum to prevent oscillation

// Player AI variables (for attract mode)
float playerAiTarget = 44.0;        // Target position for player AI
float playerAiFloat = 44.0;         // Floating point position for player AI
float playerMaxSpeed = 0.9;         // Slower than computer AI
float playerAiMomentum = 0.0;       // Player AI momentum

// Paddle smoothing variables
int paddleBuffer[PADDLE_SMOOTHING];
byte bufferIndex = 0;
boolean bufferFilled = false;
byte lastPaddleAy = 44;             // Last paddle position for deadzone

// Paddle calibration variables
int baseMinReading;           // Base minimum paddle POS reading (set in setup based on PADDLE_REVERSE)
int baseMaxReading;           // Base maximum paddle POS reading (set in setup based on PADDLE_REVERSE)
int potOffset = 0;            // Offset to shift the range up/down (negative values shift paddle down)

TVout tv;

// Function declarations
void initPong();
void playTone(unsigned int frequency, unsigned long duration_ms);
void startupTune();
void initAttractScreen();
void updateComputerPaddle();
void updatePlayerAI();
void drawPaddles();
void drawScore();
void drawLargeDigit(byte x, byte y, byte digit);
void drawLargeTwoDigit(byte x, byte y, byte number);
void drawPaddle(int x, int y);
void hitSound();
void bounceSound();
void missSound();
void drawNet();
void gameOver();
void drawIntroScreen();
void drawLargeGameOver();
void drawLargeYouWin();
void drawLargeIWin();
void drawLargeA(byte x, byte y);
void drawLargeB(byte x, byte y);
void drawLargeD(byte x, byte y);
void drawLargeE(byte x, byte y);
void drawLargeG(byte x, byte y);
void drawLargeI(byte x, byte y);
void drawLargeM(byte x, byte y);
void drawLargeN(byte x, byte y);
void drawLargeO(byte x, byte y);
void drawLargeP(byte x, byte y);
void drawLargeR(byte x, byte y);
void drawLargeS(byte x, byte y);
void drawLargeU(byte x, byte y);
void drawLargeV(byte x, byte y);
void drawLargeW(byte x, byte y);
void drawLargeY(byte x, byte y);
void drawEasyModeScreen();
void checkSkillLevel();
void moveBall();
void pong();

void setup() {  
  // Add serial for debug output
  #ifdef DEBUG
  Serial.begin(9600);
  #endif

    // Set paddle calibration based on reverse setting
  if (PADDLE_REVERSE) {
    baseMinReading = REVERSE_MIN_READING;
    baseMaxReading = REVERSE_MAX_READING;
  } else {
    baseMinReading = NORMAL_MIN_READING;
    baseMaxReading = NORMAL_MAX_READING;
  }
  
  // Set up D10 as attract mode switch
  pinMode(MODE_SWITCH, INPUT_PULLUP);  // D10 with internal pull-up
  tv.begin(NTSC, W, H);

  randomSeed(analogRead(0));

  // Check skill level on startup
  checkSkillLevel();

  // Startup tune
  startupTune();

  tv.delay(160);
  
  // Initialize paddle smoothing buffer
  for(int i = 0; i < PADDLE_SMOOTHING; i++) {
  paddleBuffer[i] = (baseMinReading + baseMaxReading) / 2; // Initialize to center value
  }

  // Show intro splash screen
  drawIntroScreen();
  
  // Show easy mode screen if enabled
  if (easyMode) {
    drawEasyModeScreen();
  }
  
  initPong();
}

void checkSkillLevel() {
  // Read A3 paddle position on startup to determine skill level
  int startupReading = analogRead(A3);
  
  if (PADDLE_REVERSE) {
    // When paddle is reversed, easy mode is at the HIGH end instead of LOW
    if (startupReading >= 950) {
      easyMode = true;
      currentFaultPercent = EASY_FAULT_PERCENT;
    } else {
      easyMode = false;
      currentFaultPercent = DEFAULT_FAULT_PERCENT;
    }
  } else {
    // Normal mode - easy mode at LOW end
    if (startupReading < baseMinReading - 100) {
      easyMode = true;
      currentFaultPercent = EASY_FAULT_PERCENT;
    } else {
      easyMode = false;
      currentFaultPercent = DEFAULT_FAULT_PERCENT;
    }
  }
}

void drawEasyModeScreen() {
 tv.fill(0);
 
 // Center "EASY" and "MODE" on screen
 byte letterSpacing = 18;

 byte xOffset = 2;
 byte yOffset = 0;
 
 byte easyStartX = 35 + xOffset;  // Centered for 4 letters with spacing
 byte easyY = 27 + yOffset;       // Upper middle of screen
 
 // Draw "EASY"
 drawLargeE(easyStartX, easyY);
 drawLargeA(easyStartX + letterSpacing, easyY);
 drawLargeS(easyStartX + (letterSpacing * 2), easyY);
 drawLargeY(easyStartX + (letterSpacing * 3), easyY);
 
 // Calculate positions for "MODE" (4 letters)
 byte modeStartX = 35 + xOffset;  // Centered for 4 letters with spacing
 byte modeY = 47 + yOffset;       // Lower middle of screen
 
 // Draw "MODE"
 drawLargeM(modeStartX, modeY);
 drawLargeO(modeStartX + letterSpacing, modeY);
 drawLargeD(modeStartX + (letterSpacing * 2), modeY);
 drawLargeE(modeStartX + (letterSpacing * 3), modeY);
 
 // Hold easy mode screen for 2 seconds
 tv.delay(2000);
}

void startupTune() {
  playTone(NOTE_C2, 300); 
  tv.delay(50);
  
  playTone(NOTE_C3, 200); 
  playTone(NOTE_G3, 200);
  tv.delay(100);
  
  playTone(NOTE_C4, 400);
  tv.delay(200);
  
  playTone(NOTE_G4, 100);
  playTone(NOTE_C5, 150);
}

void loop() {
  bool currentSwitch = (digitalRead(MODE_SWITCH) == LOW);

  #ifdef DEBUG
  // DEBUG: Print raw A3 value (comment out when not needed)
  int rawValue = analogRead(A3);
  Serial.print("A3 Raw: ");
  Serial.print(rawValue);
  Serial.print("\n");
  #endif

 // --- detect switch change ---
 // --- restart game switching --
  if (currentSwitch != lastSwitchState) {
    if (currentSwitch) {
      // switched out of attract mode → start new game
      tv.delay(3000);
      initPong();
    } else {
      // switched into attract →  switch to demo
      if(!gameEnded){ //if game in play and canceled
        tv.fill(0);
        drawLargeGameOver();
        tv.delay(1000);
        initAttractScreen();
      }
    }
  }
  lastSwitchState = currentSwitch;

  pong();
}

void playTone(unsigned int frequency, unsigned long duration_ms) {
  tv.tone(frequency, duration_ms);
}

void pong() {
  // Check attract mode switch (D10)
  attractMode = (digitalRead(MODE_SWITCH) == HIGH);
  
  if(attractMode){
    gameEnded = false;
  } 

  //game ended switch to attract mode
  if(gameEnded == true){
    attractMode = true;
  }

  // Force attract mode for AI testing
  //attractMode = true;  // Always in attract mode for testing
  //gameEnded = false;   // Never end game during testing

  drawNet();
  drawPaddles();

  if (ballServing && dy == 0) {
    // Ball auto-serves immediately instead of waiting
    if (random(0, 2) == 0) {
      dy = 1;
    } else {
      dy = -1;
    }
    ballServing = false;  // No longer serving
  }

  // Store old ball position for path checking
  int oldBallX = ballx;
  int oldBallY = bally;

  // Safe erase old ball position - don't erase in digit areas
  for (int x = ballx; x <= ballx + 1; x++) {
    for (int y = bally; y <= bally + 1; y++) {
      // Only erase if we're NOT in a digit area
      bool inDigitArea = false;
      
      if (y >= 2 && y <= 12) {
        // Check left score area
        if ((score >= 10 && x >= 20 && x <= 37) || (score < 10 && x >= 25 && x <= 32)) {
          inDigitArea = true;
        }
        // Check right score area  
        else if ((score2 >= 10 && x >= 100 && x <= 117) || (score2 < 10 && x >= 105 && x <= 112)) {
          inDigitArea = true;
        }
      }
      
      // Only erase if NOT in digit area
      if (!inDigitArea) {
        tv.set_pixel(x, y, 0);
      }
    }
  }

  moveBall();

  // Safe draw new ball position - don't draw in digit areas
  for (int x = ballx; x <= ballx + 1; x++) {
    for (int y = bally; y <= bally + 1; y++) {
      // Only draw if we're NOT in a digit area
      bool inDigitArea = false;
      
      if (y >= 2 && y <= 12) {
        // Check left score area
        if ((score >= 10 && x >= 20 && x <= 37) || (score < 10 && x >= 25 && x <= 32)) {
          inDigitArea = true;
        }
        // Check right score area  
        else if ((score2 >= 10 && x >= 100 && x <= 117) || (score2 < 10 && x >= 105 && x <= 112)) {
          inDigitArea = true;
        }
      }
      
      // Only draw if NOT in digit area
      if (!inDigitArea) {
        tv.set_pixel(x, y, 1);
      }
    }
  }

  tv.delay(1);
}

void moveBall() {
  // Detect score - reset rally count on miss
  if (ballx < 0) {
    rallyCount = 0;  // Reset rally counter
    speedLevel = 1;  // Reset speed level to 1
    if (!attractMode) score2++;
    drawScore();
    drawNet();
    missSound();
    tv.delay(1500);
    if (!attractMode && score2 == MAX_SCORE) {
      gameOver();
      initAttractScreen();
      return;
    }
    ballx = random(W / 2, (3 * W) / 4);
    bally = random(H / 4, (3 * H) / 4);
    dx = -1;
    dy = 0;
    ballServing = true;  // Ball is now serving
  }
  if (ballx >= (W + 2)) {
    rallyCount = 0;  // Reset rally counter
    speedLevel = 1;  // Reset speed level to 1
    if (!attractMode) score++;
    drawNet();
    drawScore();
    missSound();
    tv.delay(1500);
    if (!attractMode && score == MAX_SCORE) {
      gameOver();
      initAttractScreen();
      return;
    }
    ballx = random(W / 4, W / 2);
    bally = random(H / 4, (3 * H) / 4);
    dx = 1;
    dy = 0;
    ballServing = true;  // Ball is now serving
  }

  // Calculate movement distances based on speed level
  int moveX = dx;  // Base horizontal movement
  int moveY = dy;  // Base vertical movement
  
  // Smooth speed increases - no random jerky movements
  if (speedLevel == 1) {
    // 25% faster - move 1.25 pixels on average
    // move 1 pixel normally, +1 pixel every 4th frame
    static byte frameCounter1 = 0;
    frameCounter1++;
    if (frameCounter1 >= 4) {
      moveX = dx * 2;
      moveY = dy * 2;
      frameCounter1 = 0;
    }
  } else if (speedLevel == 2) {
    // 50% faster - move 1.5 pixels on average  
    // move 1 pixel normally, +1 pixel every 2nd frame
    static byte frameCounter2 = 0;
    frameCounter2++;
    if (frameCounter2 >= 2) {
      moveX = dx * 2;
      moveY = dy * 2;
      frameCounter2 = 0;
    }
  } else if (speedLevel == 3) {
    // 75% faster - move 1.75 pixels on average
    // move 2 pixels normally, +1 pixel every 4th frame
    moveX = dx * 2;
    moveY = dy * 2;
    static byte frameCounter3 = 0;
    frameCounter3++;
    if (frameCounter3 >= 4) {
      moveX = dx * 3;
      moveY = dy * 3;
      frameCounter3 = 0;
    }
  }

  // Apply calculated movement
  ballx = ballx + moveX;
  bally = bally + moveY;

  // Set ball bounds for collision detection
  int leftBall = ballx;
  int rightBall = ballx + 2;  // Ball is 2x2 pixels
  int topBall = bally;
  int bottomBall = bally + 2;

  bool paddleHit = false;
  
  // Paddle A (computer - left side) bounds
  int leftPaddleA = paddleAx;
  int rightPaddleA = paddleAx + paddleWidth;
  int topPaddleA = paddleAy - 2;
  int bottomPaddleA = paddleAy + paddleLength + 2;
  
  // Bounding box collision check
  if (topBall <= bottomPaddleA && bottomBall >= topPaddleA && 
      leftBall <= rightPaddleA && rightBall >= leftPaddleA) {
    
    paddleHit = true;
    dx = -dx;  // Reverse horizontal direction
    ballx += dx;  // Move ball back out of paddle
    
    // Center hit detection and angle calculation
    float ballCenterY = bally + 1;  // Center of 2x2 ball
    float paddleCenterY = paddleAy + (paddleLength / 2.0);
    float hitOffset = ballCenterY - paddleCenterY;
    
    if (abs(hitOffset) <= 1.0) {
      // Center hit - pure horizontal trajectory
      dy = 0;
    } else {
      // Off-center hit - give it vertical angle based on hit position
      if (hitOffset > 0) {
        dy = 1;  // Hit bottom half, go down
      } else {
        dy = -1; // Hit top half, go up  
      }
    }
    
    hitSound();
  }

  // Paddle B (human - right side) bounds
  if (!paddleHit) {
    int leftPaddleB = paddleBx;
    int rightPaddleB = paddleBx + paddleWidth;
    int topPaddleB = paddleBy - 1; //Extended top hit zone by 1 pixel
    int bottomPaddleB = paddleBy + paddleLength + 1; //Extended bottom hit zone by 1 pixel
    
    // Bounding box collision check
    if (topBall <= bottomPaddleB && bottomBall >= topPaddleB && 
        leftBall <= rightPaddleB && rightBall >= leftPaddleB) {
      
      paddleHit = true;
      dx = -dx;  // Reverse horizontal direction  
      ballx += dx;  // Move ball back out of paddle
      
      // Center hit detection and angle calculation
      float ballCenterY = bally + 1;  // Center of 2x2 ball
      float paddleCenterY = paddleBy + (paddleLength / 2.0);
      float hitOffset = ballCenterY - paddleCenterY;
      
      if (abs(hitOffset) <= 1.0) {
        // Center hit - pure horizontal trajectory
        dy = 0;
      } else {
        // Off-center hit - give it vertical angle based on hit position
        if (hitOffset > 0) {
          dy = 1;  // Hit bottom half, go down
        } else {
          dy = -1; // Hit top half, go up  
        }
      }
      
      hitSound();
    }
  }
  
  // Handle rally progression
  if (paddleHit) {
    rallyCount++;
    
    if (rallyCount >= 3 && speedLevel == 1) {
      speedLevel = 2;  // Second speed increase after 3 hits
    } else if (rallyCount >= 6 && speedLevel == 2) {
      speedLevel = 3;  // Third speed increase after 6 hits
    }
  }

  // Wall bounces
  if (bally <= 0) {
    bounceSound();
    dy = -dy;
    bally = 2;  // Move ball away from wall
  }
  if (bally >= (H - 2)) {
    bounceSound();
    dy = -dy;
    bally = H - 4;  // Move ball away from wall
  }
}

void drawPaddles() {
  // Computer AI for left paddle
  updateComputerPaddle();
  
  if (attractMode) {
    // In attract mode, use AI for right paddle too (same skill level)
    updatePlayerAI();
  } else {
    // Human player - Read raw analog value from A3 (potentiometer with 10K pull-up)
    int rawValue = analogRead(A3);

    // Apply offset to shift the effective range
    int adjustedMin = baseMinReading + potOffset;
    int adjustedMax = baseMaxReading + potOffset;
    
    // Constrain the raw value to adjusted range
    rawValue = constrain(rawValue, adjustedMin, adjustedMax);
    
    // Paddle smoothing
    // Add to circular buffer for smoothing
    paddleBuffer[bufferIndex] = rawValue;
    bufferIndex = (bufferIndex + 1) % PADDLE_SMOOTHING;
    if (!bufferFilled && bufferIndex == 0) {
      bufferFilled = true;
    }
    
    // Calculate moving average
    long sum = 0;
    int samplesUsed = bufferFilled ? PADDLE_SMOOTHING : bufferIndex;
    for(int i = 0; i < samplesUsed; i++) {
      sum += paddleBuffer[i];
    }
    int smoothedValue = sum / samplesUsed;
    
    // Map smoothed value to RIGHT paddle position (human player) using adjusted range
    byte newPaddleBy;
    if (PADDLE_REVERSE) {
      // Reversed mapping - pot up = paddle down, pot down = paddle up
      newPaddleBy = map(smoothedValue, adjustedMin, adjustedMax, H - paddleLength - 1, 1);
    } else {
      // Normal mapping - pot up = paddle up, pot down = paddle down
      newPaddleBy = map(smoothedValue, adjustedMin, adjustedMax, 1, H - paddleLength - 1);
    }
    
    // Paddle sensitivity
    // deadzone for human paddle to prevent jittering from POT value
    if (abs(newPaddleBy - lastPaddleAy) > PADDLE_SENSITIVITY) {  
      paddleBy = newPaddleBy;
      lastPaddleAy = paddleBy;  // Update tracking
    }
  }
  
  drawPaddle(paddleAx, paddleAy);  // Computer paddle (left)
  drawPaddle(paddleBx, paddleBy);  // Human paddle (right)
}

void drawScore() {
  // Clear score areas - make them wider for two-digit numbers
  for (byte y = 0; y < 15; y++) {
    for (byte x = 15; x <= 40; x++) {  // left area
      tv.set_pixel(x, y, 0);
    }
    for (byte x = 95; x <= 120; x++) {  // right area  
      tv.set_pixel(x, y, 0);
    }
  }
  
  // Draw large digits manually
  if (score >= 10) {
    drawLargeTwoDigit(20, 2, score);   // Computer score (left) - two digits
  } else {
    drawLargeDigit(25, 2, score);      // Computer score (left) - single digit, centered
  }
  
  if (score2 >= 10) {
    drawLargeTwoDigit(100, 2, score2); // Player score (right) - two digits  
  } else {
    drawLargeDigit(105, 2, score2);    // Player score (right) - single digit, centered
  }
}

void drawLargeDigit(byte x, byte y, byte digit) {
  // Draw large pixel digits
  switch(digit) {
    case 0:
      // Draw 0
      tv.draw_line(x+1, y, x+5, y, 1);       // top
      tv.draw_line(x, y+1, x, y+9, 1);       // left
      tv.draw_line(x+6, y+1, x+6, y+9, 1);   // right
      tv.draw_line(x+1, y+10, x+5, y+10, 1); // bottom
      break;
    case 1:
      // Draw 1
      tv.draw_line(x+3, y, x+3, y+10, 1);    // vertical line
      //tv.draw_line(x+2, y+1, x+3, y, 1);     // top angle
      //tv.draw_line(x+1, y+10, x+5, y+10, 1); // bottom
      break;
    case 2:
      // Draw 2
      tv.draw_line(x+1, y, x+5, y, 1);       // top
      tv.draw_line(x+6, y+1, x+6, y+4, 1);   // top right
      tv.draw_line(x+1, y+5, x+5, y+5, 1);   // middle
      tv.draw_line(x, y+6, x, y+9, 1);       // bottom left
      tv.draw_line(x+1, y+10, x+5, y+10, 1); // bottom
      break;
    case 3:
      // Draw 3
      tv.draw_line(x+1, y, x+5, y, 1);       // top
      tv.draw_line(x+6, y+1, x+6, y+9, 1);   // right
      tv.draw_line(x+1, y+5, x+3, y+5, 1);   // middle
      tv.draw_line(x+1, y+10, x+5, y+10, 1); // bottom
      break;
    case 4:
      // Draw 4
      tv.draw_line(x, y, x, y+5, 1);         // left
      tv.draw_line(x+6, y, x+6, y+10, 1);    // right
      tv.draw_line(x+1, y+5, x+5, y+5, 1);   // middle
      break;
    case 5:
      // Draw 5
      tv.draw_line(x+1, y, x+5, y, 1);       // top
      tv.draw_line(x, y+1, x, y+4, 1);       // top left
      tv.draw_line(x+1, y+5, x+5, y+5, 1);   // middle
      tv.draw_line(x+6, y+6, x+6, y+9, 1);   // bottom right
      tv.draw_line(x+1, y+10, x+5, y+10, 1); // bottom
      break;
    case 6:
      // Draw 6
      tv.draw_line(x+1, y, x+5, y, 1);       // top
      tv.draw_line(x, y+1, x, y+9, 1);       // left
      tv.draw_line(x+1, y+5, x+5, y+5, 1);   // middle
      tv.draw_line(x+6, y+6, x+6, y+9, 1);   // bottom right
      tv.draw_line(x+1, y+10, x+5, y+10, 1); // bottom
      break;
    case 7:
      // Draw 7
      tv.draw_line(x+1, y, x+5, y, 1);       // top
      tv.draw_line(x+6, y+1, x+6, y+10, 1);  // right
      break;
    case 8:
      // Draw 8
      tv.draw_line(x+1, y, x+5, y, 1);       // top
      tv.draw_line(x, y+1, x, y+9, 1);       // left
      tv.draw_line(x+6, y+1, x+6, y+9, 1);   // right
      tv.draw_line(x+1, y+5, x+3, y+5, 1);   // middle
      tv.draw_line(x+1, y+10, x+5, y+10, 1); // bottom
      break;
    case 9:
      // Draw 9
      tv.draw_line(x+1, y, x+5, y, 1);       // top
      tv.draw_line(x, y+1, x, y+4, 1);       // top left
      tv.draw_line(x+6, y+1, x+6, y+9, 1);   // right
      tv.draw_line(x+1, y+5, x+3, y+5, 1);   // middle
      tv.draw_line(x+1, y+10, x+5, y+10, 1); // bottom
      break;
  }
}

void drawLargeTwoDigit(byte x, byte y, byte number) {
  // Draw two-digit numbers (10, 11, etc.)
  byte tensDigit = number / 10;
  byte onesDigit = number % 10;
  
  // Draw tens digit (smaller spacing for two digits)
  drawLargeDigit(x, y, tensDigit);
  // Draw ones digit (x+ pixels to the right)
  drawLargeDigit(x + 10, y, onesDigit);
}

void drawPaddle(int x, int y) {
  for (int i = x; i < x + paddleWidth; i++) {
    tv.draw_line(i, 0, i, H - 1, 0);
    tv.draw_line(i, y, i, y + paddleLength, 1);
  }
}

void hitSound() {
  if (!attractMode) playTone(493.88, 20);  // Only play sound if not in attract mode
}

void bounceSound() {
  if (!attractMode) playTone(246.94, 20);  // Only play sound if not in attract mode
}

void missSound() {
  if (!attractMode) {
    for (int i = 0; i < 19; i++) {
      tv.delay(19);
      playTone(246.94, 19);
    }
  }
}

void drawNet() {
  for (byte y = 0; y < H - 4; y = y + 8) {
    tv.draw_line(W / 2, y, W / 2, y + 4, 1);
  }
}

void gameOver() {
  tv.delay(100);
  tv.fill(0);
  
  // Check who won and display appropriate message
  if (!attractMode) {
    if (score2 == MAX_SCORE) {
      // Player won
      drawLargeYouWin();
    } else if (score == MAX_SCORE) {
      // Computer won
      drawLargeIWin();
    } else {
      // Fallback to standard game over screen
      drawLargeGameOver();
    }
  } else {
    // In attract mode, display standard game over
    drawLargeGameOver();
  }
  
  drawScore();
  gameEnded = true;
  tv.delay(3000);
}

void drawIntroScreen() {
  tv.fill(0);

  byte xOffset = 0;
  byte yOffset = 0;
  
  // Adjust positions to center text
  byte startX = 33 + xOffset; 
  byte lineSpacing = 18;  

  // Draw "SIDE"
  byte y1 = 20 + yOffset;
  drawLargeS(startX, y1);
  drawLargeI(startX + 20, y1);
  drawLargeD(startX + 40, y1);
  drawLargeE(startX + 60, y1);

  // Draw "BURN"
  byte y2 = y1 + lineSpacing;
  drawLargeB(startX, y2);
  drawLargeU(startX + 20, y2);
  drawLargeR(startX + 40, y2);
  drawLargeN(startX + 60, y2);

  // Draw "PONG"
  byte y3 = y2 + lineSpacing;
  drawLargeP(startX, y3);
  drawLargeO(startX + 20, y3);
  drawLargeN(startX + 40, y3);
  drawLargeG(startX + 60, y3);

  // Hold intro screen for 3 seconds
  tv.delay(3000);
}

void drawLargeYouWin() {
  // Positioning variables
  byte startXOffset = 10;

  byte youX = 34 + startXOffset;     // X position for "YOU" 
  byte youY = 30;     // Y position for "YOU"
  byte winX = 35 + startXOffset;     // X position for "WIN"
  byte winY = 50;     // Y position for "WIN"
  byte letterSpacing = 20;  // Space between letters
  
  // Draw "YOU" 
  tv.draw_line(youX+2, youY, youX+5, youY+6, 1);      // left diagonal
  tv.draw_line(youX+8, youY, youX+5, youY+6, 1);    // right diagonal  
  tv.draw_line(youX+5, youY+6, youX+5, youY+11, 1); // vertical down
  
  drawLargeO(youX + letterSpacing, youY);
  drawLargeU(youX + (letterSpacing * 2), youY);
  
  // Draw "WIN"
  drawLargeW(winX, winY);
  drawLargeI(winX + letterSpacing, winY);
  drawLargeN(winX + (letterSpacing * 2), winY);
}

// New function for "I WIN"  
void drawLargeIWin() {
  // Positioning variables
  byte startXOffset = 10;
  
  byte iX = 55 + startXOffset;       // X position for "I" 
  byte iY = 30;       // Y position for "I"
  byte winX = 35 + startXOffset;     // X position for "WIN"
  byte winY = 50;     // Y position for "WIN"
  byte letterSpacing = 20;  // Space between letters
  
  // Draw "I"
  drawLargeI(iX, iY);
  
  // Draw "WIN"
  drawLargeW(winX, winY);
  drawLargeI(winX + letterSpacing, winY);
  drawLargeN(winX + (letterSpacing * 2), winY);
}

void drawLargeGameOver() {
  // Positioning variables - adjust these to center the text
  byte gameX = 35;    // X position for "GAME" 
  byte gameY = 35;    // Y position for "GAME"
  byte overX = 35;    // X position for "OVER"
  byte overY = 55;    // Y position for "OVER"
  byte letterSpacing = 20;  // Space between letters
  
  // Draw "GAME" 
  drawLargeG(gameX, gameY);
  drawLargeA(gameX + letterSpacing, gameY);
  drawLargeM(gameX + (letterSpacing * 2), gameY);
  drawLargeE(gameX + (letterSpacing * 3), gameY);
  
  // Draw "OVER"
  drawLargeO(overX, overY);
  drawLargeV(overX + letterSpacing, overY);
  drawLargeE(overX + (letterSpacing * 2), overY);
  drawLargeR(overX + (letterSpacing * 3), overY);
}

void drawLargeA(byte x, byte y) {
  tv.draw_line(x+1, y, x+5, y, 1);
  tv.draw_line(x+1, y, x+1, y+11, 1);
  tv.draw_line(x+8, y, x+8, y+11, 1);
  tv.draw_line(x+1, y+7, x+5, y+7, 1);
}

void drawLargeB(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);         // left
  tv.draw_line(x+1, y, x+5, y, 1);        // top
  tv.draw_line(x+6, y+1, x+6, y+5, 1);    // upper right
  tv.draw_line(x+1, y+6, x+5, y+6, 1);    // middle
  tv.draw_line(x+6, y+7, x+6, y+10, 1);   // lower right
  tv.draw_line(x+1, y+11, x+5, y+11, 1);  // bottom
}

void drawLargeD(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);         // left
  tv.draw_line(x+1, y, x+5, y, 1);        // top
  tv.draw_line(x+6, y+1, x+6, y+10, 1);   // right
  tv.draw_line(x+1, y+11, x+5, y+11, 1);  // bottom
}

void drawLargeE(byte x, byte y) {
  // Draw large E (7x12 pixels)
  tv.draw_line(x, y, x, y+11, 1);         // left side
  tv.draw_line(x+1, y, x+6, y, 1);        // top
  tv.draw_line(x+1, y+6, x+5, y+6, 1);    // middle
  tv.draw_line(x+1, y+11, x+6, y+11, 1);  // bottom
}

void drawLargeG(byte x, byte y) {
  tv.draw_line(x+1, y, x+6, y, 1);        // top
  tv.draw_line(x, y+1, x, y+10, 1);       // left side
  tv.draw_line(x+1, y+11, x+6, y+11, 1);  // bottom
  tv.draw_line(x+7, y+7, x+7, y+10, 1);   // right side bottom
  tv.draw_line(x+4, y+6, x+7, y+6, 1);    // middle bar
}

void drawLargeI(byte x, byte y) {
  tv.draw_line(x, y, x+6, y, 1);          // top
  tv.draw_line(x+3, y, x+3, y+11, 1);     // vertical
  tv.draw_line(x, y+11, x+6, y+11, 1);    // bottom
}

void drawLargeM(byte x, byte y) {
  // Draw large M (8x12 pixels)
  tv.draw_line(x, y, x, y+11, 1);         // left side
  tv.draw_line(x+8, y, x+8, y+11, 1);     // right side
  tv.draw_line(x, y, x+4, y+4, 1);        // left diagonal
  tv.draw_line(x+7, y, x+4, y+4, 1);      // right diagonal
}

void drawLargeN(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);         // left
  tv.draw_line(x+7, y, x+7, y+11, 1);     // right
  tv.draw_line(x, y, x+7, y+11, 1);       // diagonal
}

void drawLargeO(byte x, byte y) {
  tv.draw_line(x+1, y, x+6, y, 1);        // top
  tv.draw_line(x, y+1, x, y+10, 1);       // left
  tv.draw_line(x+7, y+1, x+7, y+10, 1);   // right
  tv.draw_line(x+1, y+11, x+6, y+11, 1);  // bottom
}

void drawLargeP(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);         // left
  tv.draw_line(x+1, y, x+5, y, 1);        // top
  tv.draw_line(x+6, y+1, x+6, y+5, 1);    // right
  tv.draw_line(x+1, y+6, x+5, y+6, 1);    // middle
}

void drawLargeR(byte x, byte y) {
  // Draw large R (7x12 pixels)
  tv.draw_line(x, y, x, y+11, 1);         // left side
  tv.draw_line(x+1, y, x+5, y, 1);        // top
  tv.draw_line(x+6, y+1, x+6, y+5, 1);    // right top
  tv.draw_line(x+1, y+6, x+5, y+6, 1);    // middle
  tv.draw_line(x+3, y+7, x+4, y+8, 1);    // diagonal
  tv.draw_line(x+5, y+9, x+6, y+11, 1);   // right bottom
}

void drawLargeS(byte x, byte y) {
  tv.draw_line(x+1, y, x+6, y, 1);        // top
  tv.draw_line(x, y+1, x, y+5, 1);        // upper left
  tv.draw_line(x+1, y+6, x+6, y+6, 1);    // middle
  tv.draw_line(x+7, y+7, x+7, y+10, 1);   // lower right
  tv.draw_line(x+1, y+11, x+6, y+11, 1);  // bottom
}

void drawLargeU(byte x, byte y) {
  tv.draw_line(x, y, x, y+10, 1);         // left
  tv.draw_line(x+7, y, x+7, y+10, 1);     // right
  tv.draw_line(x+1, y+11, x+6, y+11, 1);  // bottom
}

void drawLargeV(byte x, byte y) {
  // Draw large V (8x12 pixels)
  tv.draw_line(x, y, x+3, y+11, 1);        // left side
  tv.draw_line(x+8, y, x+5, y+11, 1);      // right side
}

void drawLargeW(byte x, byte y) {
  tv.draw_line(x, y+11, x, y, 1);         // left side
  tv.draw_line(x+8, y+11, x+8, y, 1);     // right side
  tv.draw_line(x, y+11, x+4, y+7, 1);     // left diagonal
  tv.draw_line(x+7, y+11, x+4, y+7, 1);   // right diagonal
}

void drawLargeY(byte x, byte y) {
  tv.draw_line(x, y, x+3, y+5, 1);        // left diagonal to center
  tv.draw_line(x+7, y, x+4, y+5, 1);      // right diagonal to center
  tv.draw_line(x+3, y+5, x+3, y+11, 1);   // vertical down from center
}

void initAttractScreen(){
  tv.fill(0);
  drawScore();
}

void initPong() {
  tv.fill(0);
  ballx = random(40, 57);
  bally = random(56, 73);
  dx = 1;
  dy = 0;
  ballServing = true;  // Ball starts in serving mode
  score = 0;
  score2 = 0;
  rallyCount = 0;    // Reset rally counter
  speedLevel = 1;    // Reset speed level to 1
  frameSkip = 0;     // Reset frame skip counter
  drawScore();
  tv.set_pixel(ballx, bally, 1);
  tv.set_pixel(ballx + 1, bally, 1);
  tv.set_pixel(ballx, bally + 1, 1);
  tv.set_pixel(ballx + 1, bally + 1, 1);
}

void updateComputerPaddle() {
  // Smart AI for LEFT paddle (computer) - only track when ball is heading left
  // and ball has crossed the center net area
  
  // Only track the ball if it's heading towards this paddle (dx < 0 means heading left)
  // AND the ball is past the center net (ballx < W/2 + 20)
  if (dx < 0 && ballx < (W/2 + 20)) {
    byte ballCenter = bally + 1;  // Ball center Y position
    
    // Smart targeting: position paddle so ball hits WITHIN the paddle area
    byte targetPaddlePos;
    
    // ANTI-INFINITE-RALLY: Check if paddle is at extreme position and force angled hits
    if (paddleAy <= 3) {
      // Paddle at top - can only move down, target lower to hit with bottom of paddle
      targetPaddlePos = min(20, ballCenter - 2);  // Force downward trajectory
    } else if (paddleAy >= (H - paddleLength - 3)) {
      // Paddle at bottom - can only move up, target higher to hit with top of paddle
      targetPaddlePos = max(H - paddleLength - 20, ballCenter - paddleLength + 2);  // Force upward trajectory
    } else if (ballCenter <= 6) {
      // Ball is very high - position paddle at top to catch it (improved range)
      targetPaddlePos = 1;
    } else if (ballCenter >= (H - 6)) {
      // Ball is very low - position paddle at bottom to catch it (improved range)
      targetPaddlePos = H - paddleLength - 1;
    } else {
      // Ball is in middle area
      // Choose different targeting strategies  
      int strategy = random(0, 100);
      
      if (strategy < 10) {
        // Try to hit ball with top third of paddle (create downward angle)
        targetPaddlePos = ballCenter - paddleLength + 2;
      } else {
        //  Center the paddle normally
        targetPaddlePos = ballCenter - (paddleLength / 2);
      }
    }
    
    aiPaddleTarget = targetPaddlePos;
    
    // Calculate desired movement
    float targetDistance = aiPaddleTarget - aiPaddleFloat;

    // add random speed degradation based on ball speed
    float currentMaxSpeed = maxAiSpeed;  // init to full speed
    
    // slow the movement down to cause occasional misses
    int errorChance = random(0, 100);
    if (errorChance < currentFaultPercent) {  //% chance we slow him down
      if (speedLevel >= 3) {
        currentMaxSpeed = maxAiSpeed * 0.70;  // 30% slower at speed level 3
      }
    }
    
    // Limit movement speed
    if (targetDistance > currentMaxSpeed) {
      targetDistance = currentMaxSpeed;
    } else if (targetDistance < -currentMaxSpeed) {
      targetDistance = -currentMaxSpeed;
    }
    
    // Add momentum damping for direction changes
    if ((aiMomentum > 0 && targetDistance < 0) || (aiMomentum < 0 && targetDistance > 0)) {
      targetDistance *= 0.5;  // Smooth direction changes
    }
    
    // Apply the movement
    aiPaddleFloat += targetDistance;
    aiMomentum = targetDistance;
  } else {
    // Ball is not heading towards this paddle or hasn't crossed center yet
    // Slowly drift towards center position or stay put
    float centerPos = (H - paddleLength) / 2.0;
    float driftDistance = (centerPos - aiPaddleFloat) * 0.02; // Very slow drift
    
    // Only drift if the distance is significant
    if (abs(driftDistance) > 0.1) {
      aiPaddleFloat += driftDistance;
      aiMomentum = driftDistance;
    } else {
      aiMomentum *= 0.9; // Gradually reduce momentum when not tracking
    }
  }
  
  // Convert to integer position
  paddleAy = (byte)(aiPaddleFloat + 0.5);
}

void updatePlayerAI() {
  // Player AI with similar skill degradation at higher speeds
  
  // Only track the ball if it's heading towards this paddle (dx > 0 means heading right)
  // AND the ball is past the center net (ballx > W/2 - 20)
  if (dx > 0 && ballx > (W/2 - 20)) {
    byte ballCenter = bally + 1;  // Ball center Y position
    
    // Same targeting logic as computer AI
    byte targetPaddlePos;
    
    // ANTI-INFINITE-RALLY: Check if paddle is at extreme position and force angled hits
    if (paddleBy <= 3) {
      // Paddle at top - can only move down, target lower to hit with bottom of paddle
      targetPaddlePos = min(20, ballCenter - 2);  // Force downward trajectory
    } else if (paddleBy >= (H - paddleLength - 3)) {
      // Paddle at bottom - can only move up, target higher to hit with top of paddle  
      targetPaddlePos = max(H - paddleLength - 20, ballCenter - paddleLength + 2);  // Force upward trajectory
    } else if (ballCenter <= 6) {
      // Ball is very high - position paddle at top to catch it (improved range)
      targetPaddlePos = 1;
    } else if (ballCenter >= (H - 6)) {
      // Ball is very low - position paddle at bottom to catch it (improved range)  
      targetPaddlePos = H - paddleLength - 1;
    } else {
      // Ball is in middle area - vary targeting strategy
      
      // // Choose different targeting strategies  
      int strategy = random(0, 100);
      
      if (strategy < 35) {
        // 35% - Try to hit ball with top third of paddle (create downward angle)
        targetPaddlePos = ballCenter - paddleLength + 2;
      } else if (strategy < 65) {
        // 30% - Try to hit ball with bottom third of paddle (create upward angle)  
        targetPaddlePos = ballCenter - 2;
      } else {
        // 35% - Center the paddle normally
        targetPaddlePos = ballCenter - (paddleLength / 2);
        // Add small random variation
        targetPaddlePos += random(-1, 2);
      }
    }
  
    playerAiTarget = targetPaddlePos;
    
    // Calculate desired movement
    float targetDistance = playerAiTarget - playerAiFloat;
    
    // add random speed degradation based on ball speed
    float currentMaxSpeed = maxAiSpeed;  // init to full speed
    
    // slow the movement down to cause occasional misses
    int errorChance = random(0, 100);
    if (errorChance < 60) { //60% chance we slow him down
      if (speedLevel >= 3) {
        currentMaxSpeed = maxAiSpeed * 0.70;  // 30% slower at speed level 3
      }
    }

    if (targetDistance > currentMaxSpeed) {
      targetDistance = currentMaxSpeed;
    } else if (targetDistance < -currentMaxSpeed) {
      targetDistance = -currentMaxSpeed;
    }
    
    // Same momentum damping as computer AI
    if ((playerAiMomentum > 0 && targetDistance < 0) || (playerAiMomentum < 0 && targetDistance > 0)) {
      targetDistance *= 0.5;
    }
    
    // Apply the movement
    playerAiFloat += targetDistance;
    playerAiMomentum = targetDistance;
  } else {
    // Ball is not heading towards this paddle or hasn't crossed center yet
    // Same drift behavior as computer AI
    float centerPos = (H - paddleLength) / 2.0;
    
    float driftDistance = (centerPos - playerAiFloat) * 0.02;
    
    // Only drift if the distance is significant
    if (abs(driftDistance) > 0.1) {
      playerAiFloat += driftDistance;
      playerAiMomentum = driftDistance;
    } else {
      playerAiMomentum *= 0.9;
    }
  }
  
  // Convert to integer position
  paddleBy = (byte)(playerAiFloat + 0.5);
}