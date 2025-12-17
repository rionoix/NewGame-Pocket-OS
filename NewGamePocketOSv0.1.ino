#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- KONFIGURASI HARDWARE ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// PIN DEFINITION (Sesuai wiring kita)
#define VRX_PIN  34
#define VRY_PIN  35
#define SW_PIN   32
#define BUZZ_PIN 25 

// --- KONFIGURASI GAME SNAKE ---
#define SNAKE_SIZE 4 
#define MAX_SNAKE_LENGTH 100
const int widthBox  = SCREEN_WIDTH / SNAKE_SIZE;
const int heightBox = SCREEN_HEIGHT / SNAKE_SIZE;

// --- STATE MANAGEMENT ---
enum SystemState { MENU, GAME_SNAKE, INFO_SCREEN, GAME_OVER };
SystemState currentState = MENU;

// --- VARIABEL GLOBAL ---
int snakeX[MAX_SNAKE_LENGTH];
int snakeY[MAX_SNAKE_LENGTH];
int snakeLength = 3;
int dirX = 1; 
int dirY = 0; 
int foodX, foodY;
int score = 0;

// Variabel Menu
int menuIndex = 0;
const int menuTotal = 3;
// Daftar Menu
String menuItems[] = {"1. Play Snake", "2. High Score", "3. About System"};

// --- FUNGSI BANTUAN TEXT CENTER ---
void centerText(String text, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.println(text);
}

// --- SETUP (BOOTING) ---
void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(BUZZ_PIN, OUTPUT);
  digitalWrite(BUZZ_PIN, LOW);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    for(;;); // Gagal init OLED
  }

  // --- TAMPILAN BOOTING (Sesuai Request) ---
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  // Menggunakan fungsi centerText agar rapi di tengah
  centerText("NewGame Pocket OS", 10);
  
  display.setTextSize(2); // V0.1 agak besar sedikit biar keren
  centerText("V0.1", 25);
  
  display.setTextSize(1);
  centerText("by rion", 45);
  
  // Efek Loading titik-titik
  display.setCursor(35, 55);
  display.print("loading");
  display.display();
  
  delay(500); display.print("."); display.display(); beep(50);
  delay(500); display.print("."); display.display(); beep(50);
  delay(500); display.print("."); display.display(); beep(100);
  
  delay(1000); // Tahan sebentar sebelum masuk menu
}

void loop() {
  switch (currentState) {
    case MENU:
      runMenu();
      break;
    case GAME_SNAKE:
      runSnakeGame();
      break;
    case INFO_SCREEN:
      runInfoScreen();
      break;
    case GAME_OVER:
      runGameOver();
      break;
  }
}

// --- FUNGSI SUARA ---
void beep(int duration) {
  digitalWrite(BUZZ_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZ_PIN, LOW);
}

// --- SISTEM MENU ---
void runMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  // Header Menu
  display.drawFastHLine(0, 10, 128, WHITE);
  centerText("MAIN MENU", 0);

  // List Menu
  for (int i = 0; i < menuTotal; i++) {
    int yPos = 20 + (i * 12); // Jarak antar baris
    if (i == menuIndex) {
      display.setCursor(5, yPos);
      display.print("> " + menuItems[i]); // Kursor aktif
    } else {
      display.setCursor(15, yPos);
      display.print(menuItems[i]);
    }
  }

  display.display();

  // Navigasi Joystick
  int yVal = analogRead(VRY_PIN);
  int swVal = digitalRead(SW_PIN);

  if (yVal > 3000) { // Bawah
    menuIndex++;
    if (menuIndex >= menuTotal) menuIndex = 0;
    beep(20); delay(200);
  }
  else if (yVal < 1000) { // Atas
    menuIndex--;
    if (menuIndex < 0) menuIndex = menuTotal - 1;
    beep(20); delay(200);
  }

  // Eksekusi Pilihan
  if (swVal == LOW) {
    beep(100);
    if (menuIndex == 0) {
      resetSnake();
      currentState = GAME_SNAKE;
    }
    else if (menuIndex == 1) {
      // Slot High Score (Belum ada fitur save, tampilkan dummy dulu)
      // Nanti disini kita load dari Preferences
    }
    else if (menuIndex == 2) {
      currentState = INFO_SCREEN;
    }
    delay(300);
  }
}

// --- LAYAR INFO (ABOUT) ---
void runInfoScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  centerText("ABOUT SYSTEM", 0);
  display.drawLine(0, 10, 128, 10, WHITE);
  
  display.setCursor(0, 20);
  display.println("Dev: Rion");
  display.println("Org: NewGame");
  display.println("Chip: ESP32");
  display.println("\n[Klik utk Kembali]");
  display.display();

  if (digitalRead(SW_PIN) == LOW) {
    beep(100);
    currentState = MENU;
    delay(500);
  }
}

// --- ENGINE GAME SNAKE ---
void resetSnake() {
  snakeLength = 3;
  score = 0;
  dirX = 1; dirY = 0; 
  snakeX[0] = 5; snakeY[0] = 5; 
  generateFood();
}

void generateFood() {
  foodX = random(0, widthBox);
  foodY = random(0, heightBox);
}

void runSnakeGame() {
  int xVal = analogRead(VRX_PIN);
  int yVal = analogRead(VRY_PIN);

  // Logika Kontrol
  if (xVal > 3000 && dirX == 0) { dirX = 1; dirY = 0; }
  else if (xVal < 1000 && dirX == 0) { dirX = -1; dirY = 0; }
  else if (yVal > 3000 && dirY == 0) { dirX = 0; dirY = 1; }
  else if (yVal < 1000 && dirY == 0) { dirX = 0; dirY = -1; }

  // Update Badan
  for (int i = snakeLength - 1; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }
  snakeX[0] += dirX;
  snakeY[0] += dirY;

  // Tembus Tembok
  if (snakeX[0] >= widthBox) snakeX[0] = 0;
  if (snakeX[0] < 0) snakeX[0] = widthBox - 1;
  if (snakeY[0] >= heightBox) snakeY[0] = 0;
  if (snakeY[0] < 0) snakeY[0] = heightBox - 1;

  // Cek Tabrakan Badan
  for (int i = 1; i < snakeLength; i++) {
    if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) {
      beep(500); 
      currentState = GAME_OVER;
      return;
    }
  }

  // Cek Makan
  if (snakeX[0] == foodX && snakeY[0] == foodY) {
    score += 10;
    snakeLength++;
    if (snakeLength >= MAX_SNAKE_LENGTH) snakeLength = MAX_SNAKE_LENGTH;
    beep(30); 
    generateFood();
  }

  // Render
  display.clearDisplay();
  display.fillRect(foodX * SNAKE_SIZE, foodY * SNAKE_SIZE, SNAKE_SIZE, SNAKE_SIZE, WHITE);
  for (int i = 0; i < snakeLength; i++) {
    display.fillRect(snakeX[i] * SNAKE_SIZE, snakeY[i] * SNAKE_SIZE, SNAKE_SIZE, SNAKE_SIZE, WHITE);
  }
  display.setCursor(0,0); display.print(score);
  display.display();
  delay(100); 
}

void runGameOver() {
  display.clearDisplay();
  display.setTextSize(2);
  centerText("GAME OVER", 15);
  
  display.setTextSize(1);
  display.setCursor(40, 35);
  display.print("Score: "); display.print(score);
  
  display.setCursor(20, 55);
  display.print("Klik utk Menu");
  display.display();

  if (digitalRead(SW_PIN) == LOW) {
    beep(100);
    delay(300);
    currentState = MENU;
  }
}