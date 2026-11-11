#pragma once
#include <QString>
class QWidget;
namespace Ui {
void setLanguage(const QString &language);
QString text(const QString &source);
void capture(QWidget *root);
void refresh(QWidget *root);
}
