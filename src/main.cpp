#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>

#include "appconfig.h"
#include "i18n.h"
#include "mainwindow.h"
#include "theme.h"
#include "windowfx.h"

namespace {

const char *kInstanceKey = "LuoDeck.SingleInstance.v1";

// 已有实例在跑：把请求转给它（show 或 add|路径），然后自己退出
bool signalExistingInstance(const QString &addPath)
{
    QLocalSocket socket;
    socket.connectToServer(QString::fromLatin1(kInstanceKey));
    if (!socket.waitForConnected(300))
        return false;

    const QByteArray payload = addPath.isEmpty()
        ? QByteArrayLiteral("show")
        : QByteArrayLiteral("add|") + addPath.toUtf8();

    socket.write(payload);
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
            auto *buffer = new QByteArray();

            QObject::connect(client, &QLocalSocket::readyRead, client, [client, buffer] {
                *buffer += client->readAll();
            });

            QObject::connect(client, &QLocalSocket::disconnected, window,
                             [client, buffer, window] {
                                 const QByteArray data = *buffer + client->readAll();
                                 delete buffer;

                                 if (data.startsWith("add|")) {
                                     const QString path = QString::fromUtf8(data.mid(4)).trimmed();
                                     if (!path.isEmpty())
                                         AppConfig::instance().addApp(path);
                                 }
                                 window->showPanel();
                                 client->deleteLater();
                             });
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

    // 资源管理器右键菜单会以 --add "<路径>" 调用本程序
    QString addPath;
    const QStringList args = app.arguments();
    for (int i = 0; i < args.size(); ++i) {
        if (args.at(i).compare(QStringLiteral("--add"), Qt::CaseInsensitive) == 0
            && i + 1 < args.size()) {
            addPath = args.at(i + 1);
            break;
        }
    }

    if (signalExistingInstance(addPath))
        return 0;

    AppConfig &cfg = AppConfig::instance();
    cfg.load();

    Theme::instance().ensureDefaults();
    Theme::instance().load(cfg.theme());

    I18n::instance().ensureDefaults();
    I18n::instance().load(cfg.language());

    app.setStyleSheet(Theme::instance().styleSheet());
    app.setWindowIcon(luoDeckIcon());

    MainWindow window;
    window.installTray();
    startInstanceServer(&window);
    window.setupHotkey();

    if (!addPath.isEmpty())
        cfg.addApp(addPath);

    window.showPanel();

    return app.exec();
}
