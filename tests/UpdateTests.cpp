#include "UpdatePanel.h"
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
class UpdateTests:public QObject {
 Q_OBJECT
 QJsonObject release(){return {{"tag_name","v1.1.0"},{"draft",false},{"prerelease",false},{"assets",QJsonArray{QJsonObject{{"name","Obsidian-Launcher-v1.1.0-setup.exe"},{"size",12345},{"digest","sha256:"+QString(64,'a')},{"browser_download_url","https://github.com/W00ky02/Obsidian-Launcher/releases/download/v1.1.0/Obsidian-Launcher-v1.1.0-setup.exe"}}}}};}
private slots:
 void downloadAndVerification(){
  QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));bool corrupt=false;QByteArray payload("MZ-synthetic-installer-never-executed");
  auto metadata=release();auto a=metadata["assets"].toArray().first().toObject();a["size"]=payload.size();a["digest"]="sha256:"+QString::fromLatin1(QCryptographicHash::hash(payload,QCryptographicHash::Sha256).toHex());metadata["assets"]=QJsonArray{a};
  connect(&server,&QTcpServer::newConnection,&server,[&]{auto *socket=server.nextPendingConnection();connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{auto data=socket->property("request").toByteArray()+socket->readAll();socket->setProperty("request",data);if(!data.contains("\r\n\r\n"))return;QByteArray body=data.startsWith("GET /release ")?QJsonDocument(metadata).toJson():corrupt?QByteArray(payload.size(),'x'):payload;socket->write("HTTP/1.1 200 OK\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();});});
  UpdatePanel panel([]{return false;},nullptr,QUrl("http://127.0.0.1:"+QString::number(server.serverPort())));auto *check=panel.findChild<QPushButton*>("checkUpdates");auto *download=panel.findChild<QPushButton*>("installUpdate");auto *status=panel.findChild<QLabel*>("updateStatus");
  check->click();QTRY_VERIFY(!download->isHidden());download->click();QTRY_COMPARE(download->text(),QString("Install update"));QVERIFY(status->text().contains("verified"));download->click();QVERIFY(status->text().contains("Finish your game"));
  corrupt=true;check->click();QTRY_COMPARE(download->text(),QString("Download update"));download->click();QTRY_VERIFY(status->text().contains("could not be completed"));QVERIFY(download->isHidden());
 }
 void stableRelease(){UpdatePanel::Release parsed;QVERIFY(UpdatePanel::parseRelease(release(),parsed));QCOMPARE(parsed.version,QString("1.1.0"));QVERIFY(QVersionNumber::fromString(parsed.version)>QVersionNumber::fromString(UpdatePanel::currentVersion));QVERIFY(QVersionNumber::fromString("1.10.0")>QVersionNumber::fromString("1.9.0"));}
 void rejectUnsafeMetadata(){
  UpdatePanel::Release parsed;
  for(const auto &key:QStringList{"draft","prerelease"}){auto o=release();o[key]=true;QVERIFY(!UpdatePanel::parseRelease(o,parsed));}
  for(const auto &tag:QStringList{"v2.3.0-beta.1","v1.1.0evil","1.1.0","v1"}){auto o=release();o["tag_name"]=tag;QVERIFY(!UpdatePanel::parseRelease(o,parsed));}
  for(const auto &url:QStringList{"http://github.com/W00ky02/Obsidian-Launcher/releases/download/v1.1.0/Obsidian-Launcher-v1.1.0-setup.exe","https://example.com/setup.exe","https://github.com/another/repo/setup.exe"}){auto o=release();auto a=o["assets"].toArray().first().toObject();a["browser_download_url"]=url;o["assets"]=QJsonArray{a};QVERIFY(!UpdatePanel::parseRelease(o,parsed));}
  for(const auto &digest:QStringList{"","sha256:wrong","sha256:"+QString(64,'g')}){auto o=release();auto a=o["assets"].toArray().first().toObject();a["digest"]=digest;o["assets"]=QJsonArray{a};QVERIFY(!UpdatePanel::parseRelease(o,parsed));}
  auto o=release();auto assets=o["assets"].toArray();assets.append(assets.first());o["assets"]=assets;QVERIFY(!UpdatePanel::parseRelease(o,parsed));
 }
 void languageAndInitialState(){UpdatePanel panel([]{return false;});QCOMPARE(panel.findChild<QPushButton*>("checkUpdates")->text(),QString("Check for updates"));QVERIFY(panel.findChild<QPushButton*>("installUpdate")->isHidden());panel.setLanguage("de");QCOMPARE(panel.findChild<QPushButton*>("checkUpdates")->text(),QString("Nach Updates suchen"));panel.setLanguage("en");QCOMPARE(panel.findChild<QPushButton*>("checkUpdates")->text(),QString("Check for updates"));}
};
int main(int argc,char **argv){QApplication app(argc,argv);app.setProperty("disableDiscordForTests",true);UpdateTests tests;return QTest::qExec(&tests,argc,argv);}
#include "UpdateTests.moc"
