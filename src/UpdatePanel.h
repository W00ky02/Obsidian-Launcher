#pragma once
#include <QWidget>
#include <QApplication>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QVersionNumber>
#include <QCryptographicHash>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QProgressBar>
#include <QSettings>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QUuid>
#include <QStandardPaths>
#include <QProcess>
#include <functional>
#include "Preferences.h"

class UpdatePanel : public QWidget {
public:
    static constexpr auto currentVersion="1.0.0";
    struct Release { QString version, url, digest; qint64 size=0; };
    static bool parseRelease(const QJsonObject &o, Release &r) {
        if(o.value("draft").toBool() || o.value("prerelease").toBool())return false;
        const QString tag=o.value("tag_name").toString();
        if(!QRegularExpression("^v[0-9]+\\.[0-9]+\\.[0-9]+$").match(tag).hasMatch())return false;
        r.version=tag.mid(1);int matches=0;
        for(const auto &v:o.value("assets").toArray()) {
            const auto a=v.toObject();
            if(a.value("name").toString()!="Obsidian-Launcher-"+tag+"-setup.exe")continue;
            ++matches;r.url=a.value("browser_download_url").toString();r.digest=a.value("digest").toString();r.size=qint64(a.value("size").toDouble());
        }
        const QUrl u(r.url);
        return matches==1 && r.size>0 && r.size<=100*1024*1024 && u.scheme()=="https" && u.host()=="github.com" && u.userInfo().isEmpty() && u.port(-1)==-1 && u.query().isEmpty() && u.fragment().isEmpty()
            && u.path()=="/W00ky02/Obsidian-Launcher/releases/download/"+tag+"/Obsidian-Launcher-"+tag+"-setup.exe"
            && QRegularExpression("^sha256:[0-9a-f]{64}$").match(r.digest).hasMatch();
    }
    explicit UpdatePanel(std::function<bool()> idle, QWidget *parent=nullptr, const QUrl &testServer={}):QWidget(parent),canInstall(std::move(idle)) {
        if(testServer.scheme()=="http" && testServer.host()=="127.0.0.1" && qApp->property("disableDiscordForTests").toBool())testBase=testServer;
        setObjectName("updatePanel");auto *box=new QVBoxLayout(this);box->setContentsMargins(0,16,0,16);box->setSpacing(16);
        heading=new QLabel;heading->setObjectName("pageTitle");box->addWidget(heading);
        version=new QLabel("Obsidian Launcher 1.0");box->addWidget(version);
        message=new QLabel;message->setWordWrap(true);message->setObjectName("updateStatus");box->addWidget(message);
        progress=new QProgressBar;progress->setRange(0,100);progress->hide();box->addWidget(progress);
        check=new QPushButton;check->setObjectName("checkUpdates");box->addWidget(check);
        action=new QPushButton;action->setObjectName("installUpdate");action->hide();box->addWidget(action);
        automatic=new QCheckBox;QSettings s(Preferences::path(),QSettings::IniFormat);automatic->setChecked(s.value("updates/automatic",true).toBool());box->addWidget(automatic);box->addStretch();
        for(auto *w:findChildren<QWidget*>())w->setProperty("dynamicText",true);
        connect(automatic,&QCheckBox::toggled,this,[](bool on){QSettings s(Preferences::path(),QSettings::IniFormat);s.setValue("updates/automatic",on);});
        connect(check,&QPushButton::clicked,this,[this]{checkNow();});
        connect(action,&QPushButton::clicked,this,[this]{if(ready)install();else download();});
        if(!qApp->property("disableDiscordForTests").toBool())QTimer::singleShot(5000,this,[this]{if(automatic->isChecked())checkNow();});
        setLanguage("en");
    }
    ~UpdatePanel(){if(reply){reply->disconnect(this);reply->abort();}if(file.isOpen())file.close();if(!partial.isEmpty())QFile::remove(partial);}
    void setLanguage(const QString &lang){de=lang=="de";heading->setText(trText("Updates","Updates"));check->setText(trText("Check for updates","Nach Updates suchen"));automatic->setText(trText("Check automatically when Obsidian starts","Beim Start automatisch nach Updates suchen"));action->setText(ready?trText("Install update","Update installieren"):trText("Download update","Update herunterladen"));render();}
private:
    QUrl testBase;QNetworkAccessManager network;QNetworkReply *reply=nullptr;
    QLabel *heading,*version,*message;QPushButton *check,*action;QCheckBox *automatic;QProgressBar *progress;
    QFile file;QCryptographicHash hash{QCryptographicHash::Sha256};QString partial,installer;Release release;
    std::function<bool()> canInstall;bool de=false,ready=false,busy=false;QString state="idle";qint64 received=0;
    QString trText(const char *en,const char *german)const{return QString::fromUtf8(de?german:en);}
    void render(){
        if(state=="idle")message->setText(trText("Keep Obsidian up to date.","Halte Obsidian aktuell."));
        else if(state=="checking")message->setText(trText("Checking for updates…","Updates werden gesucht …"));
        else if(state=="current")message->setText(trText("You're up to date.","Du bist auf dem neuesten Stand."));
        else if(state=="available")message->setText(trText("A new version is available: ","Eine neue Version ist verfügbar: ")+release.version);
        else if(state=="downloading")message->setText(trText("Downloading and verifying the installer…","Setup wird heruntergeladen und geprüft …"));
        else if(state=="ready")message->setText(trText("Update verified. Installation will close Obsidian.","Update geprüft. Die Installation schließt Obsidian."));
        else if(state=="blocked")message->setText(trText("Finish your game and wait for background tasks before installing.","Beende dein Spiel und warte auf laufende Aufgaben, bevor du installierst."));
        else message->setText(trText("The update could not be completed. Check your connection and try again.","Das Update konnte nicht abgeschlossen werden. Prüfe deine Verbindung und versuche es erneut."));
    }
    void fail(){busy=false;ready=false;check->setEnabled(true);action->hide();progress->hide();if(file.isOpen())file.close();if(!partial.isEmpty())QFile::remove(partial);state="error";render();}
    QNetworkRequest request(const QUrl &url){QNetworkRequest req(url);req.setRawHeader("User-Agent","ObsidianLauncher/1.0.0");req.setTransferTimeout(30000);req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);return req;}
    void checkNow(){
        if(busy)return;busy=true;ready=false;action->hide();check->setEnabled(false);state="checking";render();
        reply=network.get(request(testBase.isEmpty()?QUrl("https://api.github.com/repos/W00ky02/Obsidian-Launcher/releases/latest"):testBase.resolved(QUrl("/release"))));
        auto *r=reply;connect(r,&QNetworkReply::readyRead,this,[r]{if(r->bytesAvailable()>1024*1024)r->abort();});
        connect(r,&QNetworkReply::finished,this,[this,r]{
            reply=nullptr;r->deleteLater();busy=false;check->setEnabled(true);
            const int code=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if(code==404){state="current";render();return;}
            if(r->error()!=QNetworkReply::NoError || code!=200 || r->bytesAvailable()>1024*1024){fail();return;}
            QJsonParseError error;const auto doc=QJsonDocument::fromJson(r->readAll(),&error);
            if(error.error!=QJsonParseError::NoError || !doc.isObject() || !parseRelease(doc.object(),release)){fail();return;}
            const bool newer=QVersionNumber::fromString(release.version)>QVersionNumber::fromString(currentVersion);
            state=newer?"available":"current";action->setVisible(newer);setLanguage(de?"de":"en");
        });
    }
    void download(){
        if(busy)return;
        const auto dir=(testBase.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::CacheLocation):QDir::tempPath())+"/updates/"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        if(!QDir().mkpath(dir)){fail();return;}installer=dir+"/Obsidian-update.exe";partial=installer+".part";file.setFileName(partial);
        if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)){fail();return;}
        busy=true;received=0;hash.reset();action->setEnabled(false);check->setEnabled(false);state="downloading";progress->setValue(0);progress->show();render();
        reply=network.get(request(testBase.isEmpty()?QUrl(release.url):testBase.resolved(QUrl("/setup"))));auto *r=reply;r->setReadBufferSize(128*1024);
        auto consume=[this,r]{const auto bytes=r->readAll();received+=bytes.size();if(received>release.size || file.write(bytes)!=bytes.size()){r->abort();return;}hash.addData(bytes);progress->setValue(int(received*100/release.size));};
        connect(r,&QNetworkReply::readyRead,this,consume);
        connect(r,&QNetworkReply::finished,this,[this,r,consume]{
            consume();const bool valid=r->error()==QNetworkReply::NoError && r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200 && received==release.size && "sha256:"+QString::fromLatin1(hash.result().toHex())==release.digest && file.flush();
            reply=nullptr;r->deleteLater();file.close();busy=false;action->setEnabled(true);check->setEnabled(true);progress->hide();
            if(!valid || !QFile::rename(partial,installer)){fail();return;}partial.clear();ready=true;state="ready";setLanguage(de?"de":"en");
        });
    }
    void install(){
        if(!ready || busy)return;if(!canInstall()){state="blocked";render();return;}
        QFile downloaded(installer);if(!downloaded.open(QIODevice::ReadOnly)){fail();return;}QCryptographicHash verify(QCryptographicHash::Sha256);
        if(!verify.addData(&downloaded) || "sha256:"+QString::fromLatin1(verify.result().toHex())!=release.digest){fail();return;}downloaded.close();
        if(!QProcess::startDetached(installer,{"/LANG="+QString(de?"german":"english")})){fail();return;}window()->close();
    }
};
