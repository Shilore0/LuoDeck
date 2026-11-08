#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>

#include "appconfig.h"
#include "i18n.h"
#include "mainwindow.h"
#include "theme.h"

namespace {

const char *kInstanceKey = "LuoDeck.SingleInstance.v1";

bool signalExistingInstance()
{
    QLocalSocket socket;
    socket.connectToServer(QString::fromLatin1(kInstanceKey));
    if (!socket.waitForConnected(300))
        return false;

    socket.write("show");
    socket.flush();
    socket.waitForBytesWritten(300);
    socket.disconnectFromServer();
    return true;
}

void startInstanceServer(MainWindow *window)
{
    auto *server = new QLocalServer(window);
    QLocalServer::removeServer(QString::fromLatin1(kInstanceKey));
    if (!server->listen(QString::fromLatin1(kInstanceKey)))
        return;

    QObject::connect(server, &QLocalServer::newConnection, window, [server, window] {
        while (QLocalSocket *client = server->nextPendingConnection()) {
            client->deleteLater();
            window->showPanel();
        }
    });
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LuoDeck"));
    app.setApplicationDisplayName(QStringLiteral("LuoDeck"));
    app.setOrganizationName(QStringLiteral("LuoDeck"));
    app.setQuitOnLastWindowClosed(false);

    if (signalExistingInstance())
        return 0;

    AppConfig &cfg = AppConfig::instance();
    cfg.load();

    Theme::instance().ensureDefaults();
    Theme::instance().load(cfg.theme());

    I18n::instance().ensureDefaults();
    I18n::instance().load(cfg.language());

    app.setStyleSheet(Theme::instance().styleSheet());

    MainWindow window;
    window.installTray();
    startInstanceServer(&window);
    window.setupHotkey();
    window.showPanel();

    return app.exec();
}
