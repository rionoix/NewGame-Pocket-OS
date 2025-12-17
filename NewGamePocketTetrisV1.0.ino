/*
   PROJECT: ESP32 TETRIS ULTIMATE v2.0
   EDITION: Silent & Enhanced
   HARDWARE: ESP32 + SSD1306 OLED + Joystick
   
   CHANGELOG v2.0:
   - Audio Muted (Silent Mode)
   - Fixed Game Over Logic (Instant detection)
   - Added Dotted Grid Background
   - Added Screen Wipe Animation
   - Remapped Controls (Up to Rotate, Click to Hard Drop)
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <vector> 

// ================================================================
// 1. HARDWARE
// ================================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define VRX_PIN  34
#define VRY_PIN  35
#define SW_PIN   32
#define BUZZ_PIN 25 // Tidak digunakan (Silent)

// WiFi (HARDCODED)
const char* ssid = "UNAND";
const char* password = "HardiknasDiAndalas";

WebServer server(80);
const char* updatePage = "<h1 style='font-family:sans-serif'>ESP32 TETRIS OS UPDATE</h1><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update Firmware'></form>";

Preferences prefs;

// ================================================================
// 2. SETTING GAME
// ================================================================
#define GRID_W 10
#define GRID_H 20
#define BLOCK_SIZE 3 
#define BOARD_X 40   // Posisi Papan (Geser dikit biar UI muat)
#define BOARD_Y 2    

// Tetromino Definitions (I, J, L, O, S, T, Z)
const byte TETROMINOES[8][4][4] = {
  {{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0}}, // Empty
  {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}}, // I
  {{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}, // J
  {{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}, // L
  {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}, // O
  {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}}, // S
  {{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}, // T
  {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}  // Z
};

// ================================================================
// 3. GLOBAL VARIABLES
// ================================================================
enum GameState { STATE_MENU, STATE_PLAY, STATE_GAMEOVER, STATE_UPDATE };
GameState currentState = STATE_MENU;

byte board[GRID_W][GRID_H];
unsigned long score = 0;
unsigned long highScore = 0;
int level = 1;
int linesCleared = 0;

// Pieces
int currentX, currentY, currentType, currentRotation;
int nextType;
int ghostY;
int bag[7];
int bagIndex = 0;

// Timing & Input
unsigned long lastFallTime = 0;
unsigned long lastInputTime = 0;
unsigned long lastMoveTime = 0;
int fallSpeed = 800;
int lockDelay = 500;
unsigned long lockTimer = 0;
bool isLocking = false;
bool btnPressed = false;

// Particles
struct Particle { float x, y, vx, vy; int life; };
std::vector<Particle> particles;

// Menu
int menuIndex = 0;

// ================================================================
// 4. HELPER & ENGINE
// ================================================================

// SILENT MODE (Fungsi kosong)
void beep(int f, int d) { 
  // Biarkan kosong agar hening
}

void resetBoard() {
  for(int x=0; x<GRID_W; x++) for(int y=0; y<GRID_H; y++) board[x][y] = 0;
}

void shuffleBag() {
  for(int i=0; i<7; i++) bag[i] = i+1;
  for (int i = 6; i > 0; i--) {
    int j = random(0, i + 1);
    int t = bag[i]; bag[i] = bag[j]; bag[j] = t;
  }
  bagIndex = 0;
}

int getNextPiece() {
  int piece = bag[bagIndex++];
  if(bagIndex >= 7) shuffleBag();
  return piece;
}

int getRotatedBlock(int type, int r, int x, int y) {
  switch (r % 4) {
    case 0: return TETROMINOES[type][y][x];
    case 1: return TETROMINOES[type][3-x][y];
    case 2: return TETROMINOES[type][3-y][3-x];
    case 3: return TETROMINOES[type][x][3-y];
  }
  return 0;
}

// COLLISION DETECTION (CRUCIAL FOR GAME OVER)
bool checkCollision(int x, int y, int type, int rot) {
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (getRotatedBlock(type, rot, c, r)) {
        int testX = x + c;
        int testY = y + r;
        // Batas Kiri/Kanan/Bawah
        if (testX < 0 || testX >= GRID_W || testY >= GRID_H) return true;
        // Tabrak Block Lain (Pastikan Y >= 0 karena board array mulai dari 0)
        if (testY >= 0 && board[testX][testY] != 0) return true;
      }
    }
  }
  return false;
}

void spawnNewPiece() {
  currentType = nextType;
  nextType = getNextPiece();
  currentX = 3; 
  currentY = -1; // Spawn sedikit di atas
  currentRotation = 0;
  isLocking = false;

  // FIX GAME OVER: Cek tabrakan langsung saat spawn
  if (checkCollision(currentX, currentY, currentType, currentRotation)) {
    currentState = STATE_GAMEOVER;
    // Simpan Highscore
    if (score > highScore) {
      highScore = score;
      prefs.begin("tetris", false);
      prefs.putULong("hs", highScore);
      prefs.end();
    }
  }
}

// PARTICLE EFFECTS
void spawnExplosion(int yRow) {
  for (int i = 0; i < 15; i++) {
    Particle p;
    p.x = BOARD_X + (random(0, GRID_W) * BLOCK_SIZE);
    p.y = BOARD_Y + (yRow * BLOCK_SIZE);
    p.vx = random(-20, 20) / 10.0;
    p.vy = random(-10, 10) / 5.0; // Meledak ke atas/bawah
    p.life = 15;
    particles.push_back(p);
  }
}

void updateParticles() {
  for (int i = particles.size() - 1; i >= 0; i--) {
    particles[i].x += particles[i].vx;
    particles[i].y += particles[i].vy;
    particles[i].life--;
    if (particles[i].life > 0) display.drawPixel((int)particles[i].x, (int)particles[i].y, (particles[i].life%2)?WHITE:BLACK);
    else particles.erase(particles.begin() + i);
  }
}

void lockPiece() {
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (getRotatedBlock(currentType, currentRotation, c, r)) {
        int fy = currentY + r;
        if (fy >= 0) board[currentX + c][fy] = currentType;
      }
    }
  }
  
  // Line Clear
  int lines = 0;
  for (int y = 0; y < GRID_H; y++) {
    bool full = true;
    for (int x = 0; x < GRID_W; x++) if (board[x][y] == 0) full = false;
    if (full) {
      spawnExplosion(y);
      for (int k = y; k > 0; k--) 
        for (int x = 0; x < GRID_W; x++) board[x][k] = board[x][k-1];
      for (int x = 0; x < GRID_W; x++) board[x][0] = 0;
      lines++; y--; 
    }
  }

  if (lines > 0) {
    score += (lines * lines) * 100 * level; // Kuadrat skor biar seru
    linesCleared += lines;
    if (linesCleared >= level * 5) { // Level up tiap 5 baris (lebih cepat)
      level++;
      fallSpeed = max(50, fallSpeed - 50);
    }
  }

  spawnNewPiece();
}

// ================================================================
// 5. GRAPHICS
// ================================================================

void drawLayout() {
  // Border
  display.drawRect(BOARD_X-2, BOARD_Y-2, (GRID_W*BLOCK_SIZE)+4, (GRID_H*BLOCK_SIZE)+4, WHITE);
  
  // Dotted Grid Background (Agar lebih mudah lihat lurus)
  for(int y=0; y<GRID_H; y+=2) {
    for(int x=0; x<GRID_W; x+=2) {
       if(board[x][y]==0) display.drawPixel(BOARD_X + x*BLOCK_SIZE + 1, BOARD_Y + y*BLOCK_SIZE + 1, WHITE);
    }
  }

  // Next Box
  display.drawRect(85, 2, 40, 25, WHITE);
  display.setCursor(92, 5); display.print("NEXT");
  
  // Score Box
  display.setCursor(2, 5); display.print("SCR");
  display.setCursor(2, 15); display.print(score);
  display.setCursor(2, 35); display.print("LVL");
  display.setCursor(2, 45); display.print(level);
}

void drawGame() {
  display.clearDisplay();
  drawLayout();

  // Draw Stack
  for(int x=0; x<GRID_W; x++) {
    for(int y=0; y<GRID_H; y++) {
      if(board[x][y]) display.fillRect(BOARD_X+x*BLOCK_SIZE, BOARD_Y+y*BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, WHITE);
    }
  }

  // Draw Ghost
  ghostY = currentY;
  while(!checkCollision(currentX, ghostY+1, currentType, currentRotation)) ghostY++;
  for(int r=0; r<4; r++) for(int c=0; c<4; c++) {
    if(getRotatedBlock(currentType, currentRotation, c, r)) 
       display.drawRect(BOARD_X+(currentX+c)*BLOCK_SIZE, BOARD_Y+(ghostY+r)*BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, WHITE);
  }

  // Draw Current Piece
  for(int r=0; r<4; r++) for(int c=0; c<4; c++) {
    if(getRotatedBlock(currentType, currentRotation, c, r)) 
       display.fillRect(BOARD_X+(currentX+c)*BLOCK_SIZE, BOARD_Y+(currentY+r)*BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, WHITE);
  }

  // Draw Next Piece
  for(int r=0; r<4; r++) for(int c=0; c<4; c++) {
    if(TETROMINOES[nextType][r][c]) 
      display.fillRect(95+(c*3), 14+(r*3), 2, 2, WHITE);
  }

  updateParticles();
  display.display();
}

void wipeScreen() {
  for(int i=0; i<64; i+=2) {
    display.drawFastHLine(0, i, 128, WHITE);
    display.drawFastHLine(0, 63-i, 128, WHITE);
    display.display(); delay(5);
  }
  display.clearDisplay(); display.display();
}

// ================================================================
// 6. LOGIC & CONTROL
// ================================================================

void handleInput() {
  if (millis() - lastInputTime < 60) return; // Debounce
  int x = analogRead(VRX_PIN);
  int y = analogRead(VRY_PIN);

  // KIRI / KANAN (Move)
  if (millis() - lastMoveTime > 90) {
    if (x < 1000) { if(!checkCollision(currentX-1, currentY, currentType, currentRotation)) currentX--; lastMoveTime = millis(); if(isLocking) lockTimer=millis(); }
    if (x > 3000) { if(!checkCollision(currentX+1, currentY, currentType, currentRotation)) currentX++; lastMoveTime = millis(); if(isLocking) lockTimer=millis(); }
  }

  // ATAS (Rotate) - Lebih enak daripada tombol
  if (y < 1000 && (millis() - lastInputTime > 250)) {
     int nextRot = (currentRotation + 1) % 4;
     if(!checkCollision(currentX, currentY, currentType, nextRot)) currentRotation = nextRot;
     else if(!checkCollision(currentX-1, currentY, currentType, nextRot)) { currentX--; currentRotation = nextRot; } // Wallkick
     else if(!checkCollision(currentX+1, currentY, currentType, nextRot)) { currentX++; currentRotation = nextRot; }
     lastInputTime = millis();
  }

  // BAWAH (Soft Drop)
  if (y > 3000) {
     if(!checkCollision(currentX, currentY+1, currentType, currentRotation)) { currentY++; score++; lastFallTime = millis(); }
  }

  // KLIK (Hard Drop) - Instant Lock
  if (digitalRead(SW_PIN) == LOW && !btnPressed) {
    btnPressed = true;
    while(!checkCollision(currentX, currentY+1, currentType, currentRotation)) { currentY++; score+=2; }
    lockPiece();
    lastInputTime = millis();
  } else if (digitalRead(SW_PIN) == HIGH) {
    btnPressed = false;
  }
}

void updateGame() {
  if (millis() - lastFallTime > fallSpeed) {
    if (!checkCollision(currentX, currentY + 1, currentType, currentRotation)) {
      currentY++;
    } else {
      if (!isLocking) { isLocking = true; lockTimer = millis(); }
    }
    lastFallTime = millis();
  }

  if (isLocking && (millis() - lockTimer > lockDelay)) {
    if (checkCollision(currentX, currentY + 1, currentType, currentRotation)) lockPiece();
    else isLocking = false;
  }
}

// ================================================================
// 7. MAIN LOOP
// ================================================================
void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  // BUZZ_PIN tidak di-init untuk hemat daya/silent
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) for(;;);
  
  prefs.begin("tetris", true);
  highScore = prefs.getULong("hs", 0);
  prefs.end();

  randomSeed(analogRead(0));
  display.clearDisplay(); display.display();
}

void loop() {
  switch(currentState) {
    case STATE_MENU:
      display.clearDisplay();
      display.setTextSize(2); display.setTextColor(WHITE);
      display.setCursor(20, 10); display.println("TETRIS");
      display.setTextSize(1); display.setCursor(45, 30); display.println("V2.0");
      
      display.setCursor(10, 50);
      if(menuIndex==0) display.print("[ START ]  UPDATE");
      else display.print("  START  [ UPDATE ]");
      
      display.display();

      int xVal; xVal = analogRead(VRX_PIN);
      if (xVal > 3000) menuIndex = 1; 
      if (xVal < 1000) menuIndex = 0;

      if (digitalRead(SW_PIN) == LOW) {
        delay(200);
        if(menuIndex == 0) {
          wipeScreen(); // Animasi Keren
          resetBoard();
          score = 0; level = 1; linesCleared = 0; fallSpeed = 800;
          shuffleBag(); nextType = getNextPiece(); spawnNewPiece();
          currentState = STATE_PLAY;
        } else {
          currentState = STATE_UPDATE;
          setupUpdate();
        }
      }
      break;

    case STATE_PLAY:
      handleInput();
      updateGame();
      drawGame();
      break;

    case STATE_GAMEOVER:
      display.clearDisplay();
      display.drawRect(10, 10, 108, 44, WHITE);
      display.fillRect(12, 12, 104, 40, BLACK);
      display.setCursor(35, 18); display.println("GAME OVER");
      display.setCursor(20, 32); display.print("Score: "); display.print(score);
      display.setCursor(20, 42); display.print("High : "); display.print(highScore);
      display.display();
      if(digitalRead(SW_PIN)==LOW) { delay(500); currentState = STATE_MENU; }
      break;

    case STATE_UPDATE:
      server.handleClient();
      display.clearDisplay();
      display.setCursor(0,0); display.println("WIFI UPDATE");
      display.drawLine(0,10,128,10,WHITE);
      display.setCursor(0,20); display.println(WiFi.localIP());
      display.setCursor(0,50); display.println("Press Joystick to Exit");
      display.display();
      if(digitalRead(SW_PIN)==LOW) { WiFi.disconnect(true); currentState = STATE_MENU; delay(500); }
      break;
  }
}

void setupUpdate() {
   display.clearDisplay(); display.println("Connecting..."); display.display();
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED) delay(100);
   server.on("/", HTTP_GET, []() { server.send(200, "text/html", updatePage); });
   server.on("/update", HTTP_POST, []() { server.send(200, "text/plain", (Update.hasError())?"FAIL":"OK"); ESP.restart(); }, 
     []() { HTTPUpload& u = server.upload(); 
     if (u.status == UPLOAD_FILE_START) Update.begin(UPDATE_SIZE_UNKNOWN); 
     else if (u.status == UPLOAD_FILE_WRITE) Update.write(u.buf, u.currentSize); 
     else if (u.status == UPLOAD_FILE_END) Update.end(true); 
   });
   server.begin();
}