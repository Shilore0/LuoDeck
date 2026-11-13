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

// 某个浏览器标签页里当前选中的东西（没选中就是空表）；
// viewWnd 带出这个标签页的视图窗口，用来判断它是"当前激活的那个标签页"
QStringList selectionOf(IShellBrowser *browser, HWND *viewWnd = nullptr)
{
    if (!browser)
        return {};

    QStringList out;
    IShellView *view = nullptr;
    if (FAILED(browser->QueryActiveShellView(&view)) || !view)
        return out;

    if (viewWnd)
        view->GetWindow(viewWnd);

    IDataObject *data = nullptr;
    if (SUCCEEDED(view->GetItemObject(SVGIO_SELECTION, IID_IDataObject,
                                      reinterpret_cast<void **>(&data)))
        && data) {
        out = pathsFromDataObject(data);
        data->Release();
    }
    view->Release();
    return out;
}

// 两个窗口是不是"同一个资源管理器窗口"。Win11 22H2 之后资源管理器带标签页：
// IShellBrowser::GetWindow() 给的是标签页子窗口（ShellTabWindowClass），
// 前台拿到的却是外层框架窗口，直接比 HWND 永远不相等——两边都取 GA_ROOT 再比。
bool sameExplorerWindow(HWND a, HWND b)
{
    if (!a || !b)
        return false;
    if (a == b)
        return true;
    const HWND rootA = GetAncestor(a, GA_ROOT);
    const HWND rootB = GetAncestor(b, GA_ROOT);
    if (rootA && rootB && rootA == rootB)
        return true;
    // 个别版本标签页和框架挂在不同顶层上，再加一层父链包含关系兜底
    for (HWND p = a; p; p = GetAncestor(p, GA_PARENT))
        if (p == b || p == rootB)
            return true;
    for (HWND p = b; p; p = GetAncestor(p, GA_PARENT))
        if (p == a || p == rootA)
            return true;
    return false;
}
// 前台资源管理器里真正拿着键盘焦点的窗口（多标签时用它认出激活的那个标签页）
HWND focusWindowOf(HWND root)
{
    if (!root)
        return nullptr;
    GUITHREADINFO info;
    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    if (GetGUIThreadInfo(GetWindowThreadProcessId(root, nullptr), &info))
        return info.hwndFocus ? info.hwndFocus : info.hwndActive;
    return nullptr;
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
    const HWND focusWnd = focusWindowOf(foreground);

    long count = 0;
    if (FAILED(shellWindows->get_Count(&count)) || count <= 0) {
        shellWindows->Release();
        return result;
    }

    // 三档优先级：
    //   1. 前台资源管理器里拿着焦点的那个标签页（多标签时不会拿错标签页的选中项）
    //   2. 前台资源管理器窗口（任意标签页）的选中项
    //   3. 前台不是资源管理器时，退回"唯一一个有选中项的资源管理器窗口"
    bool foregroundIsExplorer = false;
    bool focusedHit = false;
    QStringList foregroundSelection;
    QStringList onlySelection;
    int selectedWindows = 0;

    for (long i = 0; i < count; ++i) {
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

                HWND viewWnd = nullptr;
                const QStringList paths = selectionOf(browser, &viewWnd);
                const bool isForeground = sameExplorerWindow(hwnd, foreground);

                if (isForeground)
                    foregroundIsExplorer = true;

                if (!paths.isEmpty()) {
                    ++selectedWindows;
                    if (onlySelection.isEmpty())
                        onlySelection = paths;

                    if (isForeground && !focusedHit && focusWnd && viewWnd
                        && (viewWnd == focusWnd || IsChild(viewWnd, focusWnd)
                            || IsChild(focusWnd, viewWnd))) {
                        result = paths;
                        focusedHit = true;
                    }
                    if (isForeground && foregroundSelection.isEmpty())
                        foregroundSelection = paths;
                }

                browser->Release();
            }
            provider->Release();
        }
        dispatch->Release();
    }

    if (!focusedHit) {
        if (!foregroundSelection.isEmpty())
            result = foregroundSelection;
        // 前台就是资源管理器但没选中东西：判定"没选中"，
        // 别去别的窗口翻出一份不相干的选中项塞进来
        else if (!foregroundIsExplorer && selectedWindows == 1)
            result = onlySelection;
    }

    shellWindows->Release();
#endif
    return result;
}
