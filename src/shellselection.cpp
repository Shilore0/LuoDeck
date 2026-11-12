#include "shellselection.h"

#include <QDir>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <objbase.h>
#  include <oleauto.h>
#  include <shellapi.h>
#  include <shlobj.h>
#  include <servprov.h>
#  include <exdisp.h>
#endif

namespace {

#ifdef Q_OS_WIN

// 与 winapps.cpp 里的同名类一致：按需初始化 COM，退出时只清理自己那一次。
// 这里不能依赖 Qt 平台层"恰好"已经初始化过 OLE——热键触发路径上没人保证这一点。
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

QStringList pathsFromDataObject(IDataObject *data)
{
    QStringList out;
    if (!data)
        return out;

    FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stg;
    ZeroMemory(&stg, sizeof(stg));

    if (FAILED(data->GetData(&fmt, &stg)))
        return out;

    if (auto drop = static_cast<HDROP>(GlobalLock(stg.hGlobal))) {
        const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i) {
            wchar_t buffer[MAX_PATH * 2] = {0};
            if (DragQueryFileW(drop, i, buffer, MAX_PATH * 2) > 0)
                out.append(QDir::fromNativeSeparators(QString::fromWCharArray(buffer)));
        }
        GlobalUnlock(stg.hGlobal);
    }
    ReleaseStgMedium(&stg);
    return out;
}
#endif

} // namespace

QStringList ShellSelection::currentSelection()
{
    QStringList result;
#ifdef Q_OS_WIN
    ComScope com;
    if (!com.healthy())
        return result;

    IShellWindows *shellWindows = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_IShellWindows,
                                reinterpret_cast<void **>(&shellWindows))))
        return result;

    const HWND foreground = GetForegroundWindow();
    const HWND foregroundRoot = foreground ? GetAncestor(foreground, GA_ROOT) : nullptr;

    long count = 0;
    if (FAILED(shellWindows->get_Count(&count)) || count <= 0) {
        shellWindows->Release();
        return result;
    }

    for (long i = 0; i < count && result.isEmpty(); ++i) {
        VARIANT v;
        VariantInit(&v);
        v.vt = VT_I4;
        v.lVal = i;

        IDispatch *dispatch = nullptr;
        if (FAILED(shellWindows->Item(v, &dispatch)) || !dispatch)
            continue;

        IServiceProvider *provider = nullptr;
        if (SUCCEEDED(dispatch->QueryInterface(IID_IServiceProvider,
                                               reinterpret_cast<void **>(&provider)))
            && provider) {
            IShellBrowser *browser = nullptr;
            if (SUCCEEDED(provider->QueryService(SID_STopLevelBrowser, IID_IShellBrowser,
                                                 reinterpret_cast<void **>(&browser)))
                && browser) {
                HWND hwnd = nullptr;
                browser->GetWindow(&hwnd);

                if (hwnd && (hwnd == foreground || hwnd == foregroundRoot)) {
                    IShellView *view = nullptr;
                    if (SUCCEEDED(browser->QueryActiveShellView(&view)) && view) {
                        IDataObject *data = nullptr;
                        if (SUCCEEDED(view->GetItemObject(SVGIO_SELECTION, IID_IDataObject,
                                                          reinterpret_cast<void **>(&data)))
                            && data) {
                            result = pathsFromDataObject(data);
                            data->Release();
                        }
                        view->Release();
                    }
                }
                browser->Release();
            }
            provider->Release();
        }
        dispatch->Release();
    }

    shellWindows->Release();
#endif
    return result;
}
