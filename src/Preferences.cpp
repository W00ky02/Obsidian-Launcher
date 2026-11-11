#include "Preferences.h"
#include <QSettings>
#include <QDir>
#include <QFileInfo>
QString Preferences::path() {return QDir(qEnvironmentVariable("APPDATA")).filePath("ObsidianLauncher/preferences.ini");}
Preferences Preferences::load() {
    QSettings s(path(),QSettings::IniFormat); Preferences p;
    p.language=s.value("ui/language","de").toString(); if (p.language!="en") p.language="de";
    p.theme=s.value("ui/theme","dark").toString(); if (!QStringList{"dark","light","system"}.contains(p.theme)) p.theme="dark";
    p.accent=s.value("ui/accent","mint").toString(); if (!QStringList{"mint","blue","violet","amber","rose"}.contains(p.accent)) p.accent="mint";
    p.microsoftClientId=s.value("auth/clientId").toString();
    p.font=s.value("ui/font","Segoe UI Variable").toString(); if (!QStringList{"Segoe UI Variable","Segoe UI","Calibri","Arial"}.contains(p.font)) p.font="Segoe UI";
    p.design=s.value("ui/design","soft").toString(); if (!QStringList{"soft","sharp","contrast"}.contains(p.design)) p.design="soft";
    p.textSize=qBound(12,s.value("ui/textSize",14).toInt(),18);
    p.officialLauncher=s.value("launch/officialLauncher").toString(); p.favorites=s.value("launch/favorites").toStringList();
    p.memoryMb=qBound(1024,s.value("launch/memoryMb",4096).toInt(),32768);
    p.autoShowLogs=s.value("launch/autoShowLogs",true).toBool(); p.lastVersion=s.value("launch/lastVersion").toString();
    p.animations=s.value("ui/animations",true).toBool();p.sidebarWidth=qBound(176,s.value("ui/sidebarWidth",202).toInt(),240);p.contentSpacing=qBound(8,s.value("ui/contentSpacing",16).toInt(),24);
    p.discordPresence=s.value("social/discord",true).toBool();
    p.showHero=s.value("ui/showHero",true).toBool();p.rememberWindow=s.value("ui/rememberWindow",false).toBool();
    p.windowWidth=qBound(1020,s.value("ui/windowWidth",1360).toInt(),2560);p.windowHeight=qBound(730,s.value("ui/windowHeight",900).toInt(),1440);
    p.favoritesFirst=s.value("library/favoritesFirst",true).toBool();p.readyOnly=s.value("library/readyOnly",false).toBool();
    p.versionMemory=s.value("launch/versionMemory",false).toBool();p.customResolution=s.value("launch/customResolution",false).toBool();
    p.resolutionWidth=qBound(640,s.value("launch/width",1280).toInt(),7680);p.resolutionHeight=qBound(480,s.value("launch/height",720).toInt(),4320);
    p.fullscreen=s.value("launch/fullscreen",false).toBool();p.minimizeOnLaunch=s.value("launch/minimize",false).toBool();
    p.logLimit=qBound(1000,s.value("logs/limit",8000).toInt(),50000);p.logFollow=s.value("logs/follow",true).toBool();
    s.beginGroup("versionRam");for(const auto &key:s.childKeys())p.versionRam.insert(key,qBound(1024,s.value(key).toInt(),32768));s.endGroup();return p;
}
bool Preferences::save() const {
    if (!QDir().mkpath(QFileInfo(path()).absolutePath())) return false;
    QSettings s(path(),QSettings::IniFormat);
    s.setValue("auth/clientId",microsoftClientId);
    s.setValue("ui/font",font); s.setValue("ui/design",design); s.setValue("ui/textSize",textSize);
    s.setValue("launch/officialLauncher",officialLauncher); s.setValue("launch/favorites",favorites);
    s.setValue("ui/language",language); s.setValue("ui/theme",theme); s.setValue("ui/accent",accent);
    s.setValue("launch/memoryMb",memoryMb); s.setValue("launch/autoShowLogs",autoShowLogs); s.setValue("launch/lastVersion",lastVersion);
    s.setValue("ui/animations",animations);s.setValue("ui/sidebarWidth",sidebarWidth);s.setValue("ui/contentSpacing",contentSpacing);
    s.setValue("social/discord",discordPresence);
    s.setValue("ui/showHero",showHero);s.setValue("ui/rememberWindow",rememberWindow);s.setValue("ui/windowWidth",windowWidth);s.setValue("ui/windowHeight",windowHeight);
    s.setValue("library/favoritesFirst",favoritesFirst);s.setValue("library/readyOnly",readyOnly);s.setValue("launch/versionMemory",versionMemory);
    s.setValue("launch/customResolution",customResolution);s.setValue("launch/width",resolutionWidth);s.setValue("launch/height",resolutionHeight);s.setValue("launch/fullscreen",fullscreen);s.setValue("launch/minimize",minimizeOnLaunch);
    s.setValue("logs/limit",logLimit);s.setValue("logs/follow",logFollow);
    s.beginGroup("versionRam");s.remove("");for(auto it=versionRam.begin();it!=versionRam.end();++it)s.setValue(it.key(),it.value());s.endGroup();
    s.sync(); return s.status()==QSettings::NoError;
}
