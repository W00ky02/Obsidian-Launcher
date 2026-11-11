#pragma once
#include "Minecraft.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>
#include <functional>

struct AuthEndpoints {
    QUrl device{"https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode"};
    QUrl oauth{"https://login.microsoftonline.com/consumers/oauth2/v2.0/token"};
    QUrl xbox{"https://user.auth.xboxlive.com/user/authenticate"};
    QUrl xsts{"https://xsts.auth.xboxlive.com/xsts/authorize"};
    QUrl minecraft{"https://api.minecraftservices.com/launcher/login"};
    QUrl profile{"https://api.minecraftservices.com/minecraft/profile"};
};

class AuthClient : public QObject {
    Q_OBJECT
public:
    explicit AuthClient(QObject *parent=nullptr,const AuthEndpoints &endpoints={});
    ~AuthClient() override;
    void begin(const QString &clientId);
    void beginToken(const QString &token);
    void cancel();
    void signOut();
    bool busy() const {return active;}
    bool signedIn() const {return !session.token.isEmpty() && QDateTime::currentDateTimeUtc()<sessionExpires;}
    Minecraft::Credentials credentials() const {return signedIn()?session:Minecraft::Credentials{};}
signals:
    void deviceReady(const QUrl &url,const QString &code);
    void authenticated();
    void failed(const QString &message);
    void stateChanged();
private:
    using Callback=std::function<void(int,const QJsonObject &)>;
    void request(const QUrl &url,const QByteArray &body,const QByteArray &contentType,const QString &bearer,Callback callback);
    void jsonPost(const QUrl &url,const QJsonObject &body,Callback callback);
    void poll();
    void xboxLogin(const QString &accessToken);
    void xstsLogin(const QString &xboxToken);
    void minecraftLogin(const QString &hash,const QString &xstsToken);
    void profile(const QString &accessToken,int expires);
    void fail(const QString &message);
    QNetworkAccessManager network;
    AuthEndpoints endpoints;
    QTimer timer;
    QPointer<QNetworkReply> pending;
    QString client,deviceCode;
    Minecraft::Credentials session;
    QDateTime deadline,sessionExpires;
    int interval=5;
    bool active=false;
    quint64 generation=0;
};
