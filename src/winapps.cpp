#include "winapps.h"

#include <QImage>

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace {

// 每次调用都自己保证 COM 已经初始化过；返回时只清理自己初始化出来的那一次
class ComScope
{
public:
    ComScope()
    {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        m_healthy = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
        m_uninit = SUCCEEDED(hr) && hr != S_FALSE;
    }
    ~ComScope()
    {
        if (m_uninit)
            CoUninitialize();
    }
    bool healthy() const { return m_healthy; }

private:
    bool m_healthy = false;
    bool m_uninit = false;
};

LPCWSTR wch(const QString &s)
{
    return reinterpret_cast<LPCWSTR>(s.utf16());
}

// IShellItemImageFactory 给的是 HBITMAP，不能直接 QImage::fromHBITMAP：
// 一是它会把行序当成自下而上（图标看起来"倒了"），二是 32bpp 里的 alpha 常常整块为 0，
// 直接转就成了"黑底方块"。这里自己 GetDIBits，并用 biHeight 取负强制自上而下。
QImage imageFromHBITMAP(HBITMAP bmp)
{
    BITMAP bm{};
    if (!bmp || !GetObject(bmp, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0)
        return QImage();

    const int w = bm.bmWidth;
    const int h = bm.bmHeight;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    QImage img(w, h, QImage::Format_ARGB32);
    HDC dc = GetDC(nullptr);
    const int lines = GetDIBits(dc, bmp, 0, h, img.bits(), &bi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (lines != h)
        return QImage();

    // alpha 全 0 = 这张图没有透明度信息，退回"纯黑即透明"
    bool hasAlpha = false;
    for (int y = 0; y < h && !hasAlpha; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            if (qAlpha(line[x]) != 0) {
                hasAlpha = true;
                break;
            }
        }
    }
    if (!hasAlpha) {
        for (int y = 0; y < h; ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < w; ++x) {
                const QRgb px = line[x];
                const bool black = qRed(px) == 0 && qGreen(px) == 0 && qBlue(px) == 0;
                line[x] = black ? 0u : (0xff000000u | (px & 0x00ffffffu));
            }
        }
    }

    return img;
}

} // namespace

namespace WinApps {

bool isShellAppPath(const QString &path)
{
    return path.startsWith(QString::fromLatin1(kPrefix), Qt::CaseInsensitive);
}

QString makePath(const QString &aumid)
{
    return QString::fromLatin1(kPrefix) + aumid;
}

QString aumidFromPath(const QString &path)
{
    const QString prefix = QString::fromLatin1(kPrefix);
    if (!path.startsWith(prefix, Qt::CaseInsensitive))
        return path;
    return path.mid(prefix.size());
}

QVector<Entry> list()
{
    QVector<Entry> out;

    ComScope com;
    if (!com.healthy())
        return out;

    // shell:AppsFolder 就是"所有应用"那个虚拟目录
    IShellItem *folder = nullptr;
    if (FAILED(SHGetKnownFolderItem(FOLDERID_AppsFolder, KF_FLAG_DEFAULT, nullptr,
                                    IID_PPV_ARGS(&folder)))
        || !folder) {
        return out;
    }

    IEnumShellItems *en = nullptr;
    if (FAILED(folder->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&en))) || !en) {
        folder->Release();
        return out;
    }

    IShellItem *child = nullptr;
    while (en->Next(1, &child, nullptr) == S_OK) {
        LPWSTR parsing = nullptr;
        if (SUCCEEDED(child->GetDisplayName(SIGDN_PARENTRELATIVEPARSING, &parsing)) && parsing) {
            const QString aumid = QString::fromWCharArray(parsing);
            CoTaskMemFree(parsing);

            // UWP 的 AUMID 一定带 "!"（包名 + 应用 ID）；没有的多半是普通文件夹/快捷方式
            if (aumid.contains(QLatin1Char('!'))) {
                QString name;
                LPWSTR disp = nullptr;
                if (SUCCEEDED(child->GetDisplayName(SIGDN_NORMALDISPLAY, &disp)) && disp) {
                    name = QString::fromWCharArray(disp);
                    CoTaskMemFree(disp);
                }
                if (name.isEmpty())
                    name = aumid.section(QLatin1Char('!'), 0, 0);
                out.append(Entry{name, aumid});
            }
        }
        child->Release();
    }

    en->Release();
    folder->Release();
    return out;
}

QPixmap image(const QString &aumid, int size)
{
    QPixmap out;

    ComScope com;
    if (!com.healthy())
        return out;

    IShellItem *item = nullptr;
    if (FAILED(SHCreateItemFromParsingName(wch(makePath(aumid)), nullptr, IID_PPV_ARGS(&item)))
        || !item) {
        return out;
    }

    IShellItemImageFactory *factory = nullptr;
    if (SUCCEEDED(item->QueryInterface(IID_PPV_ARGS(&factory))) && factory) {
        SIZE want{size, size};
        HBITMAP bmp = nullptr;
        if (SUCCEEDED(factory->GetImage(want, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, &bmp))
            && bmp) {
            const QImage img = imageFromHBITMAP(bmp);
            if (!img.isNull())
                out = QPixmap::fromImage(img);
            DeleteObject(bmp);
        }
        factory->Release();
    }
    item->Release();

    return out;
}

bool activate(const QString &aumid, const QString &args)
{
    ComScope com;
    if (!com.healthy())
        return false;

    const LPCWSTR argText = args.isEmpty() ? nullptr : wch(args);

    // 正规做法：IApplicationActivationManager 按 AUMID 拉起应用（不在前台时也能正常激活）
    IApplicationActivationManager *mgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ApplicationActivationManager, nullptr, CLSCTX_LOCAL_SERVER,
                                   IID_PPV_ARGS(&mgr)))
        && mgr) {
        DWORD pid = 0;
        const HRESULT hr = mgr->ActivateApplication(wch(aumid), argText, AO_NONE, &pid);
        mgr->Release();
        if (SUCCEEDED(hr))
            return true;
    }

    // 兜底：让 shell 自己去开，效果等同于在"所有应用"里点它
    const HINSTANCE r =
        ShellExecuteW(nullptr, L"open", wch(makePath(aumid)), argText, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<qintptr>(r) > 32;
}

} // namespace WinApps
