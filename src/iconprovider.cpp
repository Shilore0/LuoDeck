#include "iconprovider.h"

#include "appconfig.h"
#include "winapps.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPixmap>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <commctrl.h>
#  include <commoncontrols.h>
#  include <objbase.h>
#  include <shellapi.h>
#endif

namespace {

// 缓存按 128px 存，往下缩放都够清楚
constexpr int kIconDiskSize = 128;

// 内存缓存条数上限：超过就淘汰最久没用的那批
constexpr int kMaxMemEntries = 512;
// 磁盘缓存文件数上限：第一次取图标时顺手清理一遍旧文件
constexpr int kMaxDiskFiles = 4000;

#ifdef Q_OS_WIN

// 直接从 shell 取"原始"图标：不传 SHGFI_LINKOVERLAY，
// 所以快捷方式不会带上那个蓝色小箭头（资源管理器里那个标是画图标时叠上去的）。
QPixmap shellPixmap(const QString &path)
{
    const QString native = QDir::toNativeSeparators(path);
    SHFILEINFOW sfi{};
    if (!SHGetFileInfoW(reinterpret_cast<LPCWSTR>(native.utf16()), 0, &sfi, sizeof(sfi),
                        SHGFI_ICON | SHGFI_SYSICONINDEX))
        return QPixmap();

    QPixmap out;

    // 优先从系统大图列表里取（256 / 48），比 sfi.hIcon 那 32px 清楚得多
    IImageList *list = nullptr;
    if (SUCCEEDED(SHGetImageList(SHIL_JUMBO, IID_IImageList, reinterpret_cast<void **>(&list)))
        && list) {
        HICON big = nullptr;
        if (SUCCEEDED(list->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &big)) && big) {
            out = QPixmap::fromImage(QImage::fromHICON(big));
            DestroyIcon(big);
        }
        list->Release();
    }
    if (out.isNull() && sfi.hIcon)
        out = QPixmap::fromImage(QImage::fromHICON(sfi.hIcon));

    if (sfi.hIcon)
        DestroyIcon(sfi.hIcon);
    return out;
}

#endif

// 有些程序的图标四周留了一大圈透明（例如 256 的画布里只画了 60px），
// 直接丢进格子里就成了"小图标"。这里裁掉透明边、等比撑满，看起来才一致。
QPixmap trimAndFit(const QPixmap &src, int size)
{
    if (src.isNull())
        return src;

    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (img.isNull())
        return src;

    int left = img.width();
    int top = img.height();
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(line[x]) <= 16) // 忽略几乎全透明的杂边
                continue;
            if (x < left)
                left = x;
            if (x > right)
                right = x;
            if (y < top)
                top = y;
            if (y > bottom)
                bottom = y;
        }
    }
    if (right < left || bottom < top) // 整张全透明，原样返回
        return src;

    const QImage cropped = img.copy(left, top, right - left + 1, bottom - top + 1);
    const QImage scaled =
        cropped.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QImage out(size, size, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.drawImage((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);
    p.end();
    return QPixmap::fromImage(out);
}

} // namespace

IconProvider &IconProvider::instance()
{
    static IconProvider p;
    return p;
}

QString IconProvider::cachePathFor(const QString &key) const
{
    const QString dir = AppConfig::instance().dataDir() + "/cache/icons";
    const QByteArray hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1);
    return dir + "/" + QString::fromLatin1(hash.toHex()) + ".png";
}

void IconProvider::pruneDiskCache() const
{
    const QDir dir(AppConfig::instance().dataDir() + "/cache/icons");
    if (!dir.exists())
        return;

    // 按修改时间从新到旧排，只留最新的 kMaxDiskFiles 张
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.png")}, QDir::Files,
                                                  QDir::Time);
    for (int i = kMaxDiskFiles; i < files.size(); ++i)
        QFile::remove(files.at(i).absoluteFilePath());
}

QIcon IconProvider::icon(const QString &path)
{
    const QFileInfo fi(path);
    // 末尾的版本号用来作废旧缓存（换过取图标的方式/裁边规则就升一版）
    const QString key = path + "|" + QString::number(fi.lastModified().toSecsSinceEpoch())
        + "|v3";

    auto it = m_cache.constFind(key);
    if (it != m_cache.constEnd()) {
        // 命中的挪到队尾，保证 LRU 顺序是对的
        m_lru.removeOne(key);
        m_lru.append(key);
        return it.value();
    }

    QIcon ico;
    const QString cached = cachePathFor(key);

    // 先看磁盘缓存：命中就直接用，不再惊动 shell
    if (QFile::exists(cached)) {
        QPixmap pm;
        if (pm.load(cached))
            ico = QIcon(pm);
    }

    if (ico.isNull()) {
        QPixmap pm;
#ifdef Q_OS_WIN
        if (WinApps::isShellAppPath(path)) {
            // 商店应用：从 shell 的 AppsFolder 条目里取磁贴图
            pm = WinApps::image(WinApps::aumidFromPath(path), 256);
        } else if (fi.exists()) {
            const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            const bool uninit = SUCCEEDED(hr) && hr != S_FALSE;
            pm = shellPixmap(path);
            if (uninit)
                CoUninitialize();
        }
#endif
        if (pm.isNull() && fi.exists())
            pm = m_provider.icon(fi).pixmap(QSize(kIconDiskSize, kIconDiskSize));
        if (pm.isNull())
            pm = m_provider.icon(QFileIconProvider::File).pixmap(QSize(kIconDiskSize, kIconDiskSize));

        pm = trimAndFit(pm, kIconDiskSize);
        if (!pm.isNull()) {
            ico = QIcon(pm);
            QDir().mkpath(QFileInfo(cached).absolutePath());
            pm.save(cached, "PNG");
        }
    }

    m_cache.insert(key, ico);
    m_lru.append(key);
    while (m_lru.size() > kMaxMemEntries) {
        const QString oldest = m_lru.takeFirst();
        m_cache.remove(oldest);
    }

    // 磁盘缓存只清一次（图标一旦稳定就不会再变，没必要每次都扫目录）
    if (!m_diskPruned) {
        m_diskPruned = true;
        pruneDiskCache();
    }

    return ico;
}

void IconProvider::clear()
{
    m_cache.clear();
    m_lru.clear();
}
