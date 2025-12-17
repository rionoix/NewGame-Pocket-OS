/*
   PROJECT: ESP32 TETRIS ULTIMATE v3.0
   EDITION: Dual Orientation (Landscape/Portrait)
   HARDWARE: ESP32 + SSD1306 OLED + Joystick
   
   FITUR BARU:
   - Mode Selector (Landscape / Portrait)
   - Auto-Rotate Screen & Inputs
   - Adaptive Block Size (3px di Landscape, 6px di Portrait)
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
// 1. KONFIGURASI HARDWARE
// ================================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define VRX_PIN  34
#define VRY_PIN  35
#define SW_PIN   32
#define BUZZ_PIN 25 // Silent Mode

// WiFi
const char* ssid = "UNAND";
const char* password = "HardiknasDiAndalas";

WebServer server(80);
const char* updatePage = "<h1>ESP32 UPDATE</h1><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
Preferences prefs;

// ================================================================
// 2. SETTING GAME & VARIABEL DINAMIS
// ================================================================
#define GRID_W 10
#define GRID_H 20

// Variabel ini tidak lagi konstan (#define), tapi berubah sesuai mode
int BLOCK_SIZE = 3; 
int BOARD_X = 40;   
int BOARD_Y = 2;    
bool isPortrait = false; 

// Tetromino Definitions
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
enum GameState { STATE_MODE_SELECT, STATE_MENU, STATE_PLAY, STATE_GAMEOVER, STATE_UPDATE };
GameState currentState = STATE_MODE_SELECT;

byte board[GRID_W][GRID_H];
unsigned long score = 0;
unsigned long highScore = 0;
int level = 1;
int linesCleared = 0;

int currentX, currentY, currentType, currentRotation;
int nextType, ghostY;
int bag[7], bagIndex = 0;

unsigned long lastFallTime = 0, lastInputTime = 0, lastMoveTime = 0;
int fallSpeed = 800;
int lockDelay = 500;
unsigned long lockTimer = 0;
bool isLocking = false;
bool btnPressed = false;

struct Particle { float x, y, vx, vy; int life; };
std::vector<Particle> particles;

int menuIndex = 0;

// ================================================================
// 4. CONFIGURATION HELPER
// ================================================================

void setMode(bool portrait) {
  isPortrait = portrait;
  if (portrait) {
    // ROTASI 3: Kiri layar jadi Bawah. Kanan jadi Atas. (Portrait 64x128)
    display.setRotation(3); 
    BLOCK_SIZE = 6; // Balok Besar! (6x20 = 120px height)
    BOARD_X = 2;    // Margin kiri dikit (Total width 64, board 60)
    BOARD_Y = 4;    // Margin atas/bawah dikit
  } else {
    // LANDSCAPE (Default)
    display.setRotation(0);
    BLOCK_SIZE = 3; // Balok Kecil
    BOARD_X = 40;
    BOARD_Y = 2;
  }
}

// ================================================================
// 5. ENGINE LOGIC
// ================================================================

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

bool checkCollision(int x, int y, int type, int rot) {
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (getRotatedBlock(type, rot, c, r)) {
        int testX = x + c;
        int testY = y + r;
        if (testX < 0 || testX >= GRID_W || testY >= GRID_H) return true;
        if (testY >= 0 && board[testX][testY] != 0) return true;
      }
    }
  }
  return false;
}

void spawnNewPiece() {
  currentType = nextType;
  nextType = getNextPiece();
  currentX = 3; currentY = -1; currentRotation = 0; isLocking = false;
  if (checkCollision(currentX, currentY, currentType, currentRotation)) {
    currentState = STATE_GAMEOVER;
    if (score > highScore) {
      highScore = score;
      prefs.begin("tetris", false); prefs.putULong("hs", highScore); prefs.end();
    }
  }
}

void spawnExplosion(int yRow) {
  for (int i = 0; i < 15; i++) {
    Particle p;
    p.x = BOARD_X + (random(0, GRID_W) * BLOCK_SIZE);
    p.y = BOARD_Y + (yRow * BLOCK_SIZE);
    p.vx = random(-20, 20) / 10.0;
    p.vy = random(-10, 10) / 5.0;
    p.life = 15;
    particles.push_back(p);
  }
}

void lockPiece() {
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (getRotatedBlock(currentType, currentRotation, c, r)) {
        if (currentY + r >= 0) board[currentX + c][currentY + r] = currentType;
      }
    }
  }
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
    score += (lines * lines) * 100 * level;
    linesCleared += lines;
    if (linesCleared >= level * 5) { level++; fallSpeed = max(50, fallSpeed - 50); }
  }
  spawnNewPiece();
}

// ================================================================
// 6. GRAPHICS & RENDERING
// ================================================================

void drawLayout() {
  // Border Board
  display.drawRect(BOARD_X-1, BOARD_Y-1, (GRID_W*BLOCK_SIZE)+2, (GRID_H*BLOCK_SIZE)+2, WHITE);
  
  // Dotted Grid
  if (isPortrait) {
    // Di portrait, grid dots mungkin terlalu ramai karena block besar, kita skip atau buat jarang
  } else {
    for(int y=0; y<GRID_H; y+=2) for(int x=0; x<GRID_W; x+=2)
      if(board[x][y]==0) display.drawPixel(BOARD_X + x*BLOCK_SIZE + 1, BOARD_Y + y*BLOCK_SIZE + 1, WHITE);
  }

  // UI Elements
  if (isPortrait) {
    // UI Portrait (Space sempit di samping/bawah, kita tumpuk di atas piece yang belum jatuh atau transparan)
    // Trik: Gambar Score kecil di pojok kiri atas layar (yang sebenarnya "bawah" fisik tapi atas layar)
    display.setTextSize(1);
    display.setCursor(0,0); display.print(score);
    // Next Piece? Kita gambar kecil di pojok kanan atas
    for(int r=0; r<4; r++) for(int c=0; c<4; c++) {
      if(TETROMINOES[nextType][r][c]) 
        display.fillRect(50+(c*2), 0+(r*2), 2, 2, WHITE);
    }
  } else {
    // UI Landscape (Standar)
    display.drawRect(85, 2, 40, 25, WHITE);
    display.setCursor(92, 5); display.print("NEXT");
    display.setCursor(2, 5); display.print("SCR");
    display.setCursor(2, 15); display.print(score);
    display.setCursor(2, 35); display.print("LVL");
    display.setCursor(2, 45); display.print(level);
  }
}

void drawGame() {
  display.clearDisplay();
  drawLayout();

  // Draw Stack
  for(int x=0; x<GRID_W; x++) for(int y=0; y<GRID_H; y++)
    if(board[x][y]) display.fillRect(BOARD_X+x*BLOCK_SIZE, BOARD_Y+y*BLOCK_SIZE, BLOCK_SIZE-1, BLOCK_SIZE-1, WHITE);

  // Draw Ghost
  ghostY = currentY;
  while(!checkCollision(currentX, ghostY+1, currentType, currentRotation)) ghostY++;
  for(int r=0; r<4; r++) for(int c=0; c<4; c++) {
    if(getRotatedBlock(currentType, currentRotation, c, r)) 
       display.drawRect(BOARD_X+(currentX+c)*BLOCK_SIZE, BOARD_Y+(ghostY+r)*BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, WHITE);
  }

  // Draw Active
  for(int r=0; r<4; r++) for(int c=0; c<4; c++) {
    if(getRotatedBlock(currentType, currentRotation, c, r)) 
       display.fillRect(BOARD_X+(currentX+c)*BLOCK_SIZE, BOARD_Y+(currentY+r)*BLOCK_SIZE, BLOCK_SIZE-1, BLOCK_SIZE-1, WHITE);
  }

  // Draw Next (Landscape Only in detailed view)
  if(!isPortrait) {
    for(int r=0; r<4; r++) for(int c=0; c<4; c++) {
      if(TETROMINOES[nextType][r][c]) display.fillRect(95+(c*3), 14+(r*3), 2, 2, WHITE);
    }
  }

  // Particles
  for (int i = particles.size() - 1; i >= 0; i--) {
    particles[i].x += particles[i].vx; particles[i].y += particles[i].vy; particles[i].life--;
    if (particles[i].life > 0) display.drawPixel((int)particles[i].x, (int)particles[i].y, WHITE);
    else particles.erase(particles.begin() + i);
  }
  display.display();
}

// ================================================================
// 7. INPUT HANDLING (ADAPTIVE)
// ================================================================
void handleInput() {
  if (millis() - lastInputTime < 60) return;
  int rawX = analogRead(VRX_PIN);
  int rawY = analogRead(VRY_PIN);

  // MAPPING VARIABLES
  bool moveLeft, moveRight, softDrop, rotate;
  
  if (!isPortrait) {
    // --- LANDSCAPE MAPPING ---
    // VRX: <1000 Left, >3000 Right
    // VRY: >3000 Down (SoftDrop), <1000 Up (Rotate)
    moveLeft  = (rawX < 1000);
    moveRight = (rawX > 3000);
    softDrop  = (rawY > 3000);
    rotate    = (rawY < 1000);
  } else {
    // --- PORTRAIT MAPPING ---
    // Layar diputar 90 derajat CCW (Kiri jadi Bawah).
    // Joystick fisik ikut berputar 90 derajat CCW.
    // Bayangkan Joystick:
    // Fisik AWAL: X(Kiri/Kanan), Y(Atas/Bawah)
    // Setelah diputar:
    // "Atas" Joystick sekarang mengarah ke "Kiri" fisik user.
    // "Kanan" Joystick sekarang mengarah ke "Atas" fisik user.
    
    // Logika Pemetaan Ulang (Tergantung posisi pasangmu, tapi ini logika standar rotasi):
    // Move Left (Screen Left)  -> Joystick arah "Atas" Fisik (VRY < 1000)
    // Move Right (Screen Right)-> Joystick arah "Bawah" Fisik (VRY > 3000)
    // Rotate (Screen Up)       -> Joystick arah "Kanan" Fisik (VRX > 3000)
    // Drop (Screen Down)       -> Joystick arah "Kiri" Fisik (VRX < 1000)
    
    moveLeft  = (rawY < 1000);
    moveRight = (rawY > 3000);
    rotate    = (rawX > 3000);
    softDrop  = (rawX < 1000);
  }

  // Action Logic
  if (millis() - lastMoveTime > 90) {
    if (moveLeft) { if(!checkCollision(currentX-1, currentY, currentType, currentRotation)) currentX--; lastMoveTime = millis(); if(isLocking) lockTimer=millis(); }
    if (moveRight) { if(!checkCollision(currentX+1, currentY, currentType, currentRotation)) currentX++; lastMoveTime = millis(); if(isLocking) lockTimer=millis(); }
  }

  if (rotate && (millis() - lastInputTime > 250)) {
     int nextRot = (currentRotation + 1) % 4;
     if(!checkCollision(currentX, currentY, currentType, nextRot)) currentRotation = nextRot;
     else if(!checkCollision(currentX-1, currentY, currentType, nextRot)) { currentX--; currentRotation = nextRot; } 
     else if(!checkCollision(currentX+1, currentY, currentType, nextRot)) { currentX++; currentRotation = nextRot; }
     lastInputTime = millis();
  }

  if (softDrop) {
     if(!checkCollision(currentX, currentY+1, currentType, currentRotation)) { currentY++; score++; lastFallTime = millis(); }
  }

  // Hard Drop (Click) - Sama untuk kedua mode
  if (digitalRead(SW_PIN) == LOW && !btnPressed) {
    btnPressed = true;
    while(!checkCollision(currentX, currentY+1, currentType, currentRotation)) { currentY++; score+=2; }
    lockPiece();
    lastInputTime = millis();
  } else if (digitalRead(SW_PIN) == HIGH) {
    btnPressed = false;
  }
}

// ================================================================
// 8. SETUP & LOOP UTAMA
// ================================================================

void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) for(;;);
  
  prefs.begin("tetris", true);
  highScore = prefs.getULong("hs", 0);
  prefs.end();

  randomSeed(analogRead(0));
  display.clearDisplay(); display.display();
}

void loop() {
  switch(currentState) {
    
    // MENU PILIH ORIENTASI (Landscape Default agar mudah dibaca)
    case STATE_MODE_SELECT:
      display.setRotation(0); // Pastikan landscape dulu
      display.clearDisplay();
      display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(10, 10); display.println("SELECT ORIENTATION:");
      
      display.setCursor(10, 30);
      if(menuIndex == 0) display.print("> LANDSCAPE"); else display.print("  LANDSCAPE");
      
      display.setCursor(10, 45);
      if(menuIndex == 1) display.print("> PORTRAIT (FULL)"); else display.print("  PORTRAIT (FULL)");
      
      display.display();
      
      // Navigasi Menu Mode
      {
        int y = analogRead(VRY_PIN);
        if(y > 3000) { menuIndex = 1; delay(200); }
        if(y < 1000) { menuIndex = 0; delay(200); }
        if(digitalRead(SW_PIN) == LOW) {
           delay(300);
           setMode(menuIndex == 1); // True jika Portrait
           currentState = STATE_MENU;
           menuIndex = 0; // Reset index untuk menu selanjutnya
        }
      }
      break;

    // MENU UTAMA (Sudah Ter-Rotasi sesuai pilihan)
    case STATE_MENU:
      display.clearDisplay();
      if(isPortrait) {
        display.setTextSize(1);
        display.setCursor(2, 5); display.println(" TETRIS");
        display.setCursor(2, 20); display.println(" PRO V3");
        display.setCursor(2, 50); if(menuIndex==0) display.print(">PLAY"); else display.print(" PLAY");
        display.setCursor(2, 65); if(menuIndex==1) display.print(">UPD"); else display.print(" UPD");
      } else {
        display.setTextSize(2); display.setCursor(20, 10); display.println("TETRIS");
        display.setTextSize(1); display.setCursor(45, 30); display.println("V3.0");
        display.setCursor(10, 50); if(menuIndex==0) display.print("[ START ]  UPDATE"); else display.print("  START  [ UPDATE ]");
      }
      display.display();

      // Input Menu (Adaptive)
      handleMenuInput();
      break;

    case STATE_PLAY:
      handleInput();
      if (millis() - lastFallTime > fallSpeed) {
        if (!checkCollision(currentX, currentY + 1, currentType, currentRotation)) currentY++;
        else { if (!isLocking) { isLocking = true; lockTimer = millis(); } }
        lastFallTime = millis();
      }
      if (isLocking && (millis() - lockTimer > lockDelay)) {
        if (checkCollision(currentX, currentY + 1, currentType, currentRotation)) lockPiece();
        else isLocking = false;
      }
      drawGame();
      break;

    case STATE_GAMEOVER:
      display.clearDisplay();
      if (isPortrait) {
        display.setCursor(5,20); display.println("GAME");
        display.setCursor(5,30); display.println("OVER");
        display.setCursor(5,50); display.print(score);
      } else {
        display.setCursor(35, 20); display.println("GAME OVER");
        display.setCursor(35, 35); display.print("Score: "); display.print(score);
      }
      display.display();
      if(digitalRead(SW_PIN)==LOW) { delay(500); currentState = STATE_MODE_SELECT; }
      break;

    case STATE_UPDATE:
      server.handleClient();
      display.clearDisplay(); display.setCursor(0,0); display.println("UPDATING..."); display.println(WiFi.localIP()); display.display();
      if(digitalRead(SW_PIN)==LOW) { WiFi.disconnect(true); currentState = STATE_MENU; delay(500); }
      break;
  }
}

void handleMenuInput() {
  int x = analogRead(VRX_PIN);
  int y = analogRead(VRY_PIN);
  bool next = false, prev = false;

  if(!isPortrait) { // Landscape Menu Control
     if(x > 3000) next = true; if(x < 1000) prev = true;
  } else { // Portrait Menu Control (Up/Down visual is Left/Right physical)
     if(y > 3000) next = true; if(y < 1000) prev = true;
  }

  if(next) { menuIndex = 1; delay(200); }
  if(prev) { menuIndex = 0; delay(200); }

  if(digitalRead(SW_PIN) == LOW) {
    delay(200);
    if(menuIndex == 0) {
      resetBoard(); score = 0; level = 1; linesCleared = 0; fallSpeed = 800;
      shuffleBag(); nextType = getNextPiece(); spawnNewPiece();
      currentState = STATE_PLAY;
    } else {
      currentState = STATE_UPDATE;
      WiFi.begin(ssid, password);
      server.begin(); // Setup server singkat (copy paste logic setupUpdate sebelumnya jika perlu full)
      setupUpdate();
    }
  }
}

void setupUpdate() {
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