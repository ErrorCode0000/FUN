/*
 * PROJECT: RED HORIZON - ULTIMATE HARD MODE
 * Platform: Linux Native (Raylib)
 * Derleme: g++ game.cpp -o red_horizon ./libraylib.a -lm -lpthread -ldl -lrt
 */

#include "raylib.h"
#include <vector>
#include <cmath>
#include <algorithm>

// Manuel Clamp Fonksiyonu (Hata riskini bitirmek için)
float MyClamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Renk Tanımları
#define COLOR_SHIP     (Color){ 240, 240, 255, 255 }
#define COLOR_BULLET   (Color){ 0, 255, 255, 255 }
#define COLOR_BOMB     (Color){ 255, 100, 0, 160 }

const int SCREEN_W = 800;
const int SCREEN_H = 600;
const float BOMB_COOLDOWN_MAX = 30.0f;
const float PLAYER_SPEED = 550.0f;
const float BULLET_SPEED = 1200.0f;

struct Bullet { Vector2 pos; bool active = true; };
struct Asteroid {
    Vector2 pos, vel;
    float size;
    int tier; // 3: Büyük, 2: Orta, 1: Küçük
    bool active = true;
};

// --- GLOBAL STATE ---
Vector2 shipPos = { 50.0f, 300.0f };
std::vector<Bullet> bullets;
std::vector<Asteroid> asteroids;
float shootCooldown = 0, bombTimer = 0, bombRadius = 0;
float screenShake = 0;
int score = 0;
bool isGameOver = false, isBombActive = false, isMouseMode = false;

void ResetEngine() {
    shipPos = { 50.0f, 300.0f };
    bullets.clear(); 
    asteroids.clear();
    score = 0; 
    isGameOver = false;
    shootCooldown = 0; 
    bombTimer = 0; 
    isBombActive = false; 
    screenShake = 0;
}

int main() {
    // Pencere ayarları
    InitWindow(SCREEN_W, SCREEN_H, "RED HORIZON: HARD MODE");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        // Ekran sarsıntısı sönümleme
        if (screenShake > 0) screenShake -= dt * 10.0f;

        if (!isGameOver) {
            // 1. INPUT & MODE TOGGLE
            if (IsKeyPressed(KEY_M)) isMouseMode = !isMouseMode;
            if (shootCooldown > 0) shootCooldown -= dt;
            if (bombTimer > 0) bombTimer -= dt;

            // Kontroller
            if (isMouseMode) {
                Vector2 mPos = GetMousePosition();
                shipPos.x = mPos.x - 15;
                shipPos.y = mPos.y - 10;
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && shootCooldown <= 0) {
                    bullets.push_back({ (Vector2){shipPos.x + 35, shipPos.y + 10}, true });
                    shootCooldown = 0.10f;
                }
            } else {
                if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    shipPos.y -= PLAYER_SPEED * dt;
                if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  shipPos.y += PLAYER_SPEED * dt;
                if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  shipPos.x -= PLAYER_SPEED * dt;
                if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) shipPos.x += PLAYER_SPEED * dt;
                if (IsKeyDown(KEY_SPACE) && shootCooldown <= 0) {
                    bullets.push_back({ (Vector2){shipPos.x + 35, shipPos.y + 10}, true });
                    shootCooldown = 0.10f;
                }
            }

            // Bomb (B tuşu veya Mouse Sağ Tık)
            if ((IsKeyPressed(KEY_B) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) && bombTimer <= 0) {
                isBombActive = true; 
                bombRadius = 10.0f; 
                bombTimer = BOMB_COOLDOWN_MAX;
                screenShake = 3.0f; // Patlama sarsıntısı
            }

            // Ekran Sınırları (MyClamp kullanımı)
            shipPos.x = MyClamp(shipPos.x, 0, SCREEN_W - 30);
            shipPos.y = MyClamp(shipPos.y, 0, SCREEN_H - 20);

            // 2. BOMB LOGIC
            if (isBombActive) {
                bombRadius += 1200.0f * dt;
                for (auto& a : asteroids) {
                    if (a.active && CheckCollisionCircles((Vector2){shipPos.x+15, shipPos.y+10}, bombRadius, a.pos, a.size)) {
                        a.active = false; 
                        score += 5;
                    }
                }
                if (bombRadius > SCREEN_W * 1.5f) isBombActive = false;
            }

            // 3. BULLET & COLLISION
            for (auto& b : bullets) {
                if (!b.active) continue;
                b.pos.x += BULLET_SPEED * dt;
                if (b.pos.x > SCREEN_W) b.active = false;

                for (auto& a : asteroids) {
                    if (a.active && CheckCollisionCircles(b.pos, 5, a.pos, a.size)) {
                        b.active = false; 
                        a.active = false;
                        score += (4 - a.tier) * 15;
                        screenShake = 1.0f;

                        // ASTEROID PARÇALANMA (Zorluk burada!)
                        if (a.tier > 1) {
                            for (int i = 0; i < 2; i++) {
                                asteroids.push_back({ a.pos, {a.vel.x * 1.4f, (float)GetRandomValue(-200, 200)}, a.size * 0.5f, a.tier - 1, true });
                            }
                        }
                    }
                }
            }

            // 4. ASTEROID SPAWNING
            int spawnRate = 40 - (score / 400); 
            if (spawnRate < 8) spawnRate = 8; // Limit

            if (GetRandomValue(0, spawnRate) == 0) {
                float speedBoost = (score / 150.0f) * 12.0f;
                asteroids.push_back({ 
                    {(float)SCREEN_W + 60, (float)GetRandomValue(0, SCREEN_H)}, 
                    {-(220.0f + speedBoost + GetRandomValue(0, 80)), (float)GetRandomValue(-100, 100)}, 
                    32.0f, 3, true 
                });
            }

            // 5. ASTEROID UPDATE
            for (auto& a : asteroids) {
                if (!a.active) continue;
                a.pos.x += a.vel.x * dt;
                a.pos.y += a.vel.y * dt;

                // Oyuncu ile çarpışma
                if (CheckCollisionCircles((Vector2){shipPos.x + 15, shipPos.y + 10}, 12, a.pos, a.size * 0.85f)) {
                    isGameOver = true;
                }
                if (a.pos.x < -100) a.active = false;
            }

            // Bellek temizliği
            bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const Bullet& b){ return !b.active; }), bullets.end());
            asteroids.erase(std::remove_if(asteroids.begin(), asteroids.end(), [](const Asteroid& a){ return !a.active; }), asteroids.end());

        } else {
            if (IsKeyPressed(KEY_ENTER)) ResetEngine();
        }

        // --- RENDER ---
        BeginDrawing();
        ClearBackground((Color){ 5, 5, 15, 255 }); 

        // Sarsıntı Efekti (BeginMode2D yerine manuel ofset)
        float offsetX = 0, offsetY = 0;
        if (screenShake > 0) {
            offsetX = GetRandomValue(-screenShake, screenShake);
            offsetY = GetRandomValue(-screenShake, screenShake);
        }

        // Objeleri Çiz
        if (isBombActive) DrawCircleLines(shipPos.x + 15 + offsetX, shipPos.y + 10 + offsetY, bombRadius, COLOR_BOMB);

        for (auto& b : bullets) {
            DrawLineEx((Vector2){b.pos.x + offsetX, b.pos.y + offsetY}, (Vector2){b.pos.x + 15 + offsetX, b.pos.y + offsetY}, 3.0f, COLOR_BULLET);
        }
        
        for (auto& a : asteroids) {
            Color aCol = (a.tier == 3) ? DARKGRAY : (a.tier == 2 ? MAROON : RED);
            DrawCircleV((Vector2){a.pos.x + offsetX, a.pos.y + offsetY}, a.size, aCol);
            DrawCircleLines(a.pos.x + offsetX, a.pos.y + offsetY, a.size, LIGHTGRAY);
        }

        if (!isGameOver) {
            DrawRectangleV((Vector2){shipPos.x + offsetX, shipPos.y + offsetY}, (Vector2){30, 20}, COLOR_SHIP);
            DrawRectangle(shipPos.x - 6 + offsetX, shipPos.y + 4 + offsetY, 6, 12, ORANGE); // İtici
        } else {
            DrawText("MISSION FAILED", SCREEN_W/2 - 120, SCREEN_H/2 - 40, 30, RED);
            DrawText("PRESS ENTER TO RESTART", SCREEN_W/2 - 140, SCREEN_H/2 + 10, 20, GRAY);
        }

        // --- UI ---
        DrawText(TextFormat("SCORE: %i", score), 20, 20, 25, GOLD);
        
        // Bomb Bar
        DrawRectangle(200, 575, 400, 14, (Color){30, 30, 30, 255});
        float pct = 1.0f - (bombTimer / BOMB_COOLDOWN_MAX);
        float safePct = MyClamp(pct, 0, 1);
        DrawRectangle(200, 575, (int)(400 * safePct), 14, (pct >= 1.0f ? GREEN : RED));
        DrawText("ULTRA-BOMB COOLDOWN", 320, 577, 12, WHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
