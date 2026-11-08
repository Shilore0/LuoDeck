#include "hotkey.h"

#include <QApplication>
#include <QHash>
#include <QWidget>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace {

#ifdef Q_OS_WIN
quint32 parseModifiers(const QStringList &parts)
{
    quint32 mods = 0;
    for (const QString &raw : parts) {
        const QString p = raw.trimmed().toLower();
        if (p == "ctrl" || p == "control")
            mods |= MOD_CONTROL;
        else if (p == "alt")
            mods |= MOD_ALT;
        else if (p == "shift")
            mods |= MOD_SHIFT;
        else if (p == "win" || p == "meta" || p == "super")
            mods |= MOD_WIN;
    }
    return mods;
}

quint32 parseKey(const QString &raw)
{
    const QString k = raw.trimmed().toLower();
    if (k.isEmpty())
        return 0;

    static const QHash<QString, quint32> named = {
        {"space", VK_SPACE},
        {"tab", VK_TAB},
        {"escape", VK_ESCAPE},
        {"esc", VK_ESCAPE},
        {"enter", VK_RETURN},
        {"return", VK_RETURN},
        {"backspace", VK_BACK},
        {"insert", VK_INSERT},
        {"delete", VK_DELETE},
        {"home", VK_HOME},
        {"end", VK_END},
        {"pageup", VK_PRIOR},
        {"pagedown", VK_NEXT},
        {"up", VK_UP},
        {"down", VK_DOWN},
        {"left", VK_LEFT},
        {"right", VK_RIGHT},
        {"`", VK_OEM_3},
        {"-", VK_OEM_MINUS},
        {"=", VK_OEM_PLUS},
        {",", VK_OEM_COMMA},
        {".", VK_OEM_PERIOD},
        {"/", VK_OEM_2},
        {";", VK_OEM_1},
        {"'", VK_OEM_7},
        {"[", VK_OEM_4},
        {"]", VK_OEM_6},
        {"\\", VK_OEM_5},
    };

    auto it = named.constFind(k);
    if (it != named.constEnd())
        return it.value();

    if (k.size() == 1) {
        const QChar c = k.at(0).toUpper();
        if (c >= 'A' && c <= 'Z')
            return static_cast<quint32>(c.unicode());
        if (c >= '0' && c <= '9')
            return static_cast<quint32>(c.unicode());
    }

    if (k.startsWith('f')) {
        bool ok = false;
        const int n = k.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 24)
            return VK_F1 + static_cast<quint32>(n - 1);
    }

    return 0;
}
#endif

} // namespace

Hotkey::Hotkey(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    // 隐藏宿主窗口：RegisterHotKey 需要一个真实 HWND，隐藏的窗口同样能收到 WM_HOTKEY
    m_host = new QWidget();
    m_host->setWindowTitle(QStringLiteral("LuoDeckHotkeyHost"));
    m_host->resize(1, 1);
    m_host->winId();
#endif
    if (qApp)
        qApp->installNativeEventFilter(this);
}

Hotkey::~Hotkey()
{
    unregisterCurrent();
    if (qApp)
        qApp->removeNativeEventFilter(this);
}

void Hotkey::unregisterCurrent()
{
#ifdef Q_OS_WIN
    if (!m_spec.isEmpty()) {
        UnregisterHotKey(reinterpret_cast<HWND>(m_host->winId()), m_id);
        m_spec.clear();
    }
#endif
}

bool Hotkey::registerFromString(const QString &spec)
{
#ifdef Q_OS_WIN
    unregisterCurrent();

    const QStringList parts = spec.split('+', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return false;

    const quint32 vk = parseKey(parts.last());
    if (vk == 0) {
        emit registrationFailed(spec);
        return false;
    }

    quint32 mods = 0;
    if (parts.size() > 1)
        mods = parseModifiers(parts.mid(0, parts.size() - 1));

    const auto hwnd = reinterpret_cast<HWND>(m_host->winId());
    if (!RegisterHotKey(hwnd, m_id, mods | MOD_NOREPEAT, vk)) {
        emit registrationFailed(spec);
        return false;
    }

    m_spec = spec;
    return true;
#else
    Q_UNUSED(spec)
    emit registrationFailed(spec);
    return false;
#endif
}

bool Hotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result)
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
        return false;

    auto *msg = static_cast<MSG *>(message);
    if (msg->message == WM_HOTKEY && static_cast<int>(msg->wParam) == m_id) {
        emit activated();
        return true;
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
#endif
    return false;
}
