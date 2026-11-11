#pragma once
#include <QWidget>
#include <QPainter>
#include <QApplication>
#include <QIcon>
class WorldBanner : public QWidget {
public:
    explicit WorldBanner(QWidget *parent=nullptr):QWidget(parent){setObjectName("worldBanner");setMinimumHeight(104);}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const bool dark=qApp->property("themeDark").toBool();
        p.setPen(QPen(dark?QColor("#303239"):QColor("#dddfe4"),1));
        p.setBrush(dark?QColor("#202228"):QColor("#f1f2f5"));
        p.drawRoundedRect(rect().adjusted(1,1,-1,-1),6,6);
        p.setOpacity(.7);p.drawPixmap(width()-91,(height()-64)/2,QIcon(":/icons/obsidian.ico").pixmap(64,64));
    }
};
