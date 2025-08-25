/*
  Pong game for Sony Watchman
  Sideburn Studios - August 2025
*/

#include <avr/pgmspace.h>
#include <TVout.h>
#include <video_gen.h>
//#include <font4x6.h>
#include <font6x8.h>
#include <Controllers.h>

#define W 135
#define H 98
#define GAME_START 10  // D10 switch for attract mode control

// Pong variables
int ballx, bally;
char dx;
char dy;
byte paddleAy = 44;    // Computer paddle (left side)
byte paddleBy = 44;    // Human paddle (right side)
byte paddleAx = 2;     // Computer paddle X position
byte paddleBx = W - 10; // Human paddle X position
byte paddleWidth = 2;
byte paddleLength = 12;
byte score = 0;        // Computer score (left)
byte score2 = 0;       // Player score (right)
boolean attractMode = false;  // Track if in attract mode
boolean gameEnded = false;
boolean lastSwitchState = HIGH;  // assume HIGH = attract off at startup


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
#define BUFFER_SIZE 12  // Increase for more smoothing (8=current, 16=very smooth, 6=more responsive)
int paddleBuffer[BUFFER_SIZE];
byte bufferIndex = 0;
boolean bufferFilled = false;
byte lastPaddleAy = 44;  // Track last paddle position for deadzone

TVout tv;

void setup() {  
  // Set up D10 as attract mode switch
  pinMode(GAME_START, INPUT_PULLUP);  // D10 with internal pull-up
  tv.begin(NTSC, W, H);

  tv.select_font(font6x8);
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
  
  drawNet();
  drawPaddles();

  if (dy == 0) {
    // Ball auto-serves immediately instead of waiting
    if (random(0, 2) == 0) {
      dy = 1;
    } else {
      dy = -1;
    }
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

void drawPaddles() {
  // Computer AI for left paddle
  updateComputerPaddle();
  
  if (attractMode) {
    // In attract mode, use AI for right paddle too (different skill level)
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

// Replace your moveBall() function with this enhanced version
void moveBall() {
  // detect score
  if (ballx < 0) {
    if (!attractMode) score2++;  // Only count score if not in attract mode
    drawScore();
    drawNet();
    scoreSound();
    tv.delay(1500);  // Added delay after miss before re-serve
    if (!attractMode && score2 == 9) {  // Only end game if not in attract mode
      gameOver();
      //initPong();
      initAttractScreen();
      return;
    }
    ballx = random(W / 2, (3 * W) / 4);
    bally = random(H / 4, (3 * H) / 4);
    dx = -1;
    dy = 0;
  }
  if (ballx >= (W + 2)) {
    if (!attractMode) score++;   // Only count score if not in attract mode
    drawNet();
    drawScore();
    scoreSound();
    tv.delay(1500);  // Added delay after miss before re-serve
    if (!attractMode && score == 9) {  // Only end game if not in attract mode
      gameOver();
      //initPong();
      initAttractScreen();
      return;
    }
    ballx = random(W / 4, W / 2);
    bally = random(H / 4, (3 * H) / 4);
    dx = 1;
    dy = 0;
  }

  // Enhanced paddle collision detection with angle physics
  
  // detect hit with paddle A (computer - left side)
  if (ballx == paddleAx + paddleWidth) {
    if ((bally >= paddleAy - 2) && (bally < (paddleAy + paddleLength + 2))) {
      dx = 1;
      
      // Calculate where ball hit on paddle (0.0 = top edge, 1.0 = bottom edge)
      float hitPosition = (float)(bally - paddleAy + 2) / (float)(paddleLength + 4);
      
      // Clamp to valid range
      if (hitPosition < 0.0) hitPosition = 0.0;
      if (hitPosition > 1.0) hitPosition = 1.0;
      
      // Calculate new dy based on hit position
      // Center hits (around 0.5) give dy = 0 or ±1
      // Edge hits give stronger angles (±2 or ±3)
      float centerOffset = hitPosition - 0.5; // -0.5 to +0.5
      
      if (abs(centerOffset) < 0.15) {
        // Center hit - slow and straight
        dy = (centerOffset > 0) ? 1 : -1;
        if (random(0, 3) == 0) dy = 0; // Sometimes perfectly straight
      } else if (abs(centerOffset) < 0.35) {
        // Medium hit
        dy = (centerOffset > 0) ? 2 : -2;
      } else {
        // Edge hit - fast and steep angle
        dy = (centerOffset > 0) ? 3 : -3;
      }
      
      hitSound();
    }
  }

  // detect hit with paddle B (human - right side)
  if (ballx == paddleBx - 2) {
    if ((bally >= paddleBy - 4) && (bally < (paddleBy + paddleLength + 4))) {
      dx = -1;
      
      // Calculate where ball hit on paddle (0.0 = top edge, 1.0 = bottom edge)
      float hitPosition = (float)(bally - paddleBy + 2) / (float)(paddleLength + 4);
      
      // Clamp to valid range
      if (hitPosition < 0.0) hitPosition = 0.0;
      if (hitPosition > 1.0) hitPosition = 1.0;
      
      // Calculate new dy based on hit position
      float centerOffset = hitPosition - 0.5; // -0.5 to +0.5
      
      if (abs(centerOffset) < 0.15) {
        // Center hit - slow and straight
        dy = (centerOffset > 0) ? 1 : -1;
        if (random(0, 3) == 0) dy = 0; // Sometimes perfectly straight
      } else if (abs(centerOffset) < 0.35) {
        // Medium hit
        dy = (centerOffset > 0) ? 2 : -2;
      } else {
        // Edge hit - fast and steep angle
        dy = (centerOffset > 0) ? 3 : -3;
      }
      
      hitSound();
    }
  }

  // detect hit with top/bottom walls
  if (bally <= 0) {
    bounceSound();
    dy = abs(dy); // Always bounce down from top wall
  }
  if (bally >= (H - 2)) {
    bounceSound();
    dy = -abs(dy); // Always bounce up from bottom wall
  }

  // Apply movement with variable speed
  ballx = ballx + dx;
  
  // Apply vertical movement based on dy magnitude
  if (abs(dy) >= 3) {
    // Fast ball - move 2 pixels vertically sometimes for extra speed
    bally = bally + dy + (dy > 0 ? 1 : -1);
  } else if (abs(dy) == 2) {
    // Medium speed - normal movement but chance for extra pixel
    bally = bally + dy;
    if (random(0, 3) == 0) {
      bally = bally + (dy > 0 ? 1 : -1);
    }
  } else {
    // Normal or slow speed
    bally = bally + dy;
  }
  
  // Prevent ball from going off screen vertically
  if (bally < 0) {
    bally = 0;
    dy = abs(dy);
  }
  if (bally >= H - 1) {
    bally = H - 2;
    dy = -abs(dy);
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

void scoreSound() {
  if (!attractMode) playTone(105, 500);  // Only play sound if not in attract mode
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

void drawLargeG(byte x, byte y) {
  // Draw large G (8x12 pixels)
  tv.draw_line(x+1, y, x+6, y, 1);        // top
  tv.draw_line(x, y+1, x, y+10, 1);       // left side
  tv.draw_line(x+1, y+11, x+6, y+11, 1);  // bottom
  tv.draw_line(x+7, y+7, x+7, y+10, 1);   // right side bottom
  tv.draw_line(x+4, y+6, x+7, y+6, 1);    // middle bar
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
  tv.draw_line(x, y, x+4, y+4, 1);    // left diagonal
  tv.draw_line(x+5, y, x+4, y+4, 1);    // left diagonal
 }

void drawLargeE(byte x, byte y) {
  // Draw large E (7x12 pixels)
  tv.draw_line(x, y, x, y+11, 1);         // left side
  tv.draw_line(x+1, y, x+6, y, 1);        // top
  tv.draw_line(x+1, y+6, x+5, y+6, 1);    // middle
  tv.draw_line(x+1, y+11, x+6, y+11, 1);  // bottom
}

void drawLargeO(byte x, byte y) {
  // Draw large O (8x12 pixels) - same as digit 0 but bigger
  tv.draw_line(x+1, y, x+6, y, 1);        // top
  tv.draw_line(x, y+1, x, y+10, 1);       // left
  tv.draw_line(x+7, y+1, x+7, y+10, 1);   // right
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
  score = 0;
  score2 = 0;
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
      // Ball is in middle area - center paddle on ball
      targetPaddlePos = ballCenter - (paddleLength / 2);
      
      // Constrain to valid range
      if (targetPaddlePos < 1) targetPaddlePos = 1;
      if (targetPaddlePos > H - paddleLength - 1) targetPaddlePos = H - paddleLength - 1;
    }
    
    aiPaddleTarget = targetPaddlePos;
    
    // Calculate desired movement
    float targetDistance = aiPaddleTarget - aiPaddleFloat;
    
    // Limit movement speed
    if (targetDistance > maxAiSpeed) {
      targetDistance = maxAiSpeed;
    } else if (targetDistance < -maxAiSpeed) {
      targetDistance = -maxAiSpeed;
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
  // Less skilled AI for RIGHT paddle (player side in attract mode)
  // Only track when ball is heading right and near center
  
  // Only track the ball if it's heading towards this paddle (dx > 0 means heading right)
  // AND the ball is past the center net (ballx > W/2 - 20)
  if (dx > 0 && ballx > (W/2 - 20)) {
    byte ballCenter = bally + 1;  // Ball center Y position
    
    // Smart targeting with less precision than computer AI
    byte targetPaddlePos;
    
    if (ballCenter <= 10) {  // Slightly less precise boundary detection
      // Ball is very high - position paddle at top to catch it
      targetPaddlePos = 2;  // Not quite as precise as computer
    } else if (ballCenter >= (H - 10)) {
      // Ball is very low - position paddle at bottom to catch it  
      targetPaddlePos = H - paddleLength - 3;  // Not quite as precise
    } else {
      // Ball is in middle area - center paddle on ball with some error
      targetPaddlePos = ballCenter - (paddleLength / 2);
      
      // Add some random error to make it less perfect (only occasionally)
      if (random(0, 10) < 2) {  // 20% chance of slight error
        targetPaddlePos += random(-2, 3);
      }
      
      // Constrain to valid range
      if (targetPaddlePos < 1) targetPaddlePos = 1;
      if (targetPaddlePos > H - paddleLength - 1) targetPaddlePos = H - paddleLength - 1;
    }
    
    playerAiTarget = targetPaddlePos;
    
    // Calculate desired movement
    float targetDistance = playerAiTarget - playerAiFloat;
    
    // Limit movement speed (slower than computer)
    if (targetDistance > playerMaxSpeed) {
      targetDistance = playerMaxSpeed;
    } else if (targetDistance < -playerMaxSpeed) {
      targetDistance = -playerMaxSpeed;
    }
    
    // Add momentum damping (same as computer for smoothness)
    if ((playerAiMomentum > 0 && targetDistance < 0) || (playerAiMomentum < 0 && targetDistance > 0)) {
      targetDistance *= 0.5;  // Same damping as computer AI
    }
    
    // Apply the movement
    playerAiFloat += targetDistance;
    playerAiMomentum = targetDistance;
  } else {
    // Ball is not heading towards this paddle or hasn't crossed center yet
    // Slowly drift towards center position with more error than computer
    float centerPos = (H - paddleLength) / 2.0;
    
    // Add some randomness to the center drift for less predictable behavior
    if (random(0, 100) < 1) {  // 1% chance each frame to add some random drift
      centerPos += random(-5, 6);
    }
    
    float driftDistance = (centerPos - playerAiFloat) * 0.015; // Slower drift than computer
    
    // Only drift if the distance is significant
    if (abs(driftDistance) > 0.1) {
      playerAiFloat += driftDistance;
      playerAiMomentum = driftDistance;
    } else {
      playerAiMomentum *= 0.8; // Reduce momentum faster than computer (less stable)
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

void playTone(unsigned int frequency, unsigned long duration_ms) {
  tv.tone(frequency, duration_ms);
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