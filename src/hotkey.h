#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QString>

class QWidget;

class Hotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit Hotkey(QObject *parent = nullptr);
    ~Hotkey() override;

    bool registerFromString(const QString &spec);
    void unregisterCurrent();
    QString current() const { return m_spec; }

signals:
    void activated();
    void registrationFailed(const QString &spec);

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    QWidget *m_host = nullptr;
    int m_id = 0xB00D;
    QString m_spec;
};
