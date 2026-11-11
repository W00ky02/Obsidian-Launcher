#include "AuthClient.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QRegularExpression>

namespace {
bool guid(const QString &value) {return QRegularExpression("^[0-9a-fA-F]{8}(-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}$").match(value).hasMatch();}
QString hash(const QJsonObject &object) {const auto users=object.value("DisplayClaims").toObject().value("xui").toArray();return users.isEmpty()?QString():users.first().toObject().value("uhs").toString();}
bool ok(int status) {return status>=200 && status<300;}
}
AuthClient::AuthClient(QObject *parent,const AuthEndpoints &urls):QObject(parent),endpoints(urls) {
    timer.setSingleShot(true);connect(&timer,&QTimer::timeout,this,&AuthClient::poll);
}
AuthClient::~AuthClient(){disconnect(this,nullptr,nullptr,nullptr);cancel();}
void AuthClient::cancel() {
    ++generation;timer.stop();active=false;deviceCode.clear();client.clear();
    if(pending){disconnect(pending,nullptr,this,nullptr);pending->abort();pending->deleteLater();pending=nullptr;}
    emit stateChanged();
}
void AuthClient::signOut(){cancel();session={};sessionExpires={};emit stateChanged();}
void AuthClient::fail(const QString &message){cancel();emit failed(message);}
void AuthClient::request(const QUrl &url,const QByteArray &body,const QByteArray &contentType,const QString &bearer,Callback callback) {
    if(!active)return;
    const bool local=url.scheme()=="http" && (url.host()=="127.0.0.1" || url.host()=="localhost");
    if(url.scheme()!="https" && !local){fail("Anmeldung: Unsichere Serveradresse.");return;}
    QNetworkRequest req(url);req.setTransferTimeout(15000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("Accept","application/json");req.setRawHeader("User-Agent","ObsidianLauncher/1.3");
    if(!contentType.isEmpty())req.setHeader(QNetworkRequest::ContentTypeHeader,contentType);
    if(!bearer.isEmpty())req.setRawHeader("Authorization",("Bearer "+bearer).toUtf8());
    auto *reply=contentType.isEmpty()?network.get(req):network.post(req,body);pending=reply;
    const auto ticket=generation;
    connect(reply,&QNetworkReply::finished,this,[this,reply,ticket,callback]{
        if(ticket!=generation){reply->deleteLater();return;}
        pending=nullptr;const auto code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->read(1024*1024+1);const auto error=reply->error();reply->deleteLater();
        if(!code || (error!=QNetworkReply::NoError && code<400)){fail("Anmeldung: Netzwerk- oder TLS-Fehler. Internetverbindung prüfen.");return;}
        QJsonParseError parse;const auto document=QJsonDocument::fromJson(bytes,&parse);
        if(bytes.size()>1024*1024 || parse.error!=QJsonParseError::NoError || !document.isObject()){fail("Anmeldung: Ungültige Serverantwort.");return;}
        callback(code,document.object());
    });
}
void AuthClient::jsonPost(const QUrl &url,const QJsonObject &body,Callback callback){request(url,QJsonDocument(body).toJson(QJsonDocument::Compact),"application/json",{},callback);}
void AuthClient::begin(const QString &clientId) {
    signOut();
    if(!guid(clientId.trimmed())){emit failed("Microsoft-Anmeldung noch nicht eingerichtet. Eigene Client-ID unter Einstellungen eintragen.");return;}
    active=true;client=clientId.trimmed();emit stateChanged();
    QUrlQuery form;form.addQueryItem("client_id",client);form.addQueryItem("scope","XboxLive.SignIn");
    request(endpoints.device,form.query(QUrl::FullyEncoded).toUtf8().replace("+","%2B"),"application/x-www-form-urlencoded",{},[this](int code,const QJsonObject &obj){
        if(!ok(code)){fail("Microsoft-Anmeldung abgelehnt. Client-ID und Public-Client-Konfiguration prüfen.");return;}
        const QUrl url(obj.value("verification_uri").toString());const auto userCode=obj.value("user_code").toString();
        const auto host=url.host().toLower();deviceCode=obj.value("device_code").toString();const int expires=obj.value("expires_in").toInt();
        if(url.scheme()!="https" || !QStringList{"microsoft.com","www.microsoft.com","login.microsoftonline.com"}.contains(host) || !url.userInfo().isEmpty() || (url.port(-1)!=-1 && url.port()!=443) || userCode.isEmpty() || deviceCode.isEmpty() || expires<=0){fail("Anmeldung: Ungültige Serverantwort.");return;}
        interval=qBound(5,obj.value("interval").toInt(5),60);deadline=QDateTime::currentDateTimeUtc().addSecs(qMin(expires,1800));
        timer.start(interval*1000);emit deviceReady(url,userCode);
    });
}
void AuthClient::beginToken(const QString &value) {
    signOut();const auto access=value.trimmed();
    if(access.isEmpty() || access.size()>8192 || QRegularExpression("[\\s\\x00-\\x1f\\x7f]").match(access).hasMatch()){
        emit failed("Bitte einen gültigen Minecraft Access Token ohne Leerzeichen eingeben.");return;
    }
    active=true;emit stateChanged();profile(access,3600);
}
void AuthClient::poll() {
    if(!active)return;
    if(QDateTime::currentDateTimeUtc()>=deadline){fail("Anmeldecode abgelaufen. Bitte erneut anmelden.");return;}
    QUrlQuery form;form.addQueryItem("client_id",client);form.addQueryItem("grant_type","urn:ietf:params:oauth:grant-type:device_code");form.addQueryItem("device_code",deviceCode);
    request(endpoints.oauth,form.query(QUrl::FullyEncoded).toUtf8().replace("+","%2B"),"application/x-www-form-urlencoded",{},[this](int code,const QJsonObject &obj){
        const auto error=obj.value("error").toString();
        if(error=="authorization_pending"){timer.start(interval*1000);return;}
        if(error=="slow_down"){interval=qMin(120,interval+5);timer.start(interval*1000);return;}
        if(error=="authorization_declined" || error=="access_denied"){fail("Microsoft-Anmeldung wurde abgelehnt.");return;}
        if(error=="expired_token"){fail("Anmeldecode abgelaufen. Bitte erneut anmelden.");return;}
        const auto access=obj.value("access_token").toString();
        if(!ok(code) || access.isEmpty()){fail("Microsoft-Anmeldung fehlgeschlagen. App-Konfiguration prüfen.");return;}
        deviceCode.clear();xboxLogin(access);
    });
}
void AuthClient::xboxLogin(const QString &access) {
    jsonPost(endpoints.xbox,{{"Properties",QJsonObject{{"AuthMethod","RPS"},{"SiteName","user.auth.xboxlive.com"},{"RpsTicket","d="+access}}},{"RelyingParty","http://auth.xboxlive.com"},{"TokenType","JWT"}},[this](int code,const QJsonObject &obj){
        const auto token=obj.value("Token").toString();
        if(!ok(code) || token.isEmpty()){fail("Xbox-Anmeldung fehlgeschlagen. Xbox-Profil und App-Konfiguration prüfen.");return;}
        xstsLogin(token);
    });
}
void AuthClient::xstsLogin(const QString &xboxToken) {
    jsonPost(endpoints.xsts,{{"Properties",QJsonObject{{"SandboxId","RETAIL"},{"UserTokens",QJsonArray{xboxToken}}}},{"RelyingParty","rp://api.minecraftservices.com/"},{"TokenType","JWT"}},[this](int code,const QJsonObject &obj){
        const auto token=obj.value("Token").toString();const auto userHash=hash(obj);
        if(!ok(code) || token.isEmpty() || userHash.isEmpty()){fail("Xbox-Freigabe fehlgeschlagen. Xbox-Profil, Altersfreigabe oder Familieneinstellungen prüfen.");return;}
        minecraftLogin(userHash,token);
    });
}
void AuthClient::minecraftLogin(const QString &userHash,const QString &xstsToken) {
    jsonPost(endpoints.minecraft,{{"xtoken","XBL3.0 x="+userHash+";"+xstsToken},{"platform","PC_LAUNCHER"}},[this](int code,const QJsonObject &obj){
        if(code==403){fail("Minecraft-API-Zugriff verweigert. Die Client-ID muss für Minecraft freigegeben sein.");return;}
        const auto access=obj.value("access_token").toString();const auto expires=obj.value("expires_in").toInt();
        if(!ok(code) || access.isEmpty() || expires<=0){fail("Minecraft-Anmeldung fehlgeschlagen.");return;}
        profile(access,expires);
    });
}
void AuthClient::profile(const QString &access,int expires) {
    request(endpoints.profile,{}, {},access,[this,access,expires](int code,const QJsonObject &obj){
        if(code==401){fail("Token ungültig oder abgelaufen. Bitte einen aktuellen Minecraft Access Token verwenden.");return;}
        if(code==404 || code==403){fail("Kein verfügbares Minecraft-Java-Profil. Besitz, Abonnement und Profil im offiziellen Launcher prüfen.");return;}
        if(!ok(code)){fail("Minecraft-Profil konnte nicht geladen werden.");return;}
        try {session=Minecraft::validateCredentials({obj.value("name").toString(),obj.value("id").toString(),access});}
        catch(const Minecraft::Error &){fail("Anmeldung: Ungültige Profildaten.");return;}
        sessionExpires=QDateTime::currentDateTimeUtc().addSecs(qMax(1,qMin(expires,86400)-60));
        active=false;client.clear();deviceCode.clear();timer.stop();emit stateChanged();emit authenticated();
    });
}
