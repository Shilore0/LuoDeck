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
    IShellWindows *shellWindows = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_IShellWindows,
                                reinterpret_cast<void **>(&shellWindows))))
        return result;

    const HWND foreground = GetForegroundWindow();
    const HWND foregroundRoot = foreground ? GetAncestor(foreground, GA_ROOT) : nullptr;

    long count = 0;
    shellWindows->get_Count(&count);

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
