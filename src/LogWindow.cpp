#include "LogWindow.h"
#include "Ui.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QHeaderView>
#include <QTime>
#include <QScrollBar>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>

namespace {
class LogFilter : public QSortFilterProxyModel {
public:
    QString channel, query;
    void refresh() { invalidateFilter(); }
protected:
    bool filterAcceptsRow(int row,const QModelIndex &parent) const override {
        const auto *m=sourceModel();
        return (channel.isEmpty() || m->index(row,1,parent).data().toString()==channel)
            && (query.isEmpty() || m->index(row,2,parent).data().toString().contains(query,Qt::CaseInsensitive));
    }
};
}
LogWindow::LogWindow(QWidget *parent):QDialog(parent) {
    setWindowTitle("Obsidian · Live-Konsole"); resize(1060,620); setMinimumSize(720,420);
    setWindowFlag(Qt::Window,true);
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(24,24,24,24); layout->setSpacing(16);
    auto *title=new QLabel("Live-Konsole"); title->setObjectName("sectionTitle"); layout->addWidget(title);
    auto *hint=new QLabel("Minecraft-Ausgabe · nur im Arbeitsspeicher · Export auf Wunsch"); hint->setObjectName("muted");hint->setWordWrap(true);hint->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); layout->addWidget(hint);
    auto *toolbar=new QHBoxLayout;
    search=new QLineEdit; search->setPlaceholderText("Ausgabe durchsuchen …"); search->setClearButtonEnabled(true); toolbar->addWidget(search,1);
    auto *channels=new QComboBox; channels->addItems({"Alle Kanäle","stdout","stderr","Launcher"}); toolbar->addWidget(channels);
    follow=new QCheckBox("Auto-Scroll"); follow->setChecked(true); toolbar->addWidget(follow);
    auto *clear=new QPushButton("Leeren"); toolbar->addWidget(clear); auto *exportButton=new QPushButton("Exportieren …");exportButton->setObjectName("exportLogs");toolbar->addWidget(exportButton);layout->addLayout(toolbar);
    connect(exportButton,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getSaveFileName(this,Ui::text("Logs exportieren"),"obsidian-log.txt","Text (*.txt)");if(path.isEmpty())return;QFile file(path);const auto bytes=exportText().toUtf8();if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size())QMessageBox::warning(this,Ui::text("Logs exportieren"),Ui::text("Log-Datei konnte nicht gespeichert werden."));});
    model=new QStandardItemModel(this); model->setHorizontalHeaderLabels({"Zeit","Kanal","Ausgabe"});
    auto *filter=new LogFilter; filter->setParent(this); proxy=filter; proxy->setSourceModel(model);
    view=new QTreeView; view->setModel(proxy); view->setRootIsDecorated(false); view->setAlternatingRowColors(true); view->setUniformRowHeights(true);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection); view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->header()->setStretchLastSection(true);view->header()->setSectionResizeMode(0,QHeaderView::ResizeToContents);view->header()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    view->setFont(QFont("Cascadia Mono",10)); layout->addWidget(view,1);
    auto *status=new QLabel("Bereit"); status->setObjectName("logState"); layout->addWidget(status);
    status->setProperty("dynamicText",true);state="Bereit";Ui::capture(this);refreshLanguage();
    connect(search,&QLineEdit::textChanged,this,[filter](const QString &text){filter->query=text; filter->refresh();});
    connect(channels,&QComboBox::currentIndexChanged,this,[filter,channels](int i){filter->channel=i==0?QString():channels->currentText(); filter->refresh();});
    connect(clear,&QPushButton::clicked,this,&LogWindow::reset);
}
void LogWindow::reset() { model->removeRows(0,model->rowCount()); partial.clear(); }
void LogWindow::setStatus(const QString &status) { state=status; findChild<QLabel*>("logState")->setText(Ui::text(status)); }
void LogWindow::append(const QString &channel,const QString &text) {
    if (text.isEmpty()) return;
    QString normalized=channel=="Launcher"?Ui::text(text):text; normalized.replace("\r\n","\n"); normalized.replace('\r','\n');
    const auto lines=normalized.split('\n');
    for (qsizetype i=0;i<lines.size();++i) {
        const bool complete=i<lines.size()-1;
        if (i==lines.size()-1 && lines[i].isEmpty()) break;
        QStandardItem *message=nullptr;
        const auto existing=partial.value(channel);
        if (existing.isValid()) message=model->item(existing.row(),2);
        if (message) message->setText((message->text()+lines[i]).left(16384));
        else {
            auto *time=new QStandardItem(QTime::currentTime().toString("HH:mm:ss.zzz"));
            auto *source=new QStandardItem(channel); message=new QStandardItem(lines[i].left(16384));
            const bool dark=qApp->property("themeDark").toBool();
            const QColor color=channel=="stderr"?QColor(dark?"#e79b9f":"#a22d3d"):channel=="Launcher"?qApp->property("accentColor").value<QColor>():palette().color(QPalette::Text);
            source->setForeground(color); message->setForeground(color); model->appendRow({time,source,message});
        }
        if (complete || message->text().size()>=16384) partial.remove(channel);
        else partial[channel]=model->index(message->row(),2);
        if(model->rowCount()>lineLimit)model->removeRows(0,model->rowCount()-lineLimit);
    }
    if (follow->isChecked()) view->scrollToBottom();
}
void LogWindow::refreshLanguage(){Ui::refresh(this);setWindowTitle(Ui::text("Obsidian · Live-Konsole"));model->setHorizontalHeaderLabels({Ui::text("Zeit"),Ui::text("Kanal"),Ui::text("Ausgabe")});setStatus(state);}
void LogWindow::refreshAppearance(){for(int row=0;row<model->rowCount();++row){const auto channel=model->item(row,1)->text();const bool dark=qApp->property("themeDark").toBool();const QColor color=channel=="stderr"?QColor(dark?"#e79b9f":"#a22d3d"):channel=="Launcher"?qApp->property("accentColor").value<QColor>():palette().color(QPalette::Text);model->item(row,1)->setForeground(color);model->item(row,2)->setForeground(color);}}

void LogWindow::configure(int limit,bool autoFollow){lineLimit=qBound(1000,limit,50000);follow->setChecked(autoFollow);if(model->rowCount()>lineLimit)model->removeRows(0,model->rowCount()-lineLimit);}
QString LogWindow::exportText() const {QString text;for(int row=0;row<model->rowCount();++row)text+=model->item(row,0)->text()+" ["+model->item(row,1)->text()+"] "+model->item(row,2)->text()+"\n";return text;}
