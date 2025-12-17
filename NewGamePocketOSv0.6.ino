/*
   PROJECT: NewGame Pocket OS
   VERSION: V0.6 (Major Update)
   DEV: Rion
   FEATURES: Pacman, Time Sync, Save Data, Scrolling Menu
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h> // Untuk Save Data
#include "time.h"        // Untuk Jam Internet

// ================================================================
// 1. KONFIGURASI WIFI & HARDWARE
// ================================================================
const char* ssid = "NAMA_WIFI_ANDA";     
const char* password = "PASSWORD_WIFI_ANDA"; 

// NTP Server (WIB = UTC+7)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200; // +7 Jam
const int   daylightOffset_sec = 0;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define VRX_PIN  34
#define VRY_PIN  35
#define SW_PIN   32
#define BUZZ_PIN 25 

WebServer server(80);
Preferences pref; // Objek penyimpanan

// ================================================================
// 2. STATE MANAGEMENT
// ================================================================
enum SystemState { 
  STATE_SLEEP, STATE_EYES, STATE_BOOT, STATE_MENU, 
  GAME_SNAKE, GAME_PONG, GAME_DOOM, GAME_PACMAN,
  STATE_HIGHSCORE, STATE_UPDATE_OS, STATE_GAMEOVER, STATE_INFO,
  STATE_SYNC_TIME
};
SystemState currentState = STATE_SLEEP;

// ================================================================
// 3. GLOBAL VARIABLES
// ================================================================
// --- Menu Scrolling ---
int menuIndex = 0;
int menuOffset = 0; // Posisi scroll
const int menuTotal = 8;
const int menuVisible = 5; // Jumlah menu yg tampil di layar
String menuItems[] = {
  "1. Snake", "2. PingPong", "3. Doom 3D", "4. Pacman",
  "5. Update Jam", "6. List Highscore", "7. Info System", "8. Update NewGame OS"
};

// --- Time ---
char timeString[6] = "--:--"; // Buffer Jam

// --- Scores & Save Data ---
int currentScore = 0;
int hsSnake=0, hsPong=0, hsDoom=0, hsPacman=0;
int gameID = 0; // 1:Snake, 2:Pong, 3:Doom, 4:Pacman

// --- Robot Eyes ---
int eyeW = 24, eyeH = 48, eyeGap = 34;
int cX = SCREEN_WIDTH/2, cY = SCREEN_HEIGHT/2;
int cXL = cX - eyeGap/2, cXR = cX + eyeGap/2;
unsigned long lastBlink = 0;
int blinkInterval = 3000;
bool isBlinking = false;

// HTML Update Page
const char* loginIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='UPDATE V0.6'></form>";

// ================================================================
// 4. SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(BUZZ_PIN, OUTPUT); digitalWrite(BUZZ_PIN, LOW);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) { for(;;); }
  
  // Load Highscores
  pref.begin("ng-os", true); // Read Only Mode
  hsSnake = pref.getInt("hs_snake", 0);
  hsPong = pref.getInt("hs_pong", 0);
  hsDoom = pref.getInt("hs_doom", 0);
  hsPacman = pref.getInt("hs_pacman", 0);
  pref.end();

  currentState = STATE_SLEEP;
}

// ================================================================
// 5. HELPER FUNCTIONS
// ================================================================
void beep(int duration) {
  digitalWrite(BUZZ_PIN, HIGH); delay(duration); digitalWrite(BUZZ_PIN, LOW);
}

void saveHighScore(int gID, int score) {
  pref.begin("ng-os", false); // Read/Write
  if(gID==1 && score > hsSnake) { hsSnake = score; pref.putInt("hs_snake", hsSnake); }
  if(gID==2 && score > hsPong) { hsPong = score; pref.putInt("hs_pong", hsPong); }
  if(gID==3 && score > hsDoom) { hsDoom = score; pref.putInt("hs_doom", hsDoom); }
  if(gID==4 && score > hsPacman) { hsPacman = score; pref.putInt("hs_pacman", hsPacman); }
  pref.end();
}

void updateLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    strcpy(timeString, "--:--");
    return;
  }
  sprintf(timeString, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
}

// ================================================================
// 6. MAIN LOOP
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
    case GAME_PACMAN:      runPacman(); break;
    case STATE_SYNC_TIME:  runSyncTime(); break;
    case STATE_HIGHSCORE:  runHighScoreList(); break;
    case STATE_UPDATE_OS:  runUpdateOS(); break;
    case STATE_GAMEOVER:   runGameOver(); break;
    case STATE_INFO:       runInfo(); break;
  }
}

// ================================================================
// 7. FEATURES: EYES & MENU
// ================================================================
void runSleep() {
  display.clearDisplay();
  // Mata Tidur
  display.fillRect(cXL-12, cY, 24, 3, WHITE);
  display.fillRect(cXR-12, cY, 24, 3, WHITE);
  // Jam di Bawah
  display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(50, 55); display.print(timeString);
  display.display();

  if (digitalRead(SW_PIN) == LOW) {
    beep(50); delay(100); beep(50);
    currentState = STATE_EYES; lastBlink = millis(); delay(500); 
  }
}

void runEyes() {
  display.clearDisplay();
  if (millis() - lastBlink > blinkInterval) {
    isBlinking = true; display.fillRect(cXL-12, cY, 24, 3, WHITE); display.fillRect(cXR-12, cY, 24, 3, WHITE);
    display.display(); delay(150); isBlinking = false; lastBlink = millis(); blinkInterval = random(2000, 5000); return; 
  }
  int xVal = analogRead(VRX_PIN); int yVal = analogRead(VRY_PIN);
  int offX = map(xVal, 0, 4095, -45, 45); int offY = map(yVal, 0, 4095, -20, 20);
  display.fillRoundRect(cXL-eyeW/2+offX, cY-eyeH/2+offY, eyeW, eyeH, 8, WHITE);
  display.fillRoundRect(cXR-eyeW/2+offX, cY-eyeH/2+offY, eyeW, eyeH, 8, WHITE);
  display.display();
  if (digitalRead(SW_PIN) == LOW) { currentState = STATE_BOOT; delay(500); }
}

void runBoot() {
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(15, 10); display.println("NewGame Pocket OS");
  display.setTextSize(2); display.setCursor(40, 25); display.println("V0.6");
  display.setTextSize(1); display.setCursor(42, 45); display.println("by rion");
  display.display(); beep(100); delay(50); beep(200); delay(1500);
  currentState = STATE_MENU;
}

// --- SCROLLABLE MENU ---
void runMenu() {
  display.clearDisplay();
  // Header
  display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(0, 0); display.print("MENU");
  display.setCursor(95, 0); display.print(timeString); // Jam di pojok kanan
  display.drawLine(0, 8, 128, 8, WHITE);

  // Scroll Logic
  if(menuIndex >= menuOffset + menuVisible) menuOffset = menuIndex - menuVisible + 1;
  if(menuIndex < menuOffset) menuOffset = menuIndex;

  // Render List
  for (int i = 0; i < menuVisible; i++) {
    int itemIdx = menuOffset + i;
    if(itemIdx >= menuTotal) break;
    int yPos = 10 + (i * 10);
    if (itemIdx == menuIndex) { display.setCursor(2, yPos); display.print(">"); }
    else { display.setCursor(2, yPos); display.print(" "); }
    display.setCursor(12, yPos); display.println(menuItems[itemIdx]);
  }
  display.display();

  // Input
  int yVal = analogRead(VRY_PIN);
  if (yVal > 3000) { menuIndex++; if(menuIndex >= menuTotal) menuIndex = 0; beep(10); delay(150); }
  if (yVal < 1000) { menuIndex--; if(menuIndex < 0) menuIndex = menuTotal - 1; beep(10); delay(150); }

  if (digitalRead(SW_PIN) == LOW) {
    delay(300); currentScore = 0;
    if (menuIndex == 0) { gameID=1; currentState = GAME_SNAKE; setupSnake(); }
    else if (menuIndex == 1) { gameID=2; currentState = GAME_PONG; setupPong(); }
    else if (menuIndex == 2) { gameID=3; currentState = GAME_DOOM; setupDoom(); }
    else if (menuIndex == 3) { gameID=4; currentState = GAME_PACMAN; setupPacman(); }
    else if (menuIndex == 4) { currentState = STATE_SYNC_TIME; }
    else if (menuIndex == 5) { currentState = STATE_HIGHSCORE; }
    else if (menuIndex == 6) { currentState = STATE_INFO; }
    else if (menuIndex == 7) { setupUpdate(); currentState = STATE_UPDATE_OS; }
  }
}

// ================================================================
// 8. GAME: SNAKE (SAFE ZONE & MINIMALIST)
// ================================================================
int sx[100], sy[100], slen, fX, fY, sDx, sDy;
const int HEADER_H = 10; // Area Aman Header

void setupSnake() { 
  slen=3; sx[0]=10; sy[0]=10; sDx=1; sDy=0; 
  spawnFoodSafe();
}
void spawnFoodSafe() {
  bool valid = false;
  while(!valid) {
    fX = random(0, 32); fY = random(3, 16); // Y mulai dari 3 (12 pixel) agar aman dr header
    valid = true;
    for(int i=0; i<slen; i++) if(sx[i] == fX && sy[i] == fY) valid = false;
    // Cek jangan kena area skor (0,0) sampai (8,3) - Asumsi text score
    if(fX < 10 && fY < 3) valid = false;
  }
}
void runSnake() {
  int x = analogRead(VRX_PIN); int y = analogRead(VRY_PIN);
  if(x>3000 && sDx==0){sDx=1;sDy=0;} if(x<1000 && sDx==0){sDx=-1;sDy=0;}
  if(y>3000 && sDy==0){sDx=0;sDy=1;} if(y<1000 && sDy==0){sDx=0;sDy=-1;}
  
  for(int i=slen-1; i>0; i--){sx[i]=sx[i-1]; sy[i]=sy[i-1];} sx[0]+=sDx; sy[0]+=sDy;
  if(sx[0]>31) sx[0]=0; if(sx[0]<0) sx[0]=31;
  if(sy[0]>15) sy[0]=3; if(sy[0]<3) sy[0]=15; // Wrap Y (dibawah header)

  for(int i=1; i<slen; i++) if(sx[0]==sx[i] && sy[0]==sy[i]) currentState=STATE_GAMEOVER;
  if(sx[0]==fX && sy[0]==fY){ currentScore+=10; slen++; beep(30); spawnFoodSafe(); }

  display.clearDisplay();
  // Hanya Angka
  display.setTextSize(1); display.setCursor(0,0); display.print(currentScore);
  display.drawLine(0, HEADER_H-1, 128, HEADER_H-1, WHITE);
  
  display.fillRect(fX*4, fY*4, 4, 4, WHITE);
  for(int i=0; i<slen; i++) display.fillRect(sx[i]*4, sy[i]*4, 4, 4, WHITE);
  display.display(); delay(80);
}

// ================================================================
// 9. GAME: PONG (FASTER)
// ================================================================
int pY, bX, bY, bDX, bDY, cpuY;
void setupPong() { pY=22; bX=64; bY=32; bDX=2; bDY=2; cpuY=22; } // Speed Awal lgsg 2
void runPong() {
  int yVal = analogRead(VRY_PIN);
  if(yVal>3000 && pY<48) pY+=3; if(yVal<1000 && pY>0) pY-=3;
  bX+=bDX; bY+=bDY; if(bY<=0 || bY>=60) bDY*=-1;
  if(bX>64) { if(bY>cpuY+8) cpuY+=2; else cpuY-=2; }
  
  if(bX<=4 && bY>=pY && bY<=pY+16) { bDX*=-1; currentScore++; beep(20); }
  if(bX>=120 && bY>=cpuY && bY<=cpuY+16) { bDX*=-1; }
  if(bX<0 || bX>128) currentState=STATE_GAMEOVER;

  display.clearDisplay();
  display.setCursor(60,0); display.print(currentScore);
  display.fillRect(0, pY, 4, 16, WHITE); display.fillRect(124, cpuY, 4, 16, WHITE);
  display.fillCircle(bX, bY, 2, WHITE);
  display.display(); delay(15); // Delay dipersingkat (Lebih cepat)
}

// ================================================================
// 10. GAME: DOOM 3D (MORE ENEMIES & SHAPE)
// ================================================================
// Map sederhana
const byte wm[16][16] = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1}, {1,0,0,1,1,0,1,0,0,1,0,0,0,0,0,1},
  {1,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1}, {1,0,0,1,0,0,0,0,0,1,1,1,1,0,0,1},
  {1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1}, {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,1,0,0,0,0,1,1,0,0,0,0,0,0,1}, {1,0,1,0,0,0,0,1,1,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1}, {1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1},
  {1,0,0,1,1,1,1,1,1,0,0,0,0,0,0,1}, {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};
double pX, pY, dX, dY, plX, plY, eX, eY; bool eAlive;

void setupDoom() { pX=3.0; pY=3.0; dX=1.0; dY=0.0; plX=0.0; plY=0.66; eAlive=true; eX=10.5; eY=10.5; }

void runDoom() {
  display.clearDisplay();
  int x=analogRead(VRX_PIN); int y=analogRead(VRY_PIN);
  double mS=0.15, rS=0.1;
  // Control
  if(x<1000){double oldD=dX; dX=dX*cos(rS)-dY*sin(rS); dY=oldD*sin(rS)+dY*cos(rS); double oldP=plX; plX=plX*cos(rS)-plY*sin(rS); plY=oldP*sin(rS)+plY*cos(rS);}
  if(x>3000){double oldD=dX; dX=dX*cos(-rS)-dY*sin(-rS); dY=oldD*sin(-rS)+dY*cos(-rS); double oldP=plX; plX=plX*cos(-rS)-plY*sin(-rS); plY=oldP*sin(-rS)+plY*cos(-rS);}
  if(y<1000){if(wm[int(pX+dX*mS)][int(pY)]==0)pX+=dX*mS; if(wm[int(pX)][int(pY+dY*mS)]==0)pY+=dY*mS;}
  if(y>3000){if(wm[int(pX-dX*mS)][int(pY)]==0)pX-=dX*mS; if(wm[int(pX)][int(pY-dY*mS)]==0)pY-=dY*mS;}

  // Raycasting
  for(int i=0; i<128; i+=2) {
    double cX=2*i/(double)128-1; double rDX=dX+plX*cX; double rDY=dY+plY*cX;
    int mX=int(pX), mY=int(pY); double sDX, sDY;
    double dDX=(rDX==0)?1e30:abs(1/rDX); double dDY=(rDY==0)?1e30:abs(1/rDY);
    double pwd; int stX, stY, hit=0, sd;
    if(rDX<0){stX=-1;sDX=(pX-mX)*dDX;} else{stX=1;sDX=(mX+1.0-pX)*dDX;}
    if(rDY<0){stY=-1;sDY=(pY-mY)*dDY;} else{stY=1;sDY=(mY+1.0-pY)*dDY;}
    while(hit==0){if(sDX<sDY){sDX+=dDX;mX+=stX;sd=0;}else{sDY+=dDY;mY+=stY;sd=1;} if(wm[mX][mY]>0)hit=1;}
    if(sd==0)pwd=(sDX-dDX); else pwd=(sDY-dDY);
    int h=int(64/pwd); int ds=-h/2+32; if(ds<0)ds=0; int de=h/2+32; if(de>=64)de=63;
    if(sd==1){display.drawFastVLine(i, ds, de-ds, WHITE);display.drawFastVLine(i+1, ds, de-ds, WHITE);}
    else{for(int k=ds;k<de;k+=2){display.drawPixel(i,k,WHITE);display.drawPixel(i+1,k,WHITE);}}
  }

  // Enemy (Improved Shape)
  double dE = sqrt(pow(pX-eX, 2) + pow(pY-eY, 2));
  if (eAlive && dE < 5.0) {
     int eS = 50 / dE; 
     int scrX = 64; 
     // Gambar Alien (Bulat + Mata)
     display.fillCircle(scrX, 32, eS/2, WHITE);
     display.fillCircle(scrX, 32, eS/4, BLACK); // Mata hitam
     eX+=(pX-eX)*0.03; eY+=(pY-eY)*0.03;
     if(dE < 0.5) currentState=STATE_GAMEOVER;
  }
  // Gun
  display.fillTriangle(48,64,80,64,64,45,WHITE);
  if(digitalRead(SW_PIN)==LOW) {
    display.invertDisplay(true); beep(20);
    if(eAlive && dE < 3.5) { 
      currentScore+=100; eAlive=false; beep(100); 
      // Spawn Rate 50% lebih banyak (langsung respawn dekat)
      eX=int(pX)+random(-4,4); eY=int(pY)+random(-4,4); if(eX<1)eX=1; if(eY<1)eY=1; eAlive=true;
    }
    delay(50); display.invertDisplay(false);
  }
  display.display();
}

// ================================================================
// 11. GAME: PACMAN (MINI VERSION)
// ================================================================
// 0=Pellet, 1=Wall, 2=Empty, 3=Pacman
byte pmMap[8][16] = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
  {1,0,1,1,1,0,1,0,1,1,1,0,1,1,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,1,1,0,1,1,1,1,0,1,0,1,1,0,1},
  {1,0,0,0,0,0,0,2,0,0,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} // Baris status (dummy)
};
int pacX, pacY, ghostX, ghostY, pelletsLeft;
unsigned long lastMove = 0;

void setupPacman() {
  pacX=1; pacY=1; ghostX=14; ghostY=5; pelletsLeft=0;
  // Reset Map & Hitung Pellet
  for(int y=0;y<7;y++) for(int x=0;x<16;x++) if(pmMap[y][x]==0) pelletsLeft++;
}

void runPacman() {
  // Input Movement (Grid Based)
  if(millis() - lastMove > 150) {
    int x=analogRead(VRX_PIN); int y=analogRead(VRY_PIN);
    int nextX=pacX, nextY=pacY;
    if(x<1000) nextX--; if(x>3000) nextX++;
    if(y<1000) nextY--; if(y>3000) nextY++;
    
    // Cek Tembok
    if(pmMap[nextY][nextX] != 1) { pacX=nextX; pacY=nextY; }
    
    // Makan
    if(pmMap[pacY][pacX] == 0) { 
      pmMap[pacY][pacX] = 2; // Jadi Empty
      currentScore+=10; pelletsLeft--; beep(10);
    }
    
    // Ghost AI (Simple Follow)
    if(random(0,3)==0) { // Speed hantu lebih lambat
      int gNextX=ghostX, gNextY=ghostY;
      if(pacX > ghostX) gNextX++; else if(pacX < ghostX) gNextX--;
      if(pacY > ghostY) gNextY++; else if(pacY < ghostY) gNextY--;
      if(pmMap[gNextY][gNextX] != 1) { ghostX=gNextX; ghostY=gNextY; }
    }
    lastMove = millis();
  }
  
  // Cek Kalah/Menang
  if(pacX==ghostX && pacY==ghostY) currentState=STATE_GAMEOVER;
  if(pelletsLeft <= 0) { currentScore+=500; setupPacman(); } // Level Up (Reset Map Logic needed ideally)

  display.clearDisplay();
  // Render Map (8x8 tiles)
  for(int y=0; y<7; y++) {
    for(int x=0; x<16; x++) {
      if(pmMap[y][x]==1) display.fillRect(x*8, y*8, 8, 8, WHITE); // Wall
      if(pmMap[y][x]==0) display.fillCircle(x*8+4, y*8+4, 1, WHITE); // Pellet
    }
  }
  // Render Pacman
  display.fillCircle(pacX*8+4, pacY*8+4, 3, WHITE);
  // Render Ghost
  display.fillTriangle(ghostX*8, ghostY*8+8, ghostX*8+4, ghostY*8, ghostX*8+8, ghostY*8+8, WHITE);
  
  display.setCursor(0, 56); display.print("Score:"); display.print(currentScore);
  display.display();
}

// ================================================================
// 12. UTILITIES: UPDATE, TIME, HIGHSCORE
// ================================================================

void runSyncTime() {
  display.clearDisplay(); display.setCursor(0,0); display.println("Syncing Time..."); display.display();
  WiFi.begin(ssid, password);
  int t=0; while(WiFi.status()!=WL_CONNECTED && t<20) { delay(500); display.print("."); display.display(); t++; }
  if(WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    updateLocalTime();
    display.clearDisplay(); display.println("Time Updated!"); display.println(timeString); display.display();
    WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
  } else {
    display.clearDisplay(); display.println("WiFi Fail!"); display.display();
  }
  delay(2000); currentState = STATE_MENU;
}

void runHighScoreList() {
  display.clearDisplay();
  display.setCursor(0,0); display.println("HIGHSCORES");
  display.drawLine(0,8,128,8,WHITE);
  display.setCursor(0,15); display.print("Snake : "); display.print(hsSnake);
  display.setCursor(0,25); display.print("Pong  : "); display.print(hsPong);
  display.setCursor(0,35); display.print("Doom  : "); display.print(hsDoom);
  display.setCursor(0,45); display.print("Pacman: "); display.print(hsPacman);
  display.setCursor(0,55); display.print("[Click to Back]");
  display.display();
  if(digitalRead(SW_PIN)==LOW) { delay(500); currentState=STATE_MENU; }
}

void setupUpdate() {
  display.clearDisplay(); display.println("Connect WiFi..."); display.display();
  WiFi.begin(ssid, password);
  int t=0; while(WiFi.status()!=WL_CONNECTED && t<20) { delay(500); display.print("."); display.display(); t++; }
  if(WiFi.status()!=WL_CONNECTED) { currentState=STATE_MENU; return; }
  server.on("/", HTTP_GET, [](){ server.send(200, "text/html", loginIndex); });
  server.on("/update", HTTP_POST, [](){ server.send(200, "text/plain", (Update.hasError())?"FAIL":"REBOOTING..."); ESP.restart(); }, 
  [](){ HTTPUpload& u=server.upload(); 
    if(u.status==UPLOAD_FILE_START) Update.begin(UPDATE_SIZE_UNKNOWN); 
    else if(u.status==UPLOAD_FILE_WRITE) Update.write(u.buf, u.currentSize); 
    else if(u.status==UPLOAD_FILE_END) Update.end(true); 
  });
  server.begin();
}

void runUpdateOS() {
  server.handleClient();
  display.clearDisplay(); display.setCursor(0,0); display.println("UPDATE OS V0.6");
  display.drawLine(0,10,128,10,WHITE);
  display.setCursor(0,20); display.println(WiFi.localIP());
  display.setCursor(0,50); display.println("[Click to Exit]"); display.display();
  if(digitalRead(SW_PIN)==LOW) { 
    WiFi.disconnect(true); currentState=STATE_MENU; while(digitalRead(SW_PIN)==LOW) delay(10); 
  }
}

void runGameOver() {
  saveHighScore(gameID, currentScore); // Auto Save
  display.clearDisplay(); display.setTextSize(2); display.setCursor(10,20); display.println("GAME OVER");
  display.setTextSize(1); display.setCursor(40,40); display.print("Score: "); display.print(currentScore); display.display();
  if(digitalRead(SW_PIN)==LOW) { delay(500); currentState=STATE_MENU; }
}

void runInfo() {
  display.clearDisplay(); display.setCursor(0,0); display.println("INFO SYSTEM");
  display.println("- Save Data Active"); display.println("- NTP Time Sync"); display.println("- Pacman Added");
  display.display(); if(digitalRead(SW_PIN)==LOW) { delay(500); currentState=STATE_MENU; }
}