/*
 * ORBIT OS 2.0
 * ESP32-C3 Handheld
 *
 * OLED: SH1106 128x64, I2C 0x3C
 * SDA: GPIO 6 | SCL: GPIO 7
 *
 * UP: 21 | LEFT: 20 | DOWN: 10
 * RIGHT: 1 | CENTER: 2
 *
 * Libraries:
 * Adafruit GFX
 * Adafruit SH110X
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Preferences.h>
#include <math.h>
#include <string.h>

#define OLED_SDA 6
#define OLED_SCL 7

#define BTN_UP 21
#define BTN_LEFT 20
#define BTN_DOWN 10
#define BTN_RIGHT 1
#define BTN_CENTER 2
#define BUZZER_PIN 5

#define FRAME_DELAY 25

Adafruit_SH1106G display(128, 64, &Wire, -1);
Preferences prefs;
bool soundEnabled = true;

// =====================================================
// BUZZER / SOUND EFFECTS
// =====================================================

void playSound(unsigned int frequency, unsigned long duration) {
  if (!soundEnabled) return;
  tone(BUZZER_PIN, frequency, duration);
}

void soundNavigate() { playSound(880, 22); }
void soundSelect()   { playSound(1320, 55); }
void soundStart()    { playSound(660, 65); }
void soundHit()      { playSound(520, 28); }
void soundPoint()    { playSound(1040, 65); }
void soundLevelUp()  { playSound(1560, 110); }
void soundLose()     { playSound(190, 220); }
void soundWin()      { playSound(1320, 90); }

// =====================================================
// BUTTONS
// =====================================================

struct Button {
  int pin;
  bool stable;
  bool lastReading;
  bool pressed;
  bool released;
  bool held;
  bool repeatEvent;
  bool longEvent;
  bool longHandled;
  unsigned long lastChange;
  unsigned long pressStart;
  unsigned long nextRepeat;
};

Button buttons[5] = {
  {BTN_UP, HIGH, HIGH},
  {BTN_LEFT, HIGH, HIGH},
  {BTN_DOWN, HIGH, HIGH},
  {BTN_RIGHT, HIGH, HIGH},
  {BTN_CENTER, HIGH, HIGH}
};

void updateButtons() {
  unsigned long now = millis();

  for (int i = 0; i < 5; i++) {
    Button &b = buttons[i];

    b.pressed = false;
    b.released = false;
    b.repeatEvent = false;
    b.longEvent = false;

    bool reading = digitalRead(b.pin);

    if (reading != b.lastReading) {
      b.lastChange = now;
      b.lastReading = reading;
    }

    if (now - b.lastChange > 25) {
      if (reading != b.stable) {
        b.stable = reading;

        if (reading == LOW) {
          b.pressed = true;
          b.held = true;
          b.pressStart = now;
          b.nextRepeat = now + 350;
          b.longHandled = false;
        } else {
          b.released = true;
          b.held = false;
        }
      }
    }

    if (b.held) {
      if (now - b.pressStart >= 1000 && !b.longHandled) {
        b.longEvent = true;
        b.longHandled = true;
      }

      if (now >= b.nextRepeat && i != 4) {
        b.repeatEvent = true;
        b.nextRepeat = now + 120;
      }
    }
  }
}

bool pressed(int id) {
  return buttons[id].pressed;
}

bool repeatPressed(int id) {
  return buttons[id].pressed || buttons[id].repeatEvent;
}

bool held(int id) {
  return buttons[id].held;
}

// =====================================================
// SCREEN STATES
// =====================================================

enum ScreenState {
  BOOT,
  GAME_SELECTOR,
  SYSTEM_MENU,
  HIGHSCORES,
  SETTINGS,
  CONTROLS,
  ABOUT,
  GAME_PLAYING,
  GAME_PAUSED,
  END_ANIMATION,
  GAME_OVER,
  YOU_WON
};

ScreenState currentScreen = BOOT;

const char *games[] = {
  "SNAKE",
  "TETRIS",
  "SPACE INVADERS",
  "BREAKOUT",
  "MAZE RUNNER",
  "FLAPPY BIRD",
  "DINO RUNNER",
  "PONG VS AI"
};

const int GAME_COUNT = 8;

int selectedGame = 0;
int menuSelection = 0;
int pauseSelection = 0;
int highscorePage = 0;

int gameScore = 0;
int highScores[GAME_COUNT];

unsigned long bootStart;

// Ergebnisanimation: etwa eine Sekunde, ohne Eingaben.
ScreenState endTargetScreen = GAME_OVER;
unsigned long endAnimationStart = 0;
const unsigned long END_ANIMATION_DURATION = 1000;


// =====================================================
// GALAXY
// =====================================================

struct Star {
  float x;
  float y;
  float speed;
  float vx;
  float vy;
};

Star stars[18];

void initStars() {
  for (int i = 0; i < 18; i++) {
    stars[i].x = random(0, 128);
    stars[i].y = random(0, 64);
    stars[i].speed = random(5, 20) / 10.0;
    // Unabhängige horizontale und vertikale Drift, einschließlich Diagonalen.
    stars[i].vx = random(-20, 21) / 20.0f;
    stars[i].vy = random(-20, 21) / 20.0f;
    if (fabs(stars[i].vx) < 0.15f && fabs(stars[i].vy) < 0.15f) {
      stars[i].vy = 0.25f;
    }
  }
}

void updateStars() {
  for (int i = 0; i < 18; i++) {
    stars[i].x += stars[i].vx;
    stars[i].y += stars[i].vy;

    // Beim Verlassen des Displays an einer passenden Gegenseite neu erscheinen.
    if (stars[i].x < 0) stars[i].x = 127;
    else if (stars[i].x >= 128) stars[i].x = 0;

    if (stars[i].y < 0) stars[i].y = 63;
    else if (stars[i].y >= 64) stars[i].y = 0;
  }
}

void drawStars() {
  for (int i = 0; i < 18; i++) {
    display.drawPixel(
      (int)stars[i].x,
      (int)stars[i].y,
      SH110X_WHITE
    );
  }
}

// =====================================================
// HIGHSCORES
// =====================================================

void loadHighscores() {
  prefs.begin("orbit", false);

  // Sound-Einstellung bleibt auch nach Ausschalten/Neustart erhalten.
  soundEnabled = prefs.getBool("sound", true);

  for (int i = 0; i < GAME_COUNT; i++) {
    char key[12];
    sprintf(key, "score%d", i);
    highScores[i] = prefs.getInt(key, 0);
  }
}

void saveHighscore() {
  if (gameScore > highScores[selectedGame]) {
    highScores[selectedGame] = gameScore;

    char key[12];
    sprintf(key, "score%d", selectedGame);

    prefs.putInt(key, gameScore);
  }
}

// =====================================================
// BOOT SCREEN
// =====================================================

void drawBoot() {
  unsigned long elapsed = millis() - bootStart;

  display.clearDisplay();

  // Partikel bewegen sich während des kurzen Starts diagonal.
  for (int i = 0; i < 18; i++) {
    stars[i].x += stars[i].speed * 0.45f;
    stars[i].y -= stars[i].speed * 0.18f;
    if (stars[i].x > 128 || stars[i].y < 0) {
      stars[i].x = random(-12, 1);
      stars[i].y = random(8, 64);
      stars[i].speed = random(8, 24) / 10.0f;
    }
    display.drawPixel((int)stars[i].x, (int)stars[i].y, SH110X_WHITE);
  }

  display.setTextColor(SH110X_WHITE);
  // Logo-Block vertikal mittig ausrichten.
  display.setTextSize(2);
  display.setCursor(24, 19);
  display.print("ORBIT");

  display.setTextSize(1);
  display.setCursor(48, 39);
  display.print("OS 2.0");

  display.drawRect(8, 53, 112, 6, SH110X_WHITE);
  int progress = constrain((int)(elapsed * 108UL / 1000UL), 0, 108);
  display.fillRect(10, 55, progress, 2, SH110X_WHITE);
  display.display();

  if (elapsed >= 1000) {
    currentScreen = GAME_SELECTOR;
  }
}

// =====================================================
// COMMON DISPLAY
// =====================================================

void drawHeader() {
  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);

  display.setCursor(3, 2);
  display.print("ORBIT OS");

  display.setCursor(95, 2);
  display.print("2.0");

  display.drawFastHLine(0, 11, 128, SH110X_WHITE);
}

/*
 * Einheitliche Spiel-Kopfzeile:
 * Score mittig
 * Level rechts oben bei x = 106
 *
 * level = 0 bedeutet: keine Levelanzeige.
 */
void drawGameFrame(int level = 0, int lives = 0) {
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  char scoreText[12];
  sprintf(scoreText, "%d", gameScore);

  int width = strlen(scoreText) * 6;

  display.setCursor((128 - width) / 2, 2);
  display.print(scoreText);

  // Leben links in derselben Kopfzeile wie Score und Level.
  if (lives > 0) {
    display.setCursor(3, 2);
    display.print("HP");
    display.print(lives);
  }

  if (level > 0) {
    display.setCursor(106, 2);
    display.print("L");
    display.print(level);
  }

  display.drawFastHLine(0, 11, 128, SH110X_WHITE);
  display.drawRect(1, 13, 126, 49, SH110X_WHITE);
}

// =====================================================
// MAIN MENU
// =====================================================

// Kleine Pfeile im Stil des Handheld-Menues.
void drawSelectorArrow(int x, int y, bool left) {
  if (left) {
    display.drawLine(x + 7, y - 6, x, y, SH110X_WHITE);
    display.drawLine(x, y, x + 7, y + 6, SH110X_WHITE);
    display.drawLine(x, y, x + 12, y, SH110X_WHITE);
  } else {
    display.drawLine(x, y - 6, x + 7, y, SH110X_WHITE);
    display.drawLine(x + 7, y, x, y + 6, SH110X_WHITE);
    display.drawLine(x - 5, y, x + 7, y, SH110X_WHITE);
  }
}

void drawCarouselItem(const char *heading, const char *current,
                      const char *previous, const char *next,
                      int position, int total) {
  drawHeader();
  updateStars();
  drawStars();

  // Gleiche Statuszeile und monochrome Gestaltung wie bei der Spielauswahl.
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(4, 15);
  display.print(heading);

  // Kleine Positionspunkte.
  int dotsWidth = total * 5 - 1;
  int dotStart = (128 - dotsWidth) / 2;
  for (int i = 0; i < total; i++) {
    if (i == position) display.fillCircle(dotStart + i * 5, 25, 1, SH110X_WHITE);
    else display.drawPixel(dotStart + i * 5, 25, SH110X_WHITE);
  }

  // Vorheriger und nächster Eintrag bleiben als Orientierung sichtbar.
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(4, 34);
  display.print("<");
  display.setCursor(14, 34);
  display.print(previous);
  display.setCursor(4, 55);
  display.print(">");
  display.setCursor(14, 55);
  display.print(next);

  // Ausgewählter Menüpunkt als zentrale, invertierte Karte.
  display.fillRoundRect(7, 39, 114, 14, 3, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK);
  int textWidth = (int)strlen(current) * 6;
  display.setCursor(max(10, (128 - textWidth) / 2), 43);
  display.print(current);

  display.setTextColor(SH110X_WHITE);
  display.display();
}

void drawSelector() {
  display.clearDisplay();
  updateStars();
  drawStars();

  // Statuszeile wie im Referenzvideo: Systemname links, Bestwert rechts.
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(3, 3);
  display.print("ORBIT.OS2");
  char bestText[16];
  snprintf(bestText, sizeof(bestText), "BEST %d", highScores[selectedGame]);
  int bestWidth = (int)strlen(bestText) * 6;
  display.setCursor(125 - bestWidth, 3);
  display.print(bestText);
  display.drawFastHLine(0, 12, 128, SH110X_WHITE);

  // Seitliche Pfeile und mittig ausgerichteter Spielname.

  drawSelectorArrow(5, 39, true);
  drawSelectorArrow(111, 39, false);

  const char *name = games[selectedGame];
  int nameLength = (int)strlen(name);
  uint8_t textSize = (nameLength > 9) ? 1 : 2;
  int textWidth = nameLength * 6 * textSize;
  int textX = (128 - textWidth) / 2;
  if (textX < 19) {
    textSize = 1;
    textWidth = nameLength * 6;
    textX = (128 - textWidth) / 2;
  }
  display.setTextSize(textSize);
  display.setCursor(textX, textSize == 2 ? 27 : 29);
  display.print(name);

  // Fortschrittspunkte am unteren Rand.
  int dotsWidth = GAME_COUNT * 7 - 1;
  int dotStart = (128 - dotsWidth) / 2;
  for (int i = 0; i < GAME_COUNT; i++) {
    int x = dotStart + i * 7;
    if (i == selectedGame) display.fillCircle(x, 51, 2, SH110X_WHITE);
    else display.drawPixel(x, 51, SH110X_WHITE);
  }

  display.setTextSize(1);
  display.setCursor(3, 57);
  display.print("L/R MOVE");
  display.setCursor(83, 57);
  display.print("OK PLAY");
  display.display();
}

const char *menuItems[] = {
  "HIGHSCORES",
  "SETTINGS",
  "CONTROLS",
  "ABOUT",
  "BACK TO GAMES"
};

void drawSystemMenu() {
  drawHeader();
  updateStars();
  drawStars();

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(4, 13);
  display.print("SYSTEM MENU");

  // Fünf Zeilen mit jeweils 8 Pixeln Höhe passen vollständig ins 128x64-Display.
  for (int i = 0; i < 5; i++) {
    int y = 22 + i * 8;

    if (i == menuSelection) {
      display.fillRect(2, y - 1, 124, 8, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    } else {
      display.setTextColor(SH110X_WHITE);
    }

    display.setCursor(7, y);
    display.print(menuItems[i]);
  }

  display.setTextColor(SH110X_WHITE);
  display.display();
}

// =====================================================
// HIGHSCORES
// =====================================================

void drawHighscores() {
  drawHeader();

  display.setCursor(4, 15);
  display.print("HIGH SCORES");

  for (int i = 0; i < 4; i++) {
    int index = highscorePage * 4 + i;
    int y = 26 + i * 8;

    display.setCursor(3, y);
    display.print(games[index]);

    display.setCursor(100, y);
    display.print(highScores[index]);
  }

  display.drawFastHLine(3, 59, 122, SH110X_WHITE);

  display.setCursor(43, 56);
  display.print(highscorePage == 0 ? "1 / 2" : "2 / 2");

  display.display();
}

// =====================================================
// CONTROLS
// =====================================================

void drawControls() {
  drawHeader();

  // Eine Spieleseite pro Bildschirm: genug Platz fuer klare Zeilen.
  const char *gameNames[] = {
    "SNAKE", "TETRIS", "INVADERS", "BREAKOUT",
    "MAZE", "FLAPPY BIRD", "DINO RUNNER", "PONG"
  };

  const char *controlLine1[] = {
    "Use arrow buttons", "LEFT / RIGHT: move", "LEFT / RIGHT: move",
    "LEFT / RIGHT: paddle", "Use arrow buttons", "CENTER: flap",
    "CENTER: jump", "UP / DOWN: paddle"
  };

  const char *controlLine2[] = {
    "to change direction", "UP: rotate", "CENTER: fire",
    "", "to change direction", "", "", ""
  };

  int page = highscorePage % 8;

  display.setCursor(4, 15);
  display.print("CONTROLS ");
  display.print(page + 1);
  display.print(" / 8");

  // Abschnitt: Spielname
  display.setCursor(4, 27);
  display.print(gameNames[page]);
  display.drawFastHLine(4, 38, 120, SH110X_WHITE);

  // Abschnitt: Tastenbelegung mit Abstand zwischen den Zeilen
  display.setCursor(4, 43);
  display.print(controlLine1[page]);

  if (controlLine2[page][0] != '\0') {
    display.setCursor(4, 52);
    display.print(controlLine2[page]);
  }

  // Bedienhinweis am unteren Rand
  display.setCursor(4, 59);
  display.print("R/C: NEXT  L: BACK");

  display.display();
}

// =====================================================
// SETTINGS
// =====================================================

void drawSettings() {
  drawHeader();

  display.setCursor(4, 17);
  display.print("SETTINGS");

  display.setCursor(5, 32);
  display.print("SOUND:");

  display.setCursor(75, 32);
  display.print(soundEnabled ? "ON" : "OFF");

  display.setCursor(5, 48);
  display.print("CENTER: TOGGLE");

  display.display();
}

// =====================================================
// ABOUT
// =====================================================

void drawAbout() {
  drawHeader();

  display.setCursor(4, 17);
  display.print("ORBIT OS 2.0");

  display.setCursor(4, 31);
  display.print("ESP32-C3");

  display.setCursor(4, 43);
  display.print("8 MINI GAMES");

  display.setCursor(4, 55);
  display.print("MADE FOR OLED");

  display.display();
}

// =====================================================
// GAME OVER
// =====================================================

void drawGameOver() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);
  display.setCursor(42, 9);
  display.print("GAME OVER");

  display.setTextSize(2);
  char scoreText[12];
  sprintf(scoreText, "%d", gameScore);
  display.setCursor((128 - strlen(scoreText) * 12) / 2, 23);
  display.print(scoreText);

  display.setTextSize(1);
  display.setCursor(28, 44);
  display.print("LEFT: MENU");
  display.setCursor(16, 55);
  display.print("CENTER: RETRY");

  display.display();
}

// =====================================================
// YOU WON
// =====================================================

void drawYouWon() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  display.setTextSize(2);
  display.setCursor(20, 8);
  display.print("YOU WON!");

  display.setTextSize(1);
  display.setCursor(35, 31);
  display.print("ALL LEVELS");
  display.setCursor(35, 41);
  display.print("COMPLETED");

  display.setCursor(48, 54);
  display.print("SCORE ");
  display.print(gameScore);

  display.display();
}

// =====================================================
// PAUSE
// =====================================================

void drawPause() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  display.setCursor(48, 10);
  display.print("PAUSED");

  const char *items[] = {"RESUME", "QUIT"};

  for (int i = 0; i < 2; i++) {
    int y = 29 + i * 13;

    if (i == pauseSelection) {
      display.fillRect(25, y - 2, 78, 11, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    } else {
      display.setTextColor(SH110X_WHITE);
    }

    display.setCursor(47, y);
    display.print(items[i]);
  }

  display.setTextColor(SH110X_WHITE);
  display.display();
}

// =====================================================
// GAME ENGINE DECLARATIONS
// =====================================================

void finishGame();
void finishWin();
void startGame();
void updateGame();

// =====================================================
// SNAKE
// =====================================================

#define SNAKE_W 30
#define SNAKE_H 11

int snakeX[330];
int snakeY[330];
int snakeLength;

int snakeDX;
int snakeDY;

int foodX;
int foodY;

unsigned long snakeTimer;

void spawnFood() {
  foodX = random(0, SNAKE_W);
  foodY = random(0, SNAKE_H);
}

void initSnake() {
  snakeLength = 3;

  snakeX[0] = 15;
  snakeY[0] = 5;

  snakeX[1] = 14;
  snakeY[1] = 5;

  snakeX[2] = 13;
  snakeY[2] = 5;

  snakeDX = 1;
  snakeDY = 0;

  spawnFood();
  snakeTimer = millis();
}

void updateSnake() {
  if (repeatPressed(0) && snakeDY != 1) {
    snakeDX = 0;
    snakeDY = -1;
  }

  if (repeatPressed(2) && snakeDY != -1) {
    snakeDX = 0;
    snakeDY = 1;
  }

  if (repeatPressed(1) && snakeDX != 1) {
    snakeDX = -1;
    snakeDY = 0;
  }

  if (repeatPressed(3) && snakeDX != -1) {
    snakeDX = 1;
    snakeDY = 0;
  }

  if (millis() - snakeTimer < 125) return;
  snakeTimer = millis();

  int nx = snakeX[0] + snakeDX;
  int ny = snakeY[0] + snakeDY;

  if (nx < 0 || nx >= SNAKE_W || ny < 0 || ny >= SNAKE_H) {
    finishGame();
    return;
  }

  for (int i = 0; i < snakeLength; i++) {
    if (snakeX[i] == nx && snakeY[i] == ny) {
      finishGame();
      return;
    }
  }

  for (int i = snakeLength; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }

  snakeX[0] = nx;
  snakeY[0] = ny;

  if (nx == foodX && ny == foodY) {
    if (snakeLength < 329) snakeLength++;
    gameScore += 10;
    soundPoint();
    spawnFood();
  }
}

void drawSnake() {
  drawGameFrame();

  display.fillRect(2 + foodX * 4, 15 + foodY * 4, 3, 3, SH110X_WHITE);

  for (int i = 0; i < snakeLength; i++) {
    display.fillRect(
      2 + snakeX[i] * 4,
      15 + snakeY[i] * 4,
      3, 3,
      SH110X_WHITE
    );
  }

  display.display();
}

// =====================================================
// TETRIS
// =====================================================

#define TETRIS_W 10
#define TETRIS_H 14

byte tetrisBoard[TETRIS_H][TETRIS_W];

const byte shapes[12][4][4] = {
  // I
  {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
  // O
  {{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
  // T
  {{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
  // J
  {{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
  // L
  {{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
  // S
  {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
  // Z
  {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
  // U
  {{1,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
  // Plus
  {{0,1,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}},
  // V
  {{1,0,0,0},{1,0,0,0},{1,1,1,0},{0,0,0,0}},
  // W
  {{1,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}},
  // P
  {{1,1,0,0},{1,1,0,0},{1,0,0,0},{0,0,0,0}}
};

int tetrisPiece;
int tetrisX;
int tetrisY;
int tetrisRotation = 0;
unsigned long tetrisTimer;

bool tetrisCollision(int x, int y, int rotation) {
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (!shapes[tetrisPiece][r][c]) continue;

      int px = r;
      int py = c;

      for (int k = 0; k < rotation; k++) {
        int temp = px;
        px = py;
        py = 3 - temp;
      }

      int bx = x + py;
      int by = y + px;

      if (bx < 0 || bx >= TETRIS_W || by >= TETRIS_H) return true;
      if (by >= 0 && tetrisBoard[by][bx]) return true;
    }
  }

  return false;
}

void newTetrisPiece() {
  tetrisPiece = random(0, 12);
  tetrisX = 3;
  tetrisY = 0;

  if (tetrisCollision(tetrisX, tetrisY, 0)) {
    finishGame();
  }
}

void initTetris() {
  memset(tetrisBoard, 0, sizeof(tetrisBoard));
  tetrisRotation = 0;
  tetrisTimer = millis();
  newTetrisPiece();
}

void lockTetris() {
  gameScore += 10;
  playSound(420, 65);
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (!shapes[tetrisPiece][r][c]) continue;

      int px = r;
      int py = c;

      for (int k = 0; k < tetrisRotation; k++) {
        int temp = px;
        px = py;
        py = 3 - temp;
      }

      int bx = tetrisX + py;
      int by = tetrisY + px;

      if (by >= 0 && by < TETRIS_H && bx >= 0 && bx < TETRIS_W) {
        tetrisBoard[by][bx] = 1;
      }
    }
  }

  int lines = 0;

  for (int y = TETRIS_H - 1; y >= 0; y--) {
    bool full = true;

    for (int x = 0; x < TETRIS_W; x++) {
      if (!tetrisBoard[y][x]) full = false;
    }

    if (full) {
      lines++;

      for (int yy = y; yy > 0; yy--) {
        for (int x = 0; x < TETRIS_W; x++) {
          tetrisBoard[yy][x] = tetrisBoard[yy - 1][x];
        }
      }

      memset(tetrisBoard[0], 0, TETRIS_W);
      y++;
    }
  }

  gameScore += lines * 100;
  if (lines > 0) soundLevelUp();
  newTetrisPiece();
}

void updateTetris() {
  if (repeatPressed(1) &&
      !tetrisCollision(tetrisX - 1, tetrisY, tetrisRotation)) {
    tetrisX--;
  }

  if (repeatPressed(3) &&
      !tetrisCollision(tetrisX + 1, tetrisY, tetrisRotation)) {
    tetrisX++;
  }

  if (pressed(4)) {
    int next = (tetrisRotation + 1) % 4;

    if (!tetrisCollision(tetrisX, tetrisY, next)) {
      tetrisRotation = next;
    }
  }

  int speed = max(100, 500 - gameScore / 10);

  if (millis() - tetrisTimer >= (held(2) ? 60 : speed)) {
    tetrisTimer = millis();

    if (!tetrisCollision(tetrisX, tetrisY + 1, tetrisRotation)) {
      tetrisY++;
    } else {
      lockTetris();
    }
  }
}

void drawTetris() {
  drawGameFrame();

  display.drawRect(36, 14, 56, 49, SH110X_WHITE);

  for (int y = 0; y < TETRIS_H; y++) {
    for (int x = 0; x < TETRIS_W; x++) {
      if (tetrisBoard[y][x]) {
        display.fillRect(39 + x * 5, 17 + y * 3, 4, 2, SH110X_WHITE);
      }
    }
  }

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (!shapes[tetrisPiece][r][c]) continue;

      int px = r;
      int py = c;

      for (int k = 0; k < tetrisRotation; k++) {
        int temp = px;
        px = py;
        py = 3 - temp;
      }

      int bx = tetrisX + py;
      int by = tetrisY + px;

      if (bx >= 0 && bx < TETRIS_W && by >= 0 && by < TETRIS_H) {
        display.fillRect(39 + bx * 5, 17 + by * 3, 4, 2, SH110X_WHITE);
      }
    }
  }

  display.display();
}

// =====================================================
// SPACE INVADERS - LEVEL SYSTEM
// =====================================================

#define INVADER_ROWS 4
#define INVADER_COLS 5

bool invaders[INVADER_ROWS][INVADER_COLS];

float invaderX[INVADER_ROWS][INVADER_COLS];
float invaderY[INVADER_ROWS][INVADER_COLS];
float invaderSpeed[INVADER_ROWS][INVADER_COLS];

int invaderDirection[INVADER_ROWS][INVADER_COLS];

int invaderLevel;
int invaderPlayerX;
int invaderBulletX;
int invaderBulletY;

bool invaderBulletActive;

unsigned long invaderTimer;

void resetInvaders() {
  for (int r = 0; r < INVADER_ROWS; r++) {
    for (int c = 0; c < INVADER_COLS; c++) {
      invaders[r][c] = true;

      invaderX[r][c] = 8 + c * 21;
      invaderY[r][c] = 19 + r * 7;

      float levelBoost = min(2.0f, (invaderLevel - 1) * 0.22f);
      invaderSpeed[r][c] = random(8, 20) / 10.0 + levelBoost;

      invaderDirection[r][c] = random(0, 2) ? 1 : -1;
    }
  }

  invaderBulletActive = false;
  invaderTimer = millis();
}

void initInvaders() {
  invaderLevel = 1;
  invaderPlayerX = 64;
  resetInvaders();
}

void updateInvaders() {
  if (held(1)) invaderPlayerX -= 2;
  if (held(3)) invaderPlayerX += 2;

  invaderPlayerX = constrain(invaderPlayerX, 5, 123);

  if (pressed(4) && !invaderBulletActive) {
    invaderBulletActive = true;
    invaderBulletX = invaderPlayerX;
    invaderBulletY = 53;
  }

  if (millis() - invaderTimer < 35) return;
  invaderTimer = millis();

  for (int r = 0; r < INVADER_ROWS; r++) {
    for (int c = 0; c < INVADER_COLS; c++) {
      if (!invaders[r][c]) continue;

      invaderX[r][c] += invaderDirection[r][c] * invaderSpeed[r][c] * 0.45;

      if (invaderX[r][c] < 3) {
        invaderX[r][c] = 3;
        invaderDirection[r][c] = 1;
        invaderY[r][c] += 2;
      }

      if (invaderX[r][c] > 120) {
        invaderX[r][c] = 120;
        invaderDirection[r][c] = -1;
        invaderY[r][c] += 2;
      }

      if (invaderY[r][c] > 49) {
        finishGame();
        return;
      }
    }
  }

  if (invaderBulletActive) {
    invaderBulletY -= 3;

    if (invaderBulletY < 14) {
      invaderBulletActive = false;
    }

    for (int r = 0; r < INVADER_ROWS; r++) {
      for (int c = 0; c < INVADER_COLS; c++) {
        if (!invaders[r][c]) continue;

        int ex = invaderX[r][c];
        int ey = invaderY[r][c];

        if (abs(invaderBulletX - ex) < 6 &&
            abs(invaderBulletY - ey) < 5) {
          invaders[r][c] = false;
          invaderBulletActive = false;
          gameScore += 10;
          soundHit();
          break;
        }
      }
    }
  }

  bool remaining = false;

  for (int r = 0; r < INVADER_ROWS; r++) {
    for (int c = 0; c < INVADER_COLS; c++) {
      if (invaders[r][c]) remaining = true;
    }
  }

  if (!remaining) {
    invaderLevel++;
    gameScore += 100;
    soundLevelUp();
    resetInvaders();
  }
}

void drawInvaders() {
  // Levelanzeige automatisch in der gemeinsamen Kopfzeile.
  drawGameFrame(invaderLevel);

  for (int r = 0; r < INVADER_ROWS; r++) {
    for (int c = 0; c < INVADER_COLS; c++) {
      if (invaders[r][c]) {
        int x = invaderX[r][c];
        int y = invaderY[r][c];

        display.fillRect(x, y, 5, 4, SH110X_WHITE);
        display.drawPixel(x + 1, y + 3, SH110X_BLACK);
        display.drawPixel(x + 3, y + 3, SH110X_BLACK);
      }
    }
  }

  display.fillTriangle(
    invaderPlayerX, 57,
    invaderPlayerX - 4, 62,
    invaderPlayerX + 4, 62,
    SH110X_WHITE
  );

  if (invaderBulletActive) {
    display.drawFastVLine(invaderBulletX, invaderBulletY, 4, SH110X_WHITE);
  }

  display.display();
}

// =====================================================
// COUNTDOWN SYSTEM
// =====================================================

const unsigned long COUNTDOWN_DURATION = 2000;

bool breakoutCountdown = false;
bool pongCountdown = false;

unsigned long breakoutCountdownStart = 0;
unsigned long pongCountdownStart = 0;

void startBreakoutCountdown() {
  breakoutCountdown = true;
  breakoutCountdownStart = millis();
}

void startPongCountdown() {
  pongCountdown = true;
  pongCountdownStart = millis();
}

void drawTransparentCountdown(unsigned long startTime, float dx, int y) {
  unsigned long elapsed = millis() - startTime;

  if (elapsed >= COUNTDOWN_DURATION) return;

  int remaining = 2 - (elapsed / 1000);

  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);

  char number[3];
  sprintf(number, "%d", remaining);

  int textWidth = strlen(number) * 12;
  display.setCursor((128 - textWidth) / 2, y);
  display.print(number);

  display.setTextSize(1);

  int arrowX = 64;
  int arrowY = y + 19;
  int direction = dx >= 0 ? 1 : -1;

  display.drawLine(
    arrowX, arrowY,
    arrowX + direction * 10, arrowY,
    SH110X_WHITE
  );

  display.drawLine(
    arrowX + direction * 10, arrowY,
    arrowX + direction * 6, arrowY - 3,
    SH110X_WHITE
  );

  display.drawLine(
    arrowX + direction * 10, arrowY,
    arrowX + direction * 6, arrowY + 3,
    SH110X_WHITE
  );
}

// =====================================================
// BREAKOUT
// =====================================================

#define BRICK_ROWS 5
#define BRICK_COLS 8

bool bricks[BRICK_ROWS][BRICK_COLS];

float breakoutBallX;
float breakoutBallY;
float breakoutDX;
float breakoutDY;

int breakoutPaddle;
int breakoutLives;
int breakoutLevel;

unsigned long breakoutTimer;

void resetBricks() {
  memset(bricks, 0, sizeof(bricks));

  int rows = min(BRICK_ROWS, 2 + breakoutLevel);

  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < BRICK_COLS; x++) {
      bricks[y][x] = true;
    }
  }
}

void resetBreakoutBall() {
  breakoutBallX = 64;
  breakoutBallY = 43;

  breakoutDX = random(0, 2) ? 1.3 : -1.3;
  breakoutDY = -1.5 - breakoutLevel * 0.1;
}

void initBreakout() {
  breakoutLevel = 1;
  breakoutLives = 3;
  breakoutPaddle = 64;

  resetBricks();
  resetBreakoutBall();

  breakoutTimer = millis();
  startBreakoutCountdown();
}

void updateBreakout() {
  if (held(1)) breakoutPaddle -= 2;
  if (held(3)) breakoutPaddle += 2;

  breakoutPaddle = constrain(breakoutPaddle, 9, 119);

  if (breakoutCountdown) {
    if (millis() - breakoutCountdownStart >= COUNTDOWN_DURATION) {
      breakoutCountdown = false;
      breakoutTimer = millis();
    } else {
      return;
    }
  }

  if (millis() - breakoutTimer < 15) return;
  breakoutTimer = millis();

  breakoutBallX += breakoutDX;
  breakoutBallY += breakoutDY;

  if (breakoutBallX < 3 || breakoutBallX > 124) {
    breakoutDX *= -1;
  }

  if (breakoutBallY < 14) {
    breakoutDY = abs(breakoutDY);
  }

  if (breakoutBallY >= 54 &&
      breakoutBallY <= 59 &&
      abs(breakoutBallX - breakoutPaddle) < 12) {

    breakoutDY = -abs(breakoutDY);

    breakoutDX += (breakoutBallX - breakoutPaddle) * 0.08;
    breakoutDX = constrain(breakoutDX, -2.8, 2.8);
  }

  if (breakoutBallY > 62) {
    breakoutLives--;

    if (breakoutLives <= 0) {
      finishGame();
      return;
    }

    resetBreakoutBall();
    startBreakoutCountdown();
    return;
  }

  for (int y = 0; y < BRICK_ROWS; y++) {
    for (int x = 0; x < BRICK_COLS; x++) {
      if (!bricks[y][x]) continue;

      int bx = 5 + x * 15;
      int by = 16 + y * 5;

      if (breakoutBallX >= bx &&
          breakoutBallX <= bx + 12 &&
          breakoutBallY >= by &&
          breakoutBallY <= by + 4) {

        bricks[y][x] = false;
        breakoutDY *= -1;
        gameScore += 10;
        soundHit();
      }
    }
  }

  bool remaining = false;

  for (int y = 0; y < BRICK_ROWS; y++) {
    for (int x = 0; x < BRICK_COLS; x++) {
      if (bricks[y][x]) remaining = true;
    }
  }

  if (!remaining) {
    breakoutLevel++;
    gameScore += 100;
    soundLevelUp();

    resetBricks();
    resetBreakoutBall();
    startBreakoutCountdown();
  }
}

void drawBreakout() {
  // Links Leben, mittig Score, rechts Level.
  drawGameFrame(breakoutLevel, breakoutLives);

  for (int y = 0; y < BRICK_ROWS; y++) {
    for (int x = 0; x < BRICK_COLS; x++) {
      if (bricks[y][x]) {
        display.fillRect(5 + x * 15, 16 + y * 5, 12, 3, SH110X_WHITE);
      }
    }
  }

  display.fillRect(breakoutPaddle - 9, 57, 18, 3, SH110X_WHITE);
  display.fillCircle(breakoutBallX, breakoutBallY, 2, SH110X_WHITE);

  if (breakoutCountdown) {
    drawTransparentCountdown(breakoutCountdownStart, breakoutDX, 19);
  }

  display.display();
}

// =====================================================
// MAZE RUNNER - 8 LEVELS
// =====================================================

const char mazeLevels[8][7][17] = {
  {
    "################",
    "#P.............#",
    "#.####.#####...#",
    "#......#.......#",
    "######.#.#####.#",
    "#.............G#",
    "################"
  },
  {
    "################",
    "#P....#........#",
    "####..#.######.#",
    "#.....#........#",
    "#.##########.#.#",
    "#............G#",
    "################"
  },
  {
    "################",
    "#P.....#.......#",
    "#####..#.#####.#",
    "#......#.......#",
    "#.###########..#",
    "#.............G#",
    "################"
  },
  {
    "################",
    "#P....#........#",
    "#.##..#.######.#",
    "#..#..#........#",
    "##.#.#######.#.#",
    "#.............G#",
    "################"
  },
  {
    "################",
    "#P.............#",
    "#.###########..#",
    "#...........#..#",
    "#.#########.#..#",
    "#.............G#",
    "################"
  },
  {
    "################",
    "#P....#........#",
    "####..#.######.#",
    "#.....#......#.#",
    "#.##########.#.#",
    "#............G#",
    "################"
  },
  {
    "################",
    "#P.....#.......#",
    "#.###..#.#####.#",
    "#...#..#.......#",
    "###.########.#.#",
    "#.............G#",
    "################"
  },
  {
    "################",
    "#P...#.........#",
    "###..#.#########",
    "#....#.........#",
    "#.###########..#",
    "#.............G#",
    "################"
  }
};

#define MAZE_LEVEL_COUNT 8

int mazeX;
int mazeY;
int mazeLevel;

void initMaze() {
  mazeLevel = 0;
  mazeX = 1;
  mazeY = 1;
}

void updateMaze() {
  int nx = mazeX;
  int ny = mazeY;

  if (repeatPressed(0)) ny--;
  if (repeatPressed(2)) ny++;
  if (repeatPressed(1)) nx--;
  if (repeatPressed(3)) nx++;

  if (nx < 1 || nx > 14 || ny < 1 || ny > 5) return;

  if (mazeLevels[mazeLevel][ny][nx] != '#') {
    mazeX = nx;
    mazeY = ny;
  }

  if (mazeLevels[mazeLevel][mazeY][mazeX] == 'G') {
    gameScore += 100 + mazeLevel * 25;
    soundPoint();

    mazeLevel++;

    if (mazeLevel >= MAZE_LEVEL_COUNT) {
      finishWin();
      return;
    }

    mazeX = 1;
    mazeY = 1;
  }
}

void drawMaze() {
  // Levelanzeige in der gemeinsamen Kopfzeile.
  drawGameFrame(mazeLevel + 1);

  for (int y = 0; y < 7; y++) {
    for (int x = 0; x < 16; x++) {
      int px = 8 + x * 7;
      int py = 14 + y * 7;

      if (mazeLevels[mazeLevel][y][x] == '#') {
        display.fillRect(px, py, 6, 6, SH110X_WHITE);
      }

      if (mazeLevels[mazeLevel][y][x] == 'G') {
        display.drawRect(px, py, 6, 6, SH110X_WHITE);
      }
    }
  }

  display.fillCircle(
    8 + mazeX * 7 + 3,
    14 + mazeY * 7 + 3,
    2,
    SH110X_WHITE
  );

  display.display();
}

// =====================================================
// FLAPPY BIRD
// =====================================================

float birdY;
float birdVelocity;

int pipeX[2];
int pipeGap[2];

unsigned long flappyTimer;

void resetPipe(int i) {
  pipeGap[i] = random(26, 44);
  pipeX[i] = 125;
}

void initFlappy() {
  birdY = 37;
  birdVelocity = 0;

  pipeX[0] = 90;
  pipeX[1] = 155;

  pipeGap[0] = 34;
  pipeGap[1] = 40;

  flappyTimer = millis();
}

void updateFlappy() {
  if (pressed(4)) {
    birdVelocity = -1.55;
  }

  if (millis() - flappyTimer < 20) return;
  flappyTimer = millis();

  birdVelocity += 0.16;
  birdY += birdVelocity;

  if (birdY < 14 || birdY > 60) {
    finishGame();
    return;
  }

  for (int i = 0; i < 2; i++) {
    pipeX[i] -= 1.8;

    if (pipeX[i] < -6) {
      resetPipe(i);
      gameScore++;
      soundPoint();
    }

    bool horizontal =
      24 + 3 > pipeX[i] &&
      24 - 3 < pipeX[i] + 6;

    if (horizontal) {
      if (birdY - 3 < pipeGap[i] - 10 ||
          birdY + 3 > pipeGap[i] + 10) {
        finishGame();
        return;
      }
    }
  }
}

void drawFlappy() {
  drawGameFrame();

  for (int i = 0; i < 2; i++) {
    display.fillRect(
      pipeX[i],
      14,
      6,
      max(0, pipeGap[i] - 24),
      SH110X_WHITE
    );

    display.fillRect(
      pipeX[i],
      pipeGap[i] + 10,
      6,
      max(0, 62 - pipeGap[i] - 10),
      SH110X_WHITE
    );
  }

  display.fillCircle(24, birdY, 3, SH110X_WHITE);

  display.display();
}

// =====================================================
// DINO RUNNER - GEOMETRY DASH STYLE
// =====================================================

float dinoY;                 // Oberkante des Quadrats
float dinoVelocity;
float dinoSpeed;

struct SpikeGroup {
  float x;
  byte count;
  byte heights[3];
  float spacing;
  bool scored;
};

SpikeGroup spikeGroups[3];
const int DINO_X = 15;
const int DINO_SIZE = 7;
const int DINO_GROUND = 56;

unsigned long dinoTimer;
unsigned long dinoStartTime;

const float DINO_START_SPEED = 2.0f;
const float DINO_MAX_SPEED = 3.3f;
const float DINO_SPEED_INCREASE_PER_SECOND = 0.06f;

void createSpikeGroup(int index, float x) {
  spikeGroups[index].x = x;
  spikeGroups[index].count = random(1, 4);
  spikeGroups[index].spacing = random(7, 12);
  spikeGroups[index].scored = false;

  for (int i = 0; i < 3; i++) {
    spikeGroups[index].heights[i] =
      (i < spikeGroups[index].count) ? random(7, 14) : 0;
  }
}

void initDino() {
  dinoY = DINO_GROUND - DINO_SIZE;
  dinoVelocity = 0;
  dinoSpeed = DINO_START_SPEED;
  dinoStartTime = millis();

  createSpikeGroup(0, 132);
  createSpikeGroup(1, 132 + random(65, 90));
  createSpikeGroup(2, 132 + random(145, 185));

  dinoTimer = millis();
}

void updateDino() {
  // Kleiner, kontrollierter Sprung; erneutes Drücken in der Luft ignoriert.
  if (pressed(4) && dinoY >= DINO_GROUND - DINO_SIZE - 0.5f) {
    dinoVelocity = -2.65f;
    playSound(760, 35);
  }

  if (millis() - dinoTimer < 25) return;
  dinoTimer = millis();

  dinoVelocity += 0.17f;
  dinoY += dinoVelocity;

  if (dinoY >= DINO_GROUND - DINO_SIZE) {
    dinoY = DINO_GROUND - DINO_SIZE;
    dinoVelocity = 0;
  }

  // Die Laufgeschwindigkeit steigt anhand der Rundenzeit, nicht des Punktestands.
  float elapsedSeconds = (millis() - dinoStartTime) / 1000.0f;
  dinoSpeed = min(DINO_MAX_SPEED,
                  DINO_START_SPEED +
                  elapsedSeconds * DINO_SPEED_INCREASE_PER_SECOND);

  for (int i = 0; i < 3; i++) {
    SpikeGroup &g = spikeGroups[i];
    g.x -= dinoSpeed;

    float groupWidth = (g.count - 1) * g.spacing + 8;
    if (g.x + groupWidth < 0) {
      float rightmost = 0;
      for (int j = 0; j < 3; j++) {
        if (j != i) {
          float otherWidth = (spikeGroups[j].count - 1) *
                             spikeGroups[j].spacing + 8;
          rightmost = max(rightmost, spikeGroups[j].x + otherWidth);
        }
      }
      g.x = rightmost + random(48, 82);
      createSpikeGroup(i, g.x);
    }

    if (!g.scored && g.x + groupWidth < DINO_X) {
      g.scored = true;
      gameScore++;
      soundPoint();
    }

    // Näherungsweise Dreieckskollision: enger als die gesamte Spike-Gruppe.
    for (int k = 0; k < g.count; k++) {
      int sx = (int)(g.x + k * g.spacing);
      int h = g.heights[k];
      int playerLeft = DINO_X;
      int playerRight = DINO_X + DINO_SIZE - 1;
      int playerTop = (int)dinoY;
      int playerBottom = (int)dinoY + DINO_SIZE - 1;
      int spikeLeft = sx + 1;
      int spikeRight = sx + 6;
      int spikeTop = DINO_GROUND - h;

      if (playerRight > spikeLeft && playerLeft < spikeRight &&
          playerBottom >= spikeTop + 2 && playerTop < DINO_GROUND) {
        finishGame();
        return;
      }
    }
  }
}

void drawDino() {
  drawGameFrame();

  // Durchgehender weißer Bodenblock; die Oberkante bleibt bei DINO_GROUND.
  display.fillRect(3, DINO_GROUND, 122, 7, SH110X_WHITE);

  // Geometrische Spielfigur
  display.fillRect(DINO_X, (int)dinoY, DINO_SIZE, DINO_SIZE, SH110X_WHITE);
  display.drawPixel(DINO_X + 5, (int)dinoY + 2, SH110X_BLACK);

  // Zufällige Gruppen aus ein bis drei unterschiedlich hohen Spikes
  for (int i = 0; i < 3; i++) {
    SpikeGroup &g = spikeGroups[i];
    for (int k = 0; k < g.count; k++) {
      int sx = (int)(g.x + k * g.spacing);
      int h = g.heights[k];
      display.fillTriangle(
        sx, DINO_GROUND,
        sx + 4, DINO_GROUND - h,
        sx + 8, DINO_GROUND,
        SH110X_WHITE
      );
    }
  }

  display.display();
}

// =====================================================
// PONG VS AI - PREDICTIVE OPPONENT
// =====================================================

float pongBallX;
float pongBallY;
float pongDX;
float pongDY;

int pongPlayerY;
float pongAIY;
float pongAIOffset = 0.0f;

int pongLives;

unsigned long pongTimer;

const float PONG_TOP = 15.0f;
const float PONG_BOTTOM = 61.0f;
const float PONG_AI_X = 120.0f;

float predictPongImpactY() {
  // Prognostiziert die Ballposition an der rechten Spielfeldseite
  // und berücksichtigt dabei Reflexionen an oberer/unterer Wand.
  if (pongDX <= 0.05f) return 38.0f;

  float frames = (PONG_AI_X - pongBallX) / pongDX;
  float projected = pongBallY + pongDY * frames;

  const float span = PONG_BOTTOM - PONG_TOP;
  float folded = fmod(projected - PONG_TOP, 2.0f * span);
  if (folded < 0) folded += 2.0f * span;
  if (folded > span) folded = 2.0f * span - folded;

  return PONG_TOP + folded;
}

void resetPongBall() {
  pongBallX = 64;
  pongBallY = 37;

  pongDX = random(0, 2) ? 2.0f : -2.0f;
  pongDY = random(-10, 11) / 10.0f;
  // Kleine Ungenauigkeit pro Ballwechsel, damit die KI schlagbar bleibt.
  pongAIOffset = random(-80, 81) / 10.0f;

  if (abs(pongDY) < 0.35f) {
    pongDY = pongDY < 0 ? -0.55f : 0.55f;
  }
}

void initPong() {
  pongPlayerY = 38;
  pongAIY = 38;
  pongLives = 3;

  resetPongBall();

  pongTimer = millis();
  startPongCountdown();
}

void updatePong() {
  // Spielersteuerung
  if (held(0)) pongPlayerY -= 2;
  if (held(2)) pongPlayerY += 2;
  pongPlayerY = constrain(pongPlayerY, 22, 55);

  if (pongCountdown) {
    if (millis() - pongCountdownStart >= COUNTDOWN_DURATION) {
      pongCountdown = false;
      pongTimer = millis();
    } else {
      return;
    }
  }

  if (millis() - pongTimer < 15) return;
  pongTimer = millis();

  // Mittlere Schwierigkeit:
  // Die KI verfolgt den Ball mit leichter Verzögerung statt den
  // exakten Auftreffpunkt vorauszuberechnen.
  float targetY = pongBallY;

  if (pongDX > 0) {
    targetY += pongDY * 6.0f;  // nur kurze, ungenaue Vorschau
    targetY += pongAIOffset;
  } else {
    // Wenn der Ball vom KI-Paddle wegfliegt, bewegt sie sich
    // etwas zurück zur Mitte statt dem Ball perfekt zu folgen.
    targetY = 38.0f + (pongBallY - 38.0f) * 0.25f;
  }

  targetY = constrain(targetY, 22.0f, 55.0f);

  const float aiSpeed = 0.72f;
  const float errorMargin = 4.5f;

  if (pongAIY < targetY - errorMargin) {
    pongAIY += min(aiSpeed, targetY - pongAIY);
  } else if (pongAIY > targetY + errorMargin) {
    pongAIY -= min(aiSpeed, pongAIY - targetY);
  }

  pongAIY = constrain(pongAIY, 22.0f, 55.0f);

  // Ball bewegen
  float oldX = pongBallX;
  pongBallX += pongDX;
  pongBallY += pongDY;

  // Wandreflexion
  if (pongBallY < PONG_TOP) {
    pongBallY = PONG_TOP;
    pongDY = abs(pongDY);
  } else if (pongBallY > PONG_BOTTOM) {
    pongBallY = PONG_BOTTOM;
    pongDY = -abs(pongDY);
  }

  // Kollision mit Spieler-Paddle (Ball fliegt nach links)
  if (pongDX < 0 &&
      pongBallX <= 9 &&
      oldX >= 7 &&
      abs(pongBallY - pongPlayerY) <= 9) {
    float hitOffset = (pongBallY - pongPlayerY) / 9.0f;
    pongBallX = 9;
    pongDX = abs(pongDX) + 0.08f;
    pongDY = constrain(pongDY + hitOffset * 0.75f, -2.3f, 2.3f);
  }

  // Kollision mit KI-Paddle
  if (pongDX > 0 &&
      pongBallX >= 118 &&
      oldX <= 121 &&
      abs(pongBallY - pongAIY) <= 9) {
    float hitOffset = (pongBallY - pongAIY) / 9.0f;
    pongBallX = 118;
    pongDX = -(abs(pongDX) + 0.04f);
    pongDY = constrain(pongDY + hitOffset * 0.55f, -2.2f, 2.2f);
  }

  pongDX = constrain(pongDX, -3.0f, 3.0f);

  // Spieler verfehlt den Ball
  if (pongBallX < 2) {
    pongLives--;

    if (pongLives <= 0) {
      finishGame();
      return;
    }

    resetPongBall();
    startPongCountdown();
    return;
  }

  // KI verfehlt den Ball: Punkt für den Spieler
  if (pongBallX > 126) {
    gameScore += 10;
    soundPoint();
    resetPongBall();
    startPongCountdown();
    return;
  }
}

void drawPong() {
  // Leben links und Score mittig in der gemeinsamen Kopfzeile.
  drawGameFrame(0, pongLives);

  display.fillRect(5, pongPlayerY - 7, 3, 14, SH110X_WHITE);
  display.fillRect(120, (int)pongAIY - 7, 3, 14, SH110X_WHITE);
  display.fillCircle((int)pongBallX, (int)pongBallY, 2, SH110X_WHITE);

  for (int y = 15; y < 62; y += 5) {
    display.drawPixel(64, y, SH110X_WHITE);
  }

  if (pongCountdown) {
    drawTransparentCountdown(pongCountdownStart, pongDX, 17);
  }

  display.display();
}

// =====================================================
// GAME INITIALIZATION
// =====================================================

void startGame() {
  gameScore = 0;
  soundStart();

  breakoutCountdown = false;
  pongCountdown = false;

  switch (selectedGame) {
    case 0: initSnake(); break;
    case 1: initTetris(); break;
    case 2: initInvaders(); break;
    case 3: initBreakout(); break;
    case 4: initMaze(); break;
    case 5: initFlappy(); break;
    case 6: initDino(); break;
    case 7: initPong(); break;
  }

  currentScreen = GAME_PLAYING;
}

// =====================================================
// RESULT ANIMATION / GAME OVER / WIN
// =====================================================

void startEndAnimation(int target) {
  saveHighscore();

  breakoutCountdown = false;
  pongCountdown = false;

  endTargetScreen = (ScreenState)target;
  endAnimationStart = millis();

  currentScreen = END_ANIMATION;
}

void finishGame() {
  if (currentScreen != GAME_PLAYING) return;
  soundLose();
  startEndAnimation(GAME_OVER);
}

void finishWin() {
  if (currentScreen != GAME_PLAYING) return;
  soundWin();
  startEndAnimation(YOU_WON);
}

void drawEndAnimation() {
  unsigned long elapsed = millis() - endAnimationStart;
  float progress = min(1.0f, elapsed / (float)END_ANIMATION_DURATION);

  display.clearDisplay();

  // Eine schlichte Linie wächst von der Mitte nach außen.
  int halfWidth = (int)(progress * 60.0f);
  display.drawFastHLine(64 - halfWidth, 32, halfWidth * 2 + 1, SH110X_WHITE);

  display.display();

  if (elapsed >= END_ANIMATION_DURATION) {
    currentScreen = endTargetScreen;
  }
}

// =====================================================
// GAME UPDATE
// =====================================================

void updateGame() {
  switch (selectedGame) {
    case 0:
      updateSnake();
      if (currentScreen == GAME_PLAYING) drawSnake();
      break;

    case 1:
      updateTetris();
      if (currentScreen == GAME_PLAYING) drawTetris();
      break;

    case 2:
      updateInvaders();
      if (currentScreen == GAME_PLAYING) drawInvaders();
      break;

    case 3:
      updateBreakout();
      if (currentScreen == GAME_PLAYING) drawBreakout();
      break;

    case 4:
      updateMaze();
      if (currentScreen == GAME_PLAYING) drawMaze();
      break;

    case 5:
      updateFlappy();
      if (currentScreen == GAME_PLAYING) drawFlappy();
      break;

    case 6:
      updateDino();
      if (currentScreen == GAME_PLAYING) drawDino();
      break;

    case 7:
      updatePong();
      if (currentScreen == GAME_PLAYING) drawPong();
      break;
  }
}

// =====================================================
// INPUT HANDLING
// =====================================================

void handleInput() {
  switch (currentScreen) {

    case GAME_SELECTOR:
      // Links/Rechts wechseln durch die Spiele wie im Referenzvideo.
      if (repeatPressed(1)) {
        selectedGame = (selectedGame - 1 + GAME_COUNT) % GAME_COUNT;
        soundNavigate();
      }

      if (repeatPressed(3)) {
        selectedGame = (selectedGame + 1) % GAME_COUNT;
        soundNavigate();
      }

      if (pressed(4)) {
        soundSelect();
        startGame();
      }

      // UP öffnet das Systemmenue, damit LEFT/RIGHT fuer die Spiele frei bleiben.
      if (pressed(0)) {
        soundSelect();
        currentScreen = SYSTEM_MENU;
      }
      break;

    case SYSTEM_MENU:
      if (repeatPressed(0)) {
        menuSelection = (menuSelection + 4) % 5;
        soundNavigate();
      }

      if (repeatPressed(2)) {
        menuSelection = (menuSelection + 1) % 5;
        soundNavigate();
      }

      if (pressed(4)) {
        soundSelect();
        switch (menuSelection) {
          case 0:
            highscorePage = 0;
            currentScreen = HIGHSCORES;
            break;

          case 1:
            currentScreen = SETTINGS;
            break;

          case 2:
            highscorePage = 0;
            currentScreen = CONTROLS;
            break;

          case 3:
            currentScreen = ABOUT;
            break;

          case 4:
            currentScreen = GAME_SELECTOR;
            break;
        }
      }

      if (pressed(1)) {
        currentScreen = GAME_SELECTOR;
      }
      break;

    case HIGHSCORES:
      if (pressed(3) || pressed(4)) {
        highscorePage = 1 - highscorePage;
      }

      if (pressed(1)) {
        currentScreen = SYSTEM_MENU;
      }
      break;

    case CONTROLS:
      if (pressed(3) || pressed(4)) {
        highscorePage = (highscorePage + 1) % 8;
      }

      if (pressed(1)) {
        currentScreen = SYSTEM_MENU;
      }
      break;

    case SETTINGS:
      if (pressed(4)) {
        soundEnabled = !soundEnabled;
        prefs.putBool("sound", soundEnabled);

        if (soundEnabled) {
          soundSelect();
        } else {
          noTone(BUZZER_PIN);
          digitalWrite(BUZZER_PIN, LOW);
        }
      }
      if (pressed(1)) currentScreen = SYSTEM_MENU;
      break;

    case ABOUT:
      if (pressed(1) || pressed(4)) {
        currentScreen = SYSTEM_MENU;
      }
      break;

    case GAME_PLAYING:
      if (buttons[4].longEvent) {
        playSound(440, 70);
        currentScreen = GAME_PAUSED;
        pauseSelection = 0;
      }
      break;

    case GAME_PAUSED:
      if (repeatPressed(0) || repeatPressed(2)) {
        pauseSelection = (pauseSelection + 1) % 2;
      }

      if (pressed(4)) {
        soundSelect();
        if (pauseSelection == 0) {
          currentScreen = GAME_PLAYING;
        } else {
          // QUIT beendet das Spiel direkt ohne YES/NO-Abfrage.
          currentScreen = GAME_SELECTOR;
        }
      }

      if (pressed(1)) {
        currentScreen = GAME_PLAYING;
      }
      break;

    case END_ANIMATION:
      // Absichtlich keine Eingaben während der einsekündigen Animation.
      break;

    case GAME_OVER:
    case YOU_WON:
      if (pressed(1)) {
        currentScreen = GAME_SELECTOR;
      }

      if (pressed(4)) {
        soundSelect();
        startGame();
      }
      break;

    default:
      break;
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  for (int i = 0; i < 5; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);

    buttons[i].stable = digitalRead(buttons[i].pin);
    buttons[i].lastReading = buttons[i].stable;
    buttons[i].lastChange = millis();
    buttons[i].held = false;
  }

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(0x3C, true)) {
    Serial.println("OLED konnte nicht gestartet werden!");

    while (true) {
      delay(100);
    }
  }

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.display();

  randomSeed(micros());

  initStars();
  loadHighscores();

  bootStart = millis();
  currentScreen = BOOT;

  selectedGame = 0;
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {
  updateButtons();
  handleInput();

  switch (currentScreen) {

    case BOOT:
      drawBoot();
      break;

    case GAME_SELECTOR:
      drawSelector();
      break;

    case SYSTEM_MENU:
      drawSystemMenu();
      break;

    case HIGHSCORES:
      drawHighscores();
      break;

    case SETTINGS:
      drawSettings();
      break;

    case CONTROLS:
      drawControls();
      break;

    case ABOUT:
      drawAbout();
      break;

    case GAME_PLAYING:
      updateGame();
      break;

    case GAME_PAUSED:
      drawPause();
      break;

    case END_ANIMATION:
      drawEndAnimation();
      break;

    case GAME_OVER:
      drawGameOver();
      break;

    case YOU_WON:
      drawYouWon();
      break;
  }

  delay(FRAME_DELAY);
}