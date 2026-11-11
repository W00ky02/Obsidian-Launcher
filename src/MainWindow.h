#pragma once
#include "Minecraft.h"
#include "LogRedactor.h"
#include "Preferences.h"
#include "AuthClient.h"
#include <QMainWindow>
#include <QProcess>
#include <QStringConverter>
#include <memory>
class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QSpinBox;
class QTemporaryDir;
class QCheckBox;
class LogWindow;
class PathLabel;
class QListWidget;
class WorldBanner;
class DiscordPresence;
class UpdatePanel;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const AuthEndpoints &authEndpoints={});
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void rescan();
    void rebuildLibrary();
    void selectionChanged();
    void launch();
    void launchNormal();
    void openOfficialLauncher();
    void login();
    Minecraft::Credentials launchCredentials() const;
    void changeMode();
    void finish(int code,QProcess::ExitStatus status);
    void drain(bool final=false);
    void status(const QString &message,bool error=false);
    void updateEnabled();
    void stop();
    void savePreferences();
    void refreshLanguage();
    UpdatePanel *updates=nullptr;
    Preferences preferences;
    QString lastStatus;
    bool lastStatusError=false;
    QComboBox *languageChoice,*themeChoice,*accentChoice,*fontChoice,*designChoice,*launchMode;
    QSpinBox *textSizeChoice;
    QWidget *credentialPanel,*normalPanel,*offlinePanel;
    QLineEdit *offlineName;
    QPushButton *favoriteButton;
    QCheckBox *autoLogs;
    QPushButton *resetJavaButton;
    QString root, javaOverride;
    QVector<Minecraft::Entry> entries;
    QComboBox *versions;
    QLineEdit *username,*uuid,*token,*clientIdEdit;
    QPushButton *signInButton,*signOutButton,*tokenSignIn;
    QLabel *sessionSummary;
    QLabel *accountState;
    PathLabel *javaPathLabel;
    AuthClient *auth=nullptr;
    DiscordPresence *discord=nullptr;
    QLabel *discordState=nullptr;
    QSpinBox *memory,*resolutionWidth,*resolutionHeight,*logLimit;
    QLineEdit *versionSearch;
    QComboBox *loaderFilter;
    QListWidget *libraryList;
    QLabel *diagnostics;
    WorldBanner *hero;
    QLabel *statusText,*versionInfo,*pathLabel,*countLabel;
    QPushButton *launchButton,*refreshButton,*javaButton,*stopButton;
    LogWindow *logs=nullptr;
    QProcess process;
    LogRedactor redactor;
    QStringDecoder outDecoder{QStringDecoder::Utf8}, errDecoder{QStringDecoder::Utf8};
    std::shared_ptr<QTemporaryDir> natives;
    bool scanning=false, preparing=false;
    quint64 stopGeneration=0;
};
