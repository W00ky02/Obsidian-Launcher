#pragma once
#include <QLabel>
#include <QResizeEvent>
#include <QEvent>
#include <QTimer>

// One line at every width. Preserve the full path for tooltips and copy actions.
class PathLabel : public QLabel {
public:
    explicit PathLabel(const QString &value={},QWidget *parent=nullptr):QLabel(parent) {
        setWordWrap(false);setTextFormat(Qt::PlainText);
        setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        setMinimumWidth(0);setProperty("dynamicText",true);setPath(value);
    }
    void setPath(const QString &value) {full=value;setToolTip(full);render();}
    QString fullPath() const {return full;}
    QSize sizeHint() const override {return {0,QLabel::sizeHint().height()};}
protected:
    void resizeEvent(QResizeEvent *event) override {QLabel::resizeEvent(event);render();}
    void changeEvent(QEvent *event) override {
        QLabel::changeEvent(event);
        if(event->type()==QEvent::FontChange || event->type()==QEvent::StyleChange)
            QTimer::singleShot(0,this,[this]{render();});
    }
private:
    QString full;
    void render(){setText(fontMetrics().elidedText(full,Qt::ElideMiddle,qMax(0,contentsRect().width()-4)));}
};
