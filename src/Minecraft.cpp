#include "Minecraft.h"
#include <QCryptographicHash>
#include "NativeArchive.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QSet>
#include <QRegularExpression>
#include <QProcess>
#include <QStandardPaths>
#include <QOperatingSystemVersion>
#include <algorithm>

namespace Minecraft {
namespace {
void require(bool ok, const QString &message) { if (!ok) throw Error{message}; }
QJsonObject readJson(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Datei fehlt/ist nicht lesbar: " + path);
    require(file.size() <= 32*1024*1024, "JSON zu groß: " + path);
    QJsonParseError error;
    const auto document=QJsonDocument::fromJson(file.readAll(), &error);
    require(error.error==QJsonParseError::NoError && document.isObject(), "Ungültiges JSON: " + path + " — " + error.errorString());
    return document.object();
}
QString safePath(const QString &root, QString relative) {
    relative.replace('\\','/');
    require(!relative.isEmpty() && !relative.startsWith('/') && !relative.contains(':') && !relative.split('/').contains("..") && !relative.contains(QChar(0)), "Ungültiger Profilpfad: " + relative);
    return QDir::cleanPath(QDir(root).filePath(relative));
}
void safeId(const QString &id) { require(QRegularExpression("^[A-Za-z0-9._+\\-]+$").match(id).hasMatch() && id!="..", "Ungültige Versions-ID: " + id); }
QString libraryKey(const QJsonObject &lib) {
    const auto parts=lib["name"].toString().split(':');
    if (parts.size()<3) return lib["name"].toString();
    return parts[0]+":"+parts[1]+":"+(parts.size()>3?parts[3]:QString());
}
Profile resolveRecursive(const QString &root, const QString &id, QSet<QString> &seen) {
    safeId(id);
    require(!seen.contains(id) && seen.size()<32, "Zyklische/zu tiefe Profilvererbung bei " + id);
    seen.insert(id);
    QJsonObject child=readJson(safePath(root,"versions/"+id+"/"+id+".json"));
    Profile result{id, {}, id, child};
    const QString parentId=child["inheritsFrom"].toString();
    if (!parentId.isEmpty()) {
        const auto parent=resolveRecursive(root,parentId,seen);
        result.jarId=parent.jarId;
        QJsonObject merged=parent.json;
        for (auto it=child.begin(); it!=child.end(); ++it) {
            if (it.key()!="arguments" && it.key()!="libraries" && !it.value().isNull()) merged[it.key()]=it.value();
        }
        QJsonObject arguments=parent.json["arguments"].toObject();
        if (!arguments.contains("game") && parent.json.contains("minecraftArguments")) {
            QJsonArray game;
            for (const auto &arg:legacyArguments(parent.json["minecraftArguments"].toString())) game.append(arg);
            arguments["game"]=game;
        }
        const auto additions=child["arguments"].toObject();
        for (auto it=additions.begin();it!=additions.end();++it) {
            auto values=arguments[it.key()].toArray();
            for (const auto value:it.value().toArray()) values.append(value);
            arguments[it.key()]=values;
        }
        // A legacy child supplies a complete replacement for legacy game arguments.
        if (child.contains("minecraftArguments") && !additions.contains("game")) arguments.remove("game");
        merged["arguments"]=arguments;
        QJsonArray libs=child["libraries"].toArray();
        QSet<QString> keys;
        for (const auto lib:libs) keys.insert(libraryKey(lib.toObject()));
        for (const auto lib:parent.json["libraries"].toArray()) if (!keys.contains(libraryKey(lib.toObject()))) libs.append(lib);
        merged["libraries"]=libs;
        result.json=merged;
    }
    if (child.contains("jar")) { result.jarId=child["jar"].toString(); safeId(result.jarId); }
    else if (QFileInfo::exists(safePath(root,"versions/"+id+"/"+id+".jar"))) result.jarId=id;
    const QString info=(id+" "+result.json["mainClass"].toString()+" "+QString::fromUtf8(QJsonDocument(child["libraries"].toArray()).toJson())).toLower();
    result.loader=info.contains("neoforge")?"NeoForge":info.contains("quilt")?"Quilt":info.contains("fabric")?"Fabric":(info.contains("forge") || info.contains("fml"))?"Forge":"Vanilla";
    result.json["id"]=id;
    return result;
}
QString artifactPath(const QJsonObject &lib) {
    const auto download=lib["downloads"].toObject();
    const QString path=download["artifact"].toObject()["path"].toString();
    if (!path.isEmpty()) return path;
    // Old native-only entries deliberately have no regular artifact.
    if (download.contains("classifiers") && !download.contains("artifact") && !lib["natives"].toObject().isEmpty()) return {};
    return mavenPath(lib["name"].toString());
}
QString nativePath(const QJsonObject &lib) {
    QString classifier=lib["natives"].toObject()["windows"].toString();
    if (classifier.isEmpty()) return {};
    classifier.replace("${arch}","64");
    QString path=lib["downloads"].toObject()["classifiers"].toObject()[classifier].toObject()["path"].toString();
    if (path.isEmpty()) {
        auto parts=lib["name"].toString().split(':');
        require(parts.size()>=3, "Ungültige Native-Koordinate");
        path=mavenPath(parts.mid(0,3).join(':')+":"+classifier);
    }
    return path;
}
bool clientLibrary(const QJsonObject &lib) {
    return (!lib.contains("clientreq") || lib["clientreq"].toBool()) && allowed(lib["rules"].toArray());
}
int javaMajor(const QString &exe) {
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(exe,{"-version"},QIODevice::ReadOnly);
    if (!process.waitForStarted(2000) || !process.waitForFinished(4000)) { process.kill(); process.waitForFinished(1000); return 0; }
    const QString output=QString::fromLocal8Bit(process.readAll());
    if (!output.contains("64-Bit",Qt::CaseInsensitive) && !output.contains("aarch64",Qt::CaseInsensitive)) return 0;
    const auto match=QRegularExpression("version\\s+\"(?:1\\.)?(\\d+)").match(output);
    return match.hasMatch()?match.captured(1).toInt():0;
}
QString substitute(QString input, const QMap<QString,QString> &variables) {
    const QRegularExpression expression("\\$\\{([^}]+)\\}");
    QString output;
    qsizetype previous=0;
    auto matches=expression.globalMatch(input);
    while (matches.hasNext()) {
        const auto m=matches.next(); const auto key=m.captured(1);
        require(variables.contains(key),"Nicht unterstützter Versionsparameter: ${"+key+"}");
        output+=input.mid(previous,m.capturedStart()-previous)+variables[key]; previous=m.capturedEnd();
    }
    return output+input.mid(previous);
}
}
QString defaultRoot() { return QDir(qEnvironmentVariable("APPDATA")).filePath(".minecraft"); }
Profile resolve(const QString &root, const QString &id) { QSet<QString> seen; return resolveRecursive(root,id,seen); }
bool allowed(const QJsonArray &rules, const QString &arch) {
    if (rules.isEmpty()) return true;
    bool allow=false;
    for (const auto value:rules) {
        const auto rule=value.toObject(), os=rule["os"].toObject();
        bool matches=true;
        if (os.contains("name") && os["name"].toString()!="windows") matches=false;
        if (os.contains("arch") && os["arch"].toString()!=arch) matches=false;
        if (os.contains("version")) {
            const auto v=QOperatingSystemVersion::current();
            const QString version=QString("%1.%2.%3").arg(v.majorVersion()).arg(v.minorVersion()).arg(v.microVersion());
            if (!QRegularExpression(os["version"].toString()).match(version).hasMatch()) matches=false;
        }
        const auto features=rule["features"].toObject();
        // No demo, quick play, custom resolution or other optional features enabled.
        for (auto it=features.begin();it!=features.end();++it) if (it.value().toBool()) matches=false;
        if (matches) allow=rule["action"].toString()=="allow";
    }
    return allow;
}
QString mavenPath(const QString &coordinate) {
    QString name=coordinate, extension="jar";
    if (name.contains('@')) { const auto split=name.split('@'); require(split.size()==2,"Ungültige Maven-Koordinate: "+coordinate); name=split[0]; extension=split[1]; }
    auto parts=name.split(':');
    require(parts.size()==3 || parts.size()==4,"Ungültige Maven-Koordinate: "+coordinate);
    QString group=parts[0]; group.replace('.','/');
    return group+"/"+parts[1]+"/"+parts[2]+"/"+parts[1]+"-"+parts[2]+(parts.size()==4?"-"+parts[3]:QString())+"."+extension;
}
QStringList argumentValues(const QJsonArray &arguments) {
    QStringList values;
    for (const auto arg:arguments) {
        if (arg.isString()) values.append(arg.toString());
        else {
            const auto object=arg.toObject();
            if (!allowed(object["rules"].toArray())) continue;
            const auto v=object["value"];
            if (v.isString()) values.append(v.toString());
            else for (const auto item:v.toArray()) values.append(item.toString());
        }
    }
    return values;
}
QStringList legacyArguments(const QString &text) {
    QStringList result; QString current; QChar quote; bool active=false;
    for (QChar ch:text) {
        if (!quote.isNull()) { if (ch==quote) quote=QChar(); else current+=ch; active=true; }
        else if (ch=='\"' || ch=='\'') { quote=ch; active=true; }
        else if (ch.isSpace()) { if (active) { result.append(current); current.clear(); active=false; } }
        else { current+=ch; active=true; }
    }
    require(quote.isNull(),"Ungültige Anführungszeichen in minecraftArguments");
    if (active) result.append(current);
    return result;
}
QString findJava(const QString &root, int major, const QString &component, const QString &overridePath) {
    if (!overridePath.isEmpty()) return javaMajor(overridePath)==major?overridePath:QString();
    QStringList candidates;
    const QString local=qEnvironmentVariable("LOCALAPPDATA"), program=qEnvironmentVariable("ProgramFiles");
    const QStringList roots={QDir(root).filePath("runtime/"+component),QDir(root).filePath("runtime"),
        local+"/Packages/Microsoft.4297127D64EC6_8wekyb3d8bbwe/LocalCache/Local/runtime",local+"/Minecraft Launcher/runtime",local+"/Programs/Minecraft Launcher/runtime",program+"/Minecraft Launcher/runtime",qEnvironmentVariable("ProgramFiles(x86)")+"/Minecraft Launcher/runtime",
        qEnvironmentVariable("JAVA_HOME"),program+"/Java",program+"/Eclipse Adoptium",program+"/Microsoft",program+"/Zulu"};
    QSet<QString> seen;
    for (const auto &base:roots) {
        if (base.isEmpty() || !QDir(base).exists()) continue;
        QDirIterator it(base,{"java.exe"},QDir::Files,QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path=it.next();
            if (!seen.contains(path)) { seen.insert(path); candidates.append(path); }
        }
    }
    const QString system=QStandardPaths::findExecutable("java.exe");
    if (!system.isEmpty() && !seen.contains(system)) candidates.append(system);
    for (const auto &path:candidates) if (javaMajor(path)==major) return path;
    return {};
}
Plan inspect(const QString &root, const Profile &profile, const QString &java) {
    Plan plan; plan.profile=profile; plan.root=root;
    const auto json=profile.json;
    plan.javaMajor=json["javaVersion"].toObject()["majorVersion"].toInt(8);
    plan.java=java;
    if (java.isEmpty()) plan.errors.append(QString("Passende 64-Bit Java-%1-Runtime fehlt. Java auswählen oder Version im offiziellen Launcher starten.").arg(plan.javaMajor));
    if (json["mainClass"].toString().isEmpty()) plan.errors.append("mainClass fehlt im Versionsprofil.");
    QStringList cp;
    for (const auto value:json["libraries"].toArray()) {
        const auto lib=value.toObject();
        if (!clientLibrary(lib)) continue;
        const auto relative=artifactPath(lib);
        if (!relative.isEmpty()) {
            const QString path=safePath(root,"libraries/"+relative);
            if (!QFileInfo::exists(path)) plan.errors.append("Bibliothek fehlt: "+relative);
            else if (!cp.contains(QDir::toNativeSeparators(path))) cp.append(QDir::toNativeSeparators(path));
        }
        const auto native=nativePath(lib);
        if (!native.isEmpty()) {
            const QString path=safePath(root,"libraries/"+native);
            if (!QFileInfo::exists(path)) plan.errors.append("Native-Bibliothek fehlt: "+native);
            else plan.natives.append(path);
        }
    }
    const QString jar=safePath(root,"versions/"+profile.jarId+"/"+profile.jarId+".jar");
    if (!QFileInfo::exists(jar)) plan.errors.append("Minecraft-JAR fehlt: "+profile.jarId);
    else cp.append(QDir::toNativeSeparators(jar));
    plan.classpath=cp.join(';');
    plan.assetIndex=json["assetIndex"].toObject()["id"].toString(json["assets"].toString());
    if (plan.assetIndex.isEmpty()) plan.errors.append("Asset-Index fehlt im Profil.");
    else if (!QFileInfo::exists(safePath(root,"assets/indexes/"+plan.assetIndex+".json"))) plan.errors.append("Asset-Index fehlt: "+plan.assetIndex);
    plan.assetsPath=QDir(root).filePath("assets");
    const auto logging=json["logging"].toObject()["client"].toObject();
    if (!logging.isEmpty()) {
        const QString file=logging["file"].toObject()["id"].toString();
        if (!QFileInfo::exists(safePath(root,"assets/log_configs/"+file))) plan.errors.append("Log-Konfiguration fehlt: "+file);
    }
    // Also preflight unsupported placeholders before enabling this entry.
    if (plan.errors.isEmpty()) {
        try { launchArguments(plan,{"Player","00000000000000000000000000000000","placeholder"},"native-preview",4096); }
        catch (const Error &error) { plan.errors.append(error.message); }
    }
    return plan;
}
QVector<Entry> scan(const QString &root, const QString &java) {
    QVector<Entry> entries;
    QMap<QString,QString> javaCache;
    const auto directories=QDir(QDir(root).filePath("versions")).entryList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name|QDir::Reversed);
    for (const auto &id:directories) {
        Entry entry; entry.id=id;
        try {
            const auto profile=resolve(root,id); entry.loader=profile.loader;
            const auto spec=profile.json["javaVersion"].toObject();
            const int major=spec["majorVersion"].toInt(8); const QString component=spec["component"].toString();
            const QString key=QString::number(major)+":"+component;
            if (!javaCache.contains(key)) javaCache[key]=findJava(root,major,component,java);
            entry.plan=inspect(root,profile,javaCache[key]); entry.error=entry.plan.errors.join('\n');
        } catch (const Error &e) { entry.error=e.message; }
        entries.append(entry);
    }
    std::stable_sort(entries.begin(),entries.end(),[](const Entry &a,const Entry &b){ return a.ready() && !b.ready(); });
    return entries;
}
void validateAssets(const Plan &plan) {
    const auto index=readJson(safePath(plan.root,"assets/indexes/"+plan.assetIndex+".json"));
    const auto objects=index["objects"].toObject();
    QStringList missing; int count=0;
    for (auto it=objects.begin();it!=objects.end();++it) {
        const auto object=it.value().toObject(); const auto hash=object["hash"].toString();
        require(QRegularExpression("^[a-f0-9]{40}$").match(hash).hasMatch(),"Ungültiger Asset-Hash im Index");
        const QString path=safePath(plan.root,"assets/objects/"+hash.left(2)+"/"+hash);
        if (!QFileInfo::exists(path) || QFileInfo(path).size()!=qint64(object["size"].toDouble())) { ++count; if (missing.size()<5) missing.append(it.key()); }
        // Legacy profiles need a materialized virtual/resources tree.
        if (index["virtual"].toBool() || index["map_to_resources"].toBool()) {
            const QString base=index["map_to_resources"].toBool()?QDir(plan.root).filePath("resources"):QDir(plan.root).filePath("assets/virtual/"+plan.assetIndex);
            const QString target=safePath(base,it.key());
            require(QFileInfo::exists(target),"Legacy-Asset fehlt: "+target+". Bitte einmal im offiziellen Launcher starten.");
        }
    }
    require(count==0,QString("%1 Assets fehlen/sind unvollständig (%2). Bitte Version im offiziellen Launcher reparieren.").arg(count).arg(missing.join(", ")));
}
void extractNatives(const Plan &plan, const QString &destination) {
    for (const auto value:plan.profile.json["libraries"].toArray()) {
        const auto lib=value.toObject(); if (!clientLibrary(lib)) continue;
        const auto relative=nativePath(lib); if (relative.isEmpty()) continue;
        QStringList excludes;
        for (const auto exclude:lib["extract"].toObject()["exclude"].toArray()) excludes.append(exclude.toString());
        NativeArchive::extract(safePath(plan.root,"libraries/"+relative),destination,excludes);
    }
}
Credentials validateCredentials(Credentials c) {
    c.username=c.username.trimmed(); c.uuid=c.uuid.trimmed().remove('-'); c.token=c.token.trimmed();
    require(QRegularExpression("^[A-Za-z0-9_]{1,16}$").match(c.username).hasMatch(),"Username: 1–16 Zeichen, Buchstaben, Zahlen oder Unterstrich.");
    require(QRegularExpression("^[A-Fa-f0-9]{32}$").match(c.uuid).hasMatch(),"UUID: 32 Hex-Zeichen, optional mit Bindestrichen.");
    require(!c.token.isEmpty() && !c.token.contains(QRegularExpression("[\\s\\x00-\\x1f]")),"Bitte einen gültigen Minecraft Access Token ohne Leerzeichen eingeben.");
    c.uuid=c.uuid.toLower(); return c;
}
QStringList launchArguments(const Plan &plan,const Credentials &c,const QString &natives,int memoryMb) {
    const auto json=plan.profile.json;
    QString gameAssets=QDir(plan.root).filePath("assets/virtual/"+plan.assetIndex);
    QMap<QString,QString> variables={
        {"auth_player_name",c.username},{"auth_uuid",c.uuid},{"auth_access_token",c.token},{"auth_session","token:"+c.token+":"+c.uuid},
        {"user_type",c.token=="0"?"legacy":"msa"},{"user_properties","{}"},{"profile_name",plan.profile.id},{"version_name",plan.profile.id},
        {"version_type",json["type"].toString("release")},{"game_directory",QDir::toNativeSeparators(plan.root)},
        {"assets_root",QDir::toNativeSeparators(plan.assetsPath)},{"game_assets",QDir::toNativeSeparators(gameAssets)},
        {"assets_index_name",plan.assetIndex},{"natives_directory",QDir::toNativeSeparators(natives)},
        {"library_directory",QDir::toNativeSeparators(QDir(plan.root).filePath("libraries"))},{"classpath",plan.classpath},
        {"classpath_separator",";"},{"launcher_name","Obsidian"},{"launcher_version","2.3.0"},{"clientid",""},{"auth_xuid",""}
    };
    const auto arguments=json["arguments"].toObject();
    QStringList jvm=argumentValues(arguments["jvm"].toArray());
    if (jvm.isEmpty()) jvm={"-Djava.library.path=${natives_directory}","-Dminecraft.launcher.brand=${launcher_name}","-Dminecraft.launcher.version=${launcher_version}","-cp","${classpath}"};
    // Always use our temp natives and classpath, including legacy/incomplete JVM profiles.
    bool cp=false, native=false;
    for (const auto &arg:jvm) { cp|=arg=="-cp" || arg=="-classpath" || arg=="--class-path"; native|=arg.startsWith("-Djava.library.path="); }
    if (!cp) jvm.append({"-cp","${classpath}"});
    if (!native) jvm.append("-Djava.library.path=${natives_directory}");
    jvm.erase(std::remove_if(jvm.begin(),jvm.end(),[](const QString &a){return a.startsWith("-Xmx") || a.startsWith("-Xms");}),jvm.end());
    // Avoid Minecraft's driver-workaround filename producing a heap dump with secrets.
    jvm.erase(std::remove_if(jvm.begin(),jvm.end(),[](const QString &a){return a.startsWith("-XX:HeapDumpPath=") || a=="-XX:+HeapDumpOnOutOfMemoryError";}),jvm.end());
    jvm.prepend(QString("-Xmx%1M").arg(qBound(1024,memoryMb,32768)));
    jvm.append("-XX:-HeapDumpOnOutOfMemoryError");
    const auto logging=json["logging"].toObject()["client"].toObject();
    if (!logging.isEmpty()) {
        auto logVariables=variables;
        logVariables["path"]=QDir::toNativeSeparators(safePath(plan.root,"assets/log_configs/"+logging["file"].toObject()["id"].toString()));
        jvm.append(substitute(logging["argument"].toString(),logVariables));
    }
    for (auto &arg:jvm) arg=substitute(arg,variables);
    jvm.append(json["mainClass"].toString());
    auto game=arguments.contains("game")?argumentValues(arguments["game"].toArray()):legacyArguments(json["minecraftArguments"].toString());
    require(!game.isEmpty(),"Minecraft-Spielargumente fehlen.");
    // Enforce the shared gameDir even if a custom profile hard-codes another path.
    for (qsizetype i=0;i<game.size();) {
        if (game[i]=="--gameDir") { game.removeAt(i); if (i<game.size()) game.removeAt(i); }
        else if (game[i].startsWith("--gameDir=")) game.removeAt(i);
        else ++i;
    }
    game.append({"--gameDir","${game_directory}"});
    for (const auto &arg:game) jvm.append(substitute(arg,variables));
    return jvm;
}
}

void Minecraft::applyWindowOptions(QStringList &arguments,int width,int height,bool fullscreen) {
    if(width>0 && height>0){for(const auto &key:QStringList{"--width","--height"}){int index;while((index=arguments.indexOf(key))>=0){arguments.removeAt(index);if(index<arguments.size())arguments.removeAt(index);}}arguments<<"--width"<<QString::number(width)<<"--height"<<QString::number(height);}
    if(fullscreen && !arguments.contains("--fullscreen"))arguments<<"--fullscreen";
}

QString Minecraft::offlineUuid(const QString &name) {
    auto bytes=QCryptographicHash::hash(("OfflinePlayer:"+name).toUtf8(),QCryptographicHash::Md5);
    bytes[6]=char((quint8(bytes[6]) & 0x0f) | 0x30);bytes[8]=char((quint8(bytes[8]) & 0x3f) | 0x80);
    return QString::fromLatin1(bytes.toHex());
}
