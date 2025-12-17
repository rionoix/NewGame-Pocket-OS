#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

// --- KONFIGURASI WIFI (WAJIB DIISI UTK UPDATE) ---
const char* ssid = "UNAND";     // Ganti dengan nama WiFi
const char* password = "HardiknasDiAndalas"; // Ganti dengan password WiFi

// --- KONFIGURASI HARDWARE ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define VRX_PIN  34
#define VRY_PIN  35
#define SW_PIN   32
#define BUZZ_PIN 25 

// --- Web Server untuk Update ---
WebServer server(80);

// --- STATE MANAGEMENT ---
enum SystemState { 
  STATE_SLEEP, STATE_EYES, STATE_BOOT, STATE_MENU, 
  GAME_SNAKE, GAME_PONG, GAME_TETRIS, GAME_DOOM, DEMO_BAD_APPLE,
  STATE_UPDATE_OS, // Mode Update Baru
  STATE_GAMEOVER, STATE_INFO 
};
SystemState currentState = STATE_SLEEP;

// --- VARIABEL MATA (Sesuai Referensi V0.4) ---
int eyeW = 24;  
int eyeH = 48;  
int eyeGap = 34; 
int cX = SCREEN_WIDTH / 2;
int cY = SCREEN_HEIGHT / 2;
int cXL = cX - eyeGap/2;
int cXR = cX + eyeGap/2;
unsigned long lastBlink = 0;
int blinkInterval = 3000;
bool isBlinking = false;

// --- VARIABEL GAME ---
int score = 0;
// Menu
int menuIndex = 0;
const int menuTotal = 7;
String menuItems[] = {"1. Snake", "2. Pong", "3. Tetris", "4. Doom", "5. Bad Apple", "6. UPDATE OS", "7. Sleep"};

// Pong Var
int paddleY = 22; int paddleH = 16;
int ballX = 64, ballY = 32;
int ballDX = 1, ballDY = 1; // Diperlambat (V0.4)
int cpuY = 22;

// --- HTML UNTUK HALAMAN UPDATE ---
const char* loginIndex = 
"<form method='POST' action='/update' enctype='multipart/form-data'>"
"<input type='file' name='update'><input type='submit' value='Update OS'></form>";

// --- FUNGSI BANTUAN ---
void beep(int duration) {
  digitalWrite(BUZZ_PIN, HIGH); delay(duration); digitalWrite(BUZZ_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(BUZZ_PIN, OUTPUT);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  
  currentState = STATE_SLEEP;
}

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
    case DEMO_BAD_APPLE: runBadApple(); break;
    case STATE_UPDATE_OS: runUpdateOS(); break;
    case STATE_GAMEOVER: runGameOver(); break;
    case STATE_INFO:   runInfo(); break;
  }
}

// ==========================================
// 1. MATA & SYSTEM (V0.4 Shape)
// ==========================================

void runSleep() {
  display.clearDisplay();
  // Mata Tidur (- -) diposisikan sesuai pusat mata baru
  display.fillRect(cXL - 12, cY, 24, 2, WHITE);
  display.fillRect(cXR - 12, cY, 24, 2, WHITE);
  display.display();

  if (digitalRead(SW_PIN) == LOW) {
    beep(50); delay(100); beep(50);
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
    // Gambar Mata Tidur saat kedip
    display.fillRect(cXL - 12, cY, 24, 2, WHITE);
    display.fillRect(cXR - 12, cY, 24, 2, WHITE);
    display.display();
    delay(150); 
    isBlinking = false;
    lastBlink = millis();
    blinkInterval = random(2000, 5000);
    return; 
  }

  // Joystick Input
  int xVal = analogRead(VRX_PIN);
  int yVal = analogRead(VRY_PIN);
  
  // Offset (Lirik) - Batas pergerakan disesuaikan agar tidak keluar layar
  int offX = map(xVal, 0, 4095, -10, 10);
  int offY = map(yVal, 0, 4095, -5, 5); // Y sedikit saja karena mata tinggi

  // Gambar Mata V0.4 (Tinggi & Rounded)
  display.fillRoundRect(cXL - eyeW/2 + offX, cY - eyeH/2 + offY, eyeW, eyeH, 8, WHITE);
  display.fillRoundRect(cXR - eyeW/2 + offX, cY - eyeH/2 + offY, eyeW, eyeH, 8, WHITE);

  display.display();

  if (digitalRead(SW_PIN) == LOW) {
    currentState = STATE_BOOT;
    delay(500);
  }
}

void runBoot() {
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(15, 10); display.println("NewGame Pocket OS");
  display.setTextSize(2); display.setCursor(40, 25); display.println("V0.4");
  display.setTextSize(1); display.setCursor(42, 45); display.println("by rion");
  display.display();
  
  // Nada Boot
  beep(100); delay(50); beep(100); delay(50); beep(200);
  delay(1500);
  currentState = STATE_MENU;
}

// ==========================================
// 2. MENU & UPDATE SYSTEM
// ==========================================

void runMenu() {
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(35, 0); display.println("MAIN MENU");
  display.drawLine(0, 8, 128, 8, WHITE);

  for (int i = 0; i < menuTotal; i++) {
    int yPos = 12 + (i * 8); // Lebih rapat lagi
    if (i == menuIndex) display.print(">"); else display.print(" ");
    display.setCursor(10, yPos); display.println(menuItems[i]);
  }
  display.display();

  int yVal = analogRead(VRY_PIN);
  if (yVal > 3000) { menuIndex++; if(menuIndex >= menuTotal) menuIndex=0; beep(10); delay(150); }
  if (yVal < 1000) { menuIndex--; if(menuIndex < 0) menuIndex=menuTotal-1; beep(10); delay(150); }

  if (digitalRead(SW_PIN) == LOW) {
    delay(300);
    if (menuIndex == 6) { currentState = STATE_SLEEP; return; }
    if (menuIndex == 5) { currentState = STATE_UPDATE_OS; setupUpdate(); return; }
    
    score = 0;
    if (menuIndex == 0) { currentState = GAME_SNAKE; setupSnake(); }
    if (menuIndex == 1) { currentState = GAME_PONG; setupPong(); }
    if (menuIndex == 2) { currentState = GAME_TETRIS; setupTetris(); }
    if (menuIndex == 3) { currentState = GAME_DOOM; setupDoom(); }
    if (menuIndex == 4) { currentState = DEMO_BAD_APPLE; }
  }
}

// --- LOGIKA UPDATE OS (WIFI) ---
void setupUpdate() {
  display.clearDisplay();
  display.setCursor(0,0); display.println("Connecting WiFi...");
  display.display();
  
  WiFi.begin(ssid, password);
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500); display.print("."); display.display();
    timeout++;
  }
  
  if(WiFi.status() != WL_CONNECTED) {
    display.clearDisplay(); display.println("WiFi Failed!"); display.display(); delay(2000); currentState = STATE_MENU; return;
  }

  // Setup Server
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK REBOOTING...");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Serial); }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) { Update.printError(Serial); }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { Serial.println("Success"); } else { Update.printError(Serial); }
    }
  });
  
  server.begin();
}

void runUpdateOS() {
  server.handleClient();
  
  display.clearDisplay();
  display.setCursor(0,0); display.println("MODE: UPDATE OS");
  display.drawLine(0,10,128,10,WHITE);
  display.setCursor(0,20); display.println("Connect HP/PC to:");
  display.print("WiFi: "); display.println(ssid);
  display.setCursor(0,45); display.println("Buka Browser IP:");
  display.println(WiFi.localIP());
  display.display();
  
  if (digitalRead(SW_PIN) == LOW) { // Batal
    WiFi.disconnect(true);
    currentState = STATE_MENU;
  }
}

// ==========================================
// 3. GAME & DEMO
// ==========================================

// --- PONG (SLOWED DOWN V0.4) ---
void setupPong() { ballX=64; ballY=32; ballDX=1; ballDY=1; } // Speed jadi 1
void runPong() {
  int yVal = analogRead(VRY_PIN);
  if(yVal>3000 && paddleY < 48) paddleY+=2; // Paddle juga diperlambat sedikit biar seimbang
  if(yVal<1000 && paddleY > 0) paddleY-=2;
  
  ballX += ballDX; ballY += ballDY;
  if(ballY <= 0 || ballY >= 60) ballDY *= -1;
  
  // AI CPU
  if(ballX > 64) { if(ballY > cpuY + 8) cpuY += 1; else cpuY -= 1; } // CPU lebih santai
  
  if(ballX <= 4 && ballY >= paddleY && ballY <= paddleY+paddleH) { ballDX *= -1; score++; beep(20); }
  if(ballX >= 120 && ballY >= cpuY && ballY <= cpuY+paddleH) { ballDX *= -1; }
  if(ballX < 0 || ballX > 128) currentState = STATE_GAMEOVER;
  
  display.clearDisplay();
  display.fillRect(0, paddleY, 4, paddleH, WHITE);
  display.fillRect(124, cpuY, 4, paddleH, WHITE);
  display.fillCircle(ballX, ballY, 2, WHITE);
  display.setCursor(60,0); display.print(score);
  display.display();
  delay(20); // Tambahan delay frame agar tidak terlalu ngebut
}

// --- BAD APPLE (PROCEDURAL ANIMATION V0.4) ---
void runBadApple() {
  // Ini simulasi Bad Apple menggunakan grafik sederhana + musik
  // Part 1: Intro (Jatuh)
  for(int y=0; y<64; y+=2) {
    display.clearDisplay();
    display.fillCircle(64, y, 10, WHITE); // Apel Putih
    display.display();
    if(y%8==0) beep(30); // Nada turun
    delay(30);
  }
  
  // Part 2: Flash
  for(int i=0; i<4; i++) {
    display.invertDisplay(true); beep(50); delay(100);
    display.invertDisplay(false); delay(100);
  }
  
  // Part 3: Silhouette Dance (Random Shapes)
  long startDemo = millis();
  while(millis() - startDemo < 15000) { // Loop 15 detik
    if(digitalRead(SW_PIN)==LOW) { currentState=STATE_MENU; return; }
    
    display.clearDisplay();
    // Gambar Siluet Acak (Meniru transisi bad apple)
    int shape = (millis() / 500) % 3;
    
    if(shape == 0) { // Mode Miku (Kuncir)
       display.fillCircle(40, 32, 15, WHITE);
       display.fillCircle(88, 32, 15, WHITE);
       display.fillRect(50, 20, 28, 40, WHITE);
    } 
    else if(shape == 1) { // Mode Apel
       display.fillCircle(64, 32, 20, WHITE);
       display.fillTriangle(64, 10, 70, 20, 64, 20, WHITE);
    }
    else { // Mode Spiral
       for(int r=0; r<64; r+=4) display.drawCircle(64, 32, r, WHITE);
    }
    
    display.display();
    
    // Irama Bad Apple Sederhana (Tu-tu-tu-tu)
    if(millis() % 250 < 50) beep(20); 
  }
  currentState = STATE_MENU;
}

// --- OTHER GAMES (SNAKE, TETRIS, DOOM) ---
// (Kode sama seperti V0.3, diringkas untuk menghemat tempat, tetap dicopy)
int sx[100], sy[100], slen, fX, fY, sDx, sDy;
void setupSnake() { slen=3; sx[0]=5; sy[0]=5; sDx=1; sDy=0; fX=random(0,32); fY=random(0,16); }
void runSnake() {
  int x = analogRead(VRX_PIN); int y = analogRead(VRY_PIN);
  if(x>3000 && sDx==0){sDx=1; sDy=0;} if(x<1000 && sDx==0){sDx=-1; sDy=0;}
  if(y>3000 && sDy==0){sDx=0; sDy=1;} if(y<1000 && sDy==0){sDx=0; sDy=-1;}
  for(int i=slen-1; i>0; i--){sx[i]=sx[i-1]; sy[i]=sy[i-1];} sx[0]+=sDx; sy[0]+=sDy;
  if(sx[0]>31) sx[0]=0; if(sx[0]<0) sx[0]=31; if(sy[0]>15) sy[0]=0; if(sy[0]<0) sy[0]=15;
  for(int i=1; i<slen; i++) if(sx[0]==sx[i] && sy[0]==sy[i]) currentState=STATE_GAMEOVER;
  if(sx[0]==fX && sy[0]==fY){score+=10; slen++; fX=random(0,32); fY=random(0,16); beep(30);}
  display.clearDisplay(); display.fillRect(fX*4, fY*4, 4, 4, WHITE);
  for(int i=0; i<slen; i++) display.fillRect(sx[i]*4, sy[i]*4, 4, 4, WHITE);
  display.display(); delay(80);
}

int stack[16], blockX, blockY;
void setupTetris() { for(int i=0; i<16; i++) stack[i]=64; blockX=8; blockY=0; }
void runTetris() {
  int x = analogRead(VRX_PIN); if(x<1000 && blockX > 0) {blockX--; delay(100);} if(x>3000 && blockX < 15) {blockX++; delay(100);}
  blockY += 2;
  if(blockY >= stack[blockX] - 4) {
    stack[blockX] -= 4; score += 10; beep(10); blockY = 0; blockX = random(0, 16);
    if(stack[blockX] <= 0) currentState = STATE_GAMEOVER;
  }
  display.clearDisplay();
  for(int i=0; i<16; i++) display.fillRect(i*8, stack[i], 7, 64-stack[i], WHITE);
  display.fillRect(blockX*8, blockY, 7, 4, WHITE);
  display.display();
}

int enemyX, enemyY, enemySize; bool enemyAlive; int crosshairX, crosshairY;
void setupDoom() { enemyAlive=false; crosshairX=64; crosshairY=32; }
void runDoom() {
  int x = analogRead(VRX_PIN); int y = analogRead(VRY_PIN);
  if(x<1000) crosshairX-=3; if(x>3000) crosshairX+=3; if(y<1000) crosshairY-=3; if(y>3000) crosshairY+=3;
  if(!enemyAlive && random(0,50)==0) { enemyAlive = true; enemySize = 2; enemyX = random(20, 100); enemyY = 32; }
  if(enemyAlive) { enemySize++; if(enemySize > 40) currentState = STATE_GAMEOVER; }
  if(digitalRead(SW_PIN)==LOW) {
    beep(10); display.invertDisplay(true); delay(20); display.invertDisplay(false);
    if(enemyAlive && abs(crosshairX - enemyX) < (enemySize/2) + 5) { score += 100; enemyAlive = false; } delay(200);
  }
  display.clearDisplay();
  display.drawLine(0, 32, 128, 32, WHITE); display.drawLine(64, 32, 0, 64, WHITE); display.drawLine(64, 32, 128, 64, WHITE);
  if(enemyAlive) display.fillRect(enemyX - (enemySize/2), 32 - (enemySize/2), enemySize, enemySize, WHITE);
  display.drawCircle(crosshairX, crosshairY, 5, WHITE);
  display.setCursor(0,0); display.print(score); display.display();
}

void runGameOver() {
  display.clearDisplay(); display.setCursor(10,20); display.println("GAME OVER");
  display.setCursor(40,40); display.print("Score: "); display.print(score); display.display();
  if(digitalRead(SW_PIN)==LOW) { delay(500); currentState = STATE_MENU; }
}

void runInfo() {
  display.clearDisplay(); display.setCursor(0,0); display.println("INFO V0.4");
  display.println("Web Update Active"); display.println("Bad Apple Demo");
  display.display(); if(digitalRead(SW_PIN)==LOW) { delay(500); currentState = STATE_MENU; }
}