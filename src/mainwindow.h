#pragma once

#include <QIcon>
#include <QRect>
#include <QWidget>

class CanvasWidget;
class Hotkey;
class QMenu;
class QSystemTrayIcon;

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void installTray();
    // 只在第一次运行时问一次：要不要把开始菜单的固定项复制过来
    void askFirstRunImport();
    void setupHotkey();
    void notify(const QString &title, const QString &body);

    // 打开任何对话框 / 系统文件选择框之前调 holdPanel()，关掉之后调 releasePanel()。
    // 期间面板不响应"失去激活就收起"，关完还会把被系统藏起来的面板拉回来。
    void holdPanel();
    void releasePanel();

public slots:
    void toggleVisible();
    void showPanel();
    void hidePanel();
    void reload();
    void retranslateUi();
    void applyTheme();
    void openSettings();
    void reregisterHotkey();
    void addCurrentSelection();
    // F5：把所有框和图标重新按格子排一遍（等于右键菜单里的「整理布局」）
    void arrangeLayout();
    void revealCanvasRect(const QRect &rect);
    void scrollBy(int delta);
    void scrollXBy(int delta);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;

private:
    void rebuildTrayMenu();
    void updateWindowSize();
    void applyScroll();
    void scrollTo(int y);
    void scrollXTo(int x);
    // 延迟放开自动收起：对话框刚关掉时系统还会补一次"失焦"，立刻放开会误收面板
    void enableAutoHideSoon(int delayMs);

    CanvasWidget *m_canvas = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    Hotkey *m_hotkey = nullptr;
    Hotkey *m_hotkeyAdd = nullptr;
    bool m_allowAutoHide = true;
    // 对话框嵌套计数：>0 时面板钉住不收（设置、文件选择框都可能叠着开）
    int m_panelHold = 0;
    // 进入 hold 之前面板是否可见，关对话框后要按这个恢复
    bool m_panelWasVisible = true;
    int m_scrollY = 0;
    int m_maxScroll = 0;
    int m_scrollX = 0;
    int m_maxScrollX = 0;
};
