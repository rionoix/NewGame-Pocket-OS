#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- KONFIGURASI HARDWARE ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define VRX_PIN  34
#define VRY_PIN  35
#define SW_PIN   32
#define BUZZ_PIN 25 

// --- STATE MANAGEMENT ---
enum SystemState { 
  STATE_SLEEP, STATE_EYES, STATE_BOOT, STATE_MENU, 
  GAME_SNAKE, GAME_PONG, GAME_TETRIS, GAME_DOOM, 
  STATE_GAMEOVER, STATE_INFO 
};
SystemState currentState = STATE_SLEEP;

// --- VARIABEL GLOBAL ---
int score = 0;
int highScore = 0; // Sementara di RAM
int gameID = 0; // 1:Snake, 2:Pong, 3:Tetris, 4:Doom

// Menu
int menuIndex = 0;
const int menuTotal = 6;
String menuItems[] = {"1. Snake", "2. Pong", "3. Tetris", "4. Doom (FPS)", "5. Info", "6. Sleep"};

// Robot Eyes
unsigned long lastBlink = 0;
int blinkInterval = 3000;
bool isBlinking = false;

// --- VARIABEL GAME PONG ---
int paddleY = 22; int paddleH = 16;
int ballX = 64, ballY = 32;
int ballDX = 2, ballDY = 2;
int cpuY = 22;

// --- VARIABEL GAME DOOM (FPS) ---
int enemyX = 64, enemyY = 32;
int enemySize = 2; // Musuh membesar (mendekat)
bool enemyAlive = false;
int crosshairX = 64, crosshairY = 32;
int ammo = 10;

// --- VARIABEL GAME TETRIS ---
int grid[10][20] = {0}; // 10 lebar, 20 tinggi
int currentPiece[4][2]; // Koordinat 4 blok
int pieceType, rotateIdx;
int px, py; 
unsigned long lastDrop = 0;

// --- FUNGSI AUDIO (RHYTHM) ---
void playTone(int count, int speed) {
  for(int i=0; i<count; i++) {
    digitalWrite(BUZZ_PIN, HIGH);
    delay(speed);
    digitalWrite(BUZZ_PIN, LOW);
    delay(speed);
  }
}

void playIntroMusic() {
  // Irama Loading: Ti-ti-ti... TEEET
  playTone(3, 50); delay(100); playTone(1, 400);
}

void playGameStartMusic() {
  // Irama Start: Tet-tet-tet-tet!
  playTone(4, 80);
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(BUZZ_PIN, OUTPUT);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  currentState = STATE_SLEEP;
}

// --- LOOP UTAMA ---
void loop() {
  switch (currentState) {
    case STATE_SLEEP:  runSleep(); break;
    case STATE_EYES:   runEyes(); break;
    case STATE_BOOT:   runBoot(); break;
    case STATE_MENU:   runMenu(); break;
    case GAME_SNAKE:   runSnake(); break;
    case GAME_PONG:    runPong(); break;
    case GAME_TETRIS:  runTetris(); break;
    case GAME_DOOM:    runDoom(); break;
    case STATE_GAMEOVER: runGameOver(); break;
    case STATE_INFO:   runInfo(); break;
  }
}

// ==========================================
// BAGIAN 1: EYES & MENU
// ==========================================

void runSleep() {
  display.clearDisplay();
  // Mata Tidur: Garis lurus (- -)
  display.fillRect(30, 32, 25, 2, WHITE);
  display.fillRect(73, 32, 25, 2, WHITE);
  display.display();

  if (digitalRead(SW_PIN) == LOW) {
    playTone(2, 50); // Suara bangun
    currentState = STATE_EYES;
    lastBlink = millis();
    delay(500);
  }
}

void runEyes() {
  display.clearDisplay();
  
  // Logika Kedip
  if (millis() - lastBlink > blinkInterval) {
    isBlinking = true;
    display.fillRect(30, 32, 25, 2, WHITE); // Mata tutup
    display.fillRect(73, 32, 25, 2, WHITE);
    display.display();
    delay(150); // Durasi kedip
    isBlinking = false;
    lastBlink = millis();
    blinkInterval = random(2000, 5000);
    return; // Skip drawing frame ini
  }

  // Baca Joystick
  int xVal = analogRead(VRX_PIN);
  int yVal = analogRead(VRY_PIN);
  
  // Mapping gerakan (Mata bergeser full body)
  // Pusat mata default: (42, 32) dan (85, 32)
  int offsetX = map(xVal, 0, 4095, -10, 10);
  int offsetY = map(yVal, 0, 4095, -8, 8);

  // Gambar Mata Persegi Panjang Melengkung (Rounded Rect)
  // fillRoundRect(x, y, w, h, radius, color)
  display.fillRoundRect(30 + offsetX, 24 + offsetY, 25, 16, 4, WHITE);
  display.fillRoundRect(73 + offsetX, 24 + offsetY, 25, 16, 4, WHITE);

  display.display();

  if (digitalRead(SW_PIN) == LOW) {
    currentState = STATE_BOOT;
    delay(500);
  }
}

void runBoot() {
  playIntroMusic(); // Musik Loading
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  display.setCursor(15, 10); display.println("NewGame Pocket OS");
  display.setTextSize(2); 
  display.setCursor(40, 25); display.println("V0.3");
  display.setTextSize(1);
  display.setCursor(42, 45); display.println("by rion");
  display.setCursor(35, 55); display.println("loading...");
  display.display();
  delay(2000);
  currentState = STATE_MENU;
}

void runMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(35, 0); display.println("MAIN MENU");
  display.drawLine(0, 8, 128, 8, WHITE);

  for (int i = 0; i < menuTotal; i++) {
    int yPos = 12 + (i * 9); // Jarak rapat
    if (i == menuIndex) display.print(">"); else display.print(" ");
    display.setCursor(10, yPos);
    display.println(menuItems[i]);
  }
  display.display();

  int yVal = analogRead(VRY_PIN);
  if (yVal > 3000) { menuIndex++; if(menuIndex >= menuTotal) menuIndex=0; delay(150); }
  if (yVal < 1000) { menuIndex--; if(menuIndex < 0) menuIndex=menuTotal-1; delay(150); }

  if (digitalRead(SW_PIN) == LOW) {
    delay(300);
    if (menuIndex == 5) { currentState = STATE_SLEEP; return; } // Exit
    if (menuIndex == 4) { currentState = STATE_INFO; return; }  // Info
    
    // Masuk Game
    playGameStartMusic();
    score = 0; 
    
    if (menuIndex == 0) { gameID=1; currentState = GAME_SNAKE; setupSnake(); }
    if (menuIndex == 1) { gameID=2; currentState = GAME_PONG; setupPong(); }
    if (menuIndex == 2) { gameID=3; currentState = GAME_TETRIS; setupTetris(); }
    if (menuIndex == 3) { gameID=4; currentState = GAME_DOOM; setupDoom(); }
  }
}

// ==========================================
// BAGIAN 2: GAME ENGINES
// ==========================================

// --- SNAKE ---
int sx[100], sy[100], slen, fX, fY, sDx, sDy;
void setupSnake() { slen=3; sx[0]=5; sy[0]=5; sDx=1; sDy=0; fX=random(0,32); fY=random(0,16); }
void runSnake() {
  int x = analogRead(VRX_PIN); int y = analogRead(VRY_PIN);
  if(x>3000 && sDx==0){sDx=1; sDy=0;} if(x<1000 && sDx==0){sDx=-1; sDy=0;}
  if(y>3000 && sDy==0){sDx=0; sDy=1;} if(y<1000 && sDy==0){sDx=0; sDy=-1;}
  
  for(int i=slen-1; i>0; i--){sx[i]=sx[i-1]; sy[i]=sy[i-1];}
  sx[0]+=sDx; sy[0]+=sDy;
  
  if(sx[0]>31) sx[0]=0; if(sx[0]<0) sx[0]=31;
  if(sy[0]>15) sy[0]=0; if(sy[0]<0) sy[0]=15;
  
  for(int i=1; i<slen; i++) if(sx[0]==sx[i] && sy[0]==sy[i]) currentState=STATE_GAMEOVER;
  
  if(sx[0]==fX && sy[0]==fY){score+=10; slen++; fX=random(0,32); fY=random(0,16); playTone(1,30);}
  
  display.clearDisplay();
  display.fillRect(fX*4, fY*4, 4, 4, WHITE);
  for(int i=0; i<slen; i++) display.fillRect(sx[i]*4, sy[i]*4, 4, 4, WHITE);
  display.display(); delay(100);
}

// --- PONG ---
void setupPong() { ballX=64; ballY=32; ballDX=3; ballDY=3; }
void runPong() {
  // Player
  int yVal = analogRead(VRY_PIN);
  if(yVal>3000 && paddleY < 48) paddleY+=4;
  if(yVal<1000 && paddleY > 0) paddleY-=4;
  
  // Ball logic
  ballX += ballDX; ballY += ballDY;
  
  // Wall collision
  if(ballY <= 0 || ballY >= 60) ballDY *= -1;
  
  // CPU Logic (Simple AI)
  if(ballY > cpuY + 8) cpuY += 2; else cpuY -= 2;
  
  // Paddle Collision
  if(ballX <= 4 && ballY >= paddleY && ballY <= paddleY+paddleH) {
    ballDX *= -1; score++; playTone(1,20);
  }
  if(ballX >= 120 && ballY >= cpuY && ballY <= cpuY+paddleH) {
    ballDX *= -1;
  }
  
  // Die
  if(ballX < 0 || ballX > 128) currentState = STATE_GAMEOVER;
  
  display.clearDisplay();
  display.fillRect(0, paddleY, 4, paddleH, WHITE); // Player
  display.fillRect(124, cpuY, 4, paddleH, WHITE);  // CPU
  display.fillCircle(ballX, ballY, 2, WHITE);      // Ball
  display.setCursor(60,0); display.print(score);
  display.display();
}

// --- DOOM (Simple FPS) ---
void setupDoom() { enemyAlive=false; ammo=10; crosshairX=64; crosshairY=32; }
void runDoom() {
  int x = analogRead(VRX_PIN);
  int y = analogRead(VRY_PIN);
  
  // Gerakkan Crosshair
  if(x<1000) crosshairX-=3; if(x>3000) crosshairX+=3;
  if(y<1000) crosshairY-=3; if(y>3000) crosshairY+=3;
  
  // Spawn Enemy
  if(!enemyAlive && random(0,50)==0) {
    enemyAlive = true; enemySize = 2;
    enemyX = random(20, 100); enemyY = 32;
  }
  
  // Enemy mendekat
  if(enemyAlive) {
    enemySize++;
    if(enemySize > 40) currentState = STATE_GAMEOVER; // Mati digigit
  }
  
  // Tembak
  if(digitalRead(SW_PIN)==LOW) {
    playTone(1, 10); // Suara dor
    display.invertDisplay(true); delay(20); display.invertDisplay(false); // Efek kilat
    
    // Cek Hit
    if(enemyAlive && abs(crosshairX - enemyX) < (enemySize/2) + 5) {
      score += 100; enemyAlive = false; // Mati
    }
    delay(200);
  }
  
  display.clearDisplay();
  
  // Gambar Lantai 3D sederhana
  display.drawLine(0, 32, 128, 32, WHITE); // Horizon
  display.drawLine(64, 32, 0, 64, WHITE);
  display.drawLine(64, 32, 128, 64, WHITE);
  
  // Gambar Musuh (Kotak makin besar)
  if(enemyAlive) {
    display.fillRect(enemyX - (enemySize/2), 32 - (enemySize/2), enemySize, enemySize, WHITE);
    display.setCursor(enemyX - 5, 32 - (enemySize/2) - 10); display.print("!");
  }
  
  // Gambar Crosshair
  display.drawCircle(crosshairX, crosshairY, 5, WHITE);
  display.drawLine(crosshairX, crosshairY-8, crosshairX, crosshairY+8, WHITE);
  display.drawLine(crosshairX-8, crosshairY, crosshairX+8, crosshairY, WHITE);
  
  // Gambar Senjata
  display.fillTriangle(50, 64, 78, 64, 64, 50, WHITE);
  
  display.setCursor(0,0); display.print("Score: "); display.print(score);
  display.display();
}

// --- TETRIS (Simplified / Stacker) ---
// Karena keterbatasan baris, ini versi Stacker: Balok jatuh lurus, tumpuk setinggi mungkin
int stack[16]; // Kolom 0-15
int blockX = 0, blockY = 0;
void setupTetris() { 
  for(int i=0; i<16; i++) stack[i]=64; // Reset stack (64 = lantai)
  blockX = 8; blockY = 0;
}
void runTetris() {
  // Move
  int x = analogRead(VRX_PIN);
  if(x<1000 && blockX > 0) {blockX--; delay(100);}
  if(x>3000 && blockX < 15) {blockX++; delay(100);}
  
  // Gravity
  blockY += 2;
  
  // Hit Floor / Stack
  if(blockY >= stack[blockX] - 4) {
    stack[blockX] -= 4; // Tumpukan naik
    score += 10;
    playTone(1, 10);
    blockY = 0; blockX = random(0, 16); // Spawn baru
    
    if(stack[blockX] <= 0) currentState = STATE_GAMEOVER; // Penuh
  }
  
  display.clearDisplay();
  // Gambar Stack
  for(int i=0; i<16; i++) {
    display.fillRect(i*8, stack[i], 7, 64-stack[i], WHITE);
  }
  // Gambar Blok Jatuh
  display.fillRect(blockX*8, blockY, 7, 4, WHITE);
  
  display.setCursor(0,0); display.print(score);
  display.display();
}


// --- UMUM ---
void runGameOver() {
  display.clearDisplay();
  display.setTextSize(2); display.setCursor(10,20); display.println("GAME OVER");
  display.setTextSize(1); display.setCursor(40,40); display.print("Score: "); display.print(score);
  display.display();
  if(digitalRead(SW_PIN)==LOW) { delay(500); currentState = STATE_MENU; }
}

void runInfo() {
  display.clearDisplay();
  display.setCursor(0,0); display.println("ABOUT SYSTEM");
  display.println("--------------");
  display.println("NewGame Pocket OS");
  display.println("Ver: 0.3");
  display.println("Dev: Rion");
  display.println("\nClick to Back");
  display.display();
  if(digitalRead(SW_PIN)==LOW) { delay(500); currentState = STATE_MENU; }
}