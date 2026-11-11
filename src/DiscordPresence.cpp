#include "DiscordPresence.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QtEndian>

namespace {constexpr quint32 MaxFrame=65536;}
DiscordPresence::DiscordPresence(QObject *parent,const QString &pipe):QObject(parent),testPipe(pipe) {
    retry.setInterval(10000);handshakeTimeout.setSingleShot(true);handshakeTimeout.setInterval(5000);
    connect(&retry,&QTimer::timeout,this,[this]{if(enabled && socket.state()==QLocalSocket::UnconnectedState){pipeIndex=0;connectNext();}});
    connect(&handshakeTimeout,&QTimer::timeout,this,[this]{socket.abort();setStatus("Discord antwortet nicht. Verbindung wird erneut versucht.");});
    connect(&socket,&QLocalSocket::connected,this,[this]{
        incoming.clear();ready=false;handshakeTimeout.start();
        sendFrame(0,QJsonDocument(QJsonObject{{"v",1},{"client_id","1556990126570078248"}}).toJson(QJsonDocument::Compact));
    });
    connect(&socket,&QLocalSocket::readyRead,this,&DiscordPresence::readFrames);
    connect(&socket,&QLocalSocket::disconnected,this,[this]{ready=false;incoming.clear();handshakeTimeout.stop();if(enabled)setStatus("Warte auf Discord …");});
    connect(&socket,&QLocalSocket::errorOccurred,this,[this](QLocalSocket::LocalSocketError){
        ready=false;handshakeTimeout.stop();
        if(!enabled)return;
        if(testPipe.isEmpty() && ++pipeIndex<10)QTimer::singleShot(0,this,&DiscordPresence::connectNext);
        else setStatus("Warte auf Discord …");
    });
    setStatus("Discord-Anzeige deaktiviert.");
}
DiscordPresence::~DiscordPresence(){disconnect(this,nullptr,nullptr,nullptr);shutdown();}
void DiscordPresence::setStatus(const QString &text){if(statusText!=text){statusText=text;emit statusChanged(text);}}
void DiscordPresence::connectNext(){
    if(!enabled || socket.state()!=QLocalSocket::UnconnectedState)return;
    socket.connectToServer(testPipe.isEmpty()?QString("discord-ipc-%1").arg(pipeIndex):testPipe,QIODevice::ReadWrite);
}
void DiscordPresence::setEnabled(bool on){
    if(enabled==on)return;
    if(!on){shutdown();return;}
    enabled=true;retry.start();pipeIndex=0;setStatus("Warte auf Discord …");connectNext();
}
void DiscordPresence::setActivity(bool isPlaying,const QString &id,const QString &kind){
    if(playing==isPlaying && version==id && loader==kind)return;
    if(playing!=isPlaying || started==0)started=QDateTime::currentSecsSinceEpoch();
    playing=isPlaying;version=id;loader=kind;if(ready && enabled)publish();
}
void DiscordPresence::sendFrame(quint32 opcode,const QByteArray &payload){
    if(socket.state()!=QLocalSocket::ConnectedState)return;
    QByteArray frame(8,Qt::Uninitialized);
    qToLittleEndian<quint32>(opcode,frame.data());qToLittleEndian<quint32>(quint32(payload.size()),frame.data()+4);frame+=payload;
    socket.write(frame);socket.flush();
}
void DiscordPresence::publish(bool clear){
    if(!ready)return;
    QJsonValue activity=QJsonValue::Null;
    if(!clear){
        if(started==0)started=QDateTime::currentSecsSinceEpoch();
        const auto state=playing?version.left(90)+(loader.isEmpty()?QString():" · "+loader.left(24)):QString("Version auswählen");
        activity=QJsonObject{{"type",0},{"details",playing?"Minecraft mit Obsidian Launcher":"Im Obsidian Launcher"},
            {"state",state},{"timestamps",QJsonObject{{"start",started}}}};
    }
    const QJsonObject message{{"cmd","SET_ACTIVITY"},{"nonce",QString::number(++nonce)},
        {"args",QJsonObject{{"pid",QCoreApplication::applicationPid()},{"activity",activity}}}};
    sendFrame(1,QJsonDocument(message).toJson(QJsonDocument::Compact));
}
void DiscordPresence::readFrames(){
    incoming+=socket.readAll();
    if(incoming.size()>MaxFrame*2){socket.abort();setStatus("Ungültige Discord-Antwort.");return;}
    while(incoming.size()>=8){
        const auto opcode=qFromLittleEndian<quint32>(incoming.constData());
        const auto size=qFromLittleEndian<quint32>(incoming.constData()+4);
        if(size>MaxFrame){socket.abort();setStatus("Ungültige Discord-Antwort.");return;}
        if(incoming.size()<qsizetype(size)+8)return;
        const auto payload=incoming.mid(8,size);incoming.remove(0,8+size);
        if(opcode==3){sendFrame(4,payload);continue;}
        if(opcode==4)continue;
        if(opcode==2){socket.abort();setStatus("Discord hat die Verbindung beendet.");return;}
        QJsonParseError error;const auto document=QJsonDocument::fromJson(payload,&error);
        if(opcode!=1 || error.error!=QJsonParseError::NoError || !document.isObject()){
            socket.abort();setStatus("Ungültige Discord-Antwort.");return;
        }
        const auto message=document.object();
        if(message.value("evt")=="READY" && message.value("cmd")=="DISPATCH"){
            ready=true;handshakeTimeout.stop();publish();
        }else if(message.value("evt")=="ERROR"){
            setStatus("Discord lehnt die Anzeige ab. Anwendungs-ID und Discord-Einstellungen prüfen.");
        }else if(message.value("cmd")=="SET_ACTIVITY" && message.value("nonce").toString()==QString::number(nonce)){
            setStatus("Mit Discord verbunden.");
        }
    }
}
void DiscordPresence::shutdown(){
    enabled=false;retry.stop();handshakeTimeout.stop();
    if(ready){publish(true);if(socket.bytesToWrite()>0)socket.waitForBytesWritten(50);}
    ready=false;socket.abort();incoming.clear();setStatus("Discord-Anzeige deaktiviert.");
}
