#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QTimer>
#include <QJsonObject>

// Local Discord IPC only. No account tokens, OAuth, or outbound network requests.
class DiscordPresence : public QObject {
    Q_OBJECT
public:
    explicit DiscordPresence(QObject *parent=nullptr,const QString &testPipe={});
    ~DiscordPresence() override;
    void setEnabled(bool enabled);
    void setActivity(bool playing,const QString &version={},const QString &loader={});
    void shutdown();
    QString status() const {return statusText;}
signals:
    void statusChanged(const QString &status);
private:
    void connectNext();
    void readFrames();
    void sendFrame(quint32 opcode,const QByteArray &payload);
    void publish(bool clear=false);
    void setStatus(const QString &text);
    QLocalSocket socket;
    QTimer retry,handshakeTimeout;
    QByteArray incoming;
    QString testPipe,statusText,version,loader;
    bool enabled=false,ready=false,playing=false;
    int pipeIndex=0;
    quint64 nonce=0;
    qint64 started=0;
};
