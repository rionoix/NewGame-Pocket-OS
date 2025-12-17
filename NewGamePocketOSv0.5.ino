/*
   PROJECT: NewGame Pocket OS
   VERSION: V0.5 (Ultimate Edition)
   DEV: Rion
   NOTES: 
   - Doom Raycasting Engine
   - Smart Snake (Safe Spawn)
   - Dynamic Pong Difficulty
   - Wide Range Eye Movement
   - Fixed OTA Update Debounce
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

// ================================================================
// 1. KONFIGURASI WIFI & HARDWARE
// ================================================================

// --- WIFI CREDENTIALS (GANTI INI) ---
const char* ssid = "UNAND";     
const char* password = "HardiknasDiAndalas"; 

// --- DISPLAY CONFIG ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- PIN DEFINITIONS ---
#define VRX_PIN  34
#define VRY_PIN  35
#define SW_PIN   32
#define BUZZ_PIN 25 

// --- WEB SERVER OBJEK ---
WebServer server(80);

// ================================================================
// 2. STATE MANAGEMENT (STATUS SISTEM)
// ================================================================

enum SystemState { 
  STATE_SLEEP,      // Mata Tidur (- -)
  STATE_EYES,       // Mata Robot (0 0)
  STATE_BOOT,       // Loading Screen
  STATE_MENU,       // Menu Utama
  GAME_SNAKE,       // Game Ular
  GAME_PONG,        // Game Ping Pong
  GAME_DOOM,        // Game FPS Raycasting (V0.5)
  STATE_UPDATE_OS,  // Mode OTA Update
  STATE_GAMEOVER,   // Layar Kalah
  STATE_INFO        // Layar Info
};

SystemState currentState = STATE_SLEEP;

// ================================================================
// 3. VARIABEL GLOBAL (GLOBAL VARIABLES)
// ================================================================

// --- Variabel Score Umum ---
int score = 0;
int highScore = 0;

// --- Variabel Menu ---
int menuIndex = 0;
const int menuTotal = 6; // Tetris & Bad Apple Dihapus
String menuItems[] = {
  "1. Snake", 
  "2. Pong (Level)", 
  "3. Doom 3D", 
  "4. UPDATE OS", 
  "5. Info System",
  "6. Sleep Mode"
};

// --- Variabel Mata Robot (Wide Range) ---
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

// --- HTML untuk Web Update (Dark Mode Style) ---
const char* loginIndex = 
"<style>"
"body{font-family:sans-serif; background:#121212; color:#e0e0e0; text-align:center; padding:50px;}"
"h1{color:#03dac6;} .card{background:#1e1e1e; padding:20px; border-radius:10px; display:inline-block;}"
"input{margin:10px; padding:10px; border-radius:5px; border:1px solid #333;}"
".btn{background:#bb86fc; color:black; font-weight:bold; border:none; cursor:pointer; padding:10px 20px;}"
"</style>"
"<div class='card'>"
"<h1>NewGame Pocket OS V0.5</h1>"
"<h2>System Update Center</h2>"
"<form method='POST' action='/update' enctype='multipart/form-data'>"
"<input type='file' name='update'><br>"
"<input class='btn' type='submit' value='UPLOAD FIRMWARE'>"
"</form></div>";

// ================================================================
// 4. SETUP (INISIALISASI)
// ================================================================
void setup() {
  Serial.begin(115200);

  // Setup Pin
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(BUZZ_PIN, OUTPUT);
  digitalWrite(BUZZ_PIN, LOW);

  // Setup OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) { 
    Serial.println(F("OLED Gagal"));
    for(;;);
  }
  
  currentState = STATE_SLEEP; // Mulai dari mode tidur
}

// ================================================================
// 5. LOOP UTAMA (BRAIN)
// ================================================================
void loop() {
  switch (currentState) {
    case STATE_SLEEP:      runSleep(); break;
    case STATE_EYES:       runEyes(); break;
    case STATE_BOOT:       runBoot(); break;
    case STATE_MENU:       runMenu(); break;
    case GAME_SNAKE:       runSnake(); break;
    case GAME_PONG:        runPong(); break;
    case GAME_DOOM:        runDoom(); break;
    case STATE_UPDATE_OS:  runUpdateOS(); break;
    case STATE_GAMEOVER:   runGameOver(); break;
    case STATE_INFO:       runInfo(); break;
  }
}

// --- FUNGSI AUDIO SEDERHANA ---
void beep(int duration) {
  digitalWrite(BUZZ_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZ_PIN, LOW);
}

// ================================================================
// 6. FITUR: ROBOT EYES (FULL SCREEN V0.5)
// ================================================================

void runSleep() {
  display.clearDisplay();
  // Gambar Mata Tidur (- -)
  display.fillRect(cXL - 12, cY, 24, 3, WHITE);
  display.fillRect(cXR - 12, cY, 24, 3, WHITE);
  display.display();

  // Bangun jika diklik
  if (digitalRead(SW_PIN) == LOW) {
    beep(50); delay(100); beep(50);
    currentState = STATE_EYES;
    lastBlink = millis();
    delay(500); 
  }
}

void runEyes() {
  display.clearDisplay();
  
  // 1. Logika Kedip
  if (millis() - lastBlink > blinkInterval) {
    isBlinking = true;
    display.fillRect(cXL - 12, cY, 24, 3, WHITE);
    display.fillRect(cXR - 12, cY, 24, 3, WHITE);
    display.display();
    delay(150); 
    isBlinking = false;
    lastBlink = millis();
    blinkInterval = random(2000, 5000);
    return; 
  }

  // 2. Logika Joystick (FULL SCREEN RANGE)
  int xVal = analogRead(VRX_PIN);
  int yVal = analogRead(VRY_PIN);
  
  // Range diperluas: -45 sampai 45 pixel (Hampir menyentuh pinggir layar)
  int offX = map(xVal, 0, 4095, -45, 45);
  int offY = map(yVal, 0, 4095, -20, 20); // Y jangan terlalu jauh agar tidak hilang

  // 3. Render Mata (Rounded Rect)
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
  display.setTextSize(2); display.setCursor(40, 25); display.println("V0.5");
  display.setTextSize(1); display.setCursor(42, 45); display.println("by rion");
  display.display();
  beep(100); delay(50); beep(200);
  delay(1500);
  currentState = STATE_MENU;
}

// ================================================================
// 7. MENU SYSTEM
// ================================================================

void runMenu() {
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(WHITE);
  
  display.setCursor(35, 0); display.println("MAIN MENU");
  display.drawLine(0, 8, 128, 8, WHITE);

  for (int i = 0; i < menuTotal; i++) {
    int yPos = 12 + (i * 9); 
    if (i == menuIndex) {
      display.setCursor(2, yPos); display.print(">"); 
    } else {
      display.setCursor(2, yPos); display.print(" ");
    }
    display.setCursor(12, yPos); display.println(menuItems[i]);
  }
  display.display();

  // Navigasi
  int yVal = analogRead(VRY_PIN);
  if (yVal > 3000) { menuIndex++; if(menuIndex >= menuTotal) menuIndex = 0; beep(10); delay(150); }
  if (yVal < 1000) { menuIndex--; if(menuIndex < 0) menuIndex = menuTotal - 1; beep(10); delay(150); }

  // Seleksi Menu
  if (digitalRead(SW_PIN) == LOW) {
    delay(300); // Debounce Awal
    score = 0; // Reset Score

    if (menuIndex == 0) { currentState = GAME_SNAKE; setupSnake(); }
    else if (menuIndex == 1) { currentState = GAME_PONG; setupPong(); }
    else if (menuIndex == 2) { currentState = GAME_DOOM; setupDoom(); }
    else if (menuIndex == 3) { setupUpdate(); currentState = STATE_UPDATE_OS; }
    else if (menuIndex == 4) { currentState = STATE_INFO; }
    else if (menuIndex == 5) { currentState = STATE_SLEEP; }
  }
}

// ================================================================
// 8. GAME: SNAKE (FIXED BUG & SCORE)
// ================================================================
int sx[100], sy[100];
int slen, fX, fY, sDx, sDy;

// Fungsi Spawn Aman (Tidak di badan ular)
void spawnFoodSafe() {
  bool valid = false;
  while(!valid) {
    fX = random(0, 32); 
    fY = random(1, 16); // Mulai dr 1 biar ga ketutup score di atas
    valid = true;
    for(int i=0; i<slen; i++) {
      if(sx[i] == fX && sy[i] == fY) valid = false; // Ulang jika kena badan
    }
  }
}

void setupSnake() { 
  slen=3; 
  sx[0]=5; sy[0]=5; 
  sDx=1; sDy=0; 
  spawnFoodSafe();
}

void runSnake() {
  int x = analogRead(VRX_PIN); 
  int y = analogRead(VRY_PIN);
  
  // Kontrol Arah
  if(x > 3000 && sDx == 0) { sDx=1; sDy=0; } 
  if(x < 1000 && sDx == 0) { sDx=-1; sDy=0; }
  if(y > 3000 && sDy == 0) { sDx=0; sDy=1; } 
  if(y < 1000 && sDy == 0) { sDx=0; sDy=-1; }
  
  // Gerak Ular
  for(int i = slen-1; i > 0; i--) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
  sx[0] += sDx; sy[0] += sDy;
  
  // Tembus Tembok
  if(sx[0] > 31) sx[0]=0; if(sx[0] < 0) sx[0]=31; 
  if(sy[0] > 15) sy[0]=1; if(sy[0] < 1) sy[0]=15; // Batas Y=1 (Header score)
  
  // Tabrakan Diri
  for(int i=1; i<slen; i++) {
    if(sx[0]==sx[i] && sy[0]==sy[i]) currentState=STATE_GAMEOVER;
  }
  
  // Makan
  if(sx[0]==fX && sy[0]==fY) {
    score+=10; slen++; beep(30); spawnFoodSafe();
  }
  
  // Gambar
  display.clearDisplay(); 
  
  // Tampilkan Score Realtime (FIXED)
  display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(0,0); display.print("Score: "); display.print(score);
  display.drawLine(0, 8, 128, 8, WHITE); // Garis batas score

  display.fillRect(fX*4, fY*4, 4, 4, WHITE); // Makanan
  for(int i=0; i<slen; i++) {
    display.fillRect(sx[i]*4, sy[i]*4, 4, 4, WHITE); // Badan
  }
  display.display(); 
  delay(80);
}

// ================================================================
// 9. GAME: PONG (DYNAMIC LEVEL SPEED)
// ================================================================
int paddleY, paddleH;
int ballX, ballY, ballDX, ballDY;
int cpuY;
int pongSpeed; // Variabel kecepatan

void setupPong() { 
  paddleY = 22; paddleH = 16; cpuY = 22;
  ballX=64; ballY=32; ballDX=1; ballDY=1;
  pongSpeed = 30; // Kecepatan awal (Makin kecil makin cepat)
}

void runPong() {
  // Input Player
  int yVal = analogRead(VRY_PIN);
  if(yVal > 3000 && paddleY < 48) paddleY += 2;
  if(yVal < 1000 && paddleY > 0) paddleY -= 2;
  
  // Gerak Bola
  ballX += ballDX; ballY += ballDY;
  if(ballY <= 0 || ballY >= 60) ballDY *= -1;
  
  // AI CPU
  if(ballX > 64) { if(ballY > cpuY + 8) cpuY += 1; else cpuY -= 1; }
  
  // Tabrakan & Scoring
  if(ballX <= 4 && ballY >= paddleY && ballY <= paddleY+paddleH) { 
    ballDX *= -1; score++; beep(20); 
    // Difficulty Logic: Percepat setiap kelipatan 3 poin
    if(score % 3 == 0 && pongSpeed > 5) pongSpeed -= 5;
  }
  
  if(ballX >= 120 && ballY >= cpuY && ballY <= cpuY+paddleH) { ballDX *= -1; }
  if(ballX < 0 || ballX > 128) currentState = STATE_GAMEOVER;
  
  display.clearDisplay();
  
  // Info Level & Score
  display.setCursor(40,0); 
  display.print("Lvl:"); display.print((35-pongSpeed)/5); 
  display.print(" Sc:"); display.print(score);

  display.fillRect(0, paddleY, 4, paddleH, WHITE);   
  display.fillRect(124, cpuY, 4, paddleH, WHITE);    
  display.fillCircle(ballX, ballY, 2, WHITE);        
  display.display();
  
  delay(pongSpeed); // Delay dinamis
}

// ================================================================
// 10. GAME: DOOM 3D (RAYCASTING V0.5)
// ================================================================
const int mapWidth = 16; const int mapHeight = 16;
const byte worldMap[16][16] = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
  {1,0,0,1,1,0,1,0,0,1,0,0,0,0,0,1},
  {1,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1},
  {1,0,0,1,0,0,0,0,0,1,1,1,1,0,0,1},
  {1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,1,0,0,0,0,1,1,0,0,0,0,0,0,1},
  {1,0,1,0,0,0,0,1,1,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1},
  {1,0,0,1,1,1,1,1,1,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};
double posX, posY, dirX, dirY, planeX, planeY;
bool enemyAlive; double enemyX, enemyY;

void setupDoom() {
  posX = 3.0; posY = 3.0; dirX = 1.0; dirY = 0.0; planeX = 0.0; planeY = 0.66;
  enemyAlive = true; enemyX = 10.5; enemyY = 10.5;
  display.clearDisplay(); display.setCursor(30,30); display.print("LOADING..."); display.display(); delay(500);
}

void runDoom() {
  display.clearDisplay();
  int xVal = analogRead(VRX_PIN); int yVal = analogRead(VRY_PIN);
  double moveSpeed = 0.15; double rotSpeed = 0.1;

  if (xVal < 1000) { // Rotate Left
    double oldDirX = dirX; dirX = dirX * cos(rotSpeed) - dirY * sin(rotSpeed); dirY = oldDirX * sin(rotSpeed) + dirY * cos(rotSpeed);
    double oldPlaneX = planeX; planeX = planeX * cos(rotSpeed) - planeY * sin(rotSpeed); planeY = oldPlaneX * sin(rotSpeed) + planeY * cos(rotSpeed);
  } else if (xVal > 3000) { // Rotate Right
    double oldDirX = dirX; dirX = dirX * cos(-rotSpeed) - dirY * sin(-rotSpeed); dirY = oldDirX * sin(-rotSpeed) + dirY * cos(-rotSpeed);
    double oldPlaneX = planeX; planeX = planeX * cos(-rotSpeed) - planeY * sin(-rotSpeed); planeY = oldPlaneX * sin(-rotSpeed) + planeY * cos(-rotSpeed);
  }

  if (yVal < 1000) { // Walk Forward
    if(worldMap[int(posX + dirX * moveSpeed)][int(posY)] == 0) posX += dirX * moveSpeed;
    if(worldMap[int(posX)][int(posY + dirY * moveSpeed)] == 0) posY += dirY * moveSpeed;
  } else if (yVal > 3000) { // Walk Backward
    if(worldMap[int(posX - dirX * moveSpeed)][int(posY)] == 0) posX -= dirX * moveSpeed;
    if(worldMap[int(posX)][int(posY - dirY * moveSpeed)] == 0) posY -= dirY * moveSpeed;
  }

  // RAYCASTING LOOP
  for (int x = 0; x < SCREEN_WIDTH; x+=2) {
    double cameraX = 2 * x / (double)SCREEN_WIDTH - 1;
    double rayDirX = dirX + planeX * cameraX;
    double rayDirY = dirY + planeY * cameraX;
    int mapX = int(posX); int mapY = int(posY);
    double sideDistX, sideDistY;
    double deltaDistX = (rayDirX == 0) ? 1e30 : abs(1 / rayDirX);
    double deltaDistY = (rayDirY == 0) ? 1e30 : abs(1 / rayDirY);
    double perpWallDist;
    int stepX, stepY, hit = 0, side;

    if (rayDirX < 0) { stepX = -1; sideDistX = (posX - mapX) * deltaDistX; }
    else { stepX = 1; sideDistX = (mapX + 1.0 - posX) * deltaDistX; }
    if (rayDirY < 0) { stepY = -1; sideDistY = (posY - mapY) * deltaDistY; }
    else { stepY = 1; sideDistY = (mapY + 1.0 - posY) * deltaDistY; }

    while (hit == 0) {
      if (sideDistX < sideDistY) { sideDistX += deltaDistX; mapX += stepX; side = 0; }
      else { sideDistY += deltaDistY; mapY += stepY; side = 1; }
      if (worldMap[mapX][mapY] > 0) hit = 1;
    }
    if (side == 0) perpWallDist = (sideDistX - deltaDistX); else perpWallDist = (sideDistY - deltaDistY);

    int lineHeight = (int)(SCREEN_HEIGHT / perpWallDist);
    int drawStart = -lineHeight / 2 + SCREEN_HEIGHT / 2;
    if (drawStart < 0) drawStart = 0;
    int drawEnd = lineHeight / 2 + SCREEN_HEIGHT / 2;
    if (drawEnd >= SCREEN_HEIGHT) drawEnd = SCREEN_HEIGHT - 1;

    if (side == 1) { for(int y=drawStart; y<drawEnd; y+=2) { display.drawPixel(x, y, WHITE); display.drawPixel(x+1, y, WHITE); } }
    else { display.drawFastVLine(x, drawStart, drawEnd - drawStart, WHITE); display.drawFastVLine(x+1, drawStart, drawEnd - drawStart, WHITE); }
  }

  // Render Gun & Enemy
  display.fillTriangle(48, 64, 80, 64, 64, 45, WHITE); display.fillRect(62, 45, 4, 10, WHITE);
  double distEnemy = sqrt(pow(posX - enemyX, 2) + pow(posY - enemyY, 2));
  if (enemyAlive && distEnemy < 4.0) {
     int eSize = 40 / distEnemy; 
     display.drawRect(64 - eSize/2, 32 - eSize/2, eSize, eSize, WHITE);
     display.drawLine(64 - eSize/2, 32 - eSize/2, 64 + eSize/2, 32 + eSize/2, WHITE);
     enemyX += (posX - enemyX) * 0.02; enemyY += (posY - enemyY) * 0.02;
     if(distEnemy < 0.5) currentState = STATE_GAMEOVER;
  }

  if (digitalRead(SW_PIN) == LOW) {
    display.invertDisplay(true); beep(20);
    if (enemyAlive && distEnemy < 3.0) { score += 500; enemyAlive = false; beep(100); 
      enemyX = int(posX) + random(-5, 5); enemyY = int(posY) + random(-5, 5); if(enemyX<1) enemyX=1; if(enemyY<1) enemyY=1; enemyAlive=true; 
    }
    delay(50); display.invertDisplay(false);
  }
  display.display();
}

// ================================================================
// 11. OTA UPDATE SYSTEM (FIXED EXIT BUG)
// ================================================================

void setupUpdate() {
  display.clearDisplay(); display.setCursor(0,0); display.println("Connect WiFi..."); display.display();
  WiFi.begin(ssid, password);
  int t=0; while (WiFi.status() != WL_CONNECTED && t < 20) { delay(500); display.print("."); display.display(); t++; }
  
  if(WiFi.status() != WL_CONNECTED) {
    display.clearDisplay(); display.setCursor(0,0); display.println("WiFi Failed!"); display.display(); delay(2000); currentState = STATE_MENU; return;
  }

  server.on("/", HTTP_GET, []() { server.sendHeader("Connection", "close"); server.send(200, "text/html", loginIndex); });
  server.on("/update", HTTP_POST, []() { server.sendHeader("Connection", "close"); server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "REBOOTING..."); ESP.restart(); }, 
  []() { HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) Update.begin(UPDATE_SIZE_UNKNOWN);
    else if (upload.status == UPLOAD_FILE_WRITE) Update.write(upload.buf, upload.currentSize);
    else if (upload.status == UPLOAD_FILE_END) Update.end(true);
  });
  server.begin();
}

void runUpdateOS() {
  server.handleClient();
  display.clearDisplay();
  display.setCursor(0,0); display.println("MODE: UPDATE OS");
  display.drawLine(0,10,128,10,WHITE);
  display.setCursor(0,20); display.println("IP Address:"); 
  display.println(WiFi.localIP());
  display.setCursor(0,50); display.println("[Click to Exit]");
  display.display();

  // FIX BUG EXIT: Gunakan while loop untuk menunggu tombol dilepas
  if (digitalRead(SW_PIN) == LOW) { 
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    currentState = STATE_MENU;
    
    // Tahan sampai tombol dilepas (mencegah klik ganda masuk lagi)
    while(digitalRead(SW_PIN) == LOW) { delay(10); } 
    delay(500); // Extra debounce
  }
}

// ================================================================
// 12. INFO & GAMEOVER
// ================================================================
void runGameOver() {
  display.clearDisplay(); display.setCursor(10,20); display.setTextSize(2); display.println("GAME OVER");
  display.setTextSize(1); display.setCursor(40,40); display.print("Score: "); display.print(score); display.display();
  if(digitalRead(SW_PIN) == LOW) { delay(500); currentState = STATE_MENU; }
}

void runInfo() {
  display.clearDisplay(); display.setCursor(0,0); display.println("INFO V0.5");
  display.drawLine(0,10,128,10,WHITE);
  display.println("- Raycasting Engine");
  display.println("- Full Screen Eyes");
  display.println("- Dynamic Pong");
  display.println("- OTA Fix");
  display.display();
  if(digitalRead(SW_PIN) == LOW) { delay(500); currentState = STATE_MENU; }
}