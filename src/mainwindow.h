#pragma once

#include <QIcon>
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

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;

private:
    void rebuildTrayMenu();
    void updateWindowSize();

    CanvasWidget *m_canvas = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    Hotkey *m_hotkey = nullptr;
    bool m_allowAutoHide = true;
};
