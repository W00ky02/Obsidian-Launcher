#include "Minecraft.h"
#include "NativeArchive.h"
#include "LogRedactor.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QDataStream>
#include <QProcess>
#include <zlib.h>

using namespace Minecraft;
namespace {
void write(const QString &path,const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size()) qFatal("Fixture write failed");
}
void profile(const QString &root,const QString &id,const QJsonObject &json) {write(root+"/versions/"+id+"/"+id+".json",QJsonDocument(json).toJson());}
QJsonObject base() {
    return {{"id","base"},{"mainClass","net.minecraft.client.main.Main"},{"type","release"},
        {"assetIndex",QJsonObject{{"id","test"}}},{"javaVersion",QJsonObject{{"majorVersion",21}}},
        {"arguments",QJsonObject{{"jvm",QJsonArray{"-cp","${classpath}","-Djava.library.path=${natives_directory}"}},
            {"game",QJsonArray{"--username","${auth_player_name}","--uuid","${auth_uuid}","--accessToken","${auth_access_token}","--gameDir","C:/wrong"}}}},
        {"libraries",QJsonArray{QJsonObject{{"name","test:lib:1.0"}}}}};
}
void fixture(const QString &root) {
    profile(root,"base",base()); write(root+"/versions/base/base.jar","jar");
    write(root+"/libraries/test/lib/1.0/lib-1.0.jar","lib");
    write(root+"/assets/indexes/test.json",QJsonDocument(QJsonObject{{"objects",QJsonObject{}}}).toJson());
}
bool fails(const std::function<void()> &action) {try {action();return false;} catch (const Error &) {return true;}}
QByteArray zip(const QByteArray &name,const QByteArray &contents,bool deflated) {
    QByteArray packed=contents;
    if (deflated) {
        packed.resize(256+contents.size()*2); z_stream stream{};
        if (deflateInit2(&stream,Z_DEFAULT_COMPRESSION,Z_DEFLATED,-MAX_WBITS,8,Z_DEFAULT_STRATEGY)!=Z_OK) qFatal("zlib init failed");
        stream.next_in=reinterpret_cast<Bytef*>(const_cast<char*>(contents.constData())); stream.avail_in=uInt(contents.size());
        stream.next_out=reinterpret_cast<Bytef*>(packed.data()); stream.avail_out=uInt(packed.size());
        if (deflate(&stream,Z_FINISH)!=Z_STREAM_END) qFatal("zlib deflate failed");
        packed.resize(qsizetype(stream.total_out)); deflateEnd(&stream);
    }
    const quint32 crc=quint32(crc32(0,reinterpret_cast<const Bytef*>(contents.constData()),uInt(contents.size())));
    QByteArray data; QDataStream s(&data,QIODevice::WriteOnly); s.setByteOrder(QDataStream::LittleEndian);
    s<<quint32(0x04034b50)<<quint16(20)<<quint16(0)<<quint16(deflated?8:0)<<quint16(0)<<quint16(0)<<crc<<quint32(packed.size())<<quint32(contents.size())<<quint16(name.size())<<quint16(0);
    s.writeRawData(name.constData(),int(name.size())); s.writeRawData(packed.constData(),int(packed.size()));
    const quint32 offset=quint32(data.size());
    s<<quint32(0x02014b50)<<quint16(20)<<quint16(20)<<quint16(0)<<quint16(deflated?8:0)<<quint16(0)<<quint16(0)<<crc<<quint32(packed.size())<<quint32(contents.size())<<quint16(name.size())<<quint16(0)<<quint16(0)<<quint16(0)<<quint16(0)<<quint32(0)<<quint32(0);
    s.writeRawData(name.constData(),int(name.size()));
    const quint32 length=quint32(data.size())-offset;
    s<<quint32(0x06054b50)<<quint16(0)<<quint16(0)<<quint16(1)<<quint16(1)<<length<<offset<<quint16(0); return data;
}
}
class LauncherTests:public QObject {
    Q_OBJECT
private slots:
    void inheritanceAndLoaders() {
        QTemporaryDir dir; const QString root=dir.path()+"/Spiel ä mit Leerzeichen"; fixture(root);
        const QMap<QString,QString> loaders={{"fabric","net.fabricmc.loader.impl.launch.knot.KnotClient"},{"quilt","org.quiltmc.loader.impl.launch.knot.KnotClient"},{"forge","cpw.mods.modlauncher.Launcher"},{"neoforge","net.neoforged.bootstraplauncher.BootstrapLauncher"}};
        for (auto it=loaders.begin();it!=loaders.end();++it) {
            profile(root,it.key(),{{"id",it.key()},{"inheritsFrom","base"},{"mainClass",it.value()},
                {"libraries",QJsonArray{QJsonObject{{"name","test:lib:2.0"}}}},
                {"arguments",QJsonObject{{"game",QJsonArray{"--loader","ok"}}}}});
            const auto p=resolve(root,it.key()); QCOMPARE(p.jarId,QString("base")); QCOMPARE(p.json["mainClass"].toString(),it.value());
            QCOMPARE(p.json["libraries"].toArray().size(),1); QCOMPARE(p.json["libraries"].toArray()[0].toObject()["name"].toString(),QString("test:lib:2.0"));
            QVERIFY(p.loader.compare(it.key(),Qt::CaseInsensitive)==0);
            QCOMPARE(p.json["arguments"].toObject()["game"].toArray().size(),10);
        }
        profile(root,"cycleA",{{"inheritsFrom","cycleB"}}); profile(root,"cycleB",{{"inheritsFrom","cycleA"}});
        QVERIFY(fails([&]{resolve(root,"cycleA");})); QVERIFY(fails([&]{resolve(root,"../base");}));
        profile(root,"missing",{{"inheritsFrom","absent"}}); QVERIFY(fails([&]{resolve(root,"missing");}));
    }
    void rulesAndCoordinates() {
        QVERIFY(allowed({})); QVERIFY(allowed({QJsonObject{{"action","allow"},{"os",QJsonObject{{"name","windows"}}}}}));
        QVERIFY(!allowed({QJsonObject{{"action","allow"},{"os",QJsonObject{{"name","osx"}}}}}));
        QVERIFY(!allowed({QJsonObject{{"action","allow"}},QJsonObject{{"action","disallow"},{"os",QJsonObject{{"arch","x86_64"}}}}}));
        QVERIFY(!allowed({QJsonObject{{"action","allow"},{"features",QJsonObject{{"is_demo_user",true}}}}}));
        QCOMPARE(mavenPath("net.example:thing:1.2:natives-windows"),QString("net/example/thing/1.2/thing-1.2-natives-windows.jar"));
        QCOMPARE(mavenPath("net.example:thing:1.2@zip"),QString("net/example/thing/1.2/thing-1.2.zip"));
        QVERIFY(fails([]{mavenPath("broken");}));
        QCOMPARE(legacyArguments("--username \"${auth_player_name}\" --empty \"\""),QStringList({"--username","${auth_player_name}","--empty",""}));
        QVERIFY(fails([]{legacyArguments("\"unfinished");}));
    }
    void preflightAndArguments() {
        QTemporaryDir dir; const QString root=dir.path()+"/Minecraft ü space"; fixture(root);
        auto plan=inspect(root,resolve(root,"base"),"C:/fake/java.exe"); QVERIFY(plan.errors.isEmpty());
        const Credentials c=validateCredentials({"Player_1","12345678-1234-1234-1234-123456789abc","secret-value"});
        const auto args=launchArguments(plan,c,"C:/temp natives",4096);
        QCOMPARE(args.count("--gameDir"),1); QCOMPARE(args[args.indexOf("--gameDir")+1],QDir::toNativeSeparators(root));
        QCOMPARE(args[args.indexOf("--accessToken")+1],c.token); QVERIFY(args.contains("-Xmx4096M")); QVERIFY(args.contains(plan.classpath));
        QVERIFY(!args.contains("C:/wrong")); validateAssets(plan);
        QFile::remove(root+"/libraries/test/lib/1.0/lib-1.0.jar"); QVERIFY(!inspect(root,resolve(root,"base"),"java").errors.isEmpty());
        auto json=base(); json["arguments"]=QJsonObject{{"game",QJsonArray{"--value","${unsupported}"}}}; profile(root,"unknown",json); write(root+"/versions/unknown/unknown.jar","jar");
        plan=inspect(root,resolve(root,"unknown"),"java"); QVERIFY(!plan.errors.isEmpty());
        QVERIFY(fails([]{validateCredentials({"Bad Name","bad",""});}));
    }
    void assetsAndNatives() {
        QTemporaryDir dir; fixture(dir.path()); auto plan=inspect(dir.path(),resolve(dir.path(),"base"),"java");
        write(dir.path()+"/assets/indexes/test.json",R"({"objects":{"missing.ogg":{"hash":"0000000000000000000000000000000000000000","size":4}}})");
        QVERIFY(fails([&]{validateAssets(plan);}));
        const QString jar=dir.path()+"/native.jar",destination=dir.path()+"/natives";
        for (bool deflated:{false,true}) {
            write(jar,zip("lwjgl.dll","native-test",deflated)); NativeArchive::extract(jar,destination);
            QFile file(destination+"/lwjgl.dll"); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),QByteArray("native-test"));
        }
        write(jar,zip("../escape.dll","escape",true)); QVERIFY(fails([&]{NativeArchive::extract(jar,destination);})); QVERIFY(!QFileInfo::exists(dir.path()+"/escape.dll"));
        auto corrupt=zip("bad.dll","data",true); corrupt[30+7]=char(corrupt[30+7]^0xff); write(jar,corrupt); QVERIFY(fails([&]{NativeArchive::extract(jar,destination);}));
        write(jar,zip("META-INF/signature","skip",false)); NativeArchive::extract(jar,destination); QVERIFY(!QFileInfo::exists(destination+"/META-INF/signature"));
    }
    void streamingRedaction() {
        const QString secret="token-with-long-value";
        for (qsizetype split=0;split<=secret.size();++split) {
            LogRedactor r; r.setSecrets({secret});
            const auto first=r.feed("stdout","before "+secret.left(split));
            const auto second=r.feed("stdout",secret.mid(split)+" after",true);
            const auto output=first+second;
            QCOMPARE(output,QString("before [REDACTED] after"));
        }
        LogRedactor r; r.setSecrets({secret}); QCOMPARE(r.feed("stdout","token-with"),QString());
        QCOMPARE(r.feed("stderr","ordinary output\n"),QString("ordinary output\n"));
        QCOMPARE(r.feed("stdout","",true),QString("token-with"));
    }
    void offlineIdentity() {QCOMPARE(offlineUuid("Notch"),QString("b50ad385829d3141a2167e7d7539ba7f"));QVERIFY(offlineUuid("Player")!=offlineUuid("player"));}
    void windowOptions() {
        QStringList args{"--username","Player","--width","800","--height","600","--width","900"};
        applyWindowOptions(args,1920,1080,true);
        QCOMPARE(args.count("--width"),1);QCOMPARE(args.count("--height"),1);
        QCOMPARE(args.value(args.indexOf("--width")+1),QString("1920"));
        QCOMPARE(args.value(args.indexOf("--height")+1),QString("1080"));
        QCOMPARE(args.count("--fullscreen"),1);QVERIFY(args.contains("Player"));
        const auto before=args;applyWindowOptions(args,0,0,true);QCOMPARE(args,before);
    }
    void childProcessPipes() {
        QProcess p; p.setProcessChannelMode(QProcess::SeparateChannels);
        p.start(QCoreApplication::applicationFilePath(),{"--child-probe","argument with spaces","secret-test"});
        QVERIFY(p.waitForStarted()); QVERIFY(p.waitForFinished()); QCOMPARE(p.exitCode(),0);
        auto out=p.readAllStandardOutput(),err=p.readAllStandardError(); out.replace("\r\n","\n"); err.replace("\r\n","\n");
        QCOMPARE(out,QByteArray("argument with spaces\nsecret-test\n")); QCOMPARE(err,QByteArray("stderr works\n"));
        LogRedactor r; r.setSecrets({"secret-test"}); QVERIFY(!r.feed("stdout",QString::fromUtf8(out),true).contains("secret-test"));
        p.start("C:/definitely-not-installed/java.exe",{}); QVERIFY(!p.waitForStarted()); QCOMPARE(p.error(),QProcess::FailedToStart);
    }
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    const auto args=app.arguments();
    if (args.contains("--scan-installed")) {
        QJsonArray result;
        for (const auto &entry:scan(defaultRoot())) result.append(QJsonObject{{"id",entry.id},{"loader",entry.loader},{"ready",entry.ready()},{"error",entry.error},{"java",entry.plan.java}});
        QFile report(args.value(args.indexOf("--scan-installed")+1));
        if (!report.open(QIODevice::WriteOnly)) return 1;
        report.write(QJsonDocument(result).toJson()); return 0;
    }
    if (args.contains("--child-probe")) {
        QFile out,err; out.open(stdout,QIODevice::WriteOnly); err.open(stderr,QIODevice::WriteOnly);
        out.write(args.value(2).toUtf8()+"\n"+args.value(3).toUtf8()+"\n"); out.flush(); err.write("stderr works\n"); err.flush(); return 0;
    }
    LauncherTests tests; return QTest::qExec(&tests,argc,argv);
}
#include "LauncherTests.moc"
