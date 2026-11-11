#include "MainWindow.h"
#include <QApplication>
#include "Theme.h"
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    app.setApplicationName("ObsidianLauncher"); app.setApplicationVersion("1.0.0"); app.setOrganizationName("Obsidian");
    applyTheme(app);
    MainWindow window; window.show(); return app.exec();
}
