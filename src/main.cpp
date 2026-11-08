#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSystemTrayIcon>

#include "appconfig.h"
#include "hotkey.h"
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

    // 已有实例在跑：唤起它的面板后立即退出
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
    window.showPanel();

    Hotkey hotkey;
    QObject::connect(&hotkey, &Hotkey::activated, &window, &MainWindow::toggleVisible);

    QString active = cfg.hotkey();
    bool ok = hotkey.registerFromString(active);

    if (!ok && active.compare(QStringLiteral("ctrl+space"), Qt::CaseInsensitive) == 0) {
        // Ctrl+Space 在中文 Windows 上常被输入法抢占，自动降级一次
        const QString fallback = QStringLiteral("ctrl+alt+space");
        if (hotkey.registerFromString(fallback)) {
            ok = true;
            window.notify(QStringLiteral("LuoDeck"),
                          QStringLiteral("Ctrl+Space 已被其他程序占用，已临时改用 Ctrl+Alt+Space。\n"
                                         "如需修改请编辑 luodeck.json 的 hotkey 字段。"));
        }
    }

    if (!ok) {
        window.notify(QStringLiteral("LuoDeck"),
                      I18n::instance().t(QStringLiteral("tray.hotkey_failed"), cfg.hotkey()));
    }

    return app.exec();
}
