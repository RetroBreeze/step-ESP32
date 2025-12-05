#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>

// Display wiring (Seeed XIAO ESP32-C3 + 2.9" GxEPD2 panel)
static const uint8_t EPD_CS = D6;
static const uint8_t EPD_DC = D7;
static const uint8_t EPD_RST = D5;
static const uint8_t EPD_BUSY = D4;

// Button pins (active-low)
static const uint8_t BTN_LEFT = D0;
static const uint8_t BTN_RIGHT = D1;
static const uint8_t BTN_UP = D2;
static const uint8_t BTN_CLICK = D3;
static const uint8_t BTN_DOWN = D9;

// Piezo speaker pin
static const uint8_t SPEAKER_PIN = D8;

// Display driver instance
GxEPD2_BW<GxEPD2_290_T5, GxEPD2_290_T5::HEIGHT> display(GxEPD2_290_T5(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// Maze configuration
static const uint8_t MAZE_WIDTH = 20;
static const uint8_t MAZE_HEIGHT = 12;
static const uint8_t TILE_SIZE = 8;
static const uint8_t HUD_HEIGHT = 16;

struct Entity {
  uint8_t x;
  uint8_t y;
};

static const uint8_t maze[MAZE_HEIGHT][MAZE_WIDTH] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1},
    {1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1},
    {1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1},
    {1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1},
    {1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1},
    {1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

Entity player = {1, 1};
Entity enemy = {18, 10};
uint8_t lives = 3;

uint16_t mazeOffsetX = 0;
uint16_t mazeOffsetY = 0;

void drawHUD();
void drawMaze();
void renderMazeTiles();
void drawTile(uint8_t x, uint8_t y);
void drawPlayer(const Entity &pos);
void drawEnemy(const Entity &pos);
void updatePlayerPosition(const Entity &oldPos, const Entity &newPos);
void updateEnemyPosition(const Entity &oldPos, const Entity &newPos);
void handleCollision();
void playBeep();
bool isWall(uint8_t x, uint8_t y);
Entity randomEnemyMove(const Entity &current);

void setupOffsets() {
  mazeOffsetX = (display.width() - (MAZE_WIDTH * TILE_SIZE)) / 2;
  uint16_t availableHeight = display.height() - HUD_HEIGHT;
  uint16_t mazePixelHeight = MAZE_HEIGHT * TILE_SIZE;
  mazeOffsetY = HUD_HEIGHT + (availableHeight - mazePixelHeight) / 2;
}

uint16_t tilePixelX(uint8_t x) { return mazeOffsetX + x * TILE_SIZE; }
uint16_t tilePixelY(uint8_t y) { return mazeOffsetY + y * TILE_SIZE; }

void setup() {
  Serial.begin(115200);

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_CLICK, INPUT_PULLUP);

  pinMode(SPEAKER_PIN, OUTPUT);
  ledcAttachPin(SPEAKER_PIN, 0);

  display.init(115200, true, 2, false);
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);
  setupOffsets();

  randomSeed(analogRead(A0));

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawHUD();
    drawMaze();
    drawEnemy(enemy);
    drawPlayer(player);
  } while (display.nextPage());
}

void loop() {
  Entity oldPlayer = player;

  if (digitalRead(BTN_UP) == LOW && player.y > 0 && !isWall(player.x, player.y - 1)) {
    player.y -= 1;
  } else if (digitalRead(BTN_DOWN) == LOW && player.y + 1 < MAZE_HEIGHT && !isWall(player.x, player.y + 1)) {
    player.y += 1;
  } else if (digitalRead(BTN_LEFT) == LOW && player.x > 0 && !isWall(player.x - 1, player.y)) {
    player.x -= 1;
  } else if (digitalRead(BTN_RIGHT) == LOW && player.x + 1 < MAZE_WIDTH && !isWall(player.x + 1, player.y)) {
    player.x += 1;
  }

  if (player.x != oldPlayer.x || player.y != oldPlayer.y) {
    playBeep();
    updatePlayerPosition(oldPlayer, player);

    Entity oldEnemy = enemy;
    enemy = randomEnemyMove(enemy);
    updateEnemyPosition(oldEnemy, enemy);

    handleCollision();
  }

  delay(80);
}

bool isWall(uint8_t x, uint8_t y) { return maze[y][x] == 1; }

void playBeep() {
  ledcWriteTone(0, 2000);
  delay(25);
  ledcWriteTone(0, 0);
}

void drawHUD() {
  display.setPartialWindow(0, 0, display.width(), HUD_HEIGHT);
  display.firstPage();
  do {
    display.fillRect(0, 0, display.width(), HUD_HEIGHT, GxEPD_WHITE);
    display.setFont(NULL);
    display.setTextSize(1);
    display.setCursor(4, 12);
    display.print("L");
    display.print(lives);
    display.setCursor(display.width() - 40, 12);
    display.print("DEMO");
  } while (display.nextPage());
}

void drawTile(uint8_t x, uint8_t y) {
  uint16_t px = tilePixelX(x);
  uint16_t py = tilePixelY(y);
  uint16_t size = TILE_SIZE;
  if (isWall(x, y)) {
    display.fillRect(px, py, size, size, GxEPD_BLACK);
  } else {
    display.fillRect(px, py, size, size, GxEPD_WHITE);
  }
}

void renderMazeTiles() {
  for (uint8_t y = 0; y < MAZE_HEIGHT; y++) {
    for (uint8_t x = 0; x < MAZE_WIDTH; x++) {
      drawTile(x, y);
    }
  }
}

void drawMaze() {
  display.setPartialWindow(mazeOffsetX, mazeOffsetY, MAZE_WIDTH * TILE_SIZE, MAZE_HEIGHT * TILE_SIZE);
  display.firstPage();
  do {
    renderMazeTiles();
  } while (display.nextPage());
}

void drawPlayer(const Entity &pos) {
  uint16_t px = tilePixelX(pos.x);
  uint16_t py = tilePixelY(pos.y);
  display.fillRect(px + 1, py + 1, TILE_SIZE - 2, TILE_SIZE - 2, GxEPD_BLACK);
}

void drawEnemy(const Entity &pos) {
  uint16_t px = tilePixelX(pos.x);
  uint16_t py = tilePixelY(pos.y);
  display.drawRect(px + 1, py + 1, TILE_SIZE - 2, TILE_SIZE - 2, GxEPD_BLACK);
}

void updatePlayerPosition(const Entity &oldPos, const Entity &newPos) {
  uint16_t minX = min(oldPos.x, newPos.x);
  uint16_t minY = min(oldPos.y, newPos.y);
  uint16_t maxX = max(oldPos.x, newPos.x);
  uint16_t maxY = max(oldPos.y, newPos.y);

  uint16_t px = tilePixelX(minX);
  uint16_t py = tilePixelY(minY);
  uint16_t w = (maxX - minX + 1) * TILE_SIZE;
  uint16_t h = (maxY - minY + 1) * TILE_SIZE;

  display.setPartialWindow(px, py, w, h);
  display.firstPage();
  do {
    drawTile(oldPos.x, oldPos.y);
    drawTile(newPos.x, newPos.y);
    drawEnemy(enemy);
    drawPlayer(newPos);
  } while (display.nextPage());
}

void updateEnemyPosition(const Entity &oldPos, const Entity &newPos) {
  uint16_t minX = min(oldPos.x, newPos.x);
  uint16_t minY = min(oldPos.y, newPos.y);
  uint16_t maxX = max(oldPos.x, newPos.x);
  uint16_t maxY = max(oldPos.y, newPos.y);

  uint16_t px = tilePixelX(minX);
  uint16_t py = tilePixelY(minY);
  uint16_t w = (maxX - minX + 1) * TILE_SIZE;
  uint16_t h = (maxY - minY + 1) * TILE_SIZE;

  display.setPartialWindow(px, py, w, h);
  display.firstPage();
  do {
    drawTile(oldPos.x, oldPos.y);
    drawTile(newPos.x, newPos.y);
    drawPlayer(player);
    drawEnemy(newPos);
  } while (display.nextPage());
}

void handleCollision() {
  if (player.x == enemy.x && player.y == enemy.y) {
    if (lives > 0) {
      lives--;
    }
    player = {1, 1};
    drawHUD();

    display.setPartialWindow(tilePixelX(0), tilePixelY(0), MAZE_WIDTH * TILE_SIZE, MAZE_HEIGHT * TILE_SIZE);
    display.firstPage();
    do {
      renderMazeTiles();
      drawEnemy(enemy);
      drawPlayer(player);
    } while (display.nextPage());
  }
}

Entity randomEnemyMove(const Entity &current) {
  static const int8_t moves[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  Entity candidate = current;

  for (uint8_t attempt = 0; attempt < 10; attempt++) {
    uint8_t dir = random(0, 4);
    int8_t nx = current.x + moves[dir][0];
    int8_t ny = current.y + moves[dir][1];
    if (nx >= 0 && nx < MAZE_WIDTH && ny >= 0 && ny < MAZE_HEIGHT && !isWall(nx, ny)) {
      candidate.x = nx;
      candidate.y = ny;
      break;
    }
  }
  return candidate;
}
