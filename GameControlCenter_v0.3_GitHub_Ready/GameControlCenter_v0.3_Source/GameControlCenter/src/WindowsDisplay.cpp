#include "WindowsDisplay.h"
#ifdef Q_OS_WIN
#include <Windows.h>
#endif
#include <algorithm>

DisplayMode WindowsDisplay::current() {
#ifdef Q_OS_WIN
    DEVMODEW dm{}; dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm))
        return {int(dm.dmPelsWidth), int(dm.dmPelsHeight), int(dm.dmDisplayFrequency)};
#endif
    return {};
}
QVector<DisplayMode> WindowsDisplay::modes() {
    QVector<DisplayMode> result;
#ifdef Q_OS_WIN
    DEVMODEW dm{};
    for (DWORD i=0; ; ++i) {
        dm = {}; dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsW(nullptr, i, &dm)) break;
        if (dm.dmPelsWidth < 640 || dm.dmPelsHeight < 480) continue;
        DisplayMode m{int(dm.dmPelsWidth), int(dm.dmPelsHeight), int(dm.dmDisplayFrequency)};
        if (std::none_of(result.begin(), result.end(), [&](const DisplayMode& x){ return x.width==m.width && x.height==m.height && x.refresh==m.refresh; })) result.push_back(m);
    }
    std::sort(result.begin(), result.end(), [](const DisplayMode& a, const DisplayMode& b){
        if (a.width != b.width) return a.width < b.width;
        if (a.height != b.height) return a.height < b.height;
        return a.refresh < b.refresh;
    });
#endif
    return result;
}
bool WindowsDisplay::apply(const DisplayMode& mode) {
#ifdef Q_OS_WIN
    DEVMODEW dm{}; dm.dmSize = sizeof(dm);
    dm.dmPelsWidth = DWORD(mode.width); dm.dmPelsHeight = DWORD(mode.height); dm.dmDisplayFrequency = DWORD(mode.refresh);
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
    // Test first so an unsupported mode does not leave a partial change.
    const LONG testResult=ChangeDisplaySettingsExW(nullptr,&dm,nullptr,CDS_TEST,nullptr);
    if(testResult!=DISP_CHANGE_SUCCESSFUL)return false;
    // Do not persist game resolution into the user's registry profile. GCC
    // restores the selected exit mode when CS2 closes.
    const LONG result=ChangeDisplaySettingsExW(nullptr,&dm,nullptr,0,nullptr);
    return result==DISP_CHANGE_SUCCESSFUL;
#else
    Q_UNUSED(mode); return false;
#endif
}
