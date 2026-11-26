#include "WindowsMonitor.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#include <TlHelp32.h>
#endif

bool WindowsMonitor::isProcessRunning(const QString& executableName)
{
#ifdef Q_OS_WIN
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    bool found = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (QString::compare(QString::fromWCharArray(entry.szExeFile),
                                 executableName, Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
#else
    Q_UNUSED(executableName);
    return false;
#endif
}

bool WindowsMonitor::isShadowPlayRunning()
{
    // NVIDIA process names vary by driver / NVIDIA App version.
    // This is a presence indicator only, not proof that Instant Replay is enabled.
    return isProcessRunning("NVIDIA Share.exe");
}
