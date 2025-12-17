#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- KONFIGURASI HARDWARE ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// PIN DEFINITION
#define VRX_PIN  34
#define VRY_PIN  35
#define SW_PIN   32
#define BUZZ_PIN 25 

// --- KONFIGURASI GAME SNAKE ---
#define SNAKE_SIZE 4 
#define MAX_SNAKE_LENGTH 100
const int widthBox  = SCREEN_WIDTH / SNAKE_SIZE;
const int heightBox = SCREEN_HEIGHT / SNAKE_SIZE;

// --- STATE MANAGEMENT (OS V0.2) ---
// Menambahkan STATE_SLEEP, STATE_ROBOT_EYES, dan STATE_BOOT
enum SystemState { 
  STATE_SLEEP,       // Mode Tidur (- -)
  STATE_ROBOT_EYES,  // Mode Robot (Mata Ellie)
  STATE_BOOT,        // Animasi Loading
  STATE_MENU,        // Menu Utama
  STATE_GAME_SNAKE,  // Game Snake
  STATE_INFO,        // Info Screen
  STATE_GAMEOVER     // Game Over Snake
};

SystemState currentState = STATE_SLEEP;

// --- VARIABEL GLOBAL ---
// Snake
int snakeX[MAX_SNAKE_LENGTH];
int snakeY[MAX_SNAKE_LENGTH];
int snakeLength = 3;
int dirX = 1; int dirY = 0; 
int foodX, foodY;
int score = 0;

// Menu
int menuIndex = 0;
const int menuTotal = 4; // Ditambah menu Exit
String menuItems[] = {"1. Play Snake", "2. High Score", "3. About System", "4. Shutdown"};

// Robot Eyes Variables
unsigned long lastBlinkTime = 0;
int blinkInterval = 3000; // Waktu kedip random
bool isBlinking = false;
unsigned long blinkDuration = 0;

// --- FUNGSI BANTUAN ---
void beep(int duration) {
  digitalWrite(BUZZ_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZ_PIN, LOW);
}

void centerText(String text, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.println(text);
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(BUZZ_PIN, OUTPUT);
  digitalWrite(BUZZ_PIN, LOW);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    for(;;); 
  }

  // Awal nyala langsung masuk mode tidur
  currentState = STATE_SLEEP;
}

// --- LOOP UTAMA (BRAIN) ---
void loop() {
  switch (currentState) {
    case STATE_SLEEP:
      runSleepMode();
      break;
    case STATE_ROBOT_EYES:
      runRobotEyes();
      break;
    case STATE_BOOT:
      runBootSequence(); // Animasi Loading dipindah kesini
      break;
    case STATE_MENU:
      runMenu();
      break;
    case STATE_GAME_SNAKE:
      runSnakeGame();
      break;
    case STATE_INFO:
      runInfoScreen();
      break;
    case STATE_GAMEOVER:
      runGameOver();
      break;
  }
}

// ==========================================
// BAGIAN 1: FITUR ROBOT (New in V0.2)
// ==========================================

// 1. MODE TIDUR (- -)
void runSleepMode() {
  display.clearDisplay();
  
  // Gambar Mata Tidur (Dua garis tebal)
  // Mata Kiri
  display.fillRect(30, 30, 30, 4, WHITE);
  // Mata Kanan
  display.fillRect(68, 30, 30, 4, WHITE);
  
  // Teks kecil di bawah (opsional)
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(55, 55);
  display.print("Zzz...");
  
  display.display();

  // Cek Tombol untuk Bangun
  if (digitalRead(SW_PIN) == LOW) {
    beep(50); delay(100); beep(50); // Suara bangun
    currentState = STATE_ROBOT_EYES;
    lastBlinkTime = millis();
    delay(500); // Debounce
  }
}

// 2. MODE ROBOT INTERAKTIF (Mata Ellie)
void runRobotEyes() {
  display.clearDisplay();

  // --- LOGIKA KEDIP (BLINK) ---
  unsigned long currentMillis = millis();
  
  // Hitung waktu kedip random
  if (currentMillis - lastBlinkTime > blinkInterval) {
    isBlinking = true;
    blinkDuration = currentMillis;
    lastBlinkTime = currentMillis;
    blinkInterval = random(2000, 5000); // Kedip tiap 2-5 detik
  }

  // Cek durasi kedip (150ms menutup mata)
  if (isBlinking && (currentMillis - blinkDuration > 150)) {
    isBlinking = false;
  }

  // --- LOGIKA LIRIK (JOYSTICK LOOK) ---
  int xVal = analogRead(VRX_PIN);
  int yVal = analogRead(VRY_PIN);
  
  // Mapping nilai joystick (0-4095) ke pergeseran pupil (-10 sampai 10 pixel)
  // Inverse Y karena joystick biasanya atas nilai kecil
  int pupilOffsetX = map(xVal, 0, 4095, -12, 12); 
  int pupilOffsetY = map(yVal, 0, 4095, -10, 10);

  // --- RENDER MATA ---
  int eyeRadius = 20;
  int eyeX_Left = 35;
  int eyeX_Right = 93;
  int eyeY = 32;

  if (isBlinking) {
    // Gambar garis (sedang kedip)
    display.fillRect(eyeX_Left - 15, eyeY - 2, 30, 4, WHITE);
    display.fillRect(eyeX_Right - 15, eyeY - 2, 30, 4, WHITE);
  } else {
    // 1. Gambar Bola Mata Putih (Outline atau Filled)
    // Kita pakai Filled Circle tapi nanti ditimpa pupil hitam (Inverse)
    // Agar mata terlihat "menyala", kita gambar lingkaran putih penuh
    display.fillCircle(eyeX_Left, eyeY, eyeRadius, WHITE);
    display.fillCircle(eyeX_Right, eyeY, eyeRadius, WHITE);

    // 2. Gambar Pupil Hitam (Yang bergerak)
    // Pupil digambar dengan warna BLACK agar "melubangi" warna putih
    int pupilSize = 8;
    display.fillCircle(eyeX_Left + pupilOffsetX, eyeY + pupilOffsetY, pupilSize, BLACK);
    display.fillCircle(eyeX_Right + pupilOffsetX, eyeY + pupilOffsetY, pupilSize, BLACK);
  }

  display.display();

  // --- TRANSISI KE OS ---
  if (digitalRead(SW_PIN) == LOW) {
    beep(100);
    currentState = STATE_BOOT;
    delay(500);
  }
}

// 3. ANIMASI LOADING (Boot Sequence)
void runBootSequence() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  centerText("NewGame Pocket OS", 10);
  
  display.setTextSize(2); 
  centerText("V0.2", 25); // Updated Version
  
  display.setTextSize(1);
  centerText("by rion", 45);
  
  display.setCursor(35, 55);
  display.print("loading");
  display.display();
  
  // Efek Loading
  delay(500); display.print("."); display.display(); beep(50);
  delay(500); display.print("."); display.display(); beep(50);
  delay(500); display.print("."); display.display(); beep(100);
  
  delay(1000); 
  currentState = STATE_MENU; // Masuk Menu Utama
}

// ==========================================
// BAGIAN 2: SYSTEM OS (Menu & Game)
// ==========================================

void runMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  display.drawFastHLine(0, 10, 128, WHITE);
  centerText("MAIN MENU", 0);

  for (int i = 0; i < menuTotal; i++) {
    int yPos = 20 + (i * 10); // Jarak lebih rapat karena ada 4 menu
    if (i == menuIndex) {
      display.setCursor(5, yPos);
      display.print("> " + menuItems[i]); 
    } else {
      display.setCursor(15, yPos);
      display.print(menuItems[i]);
    }
  }
  display.display();

  // Navigasi
  int yVal = analogRead(VRY_PIN);
  int swVal = digitalRead(SW_PIN);

  if (yVal > 3000) { 
    menuIndex++; if (menuIndex >= menuTotal) menuIndex = 0;
    beep(20); delay(200);
  } else if (yVal < 1000) { 
    menuIndex--; if (menuIndex < 0) menuIndex = menuTotal - 1;
    beep(20); delay(200);
  }

  // Pilih Menu
  if (swVal == LOW) {
    beep(100);
    if (menuIndex == 0) {
      resetSnake();
      currentState = STATE_GAME_SNAKE;
    } else if (menuIndex == 1) {
      // Placeholder Highscore
    } else if (menuIndex == 2) {
      currentState = STATE_INFO;
    } else if (menuIndex == 3) {
      // EXIT / SHUTDOWN
      display.clearDisplay();
      centerText("SHUTTING DOWN...", 30);
      display.display();
      delay(1000);
      currentState = STATE_SLEEP; // Kembali ke mata tidur
    }
    delay(300);
  }
}

// --- GAME SNAKE (Sama seperti V0.1) ---
void resetSnake() {
  snakeLength = 3; score = 0; dirX = 1; dirY = 0; 
  snakeX[0] = 5; snakeY[0] = 5; generateFood();
}
void generateFood() {
  foodX = random(0, widthBox); foodY = random(0, heightBox);
}
void runSnakeGame() {
  int xVal = analogRead(VRX_PIN); int yVal = analogRead(VRY_PIN);
  if (xVal > 3000 && dirX == 0) { dirX = 1; dirY = 0; }
  else if (xVal < 1000 && dirX == 0) { dirX = -1; dirY = 0; }
  else if (yVal > 3000 && dirY == 0) { dirX = 0; dirY = 1; }
  else if (yVal < 1000 && dirY == 0) { dirX = 0; dirY = -1; }

  for (int i = snakeLength - 1; i > 0; i--) { snakeX[i] = snakeX[i - 1]; snakeY[i] = snakeY[i - 1]; }
  snakeX[0] += dirX; snakeY[0] += dirY;

  if (snakeX[0] >= widthBox) snakeX[0] = 0; if (snakeX[0] < 0) snakeX[0] = widthBox - 1;
  if (snakeY[0] >= heightBox) snakeY[0] = 0; if (snakeY[0] < 0) snakeY[0] = heightBox - 1;

  for (int i = 1; i < snakeLength; i++) {
    if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) { beep(500); currentState = STATE_GAMEOVER; return; }
  }

  if (snakeX[0] == foodX && snakeY[0] == foodY) {
    score += 10; snakeLength++; if (snakeLength >= MAX_SNAKE_LENGTH) snakeLength = MAX_SNAKE_LENGTH; beep(30); generateFood();
  }

  display.clearDisplay();
  display.fillRect(foodX * SNAKE_SIZE, foodY * SNAKE_SIZE, SNAKE_SIZE, SNAKE_SIZE, WHITE);
  for (int i = 0; i < snakeLength; i++) { display.fillRect(snakeX[i] * SNAKE_SIZE, snakeY[i] * SNAKE_SIZE, SNAKE_SIZE, SNAKE_SIZE, WHITE); }
  display.setCursor(0,0); display.print(score);
  display.display();
  delay(100); 
}

void runGameOver() {
  display.clearDisplay();
  display.setTextSize(2); centerText("GAME OVER", 15);
  display.setTextSize(1); display.setCursor(40, 35); display.print("Score: "); display.print(score);
  display.setCursor(20, 55); display.print("Klik utk Menu"); display.display();
  if (digitalRead(SW_PIN) == LOW) { beep(100); delay(300); currentState = STATE_MENU; }
}

void runInfoScreen() {
  display.clearDisplay(); centerText("ABOUT SYSTEM", 0);
  display.drawLine(0, 10, 128, 10, WHITE);
  display.setCursor(0, 20); display.println("Dev: Rion"); display.println("Org: NewGame"); display.println("Ver: 0.2 (Ellie)");
  display.println("\n[Klik utk Kembali]"); display.display();
  if (digitalRead(SW_PIN) == LOW) { beep(100); currentState = STATE_MENU; delay(500); }
}