#include "WindowsAudio.h"
#ifdef Q_OS_WIN
#include <Windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <combaseapi.h>
#include <objbase.h>
#include <tlhelp32.h>
#include <QStringList>
#include <QtGlobal>
#include <string>

// IPolicyConfig is an undocumented Windows audio-policy interface used by the
// Windows Sound control panel. It is used here only to set the system default.
struct DeviceShareMode { DWORD mode; DWORD flags; };
MIDL_INTERFACE("f8679f50-850a-41cf-9c72-430f290290c8") IPolicyConfig : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, LONGLONG*, LONGLONG*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, LONGLONG*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, DeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, DeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};

namespace {
class ComScope {
public:
    ComScope() : hr_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComScope() { if (SUCCEEDED(hr_)) CoUninitialize(); }
    bool ok() const { return SUCCEEDED(hr_); }
private: HRESULT hr_;
};
IMMDeviceEnumerator* createEnumerator() {
    IMMDeviceEnumerator* e = nullptr;
    if (FAILED(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                               __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&e)))) return nullptr;
    return e;
}
QString endpointName(IMMDevice* device) {
    IPropertyStore* store = nullptr; PROPVARIANT value; PropVariantInit(&value); QString result;
    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store))) {
        if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR)
            result = QString::fromWCharArray(value.pwszVal);
        PropVariantClear(&value); store->Release();
    }
    return result;
}
QString processPath(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return {};
    wchar_t path[32768]{}; DWORD size = 32768;
    QString result;
    if (QueryFullProcessImageNameW(h, 0, path, &size)) result = QString::fromWCharArray(path);
    CloseHandle(h); return result;
}
QString processName(DWORD pid) {
    const QString path = processPath(pid);
    if (!path.isEmpty()) return path.section('\\', -1);
    return QString("Process %1").arg(pid);
}
}

QVector<AudioEndpoint> WindowsAudio::endpoints(bool capture) {
    QVector<AudioEndpoint> out; ComScope com; if (!com.ok()) return out;
    IMMDeviceEnumerator* e = createEnumerator(); if (!e) return out;
    IMMDeviceCollection* c = nullptr;
    EDataFlow flow = capture ? eCapture : eRender;
    if (SUCCEEDED(e->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &c))) {
        UINT count = 0; c->GetCount(&count);
        for (UINT i=0; i<count; ++i) {
            IMMDevice* d = nullptr; LPWSTR id = nullptr;
            if (SUCCEEDED(c->Item(i, &d)) && SUCCEEDED(d->GetId(&id))) {
                out.push_back({QString::fromWCharArray(id), endpointName(d), capture});
                CoTaskMemFree(id); d->Release();
            }
        }
        c->Release();
    }
    e->Release(); return out;
}
QString WindowsAudio::defaultEndpointId(bool capture) {
    ComScope com; if (!com.ok()) return {};
    IMMDeviceEnumerator* e = createEnumerator(); if (!e) return {};
    IMMDevice* d = nullptr; LPWSTR id = nullptr;
    QString result;
    if (SUCCEEDED(e->GetDefaultAudioEndpoint(capture ? eCapture : eRender, eConsole, &d)) && SUCCEEDED(d->GetId(&id))) {
        result = QString::fromWCharArray(id); CoTaskMemFree(id); d->Release();
    }
    e->Release(); return result;
}
QString WindowsAudio::defaultEndpointName(bool capture) {
    const QString id = defaultEndpointId(capture);
    for (const auto& ep : endpoints(capture)) if (ep.id == id) return ep.name;
    return id.isEmpty() ? QString("Unavailable") : id;
}
bool WindowsAudio::setDefaultEndpoint(const QString& id, bool capture) {
    if (id.isEmpty()) return false; ComScope com; if (!com.ok()) return false;
    IPolicyConfig* policy = nullptr;
    const CLSID clsid = {0x870af99c,0x171d,0x4f9e,{0xaf,0x0d,0xe6,0x3d,0xf4,0x0c,0x2b,0xc9}};
    HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_ALL, __uuidof(IPolicyConfig), reinterpret_cast<void**>(&policy));
    if (FAILED(hr)) return false;
    const std::wstring wid = id.toStdWString();
    bool ok = true;
    for (ERole role : {eConsole, eMultimedia, eCommunications}) if (FAILED(policy->SetDefaultEndpoint(wid.c_str(), role))) ok = false;
    policy->Release(); return ok;
}
float WindowsAudio::defaultVolume(bool capture) {
    ComScope com; if (!com.ok()) return 1.0f;
    IMMDeviceEnumerator* e = createEnumerator(); if (!e) return 1.0f;
    IMMDevice* d = nullptr; IAudioEndpointVolume* v = nullptr; float level = 1.0f;
    if (SUCCEEDED(e->GetDefaultAudioEndpoint(capture ? eCapture : eRender, eConsole, &d)) &&
        SUCCEEDED(d->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&v)))) {
        v->GetMasterVolumeLevelScalar(&level); v->Release(); d->Release();
    }
    e->Release(); return level;
}
bool WindowsAudio::defaultMuted(bool capture) {
    ComScope com; if (!com.ok()) return false;
    IMMDeviceEnumerator* e = createEnumerator(); if (!e) return false;
    IMMDevice* d = nullptr; IAudioEndpointVolume* v = nullptr; BOOL muted = FALSE;
    if (SUCCEEDED(e->GetDefaultAudioEndpoint(capture ? eCapture : eRender, eConsole, &d)) &&
        SUCCEEDED(d->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&v)))) {
        v->GetMute(&muted); v->Release(); d->Release();
    }
    e->Release(); return muted != FALSE;
}
bool WindowsAudio::setDefaultMute(bool muted, bool capture) {
    ComScope com; if (!com.ok()) return false;
    IMMDeviceEnumerator* e = createEnumerator(); if (!e) return false;
    IMMDevice* d = nullptr; IAudioEndpointVolume* v = nullptr; bool ok = false;
    if (SUCCEEDED(e->GetDefaultAudioEndpoint(capture ? eCapture : eRender, eConsole, &d)) &&
        SUCCEEDED(d->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&v)))) {
        ok = SUCCEEDED(v->SetMute(muted ? TRUE : FALSE, nullptr)); v->Release(); d->Release();
    }
    e->Release(); return ok;
}
bool WindowsAudio::setDefaultVolume(float volume, bool capture) {
    ComScope com; if (!com.ok()) return false;
    IMMDeviceEnumerator* e = createEnumerator(); if (!e) return false;
    IMMDevice* d = nullptr; IAudioEndpointVolume* v = nullptr; bool ok = false;
    if (SUCCEEDED(e->GetDefaultAudioEndpoint(capture ? eCapture : eRender, eConsole, &d)) &&
        SUCCEEDED(d->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&v)))) {
        ok = SUCCEEDED(v->SetMasterVolumeLevelScalar(qBound(0.0f, volume, 1.0f), nullptr));
        v->Release(); d->Release();
    }
    e->Release(); return ok;
}
QVector<AudioSessionInfo> WindowsAudio::sessions() {
    QVector<AudioSessionInfo> out; ComScope com; if (!com.ok()) return out;
    IMMDeviceEnumerator* e = createEnumerator(); if (!e) return out;
    IMMDevice* d = nullptr; IAudioSessionManager2* manager = nullptr; IAudioSessionEnumerator* se = nullptr;
    if (SUCCEEDED(e->GetDefaultAudioEndpoint(eRender, eConsole, &d)) &&
        SUCCEEDED(d->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&manager))) &&
        SUCCEEDED(manager->GetSessionEnumerator(&se))) {
        int count = 0; se->GetCount(&count);
        for (int i=0; i<count; ++i) {
            IAudioSessionControl* control = nullptr; IAudioSessionControl2* c2 = nullptr; ISimpleAudioVolume* vol = nullptr;
            if (SUCCEEDED(se->GetSession(i, &control)) && SUCCEEDED(control->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&c2)))) {
                DWORD pid = 0; c2->GetProcessId(&pid);
                float value = 1.0f; BOOL muted = FALSE;
                if (SUCCEEDED(control->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&vol)))) {
                    vol->GetMasterVolume(&value); vol->GetMute(&muted); vol->Release();
                }
                if (pid && pid != GetCurrentProcessId()) out.push_back({pid, processName(pid), processPath(pid), value, muted != FALSE});
                c2->Release();
            }
            if (control) control->Release();
        }
        se->Release(); manager->Release(); d->Release();
    }
    e->Release(); return out;
}
bool WindowsAudio::setProcessMute(unsigned long processId, bool muted) {
    ComScope com; if (!com.ok()) return false;
    IMMDeviceEnumerator* e = createEnumerator(); if (!e) return false;
    IMMDevice* d = nullptr; IAudioSessionManager2* manager = nullptr; IAudioSessionEnumerator* se = nullptr; bool changed = false;
    if (SUCCEEDED(e->GetDefaultAudioEndpoint(eRender, eConsole, &d)) &&
        SUCCEEDED(d->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&manager))) &&
        SUCCEEDED(manager->GetSessionEnumerator(&se))) {
        int count = 0; se->GetCount(&count);
        for (int i=0; i<count; ++i) {
            IAudioSessionControl* control = nullptr; IAudioSessionControl2* c2 = nullptr; ISimpleAudioVolume* vol = nullptr;
            if (SUCCEEDED(se->GetSession(i, &control)) && SUCCEEDED(control->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&c2)))) {
                DWORD pid = 0; c2->GetProcessId(&pid);
                if (pid == processId && SUCCEEDED(control->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&vol)))) {
                    changed = SUCCEEDED(vol->SetMute(muted ? TRUE : FALSE, nullptr)); vol->Release();
                }
                c2->Release();
            }
            if (control) control->Release();
        }
        se->Release(); manager->Release(); d->Release();
    }
    e->Release(); return changed;
}
bool WindowsAudio::setProcessVolume(unsigned long processId, float volume) {
    ComScope com; if (!com.ok()) return false;
    IMMDeviceEnumerator* e = createEnumerator(); if (!e) return false;
    IMMDevice* d = nullptr; IAudioSessionManager2* manager = nullptr; IAudioSessionEnumerator* se = nullptr; bool changed = false;
    if (SUCCEEDED(e->GetDefaultAudioEndpoint(eRender, eConsole, &d)) &&
        SUCCEEDED(d->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&manager))) &&
        SUCCEEDED(manager->GetSessionEnumerator(&se))) {
        int count = 0; se->GetCount(&count);
        for (int i=0; i<count; ++i) {
            IAudioSessionControl* control = nullptr; IAudioSessionControl2* c2 = nullptr; ISimpleAudioVolume* vol = nullptr;
            if (SUCCEEDED(se->GetSession(i, &control)) && SUCCEEDED(control->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&c2)))) {
                DWORD pid = 0; c2->GetProcessId(&pid);
                if (pid == processId && SUCCEEDED(control->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&vol)))) {
                    changed = SUCCEEDED(vol->SetMasterVolume(qBound(0.0f, volume, 1.0f), nullptr)); vol->Release();
                }
                c2->Release();
            }
            if (control) control->Release();
        }
        se->Release(); manager->Release(); d->Release();
    }
    e->Release(); return changed;
}
#else
QVector<AudioEndpoint> WindowsAudio::endpoints(bool) { return {}; }
QString WindowsAudio::defaultEndpointId(bool) { return {}; }
QString WindowsAudio::defaultEndpointName(bool) { return "Windows only"; }
bool WindowsAudio::setDefaultEndpoint(const QString&, bool) { return false; }
float WindowsAudio::defaultVolume(bool) { return 1.0f; }
bool WindowsAudio::setDefaultVolume(float, bool) { return false; }
bool WindowsAudio::defaultMuted(bool) { return false; }
bool WindowsAudio::setDefaultMute(bool, bool) { return false; }
QVector<AudioSessionInfo> WindowsAudio::sessions() { return {}; }
bool WindowsAudio::setProcessVolume(unsigned long, float) { return false; }
bool WindowsAudio::setProcessMute(unsigned long, bool) { return false; }
#endif
