#pragma once
#include <QApplication>
#include <QStyleFactory>
#include <QStyle>
#include <QStyleHints>
#include <QIcon>
#include <QPalette>
#include <QMap>
#include <algorithm>
#include <QFontDatabase>
inline void applyTheme(QApplication &app,const QString &theme="dark",const QString &accentName="mint", const QString &fontName="Segoe UI Variable", int textSize=14, const QString &design="soft") {
    Q_INIT_RESOURCE(resources);
    app.setWindowIcon(QIcon(":/icons/obsidian.ico"));
    if(app.style()->objectName()!="fusion") app.setStyle(QStyleFactory::create("Fusion"));
    const bool dark=theme=="dark" || (theme=="system" && app.styleHints()->colorScheme()==Qt::ColorScheme::Dark);
    const QColor accent=accentName=="amber"?QColor(dark?"#e8bf77":"#8b5e17"):accentName=="rose"?QColor(dark?"#e69cac":"#9e4162"):accentName=="blue"?QColor(dark?"#8ab9f1":"#285e9d"):accentName=="violet"?QColor(dark?"#baa9eb":"#7452b1"):QColor(dark?"#8dd5bd":"#246e5a");
    const QString bg=dark?"#14151b":"#f7f7f8",panel=dark?"#1c1e27":"#ffffff",input=dark?"#111219":"#ffffff",text=dark?"#e8e9ed":"#23252b",muted=dark?"#969ba6":"#626773",line=dark?"#32343c":"#dedfe4";
    app.setProperty("themeDark",dark);app.setProperty("accentColor",accent);
    QPalette palette;palette.setColor(QPalette::Window,QColor(bg));palette.setColor(QPalette::WindowText,QColor(text));palette.setColor(QPalette::Base,QColor(input));palette.setColor(QPalette::AlternateBase,QColor(panel));palette.setColor(QPalette::Text,QColor(text));palette.setColor(QPalette::Button,QColor(panel));palette.setColor(QPalette::ButtonText,QColor(text));palette.setColor(QPalette::Highlight,accent);palette.setColor(QPalette::HighlightedText,dark?QColor("#10241d"):QColor("#ffffff"));palette.setColor(QPalette::Disabled,QPalette::Text,QColor(muted));palette.setColor(QPalette::Disabled,QPalette::ButtonText,QColor(muted));app.setPalette(palette);const auto families=QFontDatabase::families();const QString preferred=fontName=="Segoe UI Variable"&&families.contains("Segoe UI Variable Text")?"Segoe UI Variable Text":fontName;const QString family=families.contains(preferred)?preferred:"Segoe UI"; QFont font(family);font.setPixelSize(textSize);app.setFont(font);
    QString css=R"(
        QWidget { background: @bg; color: @text; font-family: '@font'; font-size: @sizepx; }
        QFrame#sidebar { background: @input; border-right: 1px solid @line; }
        QWidget#worldBanner QLabel { background: transparent; }
        QFrame#sidebar QLabel { background: transparent; }
        QFrame#card { background: @panel; border: 1px solid @line; border-radius: 10px; }
        QWidget#normalPanel, QWidget#credentialPanel, QWidget#offlinePanel { background: transparent; }
        QFrame#launchBar { background: transparent; border: none; border-top: 1px solid @line; border-radius: 0px; }
        QFrame#card QLabel, QFrame#card QCheckBox { background: transparent; }
        QLabel#brand { font-size: 21px; font-weight: 600; }
        QLabel#pageTitle { font-size: 32px; font-weight: 600; }
        QLabel#hero { background: transparent; color: @text; font-size: 24px; font-weight: 600; }
        QLabel#bannerCaption { background: transparent; color: #bfd0ca; }
        QLabel#sectionTitle { font-size: 15px; font-weight: 600; }
        QLabel#muted, QLabel#path { color: @muted; }
        QLabel#path { font-size: 12px; }
        QLabel#eyebrow { color: @muted; font-size: 11px; font-weight: 600; letter-spacing: 1px; }
        QLabel#status { padding: 8px 0px; border: none; border-radius: 0px; color: @muted; background: transparent; }
        QLabel#status[error="true"] { color: @error; border-color: @error; }
        QLineEdit, QComboBox, QSpinBox { background: @input; border: 1px solid @line; border-radius: 5px; padding: 8px 10px; selection-background-color: @accent; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: @accent; }
        QSpinBox::up-arrow { image: url(:/icons/chevron-up.xpm); width: 10px; height: 6px; }
        QSpinBox::down-arrow { image: url(:/icons/chevron.xpm); width: 10px; height: 6px; }
        QSpinBox::up-button, QSpinBox::down-button { width: 20px; border: none; background: @panel; }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: @hover; }
        QComboBox::drop-down { border: none; width: 28px; }
        QComboBox::down-arrow { image: url(:/icons/chevron.xpm); width: 12px; height: 8px; }
        QComboBox QAbstractItemView { background: @panel; selection-background-color: @selection; padding: 6px; border: 1px solid @line; }
        QPushButton { background: @panel; border: 1px solid @line; border-radius: 6px; padding: 9px 14px; font-weight: 400; }
        QPushButton:hover { background: @hover; border-color: @accent; }
        QPushButton:pressed { background: @selection; }
        QPushButton[nav="true"] { text-align: left; background: transparent; border: 1px solid transparent; padding: 10px 12px; color: @muted; }
        QPushButton[nav="true"]:checked { background: @selection; color: @accent; border-color: @line; font-weight: 600; }
        QPushButton[nav="true"]:hover { background: @hover; color: @text; }
        QPushButton[quiet="true"] { background: transparent; color: @muted; }
        QPushButton#primary { background: @accent; color: @onAccent; border: none; font-size: 14px; font-weight: 600; padding: 12px 22px; }
        QPushButton#primary:hover { background: @accentHover; }
        QPushButton:disabled, QPushButton#primary:disabled { color: @muted; background: @input; border: 1px solid @line; }
        QTabWidget::pane { border: none; }
        QTabBar::tab { background: transparent; color: @muted; border: none; border-bottom: 1px solid @line; padding: 12px 18px; margin-right: 0px; }
        QTabBar::tab:selected { background: transparent; color: @text; border-bottom: 2px solid @accent; }
        QListWidget { background: @panel; border: none; padding: 0px; outline: 0; }
        QListWidget::item { background: @input; color: @text; border: 1px solid @line; border-radius: 8px; margin: 4px 0px; padding: 14px 16px; }
        QListWidget::item:selected { background: @selection; border-left: 2px solid @accent; }
        QListWidget::item:hover { background: @hover; }
        QTreeView { font-family: 'Consolas'; background: @input; alternate-background-color: @panel; border: 1px solid @line; border-radius: 6px; padding: 4px; }
        QTreeView::item { padding: 5px 8px; font-family: 'Cascadia Mono', 'Consolas'; }
        QTreeView::item:selected { background: @selection; }
        QHeaderView::section { background: @panel; border: none; padding: 10px; color: @muted; }
        QScrollBar:vertical { background: @bg; width: 8px; }
        QScrollBar::handle:vertical { background: @line; border-radius: 4px; min-height: 28px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QCheckBox { spacing: 8px; }
        QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid @muted; border-radius: 4px; background: @input; }
        QCheckBox::indicator:checked { background: @accent; image: url(:/icons/check.xpm); }
        QToolTip { background: @panel; color: @text; border: 1px solid @line; padding: 8px; }
    )";
    const QMap<QString,QString> tokens={{"@bg",bg},{"@panel",panel},{"@input",input},{"@text",text},{"@muted",muted},{"@line",line},{"@accentHover",accent.lighter(110).name()},{"@accent",accent.name()},{"@selection",dark?"#2b293b":"#eeebf6"},{"@hover",dark?"#292b32":"#eeeff2"},{"@error",dark?"#e79b9f":"#a22d3d"},{"@onAccent",dark?"#121318":"#ffffff"}};
    auto keys=tokens.keys();std::sort(keys.begin(),keys.end(),[](const QString&a,const QString&b){return a.size()>b.size();});for(const auto &key:keys)css.replace(key,tokens[key]);css.replace("@font",family).replace("@size",QString::number(textSize));
    if(design=="sharp") css.replace("border-radius: 10px","border-radius: 3px").replace("border-radius: 6px","border-radius: 3px");
    if(design=="contrast") css.replace(line,dark?"#6d7782":"#657582");
    if(design=="soft") css.replace("border-radius: 10px","border-radius: 8px").replace("border-radius: 6px","border-radius: 6px");
    app.setStyleSheet(css);
}
