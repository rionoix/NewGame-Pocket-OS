/*
   PROJECT: NewGame Pocket OS
   VERSION: V0.6 (Cheater Edition)
   DEV: Rion
   WIFI: UNAND (Hardcoded)
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h> 
#include "time.h"        

// ================================================================
// 1. KONFIGURASI WIFI & HARDWARE
// ================================================================
const char* ssid = "UNAND";     
const char* password = "HardiknasDiAndalas"; 

const char* ntpServer = "time.google.com";
const long  gmtOffset_sec = 25200; // WIB
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
Preferences pref; 

// ================================================================
// 2. STATE MANAGEMENT
// ================================================================
enum SystemState { 
  STATE_SLEEP, STATE_EYES, STATE_EYE_MENU, STATE_READ_SELECT, STATE_READING, 
  STATE_BOOT, STATE_MENU, 
  GAME_SNAKE, GAME_PONG, GAME_DOOM, GAME_PACMAN, GAME_DINO, GAME_RACE,
  STATE_HIGHSCORE, STATE_RESET_CONFIRM, // State baru untuk reset
  STATE_UPDATE_OS, STATE_GAMEOVER, STATE_INFO,
  STATE_SYNC_TIME
};
SystemState currentState = STATE_SLEEP;

// ================================================================
// 3. GLOBAL VARIABLES
// ================================================================
int menuIndex = 0;
int menuOffset = 0; 
const int menuTotal = 11; 
const int menuVisible = 5; 
String menuItems[] = {
  "1. Snake", "2. PingPong", "3. Doom 3D", "4. Pacman",
  "5. Dino Run", "6. Turbo Race", "7. Update Jam", "8. Highscores", 
  "9. Info System", "10. Update Game", "11. Sleep Mode"
};

// Eye & Jokes
int eyeMenuIndex = 0;
String eyeMenuItems[] = {"Game Mode", "Read Mode", "Back"};
int readCatIndex = 0;
int scrollY = 0;
String jokeTitle[] = {"Jokes Bapak2", "Jokes Programming", "Jokes Receh", "[ BACK ]"}; 
String jokesContent[] = {
  "Kenapa zombie kalo nyerang bareng2?\nKalo sendiri namanya zomblo.\n\nIkan apa yang suka berhenti?\nIkan pause.\n\nAyam apa yang paling besar?\nAyam semesta.",
  "Kenapa programmer ga bisa ganti lampu?\nKarena itu masalah hardware.\n\n0 itu false, 1 itu true.\nSelain itu nonsense.\n\nCoding itu kayak masak.\nKadang gosong (error).",
  "Sapi, sapi apa yang nempel di dinding?\nStiker sapi.\n\nKenapa pohon kelapa di depan rumah harus ditebang?\nSoalnya kalo dicabut berat.\n\nApa bedanya soto sama coto?\nSoto pake S, Coto pake C."
};

char timeString[6] = "--:--"; 
bool timeSynced = false;

// Score & Cheat
int currentScore = 0;
int hsSnake=0, hsPong=0, hsDoom=0, hsPacman=0, hsDino=0, hsRace=0;
int gameID = 0; 

// Cheat Variables
unsigned long btnPressStart = 0;
bool godMode = false;
unsigned long godModeStart = 0;
int resetStep = 0; // Untuk konfirmasi 3x

// Robot Eyes
int eyeW = 24, eyeH = 48, eyeGap = 34;
int cX = SCREEN_WIDTH/2, cY = SCREEN_HEIGHT/2;
int cXL = cX - eyeGap/2, cXR = cX + eyeGap/2;
unsigned long lastBlink = 0;
int blinkInterval = 3000;
bool isBlinking = false;
int eyeEmotion = 0; 
unsigned long lastEmotionChange = 0;

const char* loginIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='UPDATE GAME'></form>";

// ================================================================
// 4. BITMAP ASSETS
// ================================================================
// Dino Besar (14x14)
const unsigned char dino_bmp[] PROGMEM = {
  0x01, 0xe0, 0x01, 0xe0, 0x03, 0xe0, 0x03, 0xe0, 0x07, 0xf0, 0x0f, 0xf0, 0x1f, 0xf8, 0x1f, 0xfc, 
  0x1f, 0xfc, 0x07, 0xe0, 0x07, 0xe0, 0x04, 0x20, 0x06, 0x30, 0x02, 0x20
};
const unsigned char bird_bmp[] PROGMEM = { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 }; 
const unsigned char cactus_bmp[] PROGMEM = { 0x18, 0x18, 0x3c, 0x3c, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18 }; 

// ================================================================
// 5. SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(BUZZ_PIN, OUTPUT); digitalWrite(BUZZ_PIN, LOW);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) { for(;;); }
  
  pref.begin("ng-os", true); 
  hsSnake = pref.getInt("hs_snake", 0); hsPong = pref.getInt("hs_pong", 0);
  hsDoom = pref.getInt("hs_doom", 0); hsPacman = pref.getInt("hs_pacman", 0);
  hsDino = pref.getInt("hs_dino", 0); hsRace = pref.getInt("hs_race", 0);
  pref.end();

  currentState = STATE_SLEEP;
}

// ================================================================
// 6. HELPER FUNCTIONS
// ================================================================
void beep(int duration) { digitalWrite(BUZZ_PIN, HIGH); delay(duration); digitalWrite(BUZZ_PIN, LOW); }

void saveHighScore(int gID, int score) {
  pref.begin("ng-os", false); 
  if(gID==1 && score > hsSnake) { hsSnake=score; pref.putInt("hs_snake", hsSnake); }
  if(gID==2 && score > hsPong) { hsPong=score; pref.putInt("hs_pong", hsPong); }
  if(gID==3 && score > hsDoom) { hsDoom=score; pref.putInt("hs_doom", hsDoom); }
  if(gID==4 && score > hsPacman) { hsPacman=score; pref.putInt("hs_pacman", hsPacman); }
  if(gID==5 && score > hsDino) { hsDino=score; pref.putInt("hs_dino", hsDino); }
  if(gID==6 && score > hsRace) { hsRace=score; pref.putInt("hs_race", hsRace); }
  pref.end();
}

void resetAllHighscores() {
  pref.begin("ng-os", false);
  pref.clear(); // Hapus semua data
  pref.end();
  hsSnake=0; hsPong=0; hsDoom=0; hsPacman=0; hsDino=0; hsRace=0;
}

void refreshTimeDisplay() {
  if(!timeSynced) return; 
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)){ sprintf(timeString, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min); }
}

// CHEAT SYSTEM LOGIC
void checkCheats() {
  // Cek Durasi GodMode
  if(godMode && millis() - godModeStart > 30000) {
    godMode = false;
    beep(50); delay(50); beep(50); // Nada habis
  }

  // Cek Input Cheat
  if(digitalRead(SW_PIN) == LOW) {
    if(btnPressStart == 0) btnPressStart = millis();
    
    // Cheat 2: God Mode (Tahan + Bawah)
    if(analogRead(VRY_PIN) > 3500 && millis() - btnPressStart > 500) {
      godMode = true;
      godModeStart = millis();
      display.invertDisplay(true); beep(200); delay(100); display.invertDisplay(false);
      btnPressStart = millis() + 5000; // Reset timer biar gak nambah skor juga
    }
    
    // Cheat 1: Score +10 (Tahan 3 Detik)
    if(millis() - btnPressStart > 3000) {
      currentScore += 10;
      display.invertDisplay(true); beep(50); display.invertDisplay(false);
      btnPressStart = millis(); // Reset timer agar nambah per 3 detik
    }
  } else {
    btnPressStart = 0;
  }
}

// ================================================================
// 7. MAIN LOOP
// ================================================================
void loop() {
  switch (currentState) {
    case STATE_SLEEP:      runSleep(); break;
    case STATE_EYES:       runEyes(); break;
    case STATE_EYE_MENU:   runEyeMenu(); break;       
    case STATE_READ_SELECT:runReadSelect(); break;    
    case STATE_READING:    runReading(); break;       
    case STATE_BOOT:       runBoot(); break;
    case STATE_MENU:       runMenu(); break;
    
    // Games
    case GAME_SNAKE:       checkCheats(); runSnake(); break;
    case GAME_PONG:        checkCheats(); runPong(); break;
    case GAME_DOOM:        checkCheats(); runDoom(); break;
    case GAME_PACMAN:      checkCheats(); runPacman(); break;
    case GAME_DINO:        checkCheats(); runDino(); break;
    case GAME_RACE:        checkCheats(); runRace(); break;
    
    case STATE_SYNC_TIME:  runSyncTime(); break;
    case STATE_HIGHSCORE:  runHighScoreList(); break;
    case STATE_RESET_CONFIRM: runResetConfirm(); break; // New State
    case STATE_UPDATE_OS:  runUpdateOS(); break;
    case STATE_GAMEOVER:   runGameOver(); break;
    case STATE_INFO:       runInfo(); break;
  }
}

// ================================================================
// 8. EYES & INTERACTION
// ================================================================
void runSleep() {
  refreshTimeDisplay(); 
  display.clearDisplay();
  display.fillRect(cXL-12, cY, 24, 3, WHITE); display.fillRect(cXR-12, cY, 24, 3, WHITE);
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
  if(millis() - lastEmotionChange > 10000) {
    eyeEmotion++; if(eyeEmotion > 2) eyeEmotion = 0; lastEmotionChange = millis();
  }
  int xVal = analogRead(VRX_PIN); int yVal = analogRead(VRY_PIN);
  int offX = map(xVal, 0, 4095, -40, 40); int offY = map(yVal, 0, 4095, -15, 15);

  if (millis() - lastBlink > blinkInterval) {
    isBlinking = true; 
    display.fillRect(cXL-12+offX, cY+offY, 24, 3, WHITE); display.fillRect(cXR-12+offX, cY+offY, 24, 3, WHITE);
    display.display(); delay(150); isBlinking = false; lastBlink = millis(); blinkInterval = random(2000, 5000); return; 
  }
  int lx = cXL-eyeW/2+offX; int rx = cXR-eyeW/2+offX; int ty = cY-eyeH/2+offY;
  display.fillRoundRect(lx, ty, eyeW, eyeH, 8, WHITE); display.fillRoundRect(rx, ty, eyeW, eyeH, 8, WHITE);

  if(eyeEmotion == 1) { 
    display.fillTriangle(lx, ty, lx+eyeW, ty, lx+eyeW, ty+15, BLACK);
    display.fillTriangle(rx, ty, rx+eyeW, ty, rx, ty+15, BLACK);
  } else if(eyeEmotion == 2) { 
    display.fillTriangle(lx, ty, lx+eyeW, ty, lx, ty+15, BLACK);
    display.fillTriangle(rx, ty, rx+eyeW, ty, rx+eyeW, ty+15, BLACK);
  }
  display.display();
  if (digitalRead(SW_PIN) == LOW) { beep(100); currentState = STATE_EYE_MENU; eyeMenuIndex=0; delay(500); }
}

void runEyeMenu() {
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(30, 0); display.print("QUICK MENU");
  display.drawLine(0, 8, 128, 8, WHITE);
  for(int i=0; i<3; i++) {
    if(i == eyeMenuIndex) display.setCursor(10, 20+(i*12)); else display.setCursor(20, 20+(i*12));
    if(i == eyeMenuIndex) display.print("> "); display.println(eyeMenuItems[i]);
  }
  display.display();
  int y = analogRead(VRY_PIN);
  if(y > 3000) { eyeMenuIndex++; if(eyeMenuIndex>2) eyeMenuIndex=0; delay(200); }
  if(y < 1000) { eyeMenuIndex--; if(eyeMenuIndex<0) eyeMenuIndex=2; delay(200); }

  if(digitalRead(SW_PIN) == LOW) {
    beep(50);
    if(eyeMenuIndex == 0) { currentState = STATE_BOOT; } 
    else if(eyeMenuIndex == 1) { currentState = STATE_READ_SELECT; } 
    else if(eyeMenuIndex == 2) { currentState = STATE_EYES; } 
    delay(500);
  }
}

void runReadSelect() {
  display.clearDisplay(); display.setCursor(0,0); display.println("PILIH BACAAN:");
  display.drawLine(0,8,128,8,WHITE);
  for(int i=0; i<4; i++) { 
    if(i==readCatIndex) display.setCursor(0, 20+(i*10)); else display.setCursor(10, 20+(i*10));
    if(i==readCatIndex) display.print(">"); display.println(jokeTitle[i]);
  }
  display.display();
  int y = analogRead(VRY_PIN);
  if(y > 3000) { readCatIndex++; if(readCatIndex>3) readCatIndex=0; delay(200); }
  if(y < 1000) { readCatIndex--; if(readCatIndex<0) readCatIndex=3; delay(200); }
  
  if(digitalRead(SW_PIN) == LOW) { 
    if(readCatIndex == 3) { currentState = STATE_EYE_MENU; } 
    else { scrollY = 0; currentState = STATE_READING; }
    delay(300); 
  }
}

void runReading() {
  display.clearDisplay();
  display.setCursor(0, 0 - scrollY); 
  display.println(jokesContent[readCatIndex]);
  display.drawRect(124, 0, 4, 64, WHITE);
  int barY = map(scrollY, 0, 100, 0, 50); 
  display.fillRect(126, barY, 2, 5, WHITE);
  display.display();
  int y = analogRead(VRY_PIN);
  if(y > 3000) { scrollY+=2; } 
  if(y < 1000 && scrollY > 0) { scrollY-=2; } 
  if(digitalRead(SW_PIN) == LOW) { currentState = STATE_READ_SELECT; delay(300); }
}

// ================================================================
// 9. MENU & SYSTEM
// ================================================================
void runBoot() {
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(15, 10); display.println("NewGame Pocket OS");
  display.setTextSize(2); display.setCursor(40, 25); display.println("V0.6");
  display.setTextSize(1); display.setCursor(42, 45); display.println("by rion");
  display.display(); beep(100); delay(50); beep(200); delay(1500);
  currentState = STATE_MENU;
}

void runMenu() {
  refreshTimeDisplay(); 
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(0, 0); display.print("MENU");
  display.setCursor(95, 0); display.print(timeString);
  display.drawLine(0, 8, 128, 8, WHITE);
  if(menuIndex >= menuOffset + menuVisible) menuOffset = menuIndex - menuVisible + 1;
  if(menuIndex < menuOffset) menuOffset = menuIndex;
  for (int i = 0; i < menuVisible; i++) {
    int itemIdx = menuOffset + i;
    if(itemIdx >= menuTotal) break;
    int yPos = 10 + (i * 10);
    if (itemIdx == menuIndex) { display.setCursor(2, yPos); display.print(">"); }
    else { display.setCursor(2, yPos); display.print(" "); }
    display.setCursor(12, yPos); display.println(menuItems[itemIdx]);
  }
  display.display();
  int yVal = analogRead(VRY_PIN);
  if (yVal > 3000) { menuIndex++; if(menuIndex >= menuTotal) menuIndex = 0; beep(10); delay(150); }
  if (yVal < 1000) { menuIndex--; if(menuIndex < 0) menuIndex = menuTotal - 1; beep(10); delay(150); }
  if (digitalRead(SW_PIN) == LOW) {
    delay(300); currentScore = 0; godMode = false; // Reset GodMode saat mulai game
    if (menuIndex == 0) { gameID=1; currentState = GAME_SNAKE; setupSnake(); }
    else if (menuIndex == 1) { gameID=2; currentState = GAME_PONG; setupPong(); }
    else if (menuIndex == 2) { gameID=3; currentState = GAME_DOOM; setupDoom(); }
    else if (menuIndex == 3) { gameID=4; currentState = GAME_PACMAN; setupPacman(); }
    else if (menuIndex == 4) { gameID=5; currentState = GAME_DINO; setupDino(); }
    else if (menuIndex == 5) { gameID=6; currentState = GAME_RACE; setupRace(); }
    else if (menuIndex == 6) { currentState = STATE_SYNC_TIME; }
    else if (menuIndex == 7) { currentState = STATE_HIGHSCORE; }
    else if (menuIndex == 8) { currentState = STATE_INFO; }
    else if (menuIndex == 9) { setupUpdate(); currentState = STATE_UPDATE_OS; }
    else if (menuIndex == 10) { currentState = STATE_SLEEP; }
  }
}

// ================================================================
// 10. GAME: SNAKE
// ================================================================
int sx[100], sy[100], slen, fX, fY, sDx, sDy;
void spawnFoodSafe() {
  bool valid = false;
  while(!valid) {
    fX = random(0, 32); fY = random(0, 16); valid = true;
    for(int i=0; i<slen; i++) if(sx[i] == fX && sy[i] == fY) valid = false;
    if(fX < 8 && fY < 3) valid = false; 
  }
}
void setupSnake() { slen=3; sx[0]=10; sy[0]=10; sDx=1; sDy=0; spawnFoodSafe(); }
void runSnake() {
  int x = analogRead(VRX_PIN); int y = analogRead(VRY_PIN);
  if(x>3000 && sDx==0){sDx=1;sDy=0;} if(x<1000 && sDx==0){sDx=-1;sDy=0;}
  if(y>3000 && sDy==0){sDx=0;sDy=1;} if(y<1000 && sDy==0){sDx=0;sDy=-1;}
  for(int i=slen-1; i>0; i--){sx[i]=sx[i-1]; sy[i]=sy[i-1];} sx[0]+=sDx; sy[0]+=sDy;
  if(sx[0]>31) sx[0]=0; if(sx[0]<0) sx[0]=31; if(sy[0]>15) sy[0]=0; if(sy[0]<0) sy[0]=15;
  
  if(!godMode) {
     for(int i=1; i<slen; i++) if(sx[0]==sx[i] && sy[0]==sy[i]) currentState=STATE_GAMEOVER;
  }
  
  if(sx[0]==fX && sy[0]==fY){ currentScore++; slen++; beep(30); spawnFoodSafe(); }
  display.clearDisplay(); display.setTextSize(1); display.setCursor(0,0); display.print(currentScore);
  if(godMode) { display.setCursor(40,0); display.print("GOD MODE"); }
  display.fillRect(fX*4, fY*4, 4, 4, WHITE);
  for(int i=0; i<slen; i++) display.fillRect(sx[i]*4, sy[i]*4, 4, 4, WHITE);
  display.display(); delay(80);
}

// ================================================================
// 11. GAME: PONG
// ================================================================
int paddleY, bX, bY, bDX, bDY, cpuY;
void setupPong() { paddleY=22; bX=64; bY=32; bDX=2; bDY=2; cpuY=22; } 
void runPong() {
  int yVal = analogRead(VRY_PIN);
  if(yVal>3000 && paddleY<48) paddleY+=3; if(yVal<1000 && paddleY>0) paddleY-=3;
  bX+=bDX; bY+=bDY; if(bY<=0 || bY>=60) bDY*=-1;
  if(bX>64) { if(bY>cpuY+8) cpuY+=2; else cpuY-=2; }
  
  if(bX<=4 && bY>=paddleY && bY<=paddleY+16) { bDX=2; bX=5; currentScore++; beep(20); }
  if(bX>=120 && bY>=cpuY && bY<=cpuY+16) { bDX=-2; bX=119; }
  if(!godMode && (bX<0 || bX>128)) currentState=STATE_GAMEOVER;
  if(godMode && (bX<0 || bX>128)) { bDX*=-1; } // GodMode pantul balik

  display.clearDisplay(); display.setCursor(60,0); display.print(currentScore);
  if(godMode) { display.setCursor(80,0); display.print("GOD"); }
  display.fillRect(0, paddleY, 4, 16, WHITE); display.fillRect(124, cpuY, 4, 16, WHITE);
  display.fillCircle(bX, bY, 2, WHITE); display.display(); delay(15);
}

// ================================================================
// 12. GAME: DOOM 3D
// ================================================================
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
double doomX, doomY, dX, dY, plX, plY, eX, eY; bool eAlive;
void setupDoom() { doomX=3.0; doomY=3.0; dX=1.0; dY=0.0; plX=0.0; plY=0.66; eAlive=true; eX=10.5; eY=10.5; }
void runDoom() {
  display.clearDisplay();
  int x=analogRead(VRX_PIN); int y=analogRead(VRY_PIN); double mS=0.15, rS=0.1;
  if(x<1000){double oldD=dX; dX=dX*cos(rS)-dY*sin(rS); dY=oldD*sin(rS)+dY*cos(rS); double oldP=plX; plX=plX*cos(rS)-plY*sin(rS); plY=oldP*sin(rS)+plY*cos(rS);}
  if(x>3000){double oldD=dX; dX=dX*cos(-rS)-dY*sin(-rS); dY=oldD*sin(-rS)+dY*cos(-rS); double oldP=plX; plX=plX*cos(-rS)-plY*sin(-rS); plY=oldP*sin(-rS)+plY*cos(-rS);}
  if(y<1000){if(wm[int(doomX+dX*mS)][int(doomY)]==0)doomX+=dX*mS; if(wm[int(doomX)][int(doomY+dY*mS)]==0)doomY+=dY*mS;}
  if(y>3000){if(wm[int(doomX-dX*mS)][int(doomY)]==0)doomX-=dX*mS; if(wm[int(doomX)][int(doomY-dY*mS)]==0)doomY-=dY*mS;}
  for(int i=0; i<128; i+=2) {
    double cX=2*i/(double)128-1; double rDX=dX+plX*cX; double rDY=dY+plY*cX;
    int mX=int(doomX), mY=int(doomY); double sDX, sDY;
    double dDX=(rDX==0)?1e30:abs(1/rDX); double dDY=(rDY==0)?1e30:abs(1/rDY);
    double pwd; int stX, stY, hit=0, sd;
    if(rDX<0){stX=-1;sDX=(doomX-mX)*dDX;} else{stX=1;sDX=(mX+1.0-doomX)*dDX;}
    if(rDY<0){stY=-1;sDY=(doomY-mY)*dDY;} else{stY=1;sDY=(mY+1.0-doomY)*dDY;}
    while(hit==0){if(sDX<sDY){sDX+=dDX;mX+=stX;sd=0;}else{sDY+=dDY;mY+=stY;sd=1;} if(wm[mX][mY]>0)hit=1;}
    if(sd==0)pwd=(sDX-dDX); else pwd=(sDY-dDY);
    int h=int(64/pwd); int ds=-h/2+32; if(ds<0)ds=0; int de=h/2+32; if(de>=64)de=63;
    if(sd==1){display.drawFastVLine(i, ds, de-ds, WHITE);display.drawFastVLine(i+1, ds, de-ds, WHITE);}
    else{for(int k=ds;k<de;k+=2){display.drawPixel(i,k,WHITE);display.drawPixel(i+1,k,WHITE);}}
  }
  double dE = sqrt(pow(doomX-eX, 2) + pow(doomY-eY, 2));
  if (eAlive && dE < 5.0) {
     int eS = 50 / dE; int scrX = 64; 
     display.fillCircle(scrX, 32, eS/2, WHITE); display.fillCircle(scrX, 32, eS/4, BLACK); 
     eX+=(doomX-eX)*0.03; eY+=(doomY-eY)*0.03; 
     if(!godMode && dE < 0.5) currentState=STATE_GAMEOVER;
  }
  display.fillTriangle(48,64,80,64,64,45,WHITE);
  if(godMode) { display.setCursor(0,0); display.print("GOD MODE"); } else { display.setCursor(0,0); display.print(currentScore); }
  if(digitalRead(SW_PIN)==LOW) {
    display.invertDisplay(true); beep(20);
    // Fix Respawn Bug: Make sure new position is far enough
    if(eAlive && dE < 3.5) { 
      currentScore++; eAlive=false; beep(100); 
      do {
         eX=int(doomX)+random(-4,4); eY=int(doomY)+random(-4,4); 
         if(eX<1)eX=1; if(eY<1)eY=1;
      } while(sqrt(pow(doomX-eX,2) + pow(doomY-eY,2)) < 2.0); // Ensure not too close
      eAlive=true;
    }
    delay(50); display.invertDisplay(false);
  }
  display.display();
}

// ================================================================
// 13. GAME: PACMAN
// ================================================================
byte pmMap[8][16] = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
  {1,0,1,1,1,0,1,0,1,1,1,0,1,1,0,1}, {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,1,1,0,1,1,1,1,0,1,0,1,1,0,1}, {1,0,0,0,0,0,0,2,0,0,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} 
};
int pacX, pacY, ghostX, ghostY, pelletsLeft, pacDir;
unsigned long lastPacMove = 0, lastGhostMove = 0; 

void setupPacman() {
  pacX=1; pacY=1; ghostX=14; ghostY=5; pelletsLeft=0;
  for(int y=0;y<7;y++) for(int x=0;x<16;x++) {
    if(pmMap[y][x]==2) pmMap[y][x]=0;
    if(pmMap[y][x]==0) pelletsLeft++;
  }
}

void runPacman() {
  if(millis() - lastPacMove > 150) {
    int x=analogRead(VRX_PIN); int y=analogRead(VRY_PIN);
    int nextX=pacX, nextY=pacY;
    if(x<1000) { nextX--; pacDir=1; } if(x>3000) { nextX++; pacDir=0; }
    if(y<1000) { nextY--; pacDir=2; } if(y>3000) { nextY++; pacDir=3; }
    if(pmMap[nextY][nextX] != 1) { pacX=nextX; pacY=nextY; }
    if(pmMap[pacY][pacX] == 0) { pmMap[pacY][pacX] = 2; currentScore++; pelletsLeft--; beep(10); }
    lastPacMove = millis();
  }
  // Slower Ghost (350ms)
  if(millis() - lastGhostMove > 350) {
    int gNextX=ghostX, gNextY=ghostY;
    if(ghostX < pacX) gNextX++; else if(ghostX > pacX) gNextX--;
    if(pmMap[ghostY][gNextX] == 1) gNextX = ghostX; 
    if(ghostY < pacY) gNextY++; else if(ghostY > pacY) gNextY--;
    if(pmMap[gNextY][gNextX] == 1) gNextY = ghostY; 
    ghostX = gNextX; ghostY = gNextY;
    lastGhostMove = millis();
  }
  if(!godMode && pacX==ghostX && pacY==ghostY) currentState=STATE_GAMEOVER;
  if(pelletsLeft <= 0) { currentScore+=5; setupPacman(); }

  display.clearDisplay();
  for(int y=0; y<7; y++) {
    for(int x=0; x<16; x++) {
      if(pmMap[y][x]==1) display.fillRect(x*8, y*8, 8, 8, WHITE);
      if(pmMap[y][x]==0) display.fillCircle(x*8+4, y*8+4, 1, WHITE);
    }
  }
  int px = pacX*8+4; int py = pacY*8+4;
  display.fillCircle(px, py, 3, WHITE); 
  if(pacDir==0) display.fillTriangle(px, py, px+4, py-2, px+4, py+2, BLACK); 
  if(pacDir==1) display.fillTriangle(px, py, px-4, py-2, px-4, py+2, BLACK); 
  if(pacDir==2) display.fillTriangle(px, py, px-2, py-4, px+2, py-4, BLACK); 
  if(pacDir==3) display.fillTriangle(px, py, px-2, py+4, px+2, py+4, BLACK); 
  display.fillTriangle(ghostX*8, ghostY*8+8, ghostX*8+4, ghostY*8, ghostX*8+8, ghostY*8+8, WHITE);
  display.setCursor(0, 56); display.print("Score:"); display.print(currentScore);
  if(godMode) display.print(" GOD");
  display.display();
}

// ================================================================
// 14. GAME: DINO RUN (FIXED SIZE & JUMP)
// ================================================================
int dinoY, dinoVel; 
int obstacleX, obstacleType; 
bool isDucking;

void setupDino() { 
  dinoY=58; dinoVel=0; obstacleX=128; obstacleType=0; // Ground at 58
}

void runDino() {
  int y=analogRead(VRY_PIN);
  bool jump = (y<1000) || (digitalRead(SW_PIN)==LOW);
  bool duck = (y>3000);

  // Fisika Lompat (Lebih tinggi)
  if(jump && dinoY>=58) { dinoVel = -8; beep(30); } 
  dinoY += dinoVel;
  if(dinoY < 58) dinoVel += 1; // Gravity
  else { dinoY = 58; dinoVel = 0; } 

  isDucking = duck;
  obstacleX -= 3; 
  if(obstacleX < -15) { 
    obstacleX = 128; currentScore++; beep(10); 
    if(random(0,10) > 6) obstacleType = 1; else obstacleType = 0;
  }

  // Hitbox
  if(!godMode) {
    if(obstacleType == 0 && obstacleX < 20 && obstacleX > 5 && dinoY > 50) currentState=STATE_GAMEOVER;
    if(obstacleType == 1 && obstacleX < 20 && obstacleX > 5 && !isDucking) currentState=STATE_GAMEOVER;
  }

  display.clearDisplay();
  display.drawFastHLine(0, 62, 128, WHITE); // Tanah Y=62
  display.setCursor(100,0); display.print(currentScore);
  if(godMode) { display.setCursor(0,0); display.print("GOD"); }
  
  // Dino (14x14 Bitmap) - Bigger
  if(!isDucking) display.drawBitmap(10, dinoY-14, dino_bmp, 14, 14, WHITE);
  else display.fillRect(10, dinoY-7, 14, 7, WHITE); // Ducking

  if(obstacleType == 0) display.drawBitmap(obstacleX, 52, cactus_bmp, 8, 10, WHITE);
  else display.drawBitmap(obstacleX, 42, bird_bmp, 8, 8, WHITE); 
  
  display.display();
}

// ================================================================
// 15. GAME: TURBO RACE (FIXED SPAWN)
// ================================================================
int carX;
int obsX, obsZ; 
void setupRace() { carX=64; obsX=0; obsZ=0; } 

void runRace() {
  int x=analogRead(VRX_PIN);
  if(x<1000 && carX>20) carX-=3; 
  if(x>3000 && carX<108) carX+=3;

  obsZ += 2; 
  if(obsZ > 64) { 
    obsZ=0; 
    // Random menyebar penuh dari kiri(-100) sampai kanan(100)
    obsX=random(-100, 100); 
    currentScore++; beep(10);
  }

  // Perspective Logic
  int roadWidthAtZ = map(obsZ, 0, 64, 8, 128);
  int drawObsX = 64 + (obsX * roadWidthAtZ / 200); 
  int drawObsY = 32 + (obsZ / 2); 
  int obsSize = map(obsZ, 0, 64, 2, 14);

  if(!godMode && obsZ > 55 && abs(carX - drawObsX) < (obsSize/2 + 6)) currentState=STATE_GAMEOVER;

  display.clearDisplay();
  display.setCursor(0,0); display.print(currentScore);
  if(godMode) { display.setCursor(40,0); display.print("GOD"); }
  
  display.drawLine(0, 32, 128, 32, WHITE);
  display.drawLine(60, 32, 0, 64, WHITE);   
  display.drawLine(68, 32, 128, 64, WHITE); 
  
  display.fillRect(drawObsX - obsSize/2, drawObsY - obsSize/2, obsSize, obsSize, WHITE);
  display.fillRect(carX-6, 54, 12, 8, WHITE); 
  display.fillRect(carX-4, 56, 8, 2, BLACK);   

  display.display(); delay(20); 
}

// ================================================================
// 16. UTILITIES
// ================================================================
// RESET CONFIRMATION
void runResetConfirm() {
  display.clearDisplay();
  display.setCursor(20, 20); display.println("YAKIN HAPUS?");
  display.setCursor(50, 35); display.print(resetStep); display.print("/3");
  display.setCursor(10, 50); display.print("< TIDAK    YA >");
  display.display();

  int x = analogRead(VRX_PIN);
  if(x > 3000) { // YA (Kanan)
    beep(50); resetStep++; delay(300);
    if(resetStep > 3) {
      resetAllHighscores();
      display.clearDisplay(); display.setCursor(30,30); display.print("DELETED!"); display.display();
      delay(1000); currentState = STATE_HIGHSCORE;
    }
  }
  if(x < 1000) { // TIDAK (Kiri)
    beep(50); resetStep=0; currentState = STATE_HIGHSCORE; delay(300);
  }
}

void runSyncTime() {
  display.clearDisplay(); display.setCursor(0,0); display.println("Syncing Time..."); display.display();
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start < 8000) { delay(500); display.print("."); display.display(); }

  if(WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    for(int i=0; i<5; i++) {
      struct tm timeinfo;
      if(getLocalTime(&timeinfo)){
        timeSynced = true; refreshTimeDisplay();
        display.clearDisplay(); display.println("Time Updated!"); display.println(timeString); display.display();
        WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
        delay(2000); currentState = STATE_MENU; return;
      }
      delay(500);
    }
    display.clearDisplay(); display.println("NTP Timeout!"); display.display();
  } else {
    display.clearDisplay(); display.println("WiFi Fail!"); display.display();
  }
  delay(2000); currentState = STATE_MENU;
}

void runHighScoreList() {
  display.clearDisplay();
  display.setCursor(0,0); display.println("HIGHSCORES");
  display.drawLine(0,8,128,8,WHITE);
  display.setCursor(0,12); display.print("Snk: "); display.print(hsSnake);
  display.setCursor(64,12); display.print("Png: "); display.print(hsPong);
  display.setCursor(0,22); display.print("Doom: "); display.print(hsDoom);
  display.setCursor(64,22); display.print("Pac: "); display.print(hsPacman);
  display.setCursor(0,32); display.print("Din: "); display.print(hsDino);
  display.setCursor(64,32); display.print("Race: "); display.print(hsRace);
  // Tombol Reset
  display.setCursor(25, 48); display.print("[ RESET DATA ]");
  
  display.display();
  
  // Logic Navigasi ke Reset
  if(digitalRead(SW_PIN)==LOW) { delay(500); currentState=STATE_MENU; }
  
  // Shortcut Reset (Geser Bawah)
  int y = analogRead(VRY_PIN);
  if(y > 3000) { beep(50); resetStep=1; currentState = STATE_RESET_CONFIRM; delay(500); }
}

void setupUpdate() {
  display.clearDisplay(); display.println("Connect WiFi..."); display.display();
  unsigned long s = millis();
  WiFi.begin(ssid, password);
  while(WiFi.status()!=WL_CONNECTED && millis()-s < 8000) { delay(500); display.print("."); display.display(); }
  if(WiFi.status()!=WL_CONNECTED) { display.clearDisplay(); display.println("WiFi Fail!"); display.display(); delay(1000); currentState=STATE_MENU; return; }
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
  display.clearDisplay(); display.setCursor(0,0); display.println("UPDATE GAME");
  display.drawLine(0,10,128,10,WHITE);
  display.setCursor(0,20); display.println(WiFi.localIP());
  display.setCursor(0,50); display.println("[Click to Exit]"); display.display();
  if(digitalRead(SW_PIN)==LOW) { WiFi.disconnect(true); currentState=STATE_MENU; while(digitalRead(SW_PIN)==LOW) delay(10); }
}

void runGameOver() {
  saveHighScore(gameID, currentScore); 
  display.clearDisplay(); display.setTextSize(2); display.setCursor(10,20); display.println("GAME OVER");
  display.setTextSize(1); display.setCursor(40,40); display.print("Score: "); display.print(currentScore); display.display();
  if(digitalRead(SW_PIN)==LOW) { delay(500); currentState=STATE_MENU; }
}

void runInfo() {
  display.clearDisplay(); display.setCursor(0,0); display.println("INFO SYSTEM");
  display.println("- Cheat Enabled"); display.println("- Secure Reset"); display.println("- Final Fixes");
  display.display(); if(digitalRead(SW_PIN)==LOW) { delay(500); currentState=STATE_MENU; }
}