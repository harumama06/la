#pragma once
#include <QString>
#include <QVector>

struct AudioEndpoint {
    QString id;
    QString name;
    bool capture = false;
};
struct AudioSessionInfo {
    unsigned long processId = 0;
    QString processName;
    QString processPath;
    float volume = 1.0f;
    bool muted = false;
};

class WindowsAudio {
public:
    static QVector<AudioEndpoint> endpoints(bool capture);
    static QString defaultEndpointId(bool capture);
    static QString defaultEndpointName(bool capture);
    static bool setDefaultEndpoint(const QString& id, bool capture);
    static float defaultVolume(bool capture);
    static bool defaultMuted(bool capture);
    static bool setDefaultVolume(float volume, bool capture);
    static bool setDefaultMute(bool muted, bool capture);
    static QVector<AudioSessionInfo> sessions();
    static bool setProcessVolume(unsigned long processId, float volume);
    static bool setProcessMute(unsigned long processId, bool muted);
};
