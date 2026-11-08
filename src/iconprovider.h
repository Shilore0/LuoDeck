#pragma once

#include <QFileIconProvider>
#include <QHash>
#include <QIcon>
#include <QString>

class IconProvider
{
public:
    static IconProvider &instance();

    QIcon icon(const QString &path);
    void clear();

private:
    IconProvider() = default;
    QFileIconProvider m_provider;
    QHash<QString, QIcon> m_cache;
};
