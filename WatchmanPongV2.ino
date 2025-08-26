/*
  Pong game for Sony Watchman
  Sideburn Studios - August 2025
*/

#include <avr/pgmspace.h>
#include <TVout.h>
#include <video_gen.h>
#include <Controllers.h>

#define W 135
#define H 98
#define GAME_START 10  // D10 switch for attract mode control

#define MAX_SCORE 9

#define BUFFER_SIZE 12  // Increase for more smoothing (8=current, 16=very smooth, 6=more responsive)

// Pong variables
int ballx, bally;
char dx;
char dy;
boolean ballServing = false;  // Track if ball is waiting to serve
byte paddleAy = 44;           // Computer paddle (left side)
byte paddleBy = 44;           // Human paddle (right side)
byte paddleAx = 2;            // Computer paddle X position
byte paddleBx = W - 10;       // Human paddle X position
byte paddleWidth = 2;
byte paddleLength = 12;
byte score = 0;               // Computer score (left)
byte score2 = 0;              // Player score (right)
boolean attractMode = false;  // Track if in attract mode
boolean gameEnded = false;
boolean lastSwitchState = HIGH;  // assume HIGH = attract off at startup

// Rally speed system variables
byte rallyCount = 0;        // Track consecutive hits without a miss
byte speedLevel = 0;        // Current speed level (0-3)
byte frameSkip = 0;         // Frame counter for speed control
byte maxSpeedLevel = 3;     // Maximum speed level

// Computer AI variables
float aiPaddleTarget = 44.0;  // Target position for smooth AI movement
float aiPaddleFloat = 44.0;   // Floating point position for smooth movement
float maxAiSpeed = 2.5;       // Maximum pixels per frame the AI can move
float aiMomentum = 0.0;       // Track movement momentum to prevent oscillation

// Player AI variables (for attract mode)
float playerAiTarget = 44.0;  // Target position for player AI
float playerAiFloat = 44.0;   // Floating point position for player AI
float playerMaxSpeed = 0.9;   // Slower than computer AI
float playerAiMomentum = 0.0; // Player AI momentum

// Paddle smoothing variables - ADJUST THESE FOR FINE TUNING
int paddleBuffer[BUFFER_SIZE];
byte bufferIndex = 0;
boolean bufferFilled = false;
byte lastPaddleAy = 44;  // Track last paddle position for deadzone

TVout tv;

// Function declarations
void playTone(unsigned int frequency, unsigned long duration_ms);
void initPong();
void initAttractScreen();
void updateComputerPaddle();
void updatePlayerAI();
boolean ballOverlapsScore(int ballX, int ballY);
void drawPaddles();
void drawScore();
void drawLargeDigit(byte x, byte y, byte digit);
void drawPaddle(int x, int y);
void hitSound();
void bounceSound();
void missSound();
void drawNet();
void gameOver();
void drawIntroScreen();
void drawLargeS(byte x, byte y);
void drawLargeI(byte x, byte y);
void drawLargeD(byte x, byte y);
void drawLargeB(byte x, byte y);
void drawLargeU(byte x, byte y);
void drawLargeN(byte x, byte y);
void drawLargeP(byte x, byte y);
void drawLargeO(byte x, byte y);
void drawLargeG(byte x, byte y);
void drawLargeGameOver();
void drawLargeA(byte x, byte y);
void drawLargeM(byte x, byte y);
void drawLargeE(byte x, byte y);
void drawLargeV(byte x, byte y);
void drawLargeR(byte x, byte y);
void moveBall();
void pong();

void setup() {  
  // Set up D10 as attract mode switch
  pinMode(GAME_START, INPUT_PULLUP);  // D10 with internal pull-up
  tv.begin(NTSC, W, H);

  randomSeed(analogRead(0));

  // Startup tones
  playTone(100, 20);
  tv.delay(16);
  playTone(200, 20);
  tv.delay(16);
  playTone(400, 20);
  tv.delay(16);
  playTone(800, 20);

  tv.delay(160);
  
  // Initialize paddle smoothing buffer
  for(int i = 0; i < BUFFER_SIZE; i++) {
    paddleBuffer[i] = 855; // Initialize to center value (between 815-895)
  }

  // Show intro splash screen
  drawIntroScreen();
  
  initPong();
}

void loop() {
  bool currentSwitch = (digitalRead(GAME_START) == LOW);
 // --- detect switch change ---
 // --- restart game switching --
  if (currentSwitch != lastSwitchState) {
    if (currentSwitch) {
      // switched into attract mode → start fresh attract game
      //attractMode = true;
      tv.delay(3000);
      initPong();
    } else {
      // switched out of attract → start fresh real game
      //attractMode = false;
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
  attractMode = (digitalRead(GAME_START) == HIGH);  // Switch open = attract mode
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

  // Erase old ball position - but only if not in score area
  if (!ballOverlapsScore(ballx, bally)) {
    tv.set_pixel(ballx, bally, 0);
    tv.set_pixel(ballx + 1, bally, 0);
    tv.set_pixel(ballx, bally + 1, 0);
    tv.set_pixel(ballx + 1, bally + 1, 0);
  }

  moveBall();

  // Draw new ball position - but only if not in score area
  if (!ballOverlapsScore(ballx, bally)) {
    tv.set_pixel(ballx, bally, 1);
    tv.set_pixel(ballx + 1, bally, 1);
    tv.set_pixel(ballx, bally + 1, 1);
    tv.set_pixel(ballx + 1, bally + 1, 1);
  }

  tv.delay(1);
}

void moveBall() {
  // detect score - reset rally count on miss
  if (ballx < 0) {
    rallyCount = 0;  // Reset rally counter
    speedLevel = 1;  // Reset speed level
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
    speedLevel = 1;  // Reset speed level
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
    // Implement as: move 1 pixel normally, +1 pixel every 4th frame
    static byte frameCounter1 = 0;
    frameCounter1++;
    if (frameCounter1 >= 4) {
      moveX = dx * 2;
      moveY = dy * 2;
      frameCounter1 = 0;
    }
  } else if (speedLevel == 2) {
    // 50% faster - move 1.5 pixels on average  
    // Implement as: move 1 pixel normally, +1 pixel every 2nd frame
    static byte frameCounter2 = 0;
    frameCounter2++;
    if (frameCounter2 >= 2) {
      moveX = dx * 2;
      moveY = dy * 2;
      frameCounter2 = 0;
    }
  } else if (speedLevel == 3) {
    // 75% faster - move 1.75 pixels on average
    // Implement as: move 2 pixels normally, +1 pixel every 4th frame
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

  // Apply the calculated movement
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
    int rightPaddleB = paddleBx + paddleWidth + 2;  // Extended hit zone
    int topPaddleB = paddleBy - 4;
    int bottomPaddleB = paddleBy + paddleLength + 4;
    
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
    
    // Increase speed level based on rally count
    // if (rallyCount >= 2 && speedLevel == 0) {
    //   speedLevel = 1;  // 25% faster after 4 hits
    // } else if (rallyCount >= 8 && speedLevel == 1) {
    //   speedLevel = 2;  // 50% faster after 8 hits
    // } else if (rallyCount >= 12 && speedLevel == 2) {
    //   speedLevel = 3;  // 75% faster after 12 hits
    // }

    if (rallyCount >= 3 && speedLevel == 1) {
      speedLevel = 2;  // 50% faster after 8 hits
    } else if (rallyCount >= 6 && speedLevel == 2) {
      speedLevel = 3;  // 75% faster after 12 hits
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
    // Human player - Read raw analog value from A3 (your potentiometer with 10K pull-up)
    int rawValue = analogRead(A3);
    
    // Your actual measured ADC values with 10K pull-up resistor
    int minReading = 815;  // Lowered from 820 to capture full top range
    int maxReading = 895;  // Raised from 890 to capture full bottom range
    
    // Constrain the raw value to expected range
    rawValue = constrain(rawValue, minReading, maxReading);
    
    // Add to circular buffer for smoothing
    paddleBuffer[bufferIndex] = rawValue;
    bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
    if (!bufferFilled && bufferIndex == 0) {
      bufferFilled = true;
    }
    
    // Calculate moving average
    long sum = 0;
    int samplesUsed = bufferFilled ? BUFFER_SIZE : bufferIndex;
    for(int i = 0; i < samplesUsed; i++) {
      sum += paddleBuffer[i];
    }
    int smoothedValue = sum / samplesUsed;
    
    // Map smoothed value to RIGHT paddle position (human player)
    byte newPaddleBy = map(smoothedValue, minReading, maxReading, 1, H - paddleLength - 1);
    
    // FINE-TUNING: Add deadzone for human paddle
    if (abs(newPaddleBy - lastPaddleAy) > 0) {  
      paddleBy = newPaddleBy;
      lastPaddleAy = paddleBy;  // Update tracking variable
    }
  }
  
  // Boundary checking
  if(paddleAy < 1) paddleAy = 1;
  if(paddleAy > H - paddleLength - 1) paddleAy = H - paddleLength - 1;
  
  if(paddleBy < 1) paddleBy = 1;
  if(paddleBy > H - paddleLength - 1) paddleBy = H - paddleLength - 1;
  
  drawPaddle(paddleAx, paddleAy);  // Computer paddle (left)
  drawPaddle(paddleBx, paddleBy);  // Human paddle (right)
}

void drawScore() {
  // Clear score areas
  for (byte y = 0; y < 15; y++) {
    for (byte x = 20; x <= 35; x++) {
      tv.set_pixel(x, y, 0);
    }
    for (byte x = 100; x <= 115; x++) {
      tv.set_pixel(x, y, 0);
    }
  }
  
  // Draw large digits manually
  drawLargeDigit(25, 2, score);   // Computer score (left)
  drawLargeDigit(105, 2, score2); // Player score (right)
}

void drawLargeDigit(byte x, byte y, byte digit) {
  // Draw large 7x11 pixel digits
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
      tv.draw_line(x+2, y+1, x+3, y, 1);     // top angle
      tv.draw_line(x+1, y+10, x+5, y+10, 1); // bottom
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

void drawPaddle(int x, int y) {
  for (int i = x; i < x + paddleWidth; i++) {
    tv.draw_line(i, 0, i, H - 1, 0);
    tv.draw_line(i, y, i, y + paddleLength, 1);
  }
}

void hitSound() {
  if (!attractMode) playTone(523, 20);  // Only play sound if not in attract mode
}

void bounceSound() {
  if (!attractMode) playTone(261, 20);  // Only play sound if not in attract mode
}

void missSound() {
  if (!attractMode) {
    for (int i = 0; i < 19; i++) {
      tv.delay(19);
      playTone(240, 19);
    }
  }
}

void drawNet() {
  for (byte y = 0; y < H - 4; y = y + 8) {
    tv.draw_line(W / 2, y, W / 2, y + 4, 1);
  }
}

void gameOver() {
  tv.delay(1000);
  tv.fill(0);
  
  // Draw large "GAME OVER" using custom drawing
  drawLargeGameOver();
  drawScore();
  gameEnded = true;
  tv.delay(3000);
}

void drawIntroScreen() {
  tv.fill(0);

  // Adjust positions to center text
  byte startX = 30; 
  byte lineSpacing = 18;  

  // Draw "SIDE"
  byte y1 = 15;
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

void drawLargeS(byte x, byte y) {
  tv.draw_line(x+1, y, x+6, y, 1);        // top
  tv.draw_line(x, y+1, x, y+5, 1);        // upper left
  tv.draw_line(x+1, y+6, x+6, y+6, 1);    // middle
  tv.draw_line(x+7, y+7, x+7, y+10, 1);   // lower right
  tv.draw_line(x+1, y+11, x+6, y+11, 1);  // bottom
}

void drawLargeI(byte x, byte y) {
  tv.draw_line(x, y, x+6, y, 1);          // top
  tv.draw_line(x+3, y, x+3, y+11, 1);     // vertical
  tv.draw_line(x, y+11, x+6, y+11, 1);    // bottom
}

void drawLargeD(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);         // left
  tv.draw_line(x+1, y, x+5, y, 1);        // top
  tv.draw_line(x+6, y+1, x+6, y+10, 1);   // right
  tv.draw_line(x+1, y+11, x+5, y+11, 1);  // bottom
}

void drawLargeB(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);         // left
  tv.draw_line(x+1, y, x+5, y, 1);        // top
  tv.draw_line(x+6, y+1, x+6, y+5, 1);    // upper right
  tv.draw_line(x+1, y+6, x+5, y+6, 1);    // middle
  tv.draw_line(x+6, y+7, x+6, y+10, 1);   // lower right
  tv.draw_line(x+1, y+11, x+5, y+11, 1);  // bottom
}

void drawLargeU(byte x, byte y) {
  tv.draw_line(x, y, x, y+10, 1);         // left
  tv.draw_line(x+7, y, x+7, y+10, 1);     // right
  tv.draw_line(x+1, y+11, x+6, y+11, 1);  // bottom
}

void drawLargeN(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);         // left
  tv.draw_line(x+7, y, x+7, y+11, 1);     // right
  tv.draw_line(x, y, x+7, y+11, 1);       // diagonal
}

void drawLargeP(byte x, byte y) {
  tv.draw_line(x, y, x, y+11, 1);         // left
  tv.draw_line(x+1, y, x+5, y, 1);        // top
  tv.draw_line(x+6, y+1, x+6, y+5, 1);    // right
  tv.draw_line(x+1, y+6, x+5, y+6, 1);    // middle
}

void drawLargeO(byte x, byte y) {
  tv.draw_line(x+1, y, x+6, y, 1);        // top
  tv.draw_line(x, y+1, x, y+10, 1);       // left
  tv.draw_line(x+7, y+1, x+7, y+10, 1);   // right
  tv.draw_line(x+1, y+11, x+6, y+11, 1);  // bottom
}

void drawLargeG(byte x, byte y) {
  tv.draw_line(x+1, y, x+6, y, 1);        // top
  tv.draw_line(x, y+1, x, y+10, 1);       // left side
  tv.draw_line(x+1, y+11, x+6, y+11, 1);  // bottom
  tv.draw_line(x+7, y+7, x+7, y+10, 1);   // right side bottom
  tv.draw_line(x+4, y+6, x+7, y+6, 1);    // middle bar
}

void drawLargeGameOver() {
  // Positioning variables - adjust these to center the text
  byte gameX = 30;    // X position for "GAME" 
  byte gameY = 35;    // Y position for "GAME"
  byte overX = 30;    // X position for "OVER"
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
  // Draw large A (8x12 pixels)
  
  // Top horizontal bar (double-thick)
  tv.draw_line(x+1, y, x+5, y, 1);

  // Left vertical bar
  tv.draw_line(x+1, y, x+1, y+11, 1);

  // Right vertical bar
  tv.draw_line(x+8, y, x+8, y+11, 1);

  // Crossbar (double-thick)
  tv.draw_line(x+1, y+7, x+5, y+7, 1);
}

void drawLargeM(byte x, byte y) {
  // Draw large M (8x12 pixels)
  tv.draw_line(x, y, x, y+11, 1);         // left side
  tv.draw_line(x+8, y, x+8, y+11, 1);     // right side
  tv.draw_line(x, y, x+4, y+4, 1);        // left diagonal
  tv.draw_line(x+5, y, x+4, y+4, 1);      // right diagonal
}

void drawLargeE(byte x, byte y) {
  // Draw large E (7x12 pixels)
  tv.draw_line(x, y, x, y+11, 1);         // left side
  tv.draw_line(x+1, y, x+6, y, 1);        // top
  tv.draw_line(x+1, y+6, x+5, y+6, 1);    // middle
  tv.draw_line(x+1, y+11, x+6, y+11, 1);  // bottom
}

void drawLargeV(byte x, byte y) {
  // Draw large V (8x12 pixels)
  tv.draw_line(x, y, x+1, y+8, 1);        // left side
  tv.draw_line(x+6, y, x+5, y+8, 1);      // right side
  tv.draw_line(x+2, y+9, x+2, y+9, 1);    // bottom left
  tv.draw_line(x+5, y+9, x+5, y+9, 1);    // bottom right
  tv.draw_line(x+3, y+10, x+4, y+10, 1);  // bottom center
  tv.set_pixel(x+3, y+11, 1);             // bottom point
  tv.set_pixel(x+4, y+11, 1);             // bottom point
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
  speedLevel = 1;    // Reset speed level
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
    
    if (ballCenter <= 8) {
      // Ball is very high - position paddle at top to catch it
      targetPaddlePos = 1;
    } else if (ballCenter >= (H - 8)) {
      // Ball is very low - position paddle at bottom to catch it  
      targetPaddlePos = H - paddleLength - 1;
    } else {
        targetPaddlePos = ballCenter - (paddleLength / 2);   
      // Constrain to valid range
      if (targetPaddlePos < 1) targetPaddlePos = 1;
      if (targetPaddlePos > H - paddleLength - 1) targetPaddlePos = H - paddleLength - 1;
    }
    
    // BOUNDARY FIX: If paddle will be at boundary, offset targeting to avoid center hits
    if (targetPaddlePos <= 1 || targetPaddlePos >= H - paddleLength - 1) {
      targetPaddlePos += random(-3, 4);  // Add random offset when at boundary
      // Re-constrain after adding offset
      if (targetPaddlePos < 1) targetPaddlePos = 1;
      if (targetPaddlePos > H - paddleLength - 1) targetPaddlePos = H - paddleLength - 1;
    }
    
    // Add speed-based errors
    if (speedLevel >= 1) { //must be at least speed level 1

      // At higher speeds, add tracking errors
      //int errorChance = speedLevel * 2;  // 2%, 4%, 6% error chance depending on speed
      
      //fixed error chance regardless of speed
      int errorChance =  4;  // 4%, error chance

      if (random(0, 100) < errorChance) {
        // Add random error to target position
        //int error = random(-2, 3) * speedLevel;  // Smaller errors (was -4 to 5)
        int error = random(-2, 3);  // disregard speed level
        targetPaddlePos += error;
        
        // Keep within bounds
        if (targetPaddlePos < 1) targetPaddlePos = 1;
        if (targetPaddlePos > H - paddleLength - 1) targetPaddlePos = H - paddleLength - 1;
      }
    }
    
    aiPaddleTarget = targetPaddlePos;
    
    // Calculate desired movement
    float targetDistance = aiPaddleTarget - aiPaddleFloat;
    
    // Reduce AI speed at higher ball speeds (reaction time degrades) - less harsh
    float currentMaxSpeed = maxAiSpeed;
    if (speedLevel >= 2) {
      currentMaxSpeed = maxAiSpeed * 0.9;  // 10% slower at speed level 2 (was 20%)
    }
    if (speedLevel >= 3) {
      currentMaxSpeed = maxAiSpeed * 0.8;  // 20% slower at speed level 3 (was 40%)
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
  
  // Final boundary check
  if (paddleAy < 1) {
    paddleAy = 1;
    aiPaddleFloat = 1.0;
    aiMomentum = 0.0;
  }
  if (paddleAy > H - paddleLength - 1) {
    paddleAy = H - paddleLength - 1;
    aiPaddleFloat = H - paddleLength - 1;
    aiMomentum = 0.0;
  }
}

void updatePlayerAI() {
  // Player AI with similar skill degradation at higher speeds
  
  // Only track the ball if it's heading towards this paddle (dx > 0 means heading right)
  // AND the ball is past the center net (ballx > W/2 - 20)
  if (dx > 0 && ballx > (W/2 - 20)) {
    byte ballCenter = bally + 1;  // Ball center Y position
    
    // Same targeting logic as computer AI
    byte targetPaddlePos;
    
    if (ballCenter <= 8) {
      // Ball is very high - position paddle at top to catch it
      targetPaddlePos = 1;
    } else if (ballCenter >= (H - 8)) {
      // Ball is very low - position paddle at bottom to catch it  
      targetPaddlePos = H - paddleLength - 1;
    } else {
      // Ball is in middle area - vary targeting strategy (slightly different from computer)
      
      // Choose different targeting strategies  
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
      
      // Constrain to valid range
      if (targetPaddlePos < 1) targetPaddlePos = 1;
      if (targetPaddlePos > H - paddleLength - 1) targetPaddlePos = H - paddleLength - 1;
    }
    
    // BOUNDARY FIX: If paddle will be at boundary, offset targeting to avoid center hits
    if (targetPaddlePos <= 1 || targetPaddlePos >= H - paddleLength - 1) {
      targetPaddlePos += random(-3, 4);  // Add random offset when at boundary
      // Re-constrain after adding offset
      if (targetPaddlePos < 1) targetPaddlePos = 1;
      if (targetPaddlePos > H - paddleLength - 1) targetPaddlePos = H - paddleLength - 1;
    }
    
    // Add speed-based errors - Player AI also gets worse as ball gets faster (adjusted)
    if (speedLevel >= 1) {
      // Player AI has slightly more errors than computer AI
      // At higher speeds, add tracking errors
      //int errorChance = speedLevel * 2;  // 2%, 4%, 6% error chance depending on speed

      //fixed error chance regardless of speed
      int errorChance =  4;  // 4%, error chance

      if (random(0, 100) < errorChance) {
        // Add random error to target position
        //int error = random(-2, 3) * speedLevel;  // Smaller errors (was -4 to 5)
        int error = random(-2, 3);  // disregard speed level

        targetPaddlePos += error;
        
        // Keep within bounds
        if (targetPaddlePos < 1) targetPaddlePos = 1;
        if (targetPaddlePos > H - paddleLength - 1) targetPaddlePos = H - paddleLength - 1;
      }
    }
    
    playerAiTarget = targetPaddlePos;
    
    // Calculate desired movement
    float targetDistance = playerAiTarget - playerAiFloat;
    
    // Player AI is slightly slower than computer AI and degrades more gradually
    float currentMaxSpeed = maxAiSpeed * 0.95;  // Start 5% slower than computer (was 10%)
    if (speedLevel >= 2) {
      currentMaxSpeed = maxAiSpeed * 0.85;  // 15% slower at speed level 2 (was 30%)
    }
    if (speedLevel >= 3) {
      currentMaxSpeed = maxAiSpeed * 0.75;  // 25% slower at speed level 3 (was 50%)
    }
    
    // Limit movement speed
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
  
  // Final boundary check with momentum reset
  if (paddleBy < 1) {
    paddleBy = 1;
    playerAiFloat = 1.0;
    playerAiMomentum = 0.0;
  }
  if (paddleBy > H - paddleLength - 1) {
    paddleBy = H - paddleLength - 1;
    playerAiFloat = H - paddleLength - 1;
    playerAiMomentum = 0.0;
  }
}

boolean ballOverlapsScore(int ballX, int ballY) {
  // Check if any of the 4 ball pixels (2x2) overlap with score areas
  for (int x = ballX; x <= ballX + 1; x++) {
    for (int y = ballY; y <= ballY + 1; y++) {
      // Check if this pixel is in either score area
      if (y <= 13 && ((x >= 20 && x <= 35) || (x >= 100 && x <= 115))) {
        return true;  // Ball overlaps score area
      }
    }
  }
  return false;  // Ball is clear of score areas
}