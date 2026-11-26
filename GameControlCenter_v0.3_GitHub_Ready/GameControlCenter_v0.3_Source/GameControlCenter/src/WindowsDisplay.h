#pragma once
#include <QString>
#include <QVector>
struct DisplayMode { int width=0; int height=0; int refresh=0; };
class WindowsDisplay {
public:
    static DisplayMode current();
    static QVector<DisplayMode> modes();
    static bool apply(const DisplayMode& mode);
};
