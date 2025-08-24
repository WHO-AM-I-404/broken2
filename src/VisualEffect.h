#pragma once

#include <Windows.h>
#include <vector>
#include <string>
#include <random>
#include <mutex>

// Configuration
const int REFRESH_RATE = 3;
const int MAX_GLITCH_INTENSITY = 10000;
const int GLITCH_LINES = 2000;
const int MAX_GLITCH_BLOCKS = 1000;
const int SOUND_CHANCE = 1;
const int MAX_PARTICLES = 5000;
const int MAX_CORRUPTED_TEXTS = 100;

// Particle system
struct Particle {
    float x, y;
    float vx, vy;
    DWORD life;
    DWORD maxLife;
    COLORREF color;
    int size;
    int type;
    float rotation;
    float rotationSpeed;
};

// Corrupted text
struct CorruptedText {
    int x, y;
    std::wstring text;
    DWORD creationTime;
    float opacity;
    float driftX, driftY;
};

// Matrix rain
struct MatrixStream {
    int x;
    int y;
    int length;
    int speed;
    DWORD lastUpdate;
    std::wstring symbols;
};

// Advanced structures
struct FractalPoint {
    float x, y;
    float vx, vy;
    COLORREF color;
};

struct Wormhole {
    float centerX, centerY;
    float radius;
    float strength;
    DWORD creationTime;
};

// Function declarations
void CaptureScreen(HWND hwnd);
void ApplyColorShift(BYTE* pixels, int shift);
void ApplyScreenShake();
void ApplyCursorEffect();
void UpdateParticles();
void ApplyMeltingEffect(BYTE* originalPixels);
void ApplyTextCorruption();
void ApplyPixelSorting();
void ApplyStaticBars();
void ApplyInversionWaves();
void ApplyGlitchEffect();
void PlayGlitchSoundAsync();

// Global variables
extern HBITMAP hGlitchBitmap;
extern BYTE* pPixels;
extern int screenWidth, screenHeight;
extern BITMAPINFO bmi;
extern int intensityLevel;
extern DWORD startTime;
extern int cursorX, cursorY;
extern bool cursorVisible;
extern int screenShakeX, screenShakeY;
extern bool textCorruptionActive;
extern DWORD lastEffectTime;

// Containers
extern std::vector<Particle> particles;
extern std::vector<CorruptedText> corruptedTexts;
extern std::mutex g_mutex;

// Random number generation
extern std::random_device rd;
extern std::mt19937 gen;
extern std::uniform_int_distribution<> dis;
extern std::uniform_real_distribution<> disf;
