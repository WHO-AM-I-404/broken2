#include "AdvancedEffects.h"
#include "VisualEffects.h"
#include <Windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <random>

#pragma comment(lib, "gdiplus.lib")

// Global variables
bool matrixRainActive = false;
bool fractalNoiseActive = false;
bool screenBurnActive = false;
bool wormholeEffectActive = false;
bool temporalDistortionActive = false;

// Containers
std::vector<MatrixStream> matrixStreams;
std::vector<FractalPoint> fractalPoints;
std::vector<Wormhole> wormholes;

void ApplyMatrixRain() {
    if (!matrixRainActive) return;
    
    // Initialize matrix streams if empty
    if (matrixStreams.empty()) {
        for (int i = 0; i < screenWidth / 10; i++) {
            MatrixStream stream;
            stream.x = i * 10 + rand() % 10;
            stream.y = -rand() % 100;
            stream.length = 5 + rand() % 20;
            stream.speed = 1 + rand() % 5;
            stream.lastUpdate = GetTickCount();
            
            // Generate random symbols
            for (int j = 0; j < stream.length; j++) {
                if (rand() % 3 == 0) {
                    stream.symbols += static_cast<wchar_t>(0x30 + rand() % 10); // Numbers
                } else {
                    stream.symbols += static_cast<wchar_t>(0x30A0 + rand() % 96); // Japanese-like characters
                }
            }
            
            matrixStreams.push_back(stream);
        }
    }
    
    // Update and draw matrix streams
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);
    
    HFONT hFont = CreateFontW(
        14, 0, 0, 0, 
        FW_NORMAL, 
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Terminal"
    );
    
    SelectObject(hdcMem, hFont);
    SetBkMode(hdcMem, TRANSPARENT);
    
    DWORD currentTime = GetTickCount();
    
    for (auto& stream : matrixStreams) {
        if (currentTime - stream.lastUpdate > 1000 / stream.speed) {
            stream.y += stream.speed;
            stream.lastUpdate = currentTime;
            
            if (stream.y > screenHeight + stream.length * 20) {
                stream.y = -rand() % 100;
                stream.x = rand() % screenWidth;
            }
        }
        
        // Draw the stream
        for (int i = 0; i < stream.length; i++) {
            int yPos = stream.y - i * 20;
            if (yPos >= 0 && yPos < screenHeight) {
                // Fade color from bright green to dark green
                int green = 255 - (i * 255 / stream.length);
                COLORREF color = RGB(0, green, 0);
                SetTextColor(hdcMem, color);
                
                wchar_t symbol[2] = {stream.symbols[i], 0};
                TextOutW(hdcMem, stream.x, yPos, symbol, 1);
            }
        }
    }
    
    BITMAPINFOHEADER bmih = {};
    bmih.biSize = sizeof(BITMAPINFOHEADER);
    bmih.biWidth = screenWidth;
    bmih.biHeight = -screenHeight;
    bmih.biPlanes = 1;
    bmih.biBitCount = 32;
    bmih.biCompression = BI_RGB;
    bmih.biSizeImage = 0;
    bmih.biXPelsPerMeter = 0;
    bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = 0;
    bmih.biClrImportant = 0;
    
    GetDIBits(hdcMem, hBitmap, 0, screenHeight, pPixels, (BITMAPINFO*)&bmih, DIB_RGB_COLORS);
    
    DeleteObject(hFont);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

void ApplyFractalNoise() {
    if (!fractalNoiseActive) return;
    
    // Initialize fractal points if empty
    if (fractalPoints.empty()) {
        for (int i = 0; i < 1000; i++) {
            FractalPoint point;
            point.x = disf(gen) * screenWidth;
            point.y = disf(gen) * screenHeight;
            point.vx = (disf(gen) - 0.5f) * 5.0f;
            point.vy = (disf(gen) - 0.5f) * 5.0f;
            point.color = RGB(dis(gen), dis(gen), dis(gen));
            fractalPoints.push_back(point);
        }
    }
    
    // Update and draw fractal points
    for (auto& point : fractalPoints) {
        point.x += point.vx;
        point.y += point.vy;
        
        // Bounce off edges
        if (point.x < 0 || point.x >= screenWidth) point.vx = -point.vx;
        if (point.y < 0 || point.y >= screenHeight) point.vy = -point.vy;
        
        // Draw influence area
        int radius = 10 + intensityLevel * 2;
        for (int y = -radius; y <= radius; y++) {
            for (int x = -radius; x <= radius; x++) {
                int px = static_cast<int>(point.x) + x;
                int py = static_cast<int>(point.y) + y;
                
                if (px >= 0 && px < screenWidth && py >= 0 && py < screenHeight) {
                    float dist = sqrt(x*x + y*y);
                    if (dist <= radius) {
                        float influence = 1.0f - (dist / radius);
                        int pos = (py * screenWidth + px) * 4;
                        
                        if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                            pPixels[pos] = static_cast<BYTE>(pPixels[pos] * (1.0f - influence) + GetBValue(point.color) * influence);
                            pPixels[pos+1] = static_cast<BYTE>(pPixels[pos+1] * (1.0f - influence) + GetGValue(point.color) * influence);
                            pPixels[pos+2] = static_cast<BYTE>(pPixels[pos+2] * (1.0f - influence) + GetRValue(point.color) * influence);
                        }
                    }
                }
            }
        }
    }
}

void ApplyScreenBurn() {
    if (!screenBurnActive) return;
    
    static DWORD lastBurnTime = 0;
    DWORD currentTime = GetTickCount();
    
    if (currentTime - lastBurnTime > 100) {
        lastBurnTime = currentTime;
        
        // Create burn-in effect by slightly darkening the entire screen
        for (int i = 0; i < screenWidth * screenHeight * 4; i += 4) {
            if (i < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                pPixels[i] = std::max(0, pPixels[i] - 1);
                pPixels[i+1] = std::max(0, pPixels[i+1] - 1);
                pPixels[i+2] = std::max(0, pPixels[i+2] - 1);
            }
        }
        
        // Add random burn spots
        for (int i = 0; i < intensityLevel * 10; i++) {
            int x = rand() % screenWidth;
            int y = rand() % screenHeight;
            int radius = 5 + rand() % (intensityLevel * 5);
            
            for (int py = -radius; py <= radius; py++) {
                for (int px = -radius; px <= radius; px++) {
                    if (px*px + py*py <= radius*radius) {
                        int pxPos = x + px;
                        int pyPos = y + py;
                        
                        if (pxPos >= 0 && pxPos < screenWidth && pyPos >= 0 && pyPos < screenHeight) {
                            int pos = (pyPos * screenWidth + pxPos) * 4);
                            if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                                pPixels[pos] = std::max(0, pPixels[pos] - 10);
                                pPixels[pos+1] = std::max(0, pPixels[pos+1] - 10);
                                pPixels[pos+2] = std::max(0, pPixels[pos+2] - 10);
                            }
                        }
                    }
                }
            }
        }
    }
}

void ApplyWormholeEffect() {
    if (!wormholeEffectActive) return;
    
    // Create new wormholes occasionally
    if (wormholes.size() < 5 && rand() % 100 == 0) {
        Wormhole hole;
        hole.centerX = disf(gen) * screenWidth;
        hole.centerY = disf(gen) * screenHeight;
        hole.radius = 50 + disf(gen) * 100;
        hole.strength = 0.5f + disf(gen) * 2.0f;
        hole.creationTime = GetTickCount();
        wormholes.push_back(hole);
    }
    
    // Apply wormhole distortion
    std::unique_ptr<BYTE[]> pCopy(new BYTE[screenWidth * screenHeight * 4]);
    if (!pCopy) return;
    memcpy(pCopy.get(), pPixels, screenWidth * screenHeight * 4);
    
    for (auto& hole : wormholes) {
        for (int y = 0; y < screenHeight; y++) {
            for (int x = 0; x < screenWidth; x++) {
                float dx = static_cast<float>(x - hole.centerX);
                float dy = static_cast<float>(y - hole.centerY);
                float dist = sqrt(dx*dx + dy*dy);
                
                if (dist < hole.radius) {
                    float amount = pow(1.0f - (dist / hole.radius), 2.0f) * hole.strength;
                    int shiftX = static_cast<int>(dx * amount);
                    int shiftY = static_cast<int>(dy * amount);
                    
                    int srcX = x - shiftX;
                    int srcY = y - shiftY;
                    
                    if (srcX >= 0 && srcX < screenWidth && srcY >= 0 && srcY < screenHeight) {
                        int srcPos = (srcY * screenWidth + srcX) * 4;
                        int dstPos = (y * screenWidth + x) * 4;
                        
                        if (dstPos >= 0 && dstPos < static_cast<int>(screenWidth * screenHeight * 4) - 4 && 
                            srcPos >= 0 && srcPos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                            pPixels[dstPos] = pCopy[srcPos];
                            pPixels[dstPos + 1] = pCopy[srcPos + 1];
                            pPixels[dstPos + 2] = pCopy[srcPos + 2];
                        }
                    }
                }
            }
        }
    }
    
    // Remove old wormholes
    DWORD currentTime = GetTickCount();
    for (auto it = wormholes.begin(); it != wormholes.end(); ) {
        if (currentTime - it->creationTime > 10000) {
            it = wormholes.erase(it);
        } else {
            ++it;
        }
    }
}

void ApplyTemporalDistortion() {
    if (!temporalDistortionActive) return;
    
    static std::vector<std::vector<BYTE>> frameHistory;
    static int historyIndex = 0;
    
    // Initialize frame history
    if (frameHistory.empty()) {
        frameHistory.resize(10);
        for (auto& frame : frameHistory) {
            frame.resize(screenWidth * screenHeight * 4);
        }
    }
    
    // Store current frame
    memcpy(frameHistory[historyIndex].data(), pPixels, screenWidth * screenHeight * 4);
    historyIndex = (historyIndex + 1) % frameHistory.size();
    
    // Blend with previous frames
    for (int i = 0; i < screenWidth * screenHeight * 4; i += 4) {
        if (i < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
            int blendIndex = (historyIndex + rand() % (frameHistory.size() - 1)) % frameHistory.size();
            
            pPixels[i] = static_cast<BYTE>((pPixels[i] + frameHistory[blendIndex][i]) / 2);
            pPixels[i+1] = static_cast<BYTE>((pPixels[i+1] + frameHistory[blendIndex][i+1]) / 2);
            pPixels[i+2] = static_cast<BYTE>((pPixels[i+2] + frameHistory[blendIndex][i+2]) / 2);
        }
    }
}
