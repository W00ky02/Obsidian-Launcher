#include "MainWindow.h"
#include "UpdatePanel.h"
#include "DiscordPresence.h"
#include "LogWindow.h"
#include "Preferences.h"
#include "Ui.h"
#include "Theme.h"
#include "WorldBanner.h"
#include "PathLabel.h"
#include <QClipboard>
#include <QDialog>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QDesktopServices>
#include <QUrl>
#include <QStyleHints>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QStandardItemModel>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QFileDialog>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QCloseEvent>
#include <QTimer>
#include <QDir>
#include <QScrollArea>
#include <QProcessEnvironment>
#include <QStyle>
#include <QDirIterator>
#include <QShortcut>
#include <QCompleter>
#include <QListWidget>
#include <QTabWidget>
#include <QScreen>
#include <QSignalBlocker>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QRegularExpression>

namespace {
QFrame *card(QVBoxLayout *layout,const QString &number,const QString &title) {
    auto *frame=new QFrame; frame->setObjectName("card");
    auto *box=new QVBoxLayout(frame); box->setContentsMargins(20,18,20,18); box->setSpacing(10);
    Q_UNUSED(number);
    auto *heading=new QLabel(title);heading->setWordWrap(true);heading->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); heading->setObjectName("sectionTitle"); box->addWidget(heading);
    layout->addWidget(frame); return frame;
}
struct Preparation { QString error; };
}
MainWindow::MainWindow(const AuthEndpoints &authEndpoints):root(Minecraft::defaultRoot()),preferences(Preferences::load()) {
    Ui::setLanguage(preferences.language);
    applyTheme(*qApp,preferences.theme,preferences.accent,preferences.font,preferences.textSize,preferences.design);
    setWindowTitle("Obsidian Launcher · 1.0");resize(1280,860);setMinimumSize(1020,730);
    auto *shell=new QWidget;setCentralWidget(shell);
    auto *shellLayout=new QHBoxLayout(shell);shellLayout->setContentsMargins(0,0,0,0);shellLayout->setSpacing(0);
    auto *sidebar=new QFrame;sidebar->setObjectName("sidebar");sidebar->setFixedWidth(preferences.sidebarWidth);
    auto *side=new QVBoxLayout(sidebar);side->setContentsMargins(16,26,16,20);side->setSpacing(8);
    auto *brandRow=new QHBoxLayout;brandRow->setSpacing(10);
    auto *logo=new QLabel;logo->setPixmap(QIcon(":/icons/obsidian.ico").pixmap(30,30));brandRow->addWidget(logo);
    auto *brand=new QLabel("Obsidian");brand->setObjectName("brand");brandRow->addWidget(brand);brandRow->addStretch();side->addLayout(brandRow);
    auto *build=new QLabel("Version 1.0");build->setObjectName("muted");side->addWidget(build);side->addSpacing(30);
    auto *navigation=new QButtonGroup(this);
    auto nav=[&](const QString &text,const QString &name){auto *b=new QPushButton(text);b->setObjectName(name);b->setProperty("nav",true);b->setCheckable(true);navigation->addButton(b);side->addWidget(b);return b;};
    auto *playNav=nav("Bibliothek","playNav");playNav->setChecked(true);
    auto *accountNav=nav("Konto","accountNav");
    auto *settingsNav=nav("Einstellungen","settingsNav");
    auto *toolsNav=nav("Werkzeuge","toolsNav");
    auto *updatesNav=nav("Updates","updatesNav");
    side->addStretch();auto *consoleNav=new QPushButton("Live-Konsole");consoleNav->setProperty("nav",true);side->addWidget(consoleNav);
    auto *edition=new QLabel("Minecraft Java Edition");edition->setObjectName("eyebrow");side->addWidget(edition);
    auto *content=new QWidget;auto *outer=new QVBoxLayout(content);outer->setContentsMargins(28,26,28,20);outer->setSpacing(12);
    shellLayout->addWidget(sidebar);shellLayout->addWidget(content,1);
    auto *pages=new QStackedWidget;pages->setObjectName("pages");outer->addWidget(pages,1);
    auto page=[&](const QString &title,const QString &subtitle){
        auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
        auto *body=new QWidget;auto *box=new QVBoxLayout(body);box->setContentsMargins(0,0,6,0);box->setSpacing(preferences.contentSpacing);
        auto *h=new QLabel(title);h->setObjectName("pageTitle");box->addWidget(h);
        auto *s=new QLabel(subtitle);s->setObjectName("muted");s->setWordWrap(true);s->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);box->addWidget(s);
        scroll->setWidget(body);pages->addWidget(scroll);return box;
    };
    auto *library=page("Bibliothek","Deine Spiele. Bereit zum Start.");
    auto *settingsLayout=page("Einstellungen","Darstellung, Spielstart und Integrationen.");
    auto *account=page("Konto","Wähle, wie du Minecraft direkt aus Obsidian startest.");
    auto *tools=page("Werkzeuge","Dateien öffnen und die lokale Installation prüfen.");
    updates=new UpdatePanel([this]{return !scanning && !preparing && process.state()==QProcess::NotRunning;});pages->addWidget(updates);updates->setLanguage(preferences.language);
    connect(updatesNav,&QPushButton::clicked,this,[pages]{pages->setCurrentIndex(4);});
    hero=new WorldBanner;auto *heroLayout=new QVBoxLayout(hero);heroLayout->setContentsMargins(22,16,100,16);heroLayout->setSpacing(3);
    auto *heroTitle=new QLabel("Minecraft");heroTitle->setObjectName("hero");heroLayout->addWidget(heroTitle);
    auto *heroCaption=new QLabel("Java Edition · Lokale Installationen");heroCaption->setObjectName("muted");heroLayout->addWidget(heroCaption);library->addWidget(hero);
    auto *columns=new QHBoxLayout;columns->setSpacing(18);auto *browserColumn=new QVBoxLayout;auto *detailColumn=new QVBoxLayout;columns->addLayout(browserColumn,5);columns->addLayout(detailColumn,3);library->addLayout(columns,1);
    auto *browser=card(browserColumn,"INSTALLATIONEN","Installierte Versionen");auto *browserBox=qobject_cast<QVBoxLayout*>(browser->layout());
    auto *filters=new QHBoxLayout;versionSearch=new QLineEdit;versionSearch->setObjectName("versionSearch");versionSearch->setPlaceholderText("Versionen durchsuchen …");versionSearch->setClearButtonEnabled(true);filters->addWidget(versionSearch,1);
    loaderFilter=new QComboBox;loaderFilter->setObjectName("loaderFilter");loaderFilter->addItem("Alle Loader","");for(const auto &name:QStringList{"Vanilla","Fabric","Forge","NeoForge","Quilt"})loaderFilter->addItem(name,name);filters->addWidget(loaderFilter);browserBox->addLayout(filters);
    versions=new QComboBox;versions->setObjectName("versions");versions->setProperty("dynamicText",true);versions->setMinimumContentsLength(16);versions->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    libraryList=new QListWidget;libraryList->setObjectName("libraryList");libraryList->setMinimumHeight(220);libraryList->setTextElideMode(Qt::ElideRight);libraryList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);browserBox->addWidget(libraryList,1);
    countLabel=new QLabel;countLabel->setObjectName("muted");countLabel->setWordWrap(true);countLabel->setProperty("dynamicText",true);browserBox->addWidget(countLabel);
    auto *details=card(detailColumn,"AUSGEWÄHLTE VERSION","Startkonfiguration");auto *detailBox=qobject_cast<QVBoxLayout*>(details->layout());detailBox->addWidget(versions);auto *libraryAccount=new QLabel;libraryAccount->setObjectName("libraryAccount");libraryAccount->setProperty("dynamicText",true);libraryAccount->setWordWrap(true);detailBox->addWidget(libraryAccount);auto *accountLink=new QPushButton("Konto verbinden / wechseln");detailBox->addWidget(accountLink);connect(accountLink,&QPushButton::clicked,this,[pages,accountNav]{pages->setCurrentIndex(2);accountNav->setChecked(true);});
    versionInfo=new QLabel;versionInfo->setObjectName("versionInfo");versionInfo->setProperty("dynamicText",true);versionInfo->setWordWrap(true);versionInfo->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);versionInfo->setTextFormat(Qt::PlainText);detailBox->addWidget(versionInfo);
    javaPathLabel=new PathLabel;javaPathLabel->setObjectName("javaPath");auto *javaCaption=new QLabel("Java-Runtime");javaCaption->setObjectName("muted");detailBox->addWidget(javaCaption);detailBox->addWidget(javaPathLabel);
    pathLabel=new PathLabel(QDir::toNativeSeparators(root));pathLabel->setObjectName("path");auto *pathCaption=new QLabel("Minecraft-Ordner");pathCaption->setObjectName("muted");detailBox->addWidget(pathCaption);detailBox->addWidget(pathLabel);
    auto *detailActions=new QVBoxLayout;favoriteButton=new QPushButton("Als Favorit markieren");favoriteButton->setObjectName("favoriteButton");detailActions->addWidget(favoriteButton);
    refreshButton=new QPushButton("Neu scannen");browserBox->addWidget(refreshButton);
    auto *folder=new QPushButton("Installationsordner öffnen");detailActions->addWidget(folder);detailBox->addLayout(detailActions);
    auto *copyPath=new QPushButton("Pfad kopieren");copyPath->setProperty("quiet",true);detailBox->addWidget(copyPath);connect(copyPath,&QPushButton::clicked,this,[this]{qApp->clipboard()->setText(QDir::toNativeSeparators(root));});detailBox->addStretch();
    auto *modeCard=card(account,"SPIELSTART","Anmeldung");auto *modeBox=qobject_cast<QVBoxLayout*>(modeCard->layout());modeCard->setMaximumWidth(800);
    launchMode=new QComboBox;launchMode->setObjectName("launchMode");launchMode->addItem("Minecraft Access Token","custom");launchMode->addItem("Offline · Singleplayer","offline");launchMode->setCurrentIndex(0);modeBox->addWidget(launchMode);
    auto *modeHint=new QLabel("Wähle deine Anmeldemethode.");modeHint->setObjectName("modeHint");modeHint->setProperty("dynamicText",true);modeHint->setWordWrap(true);modeHint->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);modeBox->addWidget(modeHint);
    normalPanel=new QWidget;normalPanel->setObjectName("normalPanel");auto *normalLayout=new QVBoxLayout(normalPanel);normalLayout->setContentsMargins(0,10,0,0);normalLayout->setSpacing(14);modeBox->addWidget(normalPanel);
    accountState=new QLabel;accountState->setObjectName("accountState");accountState->setProperty("dynamicText",true);accountState->setWordWrap(true);accountState->setTextFormat(Qt::PlainText);modeBox->addWidget(accountState);
    signInButton=new QPushButton("Mit Microsoft anmelden");signInButton->setObjectName("signInButton");signInButton->setProperty("dynamicText",true);normalLayout->addWidget(signInButton);
    signOutButton=new QPushButton("Abmelden");signOutButton->setObjectName("signOutButton");normalLayout->addWidget(signOutButton);
    auto *officialLaunch=new QPushButton("Offiziellen Launcher öffnen");officialLaunch->setObjectName("officialLaunch");javaOverride.clear();modeBox->addWidget(officialLaunch);connect(officialLaunch,&QPushButton::clicked,this,&MainWindow::openOfficialLauncher);
    credentialPanel=new QWidget;credentialPanel->setObjectName("credentialPanel");auto *credentialLayout=new QVBoxLayout(credentialPanel);credentialLayout->setContentsMargins(0,12,0,0);credentialLayout->setSpacing(14);modeBox->addWidget(credentialPanel);
    username=new QLineEdit(credentialPanel);username->setObjectName("username");username->setMaxLength(16);username->setPlaceholderText("Minecraft-Profilname");
    uuid=new QLineEdit(credentialPanel);uuid->setObjectName("uuid");uuid->setMaxLength(36);uuid->setPlaceholderText("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx");
    token=new QLineEdit;token->setObjectName("token");token->setMaxLength(8192);token->setEchoMode(QLineEdit::Password);token->setPlaceholderText("Minecraft Access Token");
    auto credentialField=[&](const QString &label,QLineEdit *edit){auto *caption=new QLabel(label);credentialLayout->addWidget(caption);credentialLayout->addWidget(edit);};
    username->setReadOnly(true);uuid->setReadOnly(true);username->hide();uuid->hide();credentialField("Minecraft Access Token",token);
    auto *show=new QCheckBox("Token anzeigen");credentialLayout->addWidget(show);
    tokenSignIn=new QPushButton("Mit Token anmelden");tokenSignIn->setObjectName("tokenSignIn");credentialLayout->addWidget(tokenSignIn);
    sessionSummary=new QLabel;sessionSummary->setObjectName("sessionSummary");sessionSummary->setProperty("dynamicText",true);sessionSummary->setWordWrap(true);modeBox->addWidget(sessionSummary);
    modeBox->addWidget(signOutButton);
    auto *privacy=new QLabel("Profilname und UUID werden automatisch abgerufen. Die Sitzung bleibt nur bis zum Schließen im Arbeitsspeicher.");privacy->setObjectName("muted");privacy->setWordWrap(true);credentialLayout->addWidget(privacy);account->addStretch();
    offlinePanel=new QWidget;offlinePanel->setObjectName("offlinePanel");auto *offlineBox=new QVBoxLayout(offlinePanel);offlineBox->setContentsMargins(0,12,0,0);offlineBox->setSpacing(12);
    offlineBox->addWidget(new QLabel("Spielername"));offlineName=new QLineEdit;offlineName->setObjectName("offlineName");offlineName->setMaxLength(16);offlineName->setPlaceholderText("Player");offlineBox->addWidget(offlineName);
    auto *offlineHint=new QLabel("Für lokale Einzelspieler-Welten ohne Anmeldung. Die Version muss vollständig installiert sein. Server mit Konto-Prüfung benötigen eine gültige Anmeldung. Ein anderer Spielername erzeugt eine andere Offline-Identität und kann Inventar und Fortschritt verändern.");offlineHint->setWordWrap(true);offlineHint->setObjectName("muted");offlineBox->addWidget(offlineHint);modeBox->addWidget(offlinePanel);offlinePanel->hide();
    connect(offlineName,&QLineEdit::textChanged,this,&MainWindow::updateEnabled);
    auto *tabs=new QTabWidget;tabs->setObjectName("settingsTabs");settingsLayout->addWidget(tabs);
    auto settingsTab=[&](const QString &title){auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);auto *body=new QWidget;body->setMaximumWidth(810);auto *box=new QVBoxLayout(body);box->setContentsMargins(0,20,8,20);box->setSpacing(preferences.contentSpacing);scroll->setWidget(body);tabs->addTab(scroll,title);return box;};
    auto *appearanceLayout=settingsTab("Darstellung");auto *launchLayout=settingsTab("Spielstart");auto *logLayout=settingsTab("Konsole");auto *systemLayout=settingsTab("System");
    auto combo=[&](const QString &name,const QList<QPair<QString,QString>> &items,const QString &selected){auto *c=new QComboBox;c->setObjectName(name);for(const auto &item:items)c->addItem(item.first,item.second);c->setCurrentIndex(c->findData(selected));return c;};
    auto form=[](){auto *f=new QFormLayout;f->setVerticalSpacing(16);f->setRowWrapPolicy(QFormLayout::WrapLongRows);f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);return f;};
    auto *appearance=card(appearanceLayout,"DEIN STIL","Darstellung");auto *appearanceBox=qobject_cast<QVBoxLayout*>(appearance->layout());auto *appearanceForm=form();appearanceBox->addLayout(appearanceForm);appearanceForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    languageChoice=combo("languageChoice",{{"Deutsch","de"},{"English","en"}},preferences.language);
    themeChoice=combo("themeChoice",{{"Dunkel","dark"},{"Hell","light"},{"System","system"}},preferences.theme);
    accentChoice=combo("accentChoice",{{"Mint","mint"},{"Blau","blue"},{"Violett","violet"},{"Amber","amber"},{"Rose","rose"}},preferences.accent);
    fontChoice=combo("fontChoice",{{"Segoe UI Variable","Segoe UI Variable"},{"Segoe UI","Segoe UI"},{"Calibri","Calibri"},{"Arial","Arial"}},preferences.font);
    designChoice=combo("designChoice",{{"Soft · abgerundet","soft"},{"Studio · klare Kanten","sharp"},{"Hoher Kontrast","contrast"}},preferences.design);
    textSizeChoice=new QSpinBox;textSizeChoice->setObjectName("textSizeChoice");textSizeChoice->setRange(12,18);textSizeChoice->setValue(preferences.textSize);textSizeChoice->setSuffix(" px");
    appearanceForm->addRow("Sprache",languageChoice);appearanceForm->addRow("Farbschema",themeChoice);appearanceForm->addRow("Akzentfarbe",accentChoice);appearanceForm->addRow("Schrift",fontChoice);appearanceForm->addRow("Textgröße",textSizeChoice);appearanceForm->addRow("Design",designChoice);
    auto checkbox=[&](QVBoxLayout *box,const QString &text,const QString &name,bool value){auto *c=new QCheckBox(text);c->setObjectName(name);c->setChecked(value);box->addWidget(c);return c;};
    auto *showHero=checkbox(appearanceBox,"Titelgrafik anzeigen","showHero",preferences.showHero);hero->setVisible(preferences.showHero);
    auto *animate=checkbox(appearanceBox,"Animationen aktivieren","animations",preferences.animations);
    auto *layoutForm=form();appearanceBox->addLayout(layoutForm);
    auto *sidebarSize=new QSpinBox;sidebarSize->setObjectName("sidebarSize");sidebarSize->setRange(176,240);sidebarSize->setValue(preferences.sidebarWidth);sidebarSize->setSuffix(" px");layoutForm->addRow("Breite der Navigation",sidebarSize);
    auto *spacing=new QSpinBox;spacing->setObjectName("contentSpacing");spacing->setRange(8,24);spacing->setValue(preferences.contentSpacing);spacing->setSuffix(" px");layoutForm->addRow("Abstand der Bereiche",spacing);
    connect(animate,&QCheckBox::toggled,this,[this](bool on){preferences.animations=on;savePreferences();});
    connect(sidebarSize,&QSpinBox::valueChanged,this,[this,sidebar](int n){preferences.sidebarWidth=n;sidebar->setFixedWidth(n);savePreferences();});
    connect(spacing,&QSpinBox::valueChanged,this,[this,pages](int n){preferences.contentSpacing=n;for(auto *scroll:pages->findChildren<QScrollArea*>(QString(),Qt::FindDirectChildrenOnly))if(scroll->widget() && scroll->widget()->layout())scroll->widget()->layout()->setSpacing(n);savePreferences();});
    auto *rememberWindow=checkbox(appearanceBox,"Fenstergröße merken","rememberWindow",preferences.rememberWindow);
    auto *browseCard=card(appearanceLayout,"BIBLIOTHEK","Sortierung & Filter");auto *browseBox=qobject_cast<QVBoxLayout*>(browseCard->layout());
    auto *favoritesFirst=checkbox(browseBox,"Favoriten zuerst","favoritesFirst",preferences.favoritesFirst);
    auto *readyOnly=checkbox(browseBox,"Nur startbereite Versionen anzeigen","readyOnly",preferences.readyOnly);appearanceLayout->addStretch();
    auto *launchSettings=card(launchLayout,"LEISTUNG","Spielstart");auto *launchBox=qobject_cast<QVBoxLayout*>(launchSettings->layout());auto *launchForm=form();launchBox->addLayout(launchForm);
    memory=new QSpinBox;memory->setObjectName("memory");memory->setRange(1024,32768);memory->setSingleStep(512);memory->setValue(preferences.memoryMb);memory->setSuffix(" MB");launchForm->addRow("Arbeitsspeicher",memory);
    auto *versionMemory=checkbox(launchBox,"RAM pro Version speichern","versionMemory",preferences.versionMemory);
    auto *customResolution=checkbox(launchBox,"Eigene Fensterauflösung","customResolution",preferences.customResolution);
    auto *resolutionRow=new QHBoxLayout;resolutionWidth=new QSpinBox;resolutionWidth->setObjectName("resolutionWidth");resolutionWidth->setRange(640,7680);resolutionWidth->setValue(preferences.resolutionWidth);resolutionWidth->setSuffix(" px");resolutionHeight=new QSpinBox;resolutionHeight->setObjectName("resolutionHeight");resolutionHeight->setRange(480,4320);resolutionHeight->setValue(preferences.resolutionHeight);resolutionHeight->setSuffix(" px");resolutionRow->addWidget(resolutionWidth);resolutionRow->addWidget(new QLabel("×"));resolutionRow->addWidget(resolutionHeight);launchBox->addLayout(resolutionRow);
    auto *fullscreen=checkbox(launchBox,"Minecraft im Vollbild starten","fullscreen",preferences.fullscreen);
    auto *minimize=checkbox(launchBox,"Obsidian beim Spielstart minimieren","minimizeOnLaunch",preferences.minimizeOnLaunch);launchLayout->addStretch();
    auto *consoleCard=card(logLayout,"LIVE-LOGS","Konsole");auto *consoleBox=qobject_cast<QVBoxLayout*>(consoleCard->layout());
    autoLogs=checkbox(consoleBox,"Live-Konsole beim Spielstart öffnen","autoLogs",preferences.autoShowLogs);
    auto *logFollow=checkbox(consoleBox,"Neue Log-Zeilen automatisch verfolgen","logFollow",preferences.logFollow);
    auto *logForm=form();logLimit=new QSpinBox;logLimit->setObjectName("logLimit");logLimit->setRange(1000,50000);logLimit->setSingleStep(1000);logLimit->setValue(preferences.logLimit);logForm->addRow("Maximale Log-Zeilen",logLimit);consoleBox->addLayout(logForm);
    auto *exportHint=new QLabel("Logs bleiben im Arbeitsspeicher. Export nur über die Schaltfläche in der Konsole.");exportHint->setWordWrap(true);exportHint->setObjectName("muted");consoleBox->addWidget(exportHint);logLayout->addStretch();
    auto *discordCard=card(systemLayout,"DISCORD","Discord-Aktivität");auto *discordBox=qobject_cast<QVBoxLayout*>(discordCard->layout());
    auto *discordToggle=checkbox(discordBox,"Aktivität auf Discord anzeigen","discordPresence",preferences.discordPresence);
    auto *discordHint=new QLabel("Zeigt Launcher oder Minecraft-Version und Spielzeit. Discord muss geöffnet und die Aktivitätsanzeige aktiviert sein. Keine Zugangsdaten werden übertragen.");discordHint->setWordWrap(true);discordHint->setObjectName("muted");discordBox->addWidget(discordHint);
    discordState=new QLabel;discordState->setObjectName("discordState");discordState->setProperty("dynamicText",true);discordState->setWordWrap(true);discordBox->addWidget(discordState);
    discord=new DiscordPresence(this);connect(discord,&DiscordPresence::statusChanged,this,[this](const QString &text){discordState->setText(Ui::text(text));});
    connect(discordToggle,&QCheckBox::toggled,this,[this](bool on){preferences.discordPresence=on;savePreferences();discord->setEnabled(on && !qApp->property("disableDiscordForTests").toBool());});
    discord->setEnabled(preferences.discordPresence && !qApp->property("disableDiscordForTests").toBool());discordState->setText(Ui::text(discord->status()));
    auto *javaCard=card(systemLayout,"RUNTIME","Java & Launcher");auto *javaBox=qobject_cast<QVBoxLayout*>(javaCard->layout());
    auto *javaHint=new QLabel("Java wird automatisch gesucht. Eine manuelle Auswahl gilt nur für diese Sitzung.");javaHint->setWordWrap(true);javaHint->setObjectName("muted");javaBox->addWidget(javaHint);
    javaButton=new QPushButton("Java auswählen …");resetJavaButton=new QPushButton("Java-Auswahl zurücksetzen");javaBox->addWidget(javaButton);javaBox->addWidget(resetJavaButton);
    auto *officialButton=new QPushButton("Offiziellen Launcher auswählen …");javaBox->addWidget(officialButton);connect(officialButton,&QPushButton::clicked,this,[this]{const auto file=QFileDialog::getOpenFileName(this,Ui::text("Offiziellen Launcher auswählen …"),{},"Launcher (*.exe *.lnk)");if(!file.isEmpty()){preferences.officialLauncher=file;savePreferences();}});
    clientIdEdit=new QLineEdit(this);clientIdEdit->hide();systemLayout->addStretch();
    auto *folders=card(tools,"SCHNELLZUGRIFF","Minecraft-Dateien");auto *folderBox=qobject_cast<QVBoxLayout*>(folders->layout());
    for(const auto &name:QStringList{"mods","resourcepacks","screenshots","saves"}){auto *b=new QPushButton(name);folderBox->addWidget(b);connect(b,&QPushButton::clicked,this,[this,name]{const auto path=QDir(root).filePath(name);if(!QDir().mkpath(path)||!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))status("Ordner konnte nicht geöffnet werden.",true);});}
    auto *diagnosticCard=card(tools,"INSTALLATION","Diagnose");auto *diagnosticBox=qobject_cast<QVBoxLayout*>(diagnosticCard->layout());diagnostics=new QLabel;diagnostics->setObjectName("diagnostics");diagnostics->setProperty("dynamicText",true);diagnostics->setWordWrap(true);diagnostics->setTextFormat(Qt::PlainText);diagnosticBox->addWidget(diagnostics);
    auto *copyReport=new QPushButton("Diagnose kopieren");diagnosticBox->addWidget(copyReport);connect(copyReport,&QPushButton::clicked,this,[this]{qApp->clipboard()->setText("Obsidian 1.0\n"+QDir::toNativeSeparators(root)+"\n"+diagnostics->text()+"\n"+versionInfo->text()+"\n"+javaPathLabel->fullPath());status("Diagnose kopiert. Keine Zugangsdaten enthalten.");});tools->addStretch();
    statusText=new QLabel;statusText->setObjectName("status");statusText->setProperty("dynamicText",true);statusText->setTextFormat(Qt::PlainText);statusText->setWordWrap(true);statusText->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);outer->addWidget(statusText);
    auto *footer=new QFrame;footer->setObjectName("launchBar");auto *actions=new QHBoxLayout(footer);actions->setContentsMargins(0,16,0,0);actions->setSpacing(12);
    auto *logButton=new QPushButton("Konsole");actions->addWidget(logButton);stopButton=new QPushButton("Spiel beenden");actions->addWidget(stopButton);
    auto *accountShortcut=new QPushButton("Konto");actions->addWidget(accountShortcut);actions->addStretch();
    launchButton=new QPushButton;launchButton->setObjectName("primary");launchButton->setProperty("dynamicText",true);launchButton->setMinimumWidth(206);actions->addWidget(launchButton);outer->addWidget(footer);
    auto *fadeEffect=new QGraphicsOpacityEffect(pages);pages->setGraphicsEffect(fadeEffect);
    auto *fade=new QPropertyAnimation(fadeEffect,"opacity",pages);fade->setDuration(150);fade->setEasingCurve(QEasingCurve::OutCubic);
    connect(pages,&QStackedWidget::currentChanged,this,[this,fade,fadeEffect](int){fade->stop();if(preferences.animations && !qApp->property("disableAnimationsForTests").toBool()){fade->setStartValue(.35);fade->setEndValue(1.);fade->start();}else fadeEffect->setOpacity(1.);});
    connect(accountNav,&QPushButton::clicked,this,[pages]{pages->setCurrentIndex(2);});connect(toolsNav,&QPushButton::clicked,this,[pages]{pages->setCurrentIndex(3);});connect(accountShortcut,&QPushButton::clicked,this,[pages,accountNav]{pages->setCurrentIndex(2);accountNav->setChecked(true);});
    connect(showHero,&QCheckBox::toggled,this,[this](bool on){preferences.showHero=on;hero->setVisible(on);savePreferences();});
    connect(rememberWindow,&QCheckBox::toggled,this,[this](bool on){preferences.rememberWindow=on;savePreferences();});
    connect(favoritesFirst,&QCheckBox::toggled,this,[this](bool on){preferences.favoritesFirst=on;savePreferences();rebuildLibrary();});
    connect(readyOnly,&QCheckBox::toggled,this,[this](bool on){preferences.readyOnly=on;savePreferences();rebuildLibrary();});
    connect(versionMemory,&QCheckBox::toggled,this,[this](bool on){preferences.versionMemory=on;savePreferences();selectionChanged();rebuildLibrary();});
    connect(customResolution,&QCheckBox::toggled,this,[this](bool on){preferences.customResolution=on;resolutionWidth->setEnabled(on);resolutionHeight->setEnabled(on);savePreferences();});resolutionWidth->setEnabled(preferences.customResolution);resolutionHeight->setEnabled(preferences.customResolution);
    connect(resolutionWidth,&QSpinBox::valueChanged,this,[this](int n){preferences.resolutionWidth=n;savePreferences();});connect(resolutionHeight,&QSpinBox::valueChanged,this,[this](int n){preferences.resolutionHeight=n;savePreferences();});
    connect(fullscreen,&QCheckBox::toggled,this,[this](bool on){preferences.fullscreen=on;savePreferences();});connect(minimize,&QCheckBox::toggled,this,[this](bool on){preferences.minimizeOnLaunch=on;savePreferences();});
    connect(logFollow,&QCheckBox::toggled,this,[this](bool on){preferences.logFollow=on;if(logs)logs->configure(preferences.logLimit,on);savePreferences();});
    connect(logLimit,&QSpinBox::valueChanged,this,[this](int n){preferences.logLimit=n;if(logs)logs->configure(n,preferences.logFollow);savePreferences();});
    connect(versionSearch,&QLineEdit::textChanged,this,&MainWindow::rebuildLibrary);connect(loaderFilter,&QComboBox::currentIndexChanged,this,&MainWindow::rebuildLibrary);
    connect(libraryList,&QListWidget::currentItemChanged,this,[this](QListWidgetItem *item){if(item)versions->setCurrentIndex(versions->findData(item->data(Qt::UserRole)));});

    logs=new LogWindow(this);logs->configure(preferences.logLimit,preferences.logFollow);auth=new AuthClient(this,authEndpoints);
    connect(auth,&AuthClient::stateChanged,this,&MainWindow::updateEnabled);
    connect(auth,&AuthClient::authenticated,this,[this]{token->clear();status("Konto verbunden. Version in der Bibliothek auswählen und starten.");updateEnabled();});
    connect(auth,&AuthClient::failed,this,[this](const QString &message){status(message,true);updateEnabled();});
    connect(signOutButton,&QPushButton::clicked,this,[this]{auth->signOut();status("Abgemeldet.");});
    credentialPanel->setVisible(true);normalPanel->setVisible(false);
    connect(tokenSignIn,&QPushButton::clicked,this,[this]{auto access=token->text();token->clear();auth->beginToken(access);});
    connect(token,&QLineEdit::returnPressed,tokenSignIn,&QPushButton::click);
    versions->setEditable(true);versions->setInsertPolicy(QComboBox::NoInsert);versions->completer()->setFilterMode(Qt::MatchContains);versions->completer()->setCompletionMode(QCompleter::PopupCompletion);
    connect(versions,&QComboBox::editTextChanged,this,&MainWindow::updateEnabled);
    Ui::capture(shell);Ui::refresh(shell);refreshLanguage();
    connect(launchMode,&QComboBox::currentIndexChanged,this,&MainWindow::changeMode);
    connect(favoriteButton,&QPushButton::clicked,this,[this]{const auto id=versions->currentData().toString();if(id.isEmpty())return;if(preferences.favorites.contains(id))preferences.favorites.removeAll(id);else preferences.favorites.append(id);savePreferences();selectionChanged();rebuildLibrary();});
    auto *refreshKey=new QShortcut(QKeySequence("F5"),this);connect(refreshKey,&QShortcut::activated,this,&MainWindow::rescan);
    auto *settingsKey=new QShortcut(QKeySequence("Ctrl+,"),this);connect(settingsKey,&QShortcut::activated,this,[pages,settingsNav]{pages->setCurrentIndex(1);settingsNav->setChecked(true);});
    connect(playNav,&QPushButton::clicked,this,[pages]{pages->setCurrentIndex(0);});connect(settingsNav,&QPushButton::clicked,this,[pages]{pages->setCurrentIndex(1);});
    auto openLogs=[this]{logs->show();logs->raise();logs->activateWindow();};connect(consoleNav,&QPushButton::clicked,this,openLogs);connect(logButton,&QPushButton::clicked,this,openLogs);
    connect(show,&QCheckBox::toggled,this,[this](bool visible){token->setEchoMode(visible?QLineEdit::Normal:QLineEdit::Password);});
    connect(folder,&QPushButton::clicked,this,[this]{QDesktopServices::openUrl(QUrl::fromLocalFile(root));});
    connect(refreshButton,&QPushButton::clicked,this,&MainWindow::rescan);connect(versions,&QComboBox::currentIndexChanged,this,&MainWindow::selectionChanged);connect(launchButton,&QPushButton::clicked,this,&MainWindow::launch);connect(stopButton,&QPushButton::clicked,this,&MainWindow::stop);
    connect(javaButton,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,Ui::text("64-Bit Java auswählen"),{},"Java (java.exe)");if(!path.isEmpty()){javaOverride=path;javaButton->setToolTip(path);rescan();}});
    connect(resetJavaButton,&QPushButton::clicked,this,[this]{javaOverride.clear();javaButton->setToolTip({});rescan();});
    connect(languageChoice,&QComboBox::currentIndexChanged,this,[this]{preferences.language=languageChoice->currentData().toString();Ui::setLanguage(preferences.language);refreshLanguage();savePreferences();});
    auto appearanceChanged=[this]{preferences.theme=themeChoice->currentData().toString();preferences.accent=accentChoice->currentData().toString();preferences.font=fontChoice->currentData().toString();preferences.design=designChoice->currentData().toString();preferences.textSize=textSizeChoice->value();applyTheme(*qApp,preferences.theme,preferences.accent,preferences.font,preferences.textSize,preferences.design);logs->refreshAppearance();savePreferences();};
    connect(themeChoice,&QComboBox::currentIndexChanged,this,appearanceChanged);connect(accentChoice,&QComboBox::currentIndexChanged,this,appearanceChanged);connect(fontChoice,&QComboBox::currentIndexChanged,this,appearanceChanged);connect(designChoice,&QComboBox::currentIndexChanged,this,appearanceChanged);connect(textSizeChoice,&QSpinBox::valueChanged,this,appearanceChanged);
    connect(memory,&QSpinBox::valueChanged,this,[this](int value){if(preferences.versionMemory && !versions->currentData().toString().isEmpty())preferences.versionRam[versions->currentData().toString()]=value;else preferences.memoryMb=value;savePreferences();});connect(autoLogs,&QCheckBox::toggled,this,[this](bool enabled){preferences.autoShowLogs=enabled;savePreferences();});
    connect(qApp->styleHints(),&QStyleHints::colorSchemeChanged,this,[this]{if(preferences.theme=="system"){applyTheme(*qApp,preferences.theme,preferences.accent,preferences.font,preferences.textSize,preferences.design);logs->refreshAppearance();}});
    process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&process,&QProcess::readyReadStandardOutput,this,[this]{drain();});
    connect(&process,&QProcess::readyReadStandardError,this,[this]{drain();});
    connect(&process,&QProcess::started,this,[this]{
        const auto index=versions->currentIndex();if(index>=0 && index<entries.size())discord->setActivity(true,entries[index].id,entries[index].loader);
        token->clear();if(preferences.minimizeOnLaunch)showMinimized();
        status("Minecraft läuft. Die Live-Konsole zeigt stdout und stderr."); logs->setStatus("Minecraft läuft"); updateEnabled();
    });
    connect(&process,&QProcess::finished,this,&MainWindow::finish);
    connect(&process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if (error==QProcess::FailedToStart) {
            drain(true); token->clear(); process.setArguments({}); redactor.clear(); natives.reset();
            status("Java konnte nicht gestartet werden. Pfad, Zugriffsrechte und Runtime prüfen.",true);
            logs->append("Launcher","Java konnte nicht gestartet werden.\n"); logs->setStatus("Start fehlgeschlagen"); updateEnabled();
        }
    });
    auto *sessionTimer=new QTimer(this);sessionTimer->setInterval(30000);connect(sessionTimer,&QTimer::timeout,this,&MainWindow::updateEnabled);sessionTimer->start();
    if(preferences.rememberWindow){auto size=QSize(preferences.windowWidth,preferences.windowHeight);if(auto *screen=QGuiApplication::primaryScreen())size=size.boundedTo(screen->availableGeometry().size());resize(size);}
    QTimer::singleShot(0,this,&MainWindow::rescan);
}
void MainWindow::status(const QString &message,bool error) {
    lastStatus=message; lastStatusError=error; statusText->setText(Ui::text(message=="Konto verbinden → Version auswählen → Minecraft starten." && launchMode->currentData()=="offline"?"Offline-Singleplayer: Version auswählen und starten.":message)); statusText->setProperty("error",error); statusText->style()->unpolish(statusText); statusText->style()->polish(statusText);
}
void MainWindow::updateEnabled() {
    const bool busy=scanning || preparing || process.state()!=QProcess::NotRunning || (auth && auth->busy());
    const int index=versions->currentIndex();

    launchButton->setEnabled(!busy && index>=0 && index<entries.size() && entries[index].ready() && versions->currentText()==versions->itemText(index) && (launchMode->currentData()=="offline"?QRegularExpression("^[A-Za-z0-9_]{1,16}$").match(offlineName->text()).hasMatch():auth->signedIn()));
    offlineName->setEnabled(!busy);
    tokenSignIn->setEnabled(!busy);
    signInButton->setEnabled(!busy);signOutButton->setVisible(auth->signedIn());signOutButton->setEnabled(!busy && auth->signedIn());clientIdEdit->setEnabled(!busy);
    signInButton->setText(Ui::text("Mit Microsoft anmelden"));
    accountState->setText(Ui::text(launchMode->currentData()=="offline"?"Offline · Keine Anmeldung erforderlich":auth->signedIn()?"Konto verbunden":auth->busy()?"Profil wird geprüft …":"Noch kein Konto verbunden"));
    const auto profile=auth->credentials();if(auto *label=findChild<QLabel*>("libraryAccount"))label->setText(launchMode->currentData()=="offline"?Ui::text("Offline-Spieler: %1").arg(offlineName->text()):auth->signedIn()?Ui::text("Verbunden: %1").arg(profile.username):Ui::text("Noch kein Konto verbunden"));username->setText(profile.username);uuid->setText(profile.uuid);
    sessionSummary->setText(auth->signedIn()?Ui::text("Profil: %1\nUUID: %2").arg(profile.username,profile.uuid):QString());
    sessionSummary->setVisible(auth->signedIn() && launchMode->currentData()!="offline");
    launchMode->setEnabled(!busy);favoriteButton->setEnabled(!busy && index>=0 && index<entries.size());
    versions->setEnabled(!busy); refreshButton->setEnabled(!busy); javaButton->setEnabled(!busy); resetJavaButton->setEnabled(!busy); memory->setEnabled(!busy);
    username->setEnabled(!busy); uuid->setEnabled(!busy); token->setEnabled(!busy);
    stopButton->setEnabled(process.state()==QProcess::Running);
    launchButton->setText(Ui::text(preparing?"Start wird vorbereitet …":process.state()!=QProcess::NotRunning?"Minecraft läuft":"Minecraft starten  →"));
}
void MainWindow::rescan() {
    if (scanning || preparing || process.state()!=QProcess::NotRunning) return;
    scanning=true; status("Installierte Profile und Java-Runtimes werden geprüft …"); updateEnabled();
    const QString old=versions->currentData().toString().isEmpty()?preferences.lastVersion:versions->currentData().toString();
    auto *watcher=new QFutureWatcher<QVector<Minecraft::Entry>>(this);
    connect(watcher,&QFutureWatcherBase::finished,this,[this,watcher,old]{
        entries=watcher->result(); watcher->deleteLater(); scanning=false;
        std::stable_sort(entries.begin(),entries.end(),[this](const auto &a,const auto &b){return preferences.favorites.contains(a.id)>preferences.favorites.contains(b.id);});
        versions->blockSignals(true); versions->clear(); int ready=0;
        for (const auto &entry:entries) {
            versions->addItem(entry.ready()?entry.id+"  ·  "+entry.loader:Ui::text("%1  ·  %2  ·  unvollständig").arg(entry.id,entry.loader),entry.id);
            if (entry.ready()) ++ready;
            versions->setItemData(versions->count()-1,entry.error.isEmpty()?entry.id:Ui::text(entry.error),Qt::ToolTipRole);
        }
        const int previous=versions->findData(old); if (previous>=0) versions->setCurrentIndex(previous);
        versions->blockSignals(false);
        countLabel->setText(Ui::text("%1 startbereit  ·  %2 installierte Profile").arg(ready).arg(entries.size()));
        selectionChanged();rebuildLibrary();
        if (entries.isEmpty()) {
            if(launchMode->currentData()=="normal")status("Konto verbinden → Version auswählen → Minecraft starten.");
            else status("Keine Versionen gefunden. Bitte Minecraft Java Edition einmal im offiziellen Launcher installieren und starten.",true);
        }
    });
    const QString scanRoot=root, override=javaOverride;
    watcher->setFuture(QtConcurrent::run([scanRoot,override]{return Minecraft::scan(scanRoot,override);}));
}
void MainWindow::selectionChanged() {
    const int index=versions->currentIndex();
    if (index<0 || index>=entries.size()) { versionInfo->clear();javaPathLabel->setPath({}); updateEnabled(); return; }
    const auto &entry=entries[index];
    {QSignalBlocker block(memory);memory->setValue(preferences.versionMemory?preferences.versionRam.value(entry.id,preferences.memoryMb):preferences.memoryMb);}
    libraryList->blockSignals(true);for(int row=0;row<libraryList->count();++row)if(libraryList->item(row)->data(Qt::UserRole).toString()==entry.id)libraryList->setCurrentRow(row);libraryList->blockSignals(false);
    favoriteButton->setText(Ui::text(preferences.favorites.contains(entry.id)?"★ Favorit entfernen":"☆ Als Favorit markieren"));favoriteButton->setProperty("dynamicText",true);
    if (!entry.ready()) {
        javaPathLabel->setPath({});versionInfo->setToolTip(Ui::text(entry.error));
        versionInfo->setText(Ui::text(entry.error).split('\n').mid(0,5).join('\n')+(entry.error.count('\n')>=5?"\n"+Ui::text("Weitere Details im Tooltip der Version."):""));
        status("Diese Installation ist unvollständig oder benötigt eine passende Java-Runtime.",true);
    } else {
        versionInfo->setText(Ui::text("%1  ·  Java %2  ·  Startbereit").arg(entry.loader).arg(entry.plan.javaMajor));versionInfo->setToolTip({});javaPathLabel->setPath(QDir::toNativeSeparators(entry.plan.java));
        status(launchMode->currentData()=="normal"?"Konto verbinden → Version auswählen → Minecraft starten.":"Konto verbinden → Version auswählen → Minecraft starten.");
    }
    if(launchMode->currentData()=="normal")status("Konto verbinden → Version auswählen → Minecraft starten.");
    updateEnabled();
}
void MainWindow::changeMode() {
    const bool custom=launchMode->currentData()=="custom",offline=launchMode->currentData()=="offline";credentialPanel->setVisible(custom);normalPanel->setVisible(!custom && !offline);offlinePanel->setVisible(offline);
    if(auto *hint=findChild<QLabel*>("modeHint"))hint->setText(Ui::text(offline?"Spielernamen eingeben und eine installierte Version aus der Bibliothek starten.":"Wähle deine Anmeldemethode."));
    if(!custom){token->clear();status("Konto verbinden → Version auswählen → Minecraft starten.");}
    else selectionChanged();updateEnabled();
}
void MainWindow::openOfficialLauncher() {
    QString path=preferences.officialLauncher;
    if(path.isEmpty()){
        const QStringList roots={QDir(qEnvironmentVariable("APPDATA")).filePath("Microsoft/Windows/Start Menu/Programs"),QDir(qEnvironmentVariable("PROGRAMDATA")).filePath("Microsoft/Windows/Start Menu/Programs")};
        for(const auto &folder:roots){QDirIterator it(folder,{"*Minecraft*.lnk"},QDir::Files,QDirIterator::Subdirectories);while(it.hasNext()){const auto candidate=it.next();if(QFileInfo(candidate).fileName().contains("Launcher",Qt::CaseInsensitive)){path=candidate;break;}}if(!path.isEmpty())break;}
        if(path.isEmpty()){for(const auto &folder:QStringList{qEnvironmentVariable("ProgramFiles(x86)"),qEnvironmentVariable("ProgramFiles")}){const auto candidate=QDir(folder).filePath("Minecraft Launcher/MinecraftLauncher.exe");if(QFileInfo::exists(candidate)){path=candidate;break;}}}
    }
    if(path.isEmpty()){
        // Discover the Store launcher from its installed manifest, without guessing an application ID.
        QProcess discovery;
        const auto powershell=QDir(qEnvironmentVariable("SystemRoot")).filePath("System32/WindowsPowerShell/v1.0/powershell.exe");
        discovery.start(powershell,{"-NoProfile","-NonInteractive","-Command","$p=Get-AppxPackage -Name Microsoft.4297127D64EC6 -ErrorAction SilentlyContinue | Select-Object -First 1; if($p){$m=Get-AppxPackageManifest -Package $p; $a=@($m.Package.Applications.Application)[0]; if($a.Id){[Console]::Write($p.PackageFamilyName+'!'+$a.Id)}}"});
        if(discovery.waitForFinished(5000) && discovery.exitCode()==0){
            const QString id=QString::fromUtf8(discovery.readAllStandardOutput()).trimmed();
            if(id.startsWith("Microsoft.4297127D64EC6_") && !id.contains('/') && !id.contains('\\') && !id.contains('\n')){
                if(QProcess::startDetached(QDir(qEnvironmentVariable("SystemRoot")).filePath("explorer.exe"),{"shell:AppsFolder\\"+id})){
                    token->clear();status("Offizieller Launcher geöffnet. Dort anmelden und Minecraft starten.");return;
                }
            }
        } else {discovery.kill();discovery.waitForFinished(1000);}
    }
    if(path.isEmpty()){path=QFileDialog::getOpenFileName(this,Ui::text("Offiziellen Launcher auswählen …"),{},"Launcher (*.exe *.lnk)");if(path.isEmpty()){status("Bitte den offiziellen Minecraft Launcher in den Einstellungen auswählen.",true);return;}preferences.officialLauncher=path;savePreferences();}
    bool opened=false;if(QFileInfo::exists(path)){if(path.endsWith(".exe",Qt::CaseInsensitive))opened=QProcess::startDetached(path,{},QFileInfo(path).absolutePath());else if(path.endsWith(".lnk",Qt::CaseInsensitive))opened=QDesktopServices::openUrl(QUrl::fromLocalFile(path));}
    if(opened){token->clear();status("Offizieller Launcher geöffnet. Dort anmelden und Minecraft starten.");}
    else status("Offizieller Launcher konnte nicht geöffnet werden. Auswahl in den Einstellungen prüfen.",true);
}
void MainWindow::launch() {
    if(auth->busy())return;
    if(launchMode->currentData()!="offline" && !auth->signedIn()){status("Bitte zuerst unter Konto anmelden.",true);return;}
    const int index=versions->currentIndex();
    if (index<0 || index>=entries.size() || !entries[index].ready() || versions->currentText()!=versions->itemText(index) || preparing || scanning || process.state()!=QProcess::NotRunning) return;
    try { Minecraft::validateCredentials(launchCredentials()); }
    catch (const Minecraft::Error &error) { status(error.message,true); return; }
    preferences.lastVersion=entries[index].id;savePreferences();
    const auto plan=entries[index].plan;
    natives=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/obsidian-natives-XXXXXX");
    if (!natives->isValid()) { status("Temporärer Ordner für Native-Bibliotheken konnte nicht angelegt werden.",true); return; }
    const auto temp=natives;
    preparing=true; status("Assets werden geprüft und Native-Bibliotheken vorbereitet …"); updateEnabled();
    logs->reset(); if(preferences.autoShowLogs){logs->show();logs->raise();} logs->setStatus("Start wird vorbereitet");
    logs->append("Launcher","Version: "+plan.profile.id+" · "+plan.profile.loader+"\n");
    auto *watcher=new QFutureWatcher<Preparation>(this);
    // No credentials are copied into the background task.
    connect(watcher,&QFutureWatcherBase::finished,this,[this,watcher,plan,temp]{
        const auto result=watcher->result(); watcher->deleteLater(); preparing=false;
        if (!result.error.isEmpty()) {
            natives.reset(); status(result.error,true); logs->append("Launcher",result.error+"\n"); logs->setStatus("Start fehlgeschlagen"); updateEnabled(); return;
        }
        try {
            const auto c=Minecraft::validateCredentials(launchCredentials());
            redactor.setSecrets({launchMode->currentData()=="offline"?QString():c.token,c.uuid,uuid->text(),c.username}); outDecoder.resetState(); errDecoder.resetState();
            auto args=Minecraft::launchArguments(plan,c,temp->path(),memory->value());
            Minecraft::applyWindowOptions(args,preferences.customResolution?preferences.resolutionWidth:0,preferences.customResolution?preferences.resolutionHeight:0,preferences.fullscreen);
            // Upper bound for Windows quoting, including doubled backslashes at quotes/end.
            qsizetype units=plan.java.size()+3;
            for (const auto &arg:args) {
                units+=arg.size()+3; qsizetype slashes=0;
                for (const auto ch:arg) { if (ch=='\\') ++slashes; else {if (ch=='\"') units+=slashes+1; slashes=0;} }
                units+=slashes;
            }
            if (units>32000) throw Minecraft::Error{"Startargumente sind zu lang für Windows. Kürzere Installationspfade verwenden."};
            auto environment=QProcessEnvironment::systemEnvironment();
            // Prevent unrelated global Java flags from adding output/files or overriding launch flags.
            environment.remove("JAVA_TOOL_OPTIONS"); environment.remove("_JAVA_OPTIONS"); environment.remove("JDK_JAVA_OPTIONS");
            process.setProcessEnvironment(environment); process.setWorkingDirectory(root);
            status("Java wird gestartet …"); process.start(plan.java,args,QIODevice::ReadOnly); updateEnabled();
        } catch (const Minecraft::Error &error) {
            redactor.clear(); natives.reset(); status(error.message,true); logs->append("Launcher",error.message+"\n"); logs->setStatus("Start fehlgeschlagen"); updateEnabled();
        }
    });
    watcher->setFuture(QtConcurrent::run([plan,temp]{
        Preparation result;
        try { Minecraft::validateAssets(plan); Minecraft::extractNatives(plan,temp->path()); }
        catch (const Minecraft::Error &error) { result.error=error.message; }
        return result;
    }));
}
void MainWindow::drain(bool final) {
    const QString out=outDecoder(process.readAllStandardOutput()), err=errDecoder(process.readAllStandardError());
    logs->append("stdout",redactor.feed("stdout",out,final)); logs->append("stderr",redactor.feed("stderr",err,final));
}
void MainWindow::finish(int code,QProcess::ExitStatus exitStatus) {
    if(preferences.minimizeOnLaunch && isMinimized())showNormal();
    discord->setActivity(false);
    ++stopGeneration; drain(true); process.setArguments({}); token->clear(); redactor.clear(); natives.reset();
    const bool failed=code!=0 || exitStatus==QProcess::CrashExit;
    const auto message=failed?QString("Minecraft wurde mit Fehler beendet (Exitcode %1). Details in der Live-Konsole.").arg(code):"Minecraft wurde beendet. Bereit für den nächsten Start.";
    status(message,failed); logs->append("Launcher",message+"\n"); logs->setStatus(failed?"Mit Fehler beendet":"Beendet"); updateEnabled();
}
void MainWindow::stop() {
    if (process.state()!=QProcess::Running) return;
    if (QMessageBox::question(this,Ui::text("Minecraft beenden"),Ui::text("Minecraft jetzt beenden? Nicht gespeicherte Welten können verloren gehen."),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return;
    status("Minecraft wird beendet …"); process.terminate();
    const auto generation=++stopGeneration;
    QTimer::singleShot(5000,this,[this,generation]{if (generation==stopGeneration && process.state()!=QProcess::NotRunning) process.kill();});
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (scanning || preparing) { event->ignore(); status("Bitte warten, bis die laufende Prüfung abgeschlossen ist."); return; }
    if (process.state()!=QProcess::NotRunning) {
        if (QMessageBox::question(this,Ui::text("Launcher schließen"),Ui::text("Minecraft läuft noch. Minecraft beenden und Launcher schließen? Nicht gespeicherte Welten können verloren gehen."),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) {event->ignore(); return;}
        process.kill(); process.waitForFinished(3000);
    }
    if(preferences.rememberWindow && !isMinimized()){preferences.windowWidth=width();preferences.windowHeight=height();savePreferences();}
    discord->shutdown();auth->signOut();token->clear(); redactor.clear(); event->accept();
}

void MainWindow::savePreferences() {if(!preferences.save()) status("Einstellungen konnten nicht gespeichert werden. Schreibrechte im Benutzerordner prüfen.",true);}
void MainWindow::refreshLanguage() {
    if(updates)updates->setLanguage(preferences.language);
    if(auto *hint=findChild<QLabel*>("modeHint"))hint->setText(Ui::text(launchMode->currentData()=="offline"?"Spielernamen eingeben und eine installierte Version aus der Bibliothek starten.":"Wähle deine Anmeldemethode."));
    Ui::refresh(centralWidget());if(discord)discordState->setText(Ui::text(discord->status()));if(logs) logs->refreshLanguage();
    const auto saved=lastStatus;const bool error=lastStatusError;
    if(!entries.isEmpty()) selectionChanged();
    int ready=0;for(const auto &entry:entries) if(entry.ready()) ++ready;
    countLabel->setText(Ui::text("%1 startbereit  ·  %2 installierte Profile").arg(ready).arg(entries.size()));
    for(int i=0;i<entries.size();++i){const auto &entry=entries[i];versions->setItemText(i,entry.ready()?entry.id+"  ·  "+entry.loader:Ui::text("%1  ·  %2  ·  unvollständig").arg(entry.id,entry.loader));versions->setItemData(i,Ui::text(entry.error),Qt::ToolTipRole);}
    if(!saved.isEmpty())status(saved,error);rebuildLibrary();
    if(auto *tabs=findChild<QTabWidget*>("settingsTabs")){const QStringList titles={"Darstellung","Spielstart","Konsole","System"};for(int i=0;i<titles.size();++i)tabs->setTabText(i,Ui::text(titles[i]));}
    updateEnabled();
}

Minecraft::Credentials MainWindow::launchCredentials() const {
    if(launchMode->currentData()=="offline"){const auto name=offlineName->text();return {name,Minecraft::offlineUuid(name),"0"};}
    return auth->credentials();
}
void MainWindow::launchNormal(){launch();}
void MainWindow::login() {
    if(scanning || preparing || process.state()!=QProcess::NotRunning || auth->busy())return;
    auto *dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(Ui::text("Microsoft-Anmeldung"));dialog->setMinimumWidth(460);dialog->resize(520,340);
    auto *box=new QVBoxLayout(dialog);box->setContentsMargins(24,24,24,24);box->setSpacing(preferences.contentSpacing);
    auto *hint=new QLabel(Ui::text("Anmeldung wird vorbereitet …"));hint->setWordWrap(true);box->addWidget(hint);
    auto *code=new QLabel;code->setTextFormat(Qt::PlainText);code->setTextInteractionFlags(Qt::TextSelectableByMouse);code->setAlignment(Qt::AlignCenter);code->setStyleSheet("font-size: 30px; font-weight: 600; padding: 12px;");box->addWidget(code);
    auto *browser=new QPushButton(Ui::text("Microsoft im Browser öffnen"));browser->setEnabled(false);box->addWidget(browser);
    auto *copy=new QPushButton(Ui::text("Code kopieren"));copy->setEnabled(false);box->addWidget(copy);
    auto *cancel=new QPushButton(Ui::text("Abbrechen"));box->addWidget(cancel);connect(cancel,&QPushButton::clicked,dialog,&QDialog::reject);
    connect(auth,&AuthClient::deviceReady,dialog,[hint,code,browser,copy](const QUrl &url,const QString &value){hint->setText(Ui::text("Microsoft im Browser öffnen und diesen Code eingeben. Danach hier warten."));code->setText(value);browser->setEnabled(true);copy->setEnabled(true);QObject::connect(browser,&QPushButton::clicked,browser,[url]{QDesktopServices::openUrl(url);});QObject::connect(copy,&QPushButton::clicked,copy,[value]{qApp->clipboard()->setText(value);});});
    connect(auth,&AuthClient::authenticated,dialog,&QDialog::accept);
    connect(auth,&AuthClient::failed,dialog,[dialog](const QString &){dialog->reject();});
    connect(dialog,&QDialog::rejected,this,[this]{if(auth->busy())auth->cancel();});
    dialog->open();status("Anmeldung läuft …");auth->begin(preferences.microsoftClientId);
}

void MainWindow::rebuildLibrary() {
    if(!libraryList)return;QSignalBlocker block(libraryList);libraryList->clear();
    QVector<int> visible;const auto query=versionSearch->text().trimmed();const auto loader=loaderFilter->currentData().toString();
    for(int i=0;i<entries.size();++i){const auto &entry=entries[i];if((preferences.readyOnly&&!entry.ready()) || (!query.isEmpty()&&!entry.id.contains(query,Qt::CaseInsensitive)) || (!loader.isEmpty()&&entry.loader!=loader))continue;visible.append(i);}
    if(preferences.favoritesFirst)std::stable_sort(visible.begin(),visible.end(),[this](int a,int b){return preferences.favorites.contains(entries[a].id)>preferences.favorites.contains(entries[b].id);});
    for(int index:visible){const auto &entry=entries[index];auto *item=new QListWidgetItem((preferences.favorites.contains(entry.id)?"★  ":"")+entry.id+"\n"+entry.loader+"  ·  "+Ui::text(entry.ready()?"Startbereit":"Unvollständig"),libraryList);item->setData(Qt::UserRole,entry.id);item->setToolTip(entry.error.isEmpty()?entry.id:Ui::text(entry.error));item->setSizeHint(QSize(0,82));if(versions->currentData().toString()==entry.id)libraryList->setCurrentItem(item);}
    int ready=0;for(const auto &entry:entries)if(entry.ready())++ready;
    countLabel->setText(Ui::text("%1 angezeigt  ·  %2 startbereit  ·  %3 installiert").arg(visible.size()).arg(ready).arg(entries.size()));
    diagnostics->setText(Ui::text("%1 Profile gefunden. %2 startbereit. %3 benötigen eine Reparatur oder passende Java-Runtime.").arg(entries.size()).arg(ready).arg(entries.size()-ready));
}
