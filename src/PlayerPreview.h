#pragma once
#include <QWidget>
#include <QApplication>
#include <QRegularExpression>
#include <QPainter>
#include <QTimer>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QImageReader>
#include <QBuffer>
#include <cmath>

// Skin requests never include the account token. Images remain in memory.
class PlayerPreview : public QWidget {
public:
    explicit PlayerPreview(QWidget *parent=nullptr):QWidget(parent){
        setObjectName("playerPreview");setFixedSize(76,112);setAttribute(Qt::WA_OpaquePaintEvent,false);
        clock.start();timer.setInterval(33);connect(&timer,&QTimer::timeout,this,[this]{update();});
    }
    ~PlayerPreview(){if(pending){pending->disconnect(this);pending->abort();}}
    static bool allowedSkin(const QUrl &url){return url.scheme()=="https" && url.host()=="textures.minecraft.net" && url.userInfo().isEmpty() && url.port(-1)==-1 && url.query().isEmpty() && url.fragment().isEmpty() && QRegularExpression("^/texture/[a-fA-F0-9]{32,128}$").match(url.path()).hasMatch();}
    static QImage front(const QImage &skin){
        if(skin.width()!=64 || (skin.height()!=32 && skin.height()!=64))return {};
        QImage result(16,32,QImage::Format_ARGB32);result.fill(Qt::transparent);QPainter p(&result);
        auto part=[&](QRect target,QRect source){p.drawImage(target,skin,source);};
        part({4,0,8,8},{8,8,8,8});part({4,0,8,8},{40,8,8,8});part({4,8,8,12},{20,20,8,12});
        part({0,8,4,12},{44,20,4,12});part({12,8,4,12},skin.height()==64?QRect(36,52,4,12):QRect(44,20,4,12));
        part({4,20,4,12},{4,20,4,12});part({8,20,4,12},skin.height()==64?QRect(20,52,4,12):QRect(4,20,4,12));
        if(skin.height()==64){part({4,8,8,12},{20,36,8,12});part({0,8,4,12},{44,36,4,12});part({12,8,4,12},{52,52,4,12});part({4,20,4,12},{4,36,4,12});part({8,20,4,12},{4,52,4,12});}
        return result;
    }
    void setAnimated(bool enabled){animated=enabled;if(enabled && isVisible())timer.start();else timer.stop();update();}
    void setIdentity(const QString &key,const QUrl &url){
        if(key==identity && url==source)return;identity=key;source=url;image={};
        if(pending){pending->disconnect(this);pending->abort();pending->deleteLater();pending=nullptr;}update();
        if(!allowedSkin(url))return;
        QNetworkRequest request(url);request.setTransferTimeout(10000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        pending=network.get(request);auto *reply=pending;
        connect(reply,&QNetworkReply::readyRead,this,[reply]{if(reply->bytesAvailable()>256*1024)reply->abort();});
        connect(reply,&QNetworkReply::finished,this,[this,reply]{
            pending=nullptr;reply->deleteLater();if(reply->error()!=QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200 || reply->bytesAvailable()>256*1024)return;
            QByteArray bytes=reply->readAll();QBuffer buffer(&bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer,"PNG");
            const auto size=reader.size();if(size.width()!=64 || (size.height()!=32 && size.height()!=64))return;
            image=front(reader.read());update();
        });
    }
protected:
    void showEvent(QShowEvent *e)override{QWidget::showEvent(e);if(animated)timer.start();}
    void hideEvent(QHideEvent *e)override{timer.stop();QWidget::hideEvent(e);}
    void paintEvent(QPaintEvent *)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const QColor accent=qApp->property("accentColor").value<QColor>();
        p.setPen(Qt::NoPen);QColor shadow=accent;shadow.setAlpha(35);p.setBrush(shadow);p.drawEllipse(QRectF(13,102,50,7));
        const double bob=animated?std::sin(clock.elapsed()/850.0)*2.5:0;p.translate(14,4+bob);p.setRenderHint(QPainter::SmoothPixmapTransform,false);
        if(!image.isNull()){p.drawImage(QRect(0,0,48,96),image);return;}
        // An original neutral avatar for signed-out and offline profiles.
        p.fillRect(12,0,24,24,QColor("#aaa4bd"));p.fillRect(12,0,24,6,QColor("#393445"));p.fillRect(17,12,4,4,QColor("#25232e"));p.fillRect(28,12,4,4,QColor("#25232e"));
        p.fillRect(12,24,24,36,accent);p.fillRect(0,24,12,36,accent.darker(115));p.fillRect(36,24,12,36,accent.darker(115));p.fillRect(12,60,12,36,QColor("#454356"));p.fillRect(24,60,12,36,QColor("#555268"));
    }
private:
    QNetworkAccessManager network;QNetworkReply *pending=nullptr;QImage image;QUrl source;QString identity;QTimer timer;QElapsedTimer clock;bool animated=false;
};
