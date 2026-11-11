#include "AuthClient.h"
#include "MainWindow.h"
#include "LogWindow.h"
#include "Preferences.h"
#include "Theme.h"
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QJsonDocument>

#include "FakeIdentity.h"
class AuthTests:public QObject {
    Q_OBJECT
private slots:
    void tokenOnlyLogin(){
        FakeIdentity service;AuthClient auth(nullptr,service.endpoints());QSignalSpy done(&auth,&AuthClient::authenticated),failed(&auth,&AuthClient::failed);
        auth.beginToken("test-minecraft-secret");QTRY_COMPARE(done.size(),1);QCOMPARE(service.requests,QStringList{"/profile"});QCOMPARE(auth.credentials().username,QString("Player_One"));QCOMPARE(auth.credentials().uuid,QString(clientId).remove('-'));
        service.rejectProfile=true;auth.beginToken("expired-secret");QTRY_COMPARE(failed.size(),1);QVERIFY(!auth.signedIn());QVERIFY(failed.last().first().toString().contains("abgelaufen"));QVERIFY(!failed.last().first().toString().contains("expired-secret"));
        service.rejectProfile=false;service.invalidProfile=true;auth.beginToken("bad-profile-token");QTRY_COMPARE(failed.size(),2);QVERIFY(!auth.signedIn());
        auth.beginToken("space token");QCOMPARE(failed.size(),3);QVERIFY(!auth.busy());
    }
    void validationAndCancellation(){
        FakeIdentity service;AuthClient auth(nullptr,service.endpoints());QSignalSpy failed(&auth,&AuthClient::failed);
        auth.begin({});QCOMPARE(failed.size(),1);QVERIFY(!auth.busy());QVERIFY(service.requests.isEmpty());
        QSignalSpy ready(&auth,&AuthClient::deviceReady);auth.begin(clientId);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(),1,2000);auth.cancel();QVERIFY(!auth.busy());
        QTest::qWait(5200);QCOMPARE(service.polls,0);QVERIFY(!auth.signedIn());
    }
    void pendingThenSuccess(){
        FakeIdentity service;service.pendingFirst=true;AuthClient auth(nullptr,service.endpoints());QSignalSpy done(&auth,&AuthClient::authenticated);
        auth.begin(clientId);QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,13000);
        QCOMPARE(service.polls,2);QVERIFY(!service.malformedForm);QVERIFY(auth.signedIn());QVERIFY(!auth.busy());
        QCOMPARE(auth.credentials().username,QString("Player_One"));QCOMPARE(auth.credentials().token,QString("test-minecraft-secret"));
        auth.signOut();QVERIFY(!auth.signedIn());QVERIFY(auth.credentials().token.isEmpty());
    }
    void deniedWithoutLeakingResponse(){
        FakeIdentity service;service.denyMinecraft=true;AuthClient auth(nullptr,service.endpoints());QSignalSpy failed(&auth,&AuthClient::failed);
        auth.begin(clientId);QTRY_COMPARE_WITH_TIMEOUT(failed.size(),1,8000);
        QVERIFY(failed.first().first().toString().contains("freigegeben"));QVERIFY(!failed.first().first().toString().contains("never-display-secret"));QVERIFY(!auth.signedIn());
    }
    void normalLaunchUsesAuthenticatedSession(){
        QTemporaryDir temporary;QVERIFY(temporary.isValid());const auto previous=qgetenv("APPDATA");
        qputenv("APPDATA",(temporary.path()+"/AppData").toUtf8());auto restore=qScopeGuard([previous]{qputenv("APPDATA",previous);});
        const auto root=Minecraft::defaultRoot();
        auto write=[&](const QString &relative,const QByteArray &contents){const auto path=QDir(root).filePath(relative);QDir().mkpath(QFileInfo(path).absolutePath());QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(contents);};
        write("versions/test/test.json",R"({"id":"test","mainClass":"test.Main","assetIndex":{"id":"test"},"javaVersion":{"majorVersion":21,"component":"test-runtime"},"arguments":{"game":["--username","${auth_player_name}","--uuid","${auth_uuid}","--accessToken","${auth_access_token}"]}})");
        write("versions/test/test.jar","test jar");write("assets/indexes/test.json",R"({"objects":{}})");
        const auto runtime=root+"/runtime/test-runtime/bin";QVERIFY(QDir().mkpath(runtime));QVERIFY(QFile::copy(qEnvironmentVariable("MOCK_JAVA_EXE"),runtime+"/java.exe"));
        const auto core=QCoreApplication::applicationDirPath()+"/Qt6Core.dll";if(QFileInfo::exists(core))QVERIFY(QFile::copy(core,runtime+"/Qt6Core.dll"));
        Preferences preferences;preferences.language="de";preferences.microsoftClientId=clientId;preferences.autoShowLogs=false;QVERIFY(preferences.save());
        FakeIdentity service;MainWindow window(service.endpoints());window.setAttribute(Qt::WA_DontShowOnScreen);window.findChild<LogWindow*>()->setAttribute(Qt::WA_DontShowOnScreen);window.show();
        auto *button=window.findChild<QPushButton*>("primary");QTRY_COMPARE_WITH_TIMEOUT(window.findChild<QComboBox*>("versions")->count(),1,12000);QVERIFY(!button->isEnabled());
        auto *auth=window.findChild<AuthClient*>();QVERIFY(auth);auth->begin(clientId);QTRY_VERIFY_WITH_TIMEOUT(auth->signedIn(),8000);QVERIFY(button->isEnabled());
        QCOMPARE(window.findChild<QLineEdit*>("username")->text(),QString("Player_One"));QCOMPARE(window.findChild<QLineEdit*>("token")->text(),QString());
        QTest::mouseClick(button,Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel*>("status")->text().contains("wurde beendet"),12000);
        auto *model=window.findChild<LogWindow*>()->findChild<QStandardItemModel*>();QString messages;for(int row=0;row<model->rowCount();++row)messages+=model->item(row,2)->text();
        QVERIFY(messages.contains("[REDACTED]"));QVERIFY(!messages.contains("test-minecraft-secret"));QVERIFY(!messages.contains("Player_One"));
        QFile prefs(Preferences::path());QVERIFY(prefs.open(QIODevice::ReadOnly));const auto saved=prefs.readAll();QVERIFY(!saved.contains("test-minecraft-secret"));QVERIFY(!saved.contains("Player_One"));
    }
};
int main(int argc,char **argv){QApplication app(argc,argv);app.setProperty("disableDiscordForTests",true);app.setProperty("disableAnimationsForTests",true);applyTheme(app);AuthTests tests;return QTest::qExec(&tests,argc,argv);}
#include "AuthTests.moc"
