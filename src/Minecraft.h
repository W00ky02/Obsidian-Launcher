#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QVector>
#include <QMap>

namespace Minecraft {
struct Error { QString message; };
struct Profile {
    QString id, loader, jarId;
    QJsonObject json;
};
struct Plan {
    Profile profile;
    QString root, java, classpath, assetIndex, assetsPath;
    QStringList natives;
    QStringList errors;
    int javaMajor = 8;
};
struct Entry { QString id, loader; Plan plan; QString error; bool ready() const { return error.isEmpty(); } };
struct Credentials { QString username, uuid, token; };
void applyWindowOptions(QStringList &arguments,int width,int height,bool fullscreen);
QString defaultRoot();
Profile resolve(const QString &root, const QString &id);
bool allowed(const QJsonArray &rules, const QString &arch = QStringLiteral("x86_64"));
QString mavenPath(const QString &coordinate);
QStringList argumentValues(const QJsonArray &arguments);
QStringList legacyArguments(const QString &text);
QString findJava(const QString &root, int major, const QString &component, const QString &overridePath = {});
Plan inspect(const QString &root, const Profile &profile, const QString &java = {});
QVector<Entry> scan(const QString &root, const QString &java = {});
void validateAssets(const Plan &plan);
void extractNatives(const Plan &plan, const QString &destination);
QString offlineUuid(const QString &name);
Credentials validateCredentials(Credentials credentials);
QStringList launchArguments(const Plan &plan, const Credentials &credentials, const QString &natives, int memoryMb);
}
