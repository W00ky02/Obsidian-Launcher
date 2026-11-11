#include "DiscordPresence.h"
#include <QtTest>
#include <QLocalServer>
#include <QJsonDocument>
#include <QUuid>
#include <QtEndian>
#include <QElapsedTimer>

namespace {
QByteArray frame(quint32 op,const QByteArray &data){QByteArray result(8,Qt::Uninitialized);qToLittleEndian<quint32>(op,result.data());qToLittleEndian<quint32>(quint32(data.size()),result.data()+4);return result+data;}
QByteArray readFrame(QLocalSocket *socket,quint32 *opcode=nullptr){
    QElapsedTimer elapsed;elapsed.start();
    while(socket->bytesAvailable()<8 && elapsed.elapsed()<2000)QTest::qWait(5);
    if(socket->bytesAvailable()<8)return {};
    const auto header=socket->read(8);const auto size=qFromLittleEndian<quint32>(header.constData()+4);
    if(opcode)*opcode=qFromLittleEndian<quint32>(header.constData());
    while(socket->bytesAvailable()<size && elapsed.elapsed()<2000)QTest::qWait(5);
    return socket->bytesAvailable()>=size?socket->read(size):QByteArray();
}
QString pipeName(){return "obsidian-test-"+QUuid::createUuid().toString(QUuid::WithoutBraces);}
void ready(QLocalSocket *socket){socket->write(frame(1,R"({"cmd":"DISPATCH","evt":"READY"})"));socket->flush();}
QJsonObject message(QLocalSocket *socket){return QJsonDocument::fromJson(readFrame(socket)).object();}
}
class DiscordTests : public QObject {
    Q_OBJECT
private slots:
    void lifecycleAndPrivacy(){
        QLocalServer server;server.setSocketOptions(QLocalServer::UserAccessOption);const auto name=pipeName();QVERIFY(server.listen(name));
        DiscordPresence presence(nullptr,name);presence.setEnabled(true);
        QTRY_VERIFY(server.hasPendingConnections());auto *peer=server.nextPendingConnection();QVERIFY(peer);
        quint32 opcode=99;const auto hello=QJsonDocument::fromJson(readFrame(peer,&opcode)).object();QCOMPARE(opcode,0u);
        QCOMPARE(hello.value("v").toInt(),1);QCOMPARE(hello.value("client_id").toString(),QString("1556990126570078248"));
        const auto event=frame(1,R"({"cmd":"DISPATCH","evt":"READY"})");peer->write(event.left(5));peer->flush();QTest::qWait(20);QCOMPARE(peer->bytesAvailable(),0);
        peer->write(event.mid(5));peer->flush();
        auto request=message(peer);QCOMPARE(request.value("cmd").toString(),QString("SET_ACTIVITY"));
        auto args=request.value("args").toObject();QVERIFY(args.value("pid").toInteger()>0);
        auto activity=args.value("activity").toObject();QCOMPARE(activity.value("details").toString(),QString("Im Obsidian Launcher"));
        QVERIFY(activity.value("timestamps").toObject().value("start").toInteger()>0);
        peer->write(frame(1,QJsonDocument(QJsonObject{{"cmd","SET_ACTIVITY"},{"nonce",request.value("nonce")}}).toJson()));peer->flush();
        QTRY_COMPARE(presence.status(),QString("Mit Discord verbunden."));
        presence.setActivity(true,"fabric-loader-1.21.11","Fabric");request=message(peer);activity=request.value("args").toObject().value("activity").toObject();
        QCOMPARE(activity.value("details").toString(),QString("Minecraft mit Obsidian Launcher"));QCOMPARE(activity.value("state").toString(),QString("fabric-loader-1.21.11 · Fabric"));
        QCOMPARE(activity.keys(),QStringList({"details","state","timestamps","type"}));
        const auto started=activity.value("timestamps").toObject().value("start");
        presence.setActivity(true,"fabric-loader-1.21.11","Fabric");QTest::qWait(20);QCOMPARE(peer->bytesAvailable(),0);
        peer->write(frame(3,"ping-value"));peer->flush();QCOMPARE(readFrame(peer,&opcode),QByteArray("ping-value"));QCOMPARE(opcode,4u);
        peer->write(frame(1,R"({"cmd":"SET_ACTIVITY","evt":"ERROR","data":{"message":"private-server-error"}})"));peer->flush();
        QTRY_VERIFY(presence.status().contains("lehnt"));QVERIFY(!presence.status().contains("private-server-error"));
        peer->abort();QTRY_COMPARE(presence.status(),QString("Warte auf Discord …"));
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(),12000);peer=server.nextPendingConnection();message(peer);ready(peer);
        request=message(peer);activity=request.value("args").toObject().value("activity").toObject();QCOMPARE(activity.value("timestamps").toObject().value("start"),started);
        presence.setActivity(false);request=message(peer);QCOMPARE(request.value("args").toObject().value("activity").toObject().value("details").toString(),QString("Im Obsidian Launcher"));
        presence.setEnabled(false);request=message(peer);QVERIFY(request.value("args").toObject().value("activity").isNull());QCOMPARE(presence.status(),QString("Discord-Anzeige deaktiviert."));
    }
    void malformedResponse(){
        QLocalServer server;const auto name=pipeName();QVERIFY(server.listen(name));DiscordPresence presence(nullptr,name);presence.setEnabled(true);
        QTRY_VERIFY(server.hasPendingConnections());auto *peer=server.nextPendingConnection();message(peer);
        QByteArray invalid(8,Qt::Uninitialized);qToLittleEndian<quint32>(1,invalid.data());qToLittleEndian<quint32>(1000000,invalid.data()+4);
        peer->write(invalid);peer->flush();QTRY_COMPARE(presence.status(),QString("Ungültige Discord-Antwort."));presence.shutdown();
    }
};
QTEST_GUILESS_MAIN(DiscordTests)
#include "DiscordTests.moc"
