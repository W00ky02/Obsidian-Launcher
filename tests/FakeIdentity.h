#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include "AuthClient.h"
namespace {
const QString clientId="12345678-1234-1234-1234-123456789abc";
class FakeIdentity : public QObject {
public:
    QTcpServer server;
    QStringList requests;
    bool rejectProfile=false,pendingFirst=false,denyMinecraft=false,invalidProfile=false,malformedForm=false;
    int polls=0;
    FakeIdentity(){
        if(!server.listen(QHostAddress::LocalHost))qFatal("Local test server failed");
        connect(&server,&QTcpServer::newConnection,this,[this]{while(server.hasPendingConnections()){
            auto *socket=server.nextPendingConnection();
            connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            connect(socket,&QTcpSocket::readyRead,this,[this,socket]{
                auto data=socket->property("buffer").toByteArray()+socket->readAll();socket->setProperty("buffer",data);
                const auto split=data.indexOf("\r\n\r\n");if(split<0)return;
                int size=0;for(const auto &line:data.left(split).split('\n'))if(line.toLower().startsWith("content-length:"))size=line.mid(15).trimmed().toInt();
                if(data.size()<split+4+size)return;
                const auto route=QString::fromUtf8(data.split(' ').value(1));requests.append(route);QJsonObject result;int status=200;
                if(route=="/device")result={{"device_code","test-device+secret&value"},{"user_code","ABCD-EFGH"},{"verification_uri","https://microsoft.com/devicelogin"},{"expires_in",300},{"interval",5}};
                else if(route=="/oauth"){
                    if(!data.mid(split+4).contains("device_code=test-device%2Bsecret%26value"))malformedForm=true;
                    ++polls;if(pendingFirst && polls==1){status=400;result={{"error","authorization_pending"}};}
                    else result={{"access_token","test-microsoft-secret"}};
                }
                else if(route=="/xbox")result={{"Token","test-xbox-secret"}};
                else if(route=="/xsts")result={{"Token","test-xsts-secret"},{"DisplayClaims",QJsonObject{{"xui",QJsonArray{QJsonObject{{"uhs","test-hash"}}}}}}};
                else if(route=="/minecraft"){
                    if(denyMinecraft){status=403;result={{"error","forbidden"},{"debug","never-display-secret"}};}
                    else result={{"access_token","test-minecraft-secret"},{"expires_in",3600}};
                }
                else if(route=="/profile"){
                    if(rejectProfile){status=401;result={{"error","private-token-do-not-log"}};}
                    else if(invalidProfile)result={{"id","bad"},{"name","bad profile"}};
                    else result={{"id",QString(clientId).remove('-')},{"name",data.contains("another-test-token")?"FailPlayer":"Player_One"}};
                }
                else {status=404;result={{"error","unexpected endpoint"}};}
                const auto body=QJsonDocument(result).toJson(QJsonDocument::Compact);
                socket->write("HTTP/1.1 "+QByteArray::number(status)+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();
            });
        }});
    }
    AuthEndpoints endpoints() const {
        const QString prefix="http://127.0.0.1:"+QString::number(server.serverPort());
        return {QUrl(prefix+"/device"),QUrl(prefix+"/oauth"),QUrl(prefix+"/xbox"),QUrl(prefix+"/xsts"),QUrl(prefix+"/minecraft"),QUrl(prefix+"/profile")};
    }
};
}
