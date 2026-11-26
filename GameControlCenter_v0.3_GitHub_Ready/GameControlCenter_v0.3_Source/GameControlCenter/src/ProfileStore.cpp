#include "ProfileStore.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QRegularExpression>

ProfileStore::ProfileStore()
{
    QDir().mkpath(profileDirectory());
}

QString ProfileStore::profileDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + "/profiles";
}

QString ProfileStore::profilePath(const QString& name) const
{
    QString safeName = name;
    safeName.replace(QRegularExpression(R"([\\\\/:*?"<>|])"), "_");
    return profileDirectory() + "/" + safeName + ".json";
}

QString ProfileStore::defaultProfileName() const
{
    return "CS2";
}

QStringList ProfileStore::profileNames() const
{
    QDir dir(profileDirectory());
    QStringList result;
    const QStringList files = dir.entryList({"*.json"}, QDir::Files, QDir::Name);
    for (const QString& file : files)
        result << file.left(file.size() - 5);
    return result;
}

QJsonObject ProfileStore::loadProfile(const QString& name) const
{
    QFile file(profilePath(name));
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

bool ProfileStore::saveProfile(const QString& name, const QJsonObject& data) const
{
    if (name.trimmed().isEmpty())
        return false;

    QFile file(profilePath(name));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    const QByteArray bytes=QJsonDocument(data).toJson(QJsonDocument::Indented);
    const qint64 written=file.write(bytes);
    file.flush();
    return written==bytes.size()&&file.error()==QFileDevice::NoError;
}
