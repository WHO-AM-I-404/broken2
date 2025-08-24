#pragma once

#include <Windows.h>
#include <vector>
#include <string>

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
void ApplyMatrixRain();
void ApplyFractalNoise();
void ApplyScreenBurn();
void ApplyWormholeEffect();
void ApplyTemporalDistortion();

// Global variables
extern bool matrixRainActive;
extern bool fractalNoiseActive;
extern bool screenBurnActive;
extern bool wormholeEffectActive;
extern bool temporalDistortionActive;

// Containers
extern std::vector<MatrixStream> matrixStreams;
extern std::vector<FractalPoint> fractalPoints;
extern std::vector<Wormhole> wormholes;
