#pragma once
#include <QString>
#include <QStringList>
#include <QMap>
struct Preferences {
    QString language="de", theme="dark", accent="mint", lastVersion;
    QString microsoftClientId;
    QString font="Segoe UI Variable", design="soft", officialLauncher;
    QStringList favorites;
    int textSize=14;
    int memoryMb=4096;
    bool autoShowLogs=true;
    bool discordPresence=true;
    bool animations=true;
    int sidebarWidth=202,contentSpacing=16;
    bool showHero=true,rememberWindow=false,favoritesFirst=true,readyOnly=false;
    bool versionMemory=false,customResolution=false,fullscreen=false,minimizeOnLaunch=false,logFollow=true;
    int resolutionWidth=1280,resolutionHeight=720,logLimit=8000,windowWidth=1360,windowHeight=900;
    QMap<QString,int> versionRam;
    static QString path();
    static Preferences load();
    bool save() const;
};
