#pragma once

#include <QFileIconProvider>
#include <QHash>
#include <QIcon>
#include <QString>

// 系统图标提取 + 缓存。内存缓存之外还有一份磁盘缓存：
// .lnk 的图标要走 shell 解析，几百个能拖慢启动好几秒，缓存后第二次起基本零成本。
// 取图标时刻意不带「快捷方式」小箭头，并裁掉四周的透明边（见 iconprovider.cpp）。
class IconProvider
{
public:
    static IconProvider &instance();

    QIcon icon(const QString &path);
    void clear();

private:
    IconProvider() = default;

    QString cachePathFor(const QString &key) const;
    void pruneDiskCache() const;

    QFileIconProvider m_provider;
    QHash<QString, QIcon> m_cache;
    // LRU：m_lru 队首是最久没用到的 key，超上限先淘汰它，
    // 否则缓存键里带 lastModified，文件一变就多一份，内存会一直涨
    QStringList m_lru;
    bool m_diskPruned = false;
};
