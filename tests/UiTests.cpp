#include "MainWindow.h"
#include "LogWindow.h"
#include "Theme.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QCheckBox>
#include <QSpinBox>
#include <QStackedWidget>
#include <QListWidget>
#include <QTabWidget>
#include <QGraphicsOpacityEffect>
#include "Preferences.h"
#include "Ui.h"
#include "PathLabel.h"
#include "FakeIdentity.h"

class UiTests:public QObject {
    Q_OBJECT
private slots:
    void fullWorkflow() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        const QString root=temp.path()+"/AppData space/.minecraft";
        const auto old=qgetenv("APPDATA"); qputenv("APPDATA",QFileInfo(root).absolutePath().toUtf8());
        auto write=[&](const QString &relative,const QByteArray &data){
            const auto path=root+"/"+relative; QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(data),qint64(data.size()));
        };
        QJsonObject json{{"id","1.21.11"},{"mainClass","net.minecraft.client.main.Main"},{"assetIndex",QJsonObject{{"id","test"}}},
            {"javaVersion",QJsonObject{{"majorVersion",21},{"component","java-runtime-delta"}}},
            {"arguments",QJsonObject{{"game",QJsonArray{"--username","${auth_player_name}","--uuid","${auth_uuid}","--accessToken","${auth_access_token}"}}}}};
        write("versions/1.21.11/1.21.11.json",QJsonDocument(json).toJson()); write("versions/1.21.11/1.21.11.jar","synthetic jar");
        write("assets/indexes/test.json",R"({"objects":{}})");
        QDir().mkpath(root+"/runtime/java-runtime-delta/bin");
        QVERIFY(QFile::copy(qEnvironmentVariable("MOCK_JAVA_EXE"),root+"/runtime/java-runtime-delta/bin/java.exe"));
        const QString coreDll=QCoreApplication::applicationDirPath()+"/Qt6Core.dll";
        if (QFileInfo::exists(coreDll)) QVERIFY(QFile::copy(coreDll,root+"/runtime/java-runtime-delta/bin/Qt6Core.dll"));
        QCOMPARE(Minecraft::defaultRoot(),root);
        const auto inspected=Minecraft::scan(root);
        QCOMPARE(inspected.size(),1); QVERIFY2(inspected[0].ready(),qPrintable(inspected[0].error));
        Preferences initial;initial.language="de";QVERIFY(initial.save());
        FakeIdentity service;MainWindow window(service.endpoints()); window.setAttribute(Qt::WA_DontShowOnScreen);
        window.findChild<LogWindow*>()->setAttribute(Qt::WA_DontShowOnScreen); window.show();
        auto *launch=window.findChild<QPushButton*>("primary"); auto *status=window.findChild<QLabel*>("status");
        auto *name=window.findChild<QLineEdit*>("username"),*uuid=window.findChild<QLineEdit*>("uuid"),*token=window.findChild<QLineEdit*>("token");
        QVERIFY(launch && status && name && uuid && token);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QComboBox*>("versions")->count()==1,12000);
        QVERIFY(!launch->isEnabled());
        auto *list=window.findChild<QListWidget*>("libraryList");QVERIFY(list);QCOMPARE(list->count(),1);
        auto *search=window.findChild<QLineEdit*>("versionSearch");search->setText("nothing-installed");QCOMPARE(list->count(),0);search->clear();QCOMPARE(list->count(),1);
        auto *filter=window.findChild<QComboBox*>("loaderFilter");filter->setCurrentIndex(filter->findData("Fabric"));QCOMPARE(list->count(),0);filter->setCurrentIndex(0);QCOMPARE(list->count(),1);
        QTest::mouseClick(window.findChild<QPushButton*>("accountNav"),Qt::LeftButton);
        auto *modes=window.findChild<QComboBox*>("launchMode");
        QCOMPARE(modes->count(),2);QCOMPARE(modes->findData("normal"),-1);
        QCOMPARE(modes->currentData().toString(),QString("custom"));
        QTest::mouseClick(window.findChild<QPushButton*>("playNav"),Qt::LeftButton);
        const QString preview=qEnvironmentVariable("OBSIDIAN_PREVIEW_DIR");
        if (!preview.isEmpty()) {QDir().mkpath(preview); QVERIFY(window.grab().save(preview+"/launcher-preview.png"));}
        if (!preview.isEmpty()) {
            QTest::mouseClick(window.findChild<QPushButton*>("settingsNav"),Qt::LeftButton);
            auto *tabs=window.findChild<QTabWidget*>("settingsTabs");
            for(int i=0;i<tabs->count();++i){tabs->setCurrentIndex(i);QApplication::processEvents();QVERIFY(window.grab().save(preview+QString("/settings-dark-%1.png").arg(i)));}
            tabs->setCurrentIndex(0);
            QTest::mouseClick(window.findChild<QPushButton*>("toolsNav"),Qt::LeftButton);QApplication::processEvents();QVERIFY(window.grab().save(preview+"/tools-preview.png"));
            QTest::mouseClick(window.findChild<QPushButton*>("playNav"),Qt::LeftButton);
        }
        window.findChild<QComboBox*>("launchMode")->setCurrentIndex(0);
        QTest::mouseClick(launch,Qt::LeftButton); QVERIFY(!launch->isEnabled());
        QTest::mouseClick(window.findChild<QPushButton*>("accountNav"),Qt::LeftButton);
        QTRY_VERIFY(token->isVisible());QVERIFY(!name->isVisible());QVERIFY(!uuid->isVisible());QApplication::processEvents();
        if (!preview.isEmpty()) QVERIFY(window.grab().save(preview+"/account-preview.png"));
        auto connectToken=[&](const QString &value){token->setText(value);QTest::mouseClick(window.findChild<QPushButton*>("tokenSignIn"),Qt::LeftButton);};
        connectToken("synthetic-secret-token");QTRY_VERIFY(launch->isEnabled());QCOMPARE(name->text(),QString("Player_One"));QVERIFY(token->text().isEmpty());
        QTest::mouseClick(launch,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(token->text().isEmpty(),12000);
        QTRY_VERIFY_WITH_TIMEOUT(launch->isEnabled(),12000);
        QVERIFY(!status->property("error").toBool());
        auto *logs=window.findChild<LogWindow*>(); QVERIFY(logs && logs->isVisible());
        auto *model=logs->findChild<QStandardItemModel*>(); QVERIFY(model);
        QString messages; bool stdoutFound=false,stderrFound=false;
        for (int row=0;row<model->rowCount();++row) {
            messages+=model->item(row,2)->text()+"\n";
            stdoutFound|=model->item(row,1)->text()=="stdout"; stderrFound|=model->item(row,1)->text()=="stderr";
        }
        QVERIFY(logs->exportText().contains("[REDACTED]"));QVERIFY(!logs->exportText().contains("synthetic-secret-token"));
        QVERIFY(stdoutFound && stderrFound); QVERIFY(messages.contains("[REDACTED]")); QVERIFY(!messages.contains("synthetic-secret-token"));
        if (!preview.isEmpty()) QVERIFY(logs->grab().save(preview+"/logs-preview.png"));
        connectToken("another-test-token");QTRY_COMPARE(name->text(),QString("FailPlayer"));QTest::mouseClick(launch,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(token->text().isEmpty(),12000); QTRY_VERIFY_WITH_TIMEOUT(launch->isEnabled(),12000);
        QVERIFY(status->property("error").toBool()); QVERIFY(status->text().contains("42"));
        auto *settingsNav=window.findChild<QPushButton*>("settingsNav");QTest::mouseClick(settingsNav,Qt::LeftButton);
        QCOMPARE(window.findChild<QStackedWidget*>("pages")->currentIndex(),1);
        window.findChild<QComboBox*>("themeChoice")->setCurrentIndex(1);QVERIFY(!qApp->property("themeDark").toBool());
        window.findChild<QComboBox*>("languageChoice")->setCurrentIndex(1);QCOMPARE(settingsNav->text(),QString("Settings"));
        QVERIFY(status->text().contains("Minecraft exited with an error"));
        QCOMPARE(Ui::text("Bibliothek fehlt: example.jar"),QString("Missing library: example.jar"));
        QCOMPARE(Ui::text("Fabric  ·  Java 21  ·  Startbereit\nC:/Example/java.exe"),QString("Fabric  ·  Java 21  ·  Ready\nC:/Example/java.exe"));
        window.findChild<QSpinBox*>("memory")->setValue(6144);window.findChild<QCheckBox*>("autoLogs")->setChecked(false);
        window.findChild<QComboBox*>("fontChoice")->setCurrentIndex(2);window.findChild<QComboBox*>("designChoice")->setCurrentIndex(1);window.findChild<QSpinBox*>("textSizeChoice")->setValue(16);
        QTest::mouseClick(window.findChild<QPushButton*>("favoriteButton"),Qt::LeftButton);
        const auto prefs=Preferences::load();QVERIFY(prefs.favorites.contains("1.21.11"));QCOMPARE(prefs.font,QString("Calibri"));QCOMPARE(prefs.design,QString("sharp"));QCOMPARE(prefs.textSize,16);QCOMPARE(prefs.language,QString("en"));QCOMPARE(prefs.theme,QString("light"));QCOMPARE(prefs.memoryMb,6144);QVERIFY(!prefs.autoShowLogs);
        QFile preferencesFile(Preferences::path());QVERIFY(preferencesFile.open(QIODevice::ReadOnly));const auto saved=preferencesFile.readAll();preferencesFile.close();
        QVERIFY(!saved.contains("synthetic-secret-token"));QVERIFY(!saved.contains("another-test-token"));QVERIFY(!saved.contains("Player_One"));QVERIFY(!saved.contains("12345678"));
        if(!preview.isEmpty()) {QApplication::processEvents();QVERIFY(window.grab().save(preview+"/settings-preview.png"));}
        window.findChild<QCheckBox*>("versionMemory")->setChecked(true);window.findChild<QSpinBox*>("memory")->setValue(8192);
        window.findChild<QCheckBox*>("customResolution")->setChecked(true);window.findChild<QSpinBox*>("resolutionWidth")->setValue(1920);window.findChild<QSpinBox*>("resolutionHeight")->setValue(1080);
        window.findChild<QCheckBox*>("fullscreen")->setChecked(true);window.findChild<QCheckBox*>("logFollow")->setChecked(false);window.findChild<QSpinBox*>("logLimit")->setValue(1000);
        window.findChild<QCheckBox*>("discordPresence")->setChecked(false);QVERIFY(!Preferences::load().discordPresence);
        window.findChild<QCheckBox*>("animations")->setChecked(false);window.findChild<QSpinBox*>("sidebarSize")->setValue(180);window.findChild<QSpinBox*>("contentSpacing")->setValue(12);QVERIFY(!Preferences::load().animations);QCOMPARE(Preferences::load().sidebarWidth,180);
        const auto extra=Preferences::load();QCOMPARE(extra.versionRam.value("1.21.11"),8192);QCOMPARE(extra.memoryMb,6144);QVERIFY(extra.customResolution && extra.fullscreen);QCOMPARE(extra.resolutionWidth,1920);QCOMPARE(extra.resolutionHeight,1080);QVERIFY(!extra.logFollow);QCOMPARE(extra.logLimit,1000);
        logs->hide();connectToken("settings-test-token");QTRY_COMPARE(name->text(),QString("Player_One"));QTest::mouseClick(launch,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(token->text().isEmpty(),12000);QTRY_VERIFY_WITH_TIMEOUT(launch->isEnabled(),12000);QVERIFY(!logs->isVisible());
        window.findChild<QComboBox*>("launchMode")->setCurrentIndex(0);QVERIFY(!name->isVisible());QTest::mouseClick(window.findChild<QPushButton*>("signOutButton"),Qt::LeftButton);QVERIFY(!launch->isEnabled());
        const auto requestsBefore=service.requests.size();window.findChild<QComboBox*>("launchMode")->setCurrentIndex(1);QVERIFY(!launch->isEnabled());
        window.findChild<QLineEdit*>("offlineName")->setText("Player_Offline");QVERIFY(launch->isEnabled());QTest::mouseClick(window.findChild<QPushButton*>("accountNav"),Qt::LeftButton);QApplication::processEvents();if(!preview.isEmpty())QVERIFY(window.grab().save(preview+"/offline-preview.png"));QTest::mouseClick(launch,Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(status->text().contains("wurde beendet") || status->text().contains("has exited"),12000);QCOMPARE(service.requests.size(),requestsBefore);QVERIFY(!window.findChild<AuthClient*>()->signedIn());
        window.findChild<QComboBox*>("launchMode")->setCurrentIndex(0);
        auto normalPrefs=Preferences::load();normalPrefs.officialLauncher=root+"/runtime/java-runtime-delta/bin/java.exe";QVERIFY(normalPrefs.save());
        MainWindow restored;restored.setAttribute(Qt::WA_DontShowOnScreen);restored.findChild<LogWindow*>()->setAttribute(Qt::WA_DontShowOnScreen);restored.show();
        QTRY_VERIFY_WITH_TIMEOUT(restored.findChild<QComboBox*>("versions")->count()==1,12000);
        QCOMPARE(restored.findChild<QComboBox*>("themeChoice")->currentData().toString(),QString("light"));
        QCOMPARE(restored.findChild<QLineEdit*>("token")->text(),QString());QCOMPARE(restored.findChild<QLineEdit*>("username")->text(),QString());
        auto *normalLaunch=restored.findChild<QPushButton*>("primary");QVERIFY(!normalLaunch->isEnabled());
        QTest::mouseClick(restored.findChild<QPushButton*>("officialLaunch"),Qt::LeftButton);
        QVERIFY(restored.findChild<QLabel*>("status")->text().contains("Official launcher opened"));
        QVERIFY(!restored.findChild<LogWindow*>()->isVisible());
        // Normal launch is also available with no installed Java profiles.
        qputenv("APPDATA",(temp.path()+"/EmptyAppData").toUtf8());
        MainWindow empty;empty.setAttribute(Qt::WA_DontShowOnScreen);
        QTRY_VERIFY_WITH_TIMEOUT(empty.findChild<QLabel*>("status")->text().contains("No versions"),12000);
        QVERIFY(!empty.findChild<QPushButton*>("primary")->isEnabled());
        // Exercise the reported problem at minimum width and maximum supported text size.
        Ui::setLanguage("en");window.resize(1020,730);window.findChild<QSpinBox*>("textSizeChoice")->setValue(18);
        QTest::mouseClick(window.findChild<QPushButton*>("playNav"),Qt::LeftButton);
        window.findChild<QComboBox*>("launchMode")->setCurrentIndex(0);QApplication::processEvents();
        auto *path=dynamic_cast<PathLabel*>(window.findChild<QLabel*>("path"));QVERIFY(path);
        QVERIFY(!path->wordWrap());QCOMPARE(path->toolTip(),QDir::toNativeSeparators(root));
        QVERIFY(path->fontMetrics().horizontalAdvance(path->text())<=path->contentsRect().width());
        auto *javaPath=dynamic_cast<PathLabel*>(window.findChild<QLabel*>("javaPath"));QVERIFY(javaPath);QVERIFY(!javaPath->wordWrap());
        QVERIFY(javaPath->fontMetrics().horizontalAdvance(javaPath->text())<=javaPath->contentsRect().width());
        QVERIFY(window.findChild<QPushButton*>("primary")->geometry().bottom()<window.centralWidget()->height());
        if(!preview.isEmpty())QVERIFY(window.grab().save(preview+"/layout-large-text.png"));
        logs->reset();logs->configure(1000,false);for(int i=0;i<1105;++i)logs->append("stdout",QString("line %1\n").arg(i));QCOMPARE(model->rowCount(),1000);QCOMPARE(model->item(0,2)->text(),QString("line 105"));
        qApp->setProperty("disableAnimationsForTests",false);window.findChild<QCheckBox*>("animations")->setChecked(true);
        auto *pages=window.findChild<QStackedWidget*>("pages");pages->setCurrentIndex(2);pages->setCurrentIndex(1);
        auto *effect=qobject_cast<QGraphicsOpacityEffect*>(pages->graphicsEffect());QVERIFY(effect);QTRY_COMPARE(effect->opacity(),1.);
        window.findChild<QCheckBox*>("animations")->setChecked(false);pages->setCurrentIndex(0);QCOMPARE(effect->opacity(),1.);qApp->setProperty("disableAnimationsForTests",true);
        qputenv("APPDATA",old);
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv);app.setProperty("disableDiscordForTests",true);app.setProperty("disableAnimationsForTests",true); applyTheme(app); UiTests tests; return QTest::qExec(&tests,argc,argv);
}
#include "UiTests.moc"
