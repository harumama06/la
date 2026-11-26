#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>

class ProfileStore
{
public:
    ProfileStore();

    QStringList profileNames() const;
    QJsonObject loadProfile(const QString& name) const;
    bool saveProfile(const QString& name, const QJsonObject& data) const;
    QString defaultProfileName() const;

private:
    QString profileDirectory() const;
    QString profilePath(const QString& name) const;
};
