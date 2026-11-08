#pragma once

#include <QIcon>
#include <QWidget>

class QLabel;
class QListView;
class QMenu;
class QSystemTrayIcon;
class TileDelegate;
class TileModel;
class QModelIndex;

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void installTray();
    QSystemTrayIcon *trayIcon() const { return m_tray; }
    void notify(const QString &title, const QString &body);

public slots:
    void toggleVisible();
    void showPanel();
    void hidePanel();
    void reload();
    void retranslateUi();
    void applyTheme();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;

private slots:
    void onItemActivated(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);
    void updateEmptyState();

private:
    void buildUi();
    void applyStyle();
    void positionNearCursor();
    QIcon makeTrayIcon() const;
    void rebuildTrayMenu();

    QListView *m_view = nullptr;
    QLabel *m_empty = nullptr;
    TileModel *m_model = nullptr;
    TileDelegate *m_delegate = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    bool m_allowAutoHide = true;
};
