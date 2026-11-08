#include "iconprovider.h"

#include <QFileInfo>

IconProvider &IconProvider::instance()
{
    static IconProvider p;
    return p;
}

QIcon IconProvider::icon(const QString &path)
{
    const QString native = path;
    const QFileInfo fi(native);
    const QString key = native + "|" + QString::number(fi.lastModified().toSecsSinceEpoch());

    auto it = m_cache.constFind(key);
    if (it != m_cache.constEnd())
        return it.value();

    QIcon ico;
    if (fi.exists())
        ico = m_provider.icon(fi);
    if (ico.isNull())
        ico = m_provider.icon(QFileIconProvider::File);

    m_cache.insert(key, ico);
    return ico;
}

void IconProvider::clear()
{
    m_cache.clear();
}
