#include "PlayerPreview.h"
#include <QtTest>
class PlayerPreviewTests:public QObject {
 Q_OBJECT
private slots:
 void textureUrls(){
  const QString path="/texture/"+QString(64,'a');QVERIFY(PlayerPreview::allowedSkin(QUrl("https://textures.minecraft.net"+path)));
  for(const auto &url:QStringList{"http://textures.minecraft.net"+path,"https://example.com"+path,"https://textures.minecraft.net:444"+path,"https://user@textures.minecraft.net"+path,"https://textures.minecraft.net"+path+"?token=secret","https://textures.minecraft.net/texture/invalid"})QVERIFY(!PlayerPreview::allowedSkin(QUrl(url)));
 }
 void skinMapping(){
  QImage skin(64,64,QImage::Format_ARGB32);skin.fill(Qt::transparent);QPainter p(&skin);p.fillRect(8,8,8,8,Qt::red);p.fillRect(20,20,8,12,Qt::blue);p.fillRect(4,20,4,12,Qt::green);p.fillRect(20,52,4,12,Qt::yellow);p.end();
  const auto body=PlayerPreview::front(skin);QCOMPARE(body.size(),QSize(16,32));QCOMPARE(body.pixelColor(7,3),QColor(Qt::red));QCOMPARE(body.pixelColor(7,12),QColor(Qt::blue));QCOMPARE(body.pixelColor(5,25),QColor(Qt::green));QCOMPARE(body.pixelColor(10,25),QColor(Qt::yellow));
  QVERIFY(PlayerPreview::front(QImage(2048,2048,QImage::Format_ARGB32)).isNull());QVERIFY(!PlayerPreview::front(skin.copy(0,0,64,32)).isNull());
 }
 void resetAndReducedMotion(){PlayerPreview preview;preview.setIdentity("offline:Player",{});preview.setAnimated(true);preview.show();QTest::qWait(60);QVERIFY(!preview.grab().isNull());preview.setAnimated(false);preview.setIdentity({},{});QVERIFY(!preview.grab().isNull());}
};
QTEST_MAIN(PlayerPreviewTests)
#include "PlayerPreviewTests.moc"
