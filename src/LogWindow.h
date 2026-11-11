#pragma once
#include <QDialog>
#include <QMap>
#include <QPersistentModelIndex>
class QStandardItemModel;
class QSortFilterProxyModel;
class QTreeView;
class QLineEdit;
class QCheckBox;
class LogWindow : public QDialog {
    Q_OBJECT
public:
    explicit LogWindow(QWidget *parent=nullptr);
    void append(const QString &channel, const QString &text);
    void reset();
    void configure(int limit,bool autoFollow);
    QString exportText() const;
    void setStatus(const QString &status);
    void refreshLanguage();
    void refreshAppearance();
private:
    QStandardItemModel *model;
    QSortFilterProxyModel *proxy;
    QTreeView *view;
    QCheckBox *follow;
    QLineEdit *search;
    QMap<QString,QPersistentModelIndex> partial;
    QString state;
    int lineLimit=8000;
};
