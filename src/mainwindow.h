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

    CanvasWidget *m_canvas = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    Hotkey *m_hotkey = nullptr;
    Hotkey *m_hotkeyAdd = nullptr;
    bool m_allowAutoHide = true;
    int m_scrollY = 0;
    int m_maxScroll = 0;
    int m_scrollX = 0;
    int m_maxScrollX = 0;
};
