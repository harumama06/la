#pragma once

#include <QString>

class WindowsMonitor
{
public:
    static bool isProcessRunning(const QString& executableName);
    static bool isShadowPlayRunning();
};
