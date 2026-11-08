# LuoDeck

> 一个更快的 Windows 图标启动面板。文件夹原地展开、脚本可直接启动、桌面图标自动导入。

LuoDeck 是一个常驻托盘的启动器：按热键弹出图标网格，单击即启动。它针对 Windows 开始菜单的三处不便做了改进：

- **文件夹不用点两次**：文件夹磁贴在面板内原地展开成小图标，不弹资源管理器、不抢焦点；
- **不只认 exe**：`.bat` / `.cmd` / `.ps1` / `.py` / `.url` / `.lnk` / 文件夹 / 任意文档都能加，脚本启动时不会闪黑框；
- **桌面自动导入**：首次运行自动扫描桌面并把快捷方式收进来，之后可随时拖入新项目。

## 当前状态

已完成第一版骨架，可编译可运行：

- 托盘常驻 + 全局热键（默认 `Ctrl+Space`）
- 图标网格（虚拟化，项数多也不卡）
- 文件夹磁贴原地展开 / 收起
- 拖入文件即添加，右键移除
- 系统图标提取 + 内存缓存
- 外置主题（JSON）与外置多语言（JSON），改文件即生效，无需改代码
- 单实例保护（重复启动会唤出已有面板）
- 高 DPI 适配（Per-Monitor V2）

## 构建

依赖：Qt 6.x（msvc2022_64）、Visual Studio Build Tools 2022（含 MSVC）、Windows 10/11。

双击 `build.bat`，或命令行：

```bat
build.bat
```

脚本会载入 MSVC 环境、调用 `qmake` 生成 Makefile、`nmake` 编译，产物输出到 `bin\LuoDeck.exe`。

若 Qt 装在别处，修改 `build.bat` 顶部的 `QTBIN`。

首次运行后需要把 Qt 运行时 DLL 部署到 `bin`：

```bat
C:\Qt\6.11.2\msvc2022_64\bin\windeployqt.exe --release --no-translations --no-system-d3d-compiler --no-opengl-sw bin\LuoDeck.exe
```

## 使用

- 热键唤出面板（默认 `Ctrl+Space`）。**中文 Windows 上 `Ctrl+Space` 常被输入法占用**，若注册失败程序会自动降级为 `Ctrl+Alt+Space` 并弹托盘提示；
- 单击图标启动；单击文件夹磁贴展开；
- 右键磁贴：移除 / 打开所在位置 / 在资源管理器中打开；
- 把文件或文件夹拖进面板即添加；
- 托盘图标左键切换面板，右键有「重新加载 / 打开配置目录 / 退出」。

## 配置：`luodeck.json`

首次运行在 exe 同目录生成（若该目录不可写则放到 `%LOCALAPPDATA%\LuoDeck`）。

```json
{
  "version": 1,
  "language": "zh_CN",
  "theme": "default",
  "hotkey": "ctrl+space",
  "grid": {
    "iconSize": 48,
    "columns": 6
  },
  "items": [
    {
      "id": "uuid",
      "path": "C:/Users/you/Desktop/微信.lnk",
      "name": "微信",
      "isFolder": false
    }
  ]
}
```

| 字段 | 说明 |
|---|---|
| `language` | 对应 `i18n/<code>.json` 的文件名，如 `zh_CN`、`en_US` |
| `theme` | 对应 `themes/<name>.json` 的文件名，如 `default`、`dark` |
| `hotkey` | 形如 `ctrl+space`、`ctrl+alt+d`、`win+f1`；修饰键支持 `ctrl` / `alt` / `shift` / `win` |
| `grid.iconSize` | 图标基准像素，24–128 |
| `grid.columns` | 网格列数，3–20 |

改动后重启生效（托盘菜单「重新加载」只重读图标与项目，不重读热键）。

## 自定义主题

在 `themes/` 下新建任意 `.json`，把 `luodeck.json` 的 `theme` 改成它的文件名即可。内置 `default.json`（浅色）与 `dark.json`（深色）。

```json
{
  "name": "Midnight",
  "radius": 12,
  "spacing": 8,
  "colors": {
    "window": "#17181CF2",
    "surface": "#1E1F24",
    "surfaceHover": "#2B2D33",
    "text": "#E8EAED",
    "textMuted": "#9AA0A6",
    "accent": "#7AA2F7",
    "border": "#33363D"
  }
}
```

- 颜色支持 `#RRGGBB` 或 `#AARRGGBB`（带透明度，面板背景建议带 alpha）；
- `window` 是面板底色，`accent` 用于文件夹展开态提示与托盘图标；
- 尺寸（图标大小、列数）在 `luodeck.json` 里配，主题只管视觉。

## 添加语言

在 `i18n/` 下复制 `zh_CN.json` 改名（如 `ja_JP.json`），翻译其中的键值，然后改 `luodeck.json` 的 `language`。键名缺失时界面会直接显示键名本身，便于发现漏翻。

## 目录结构

```
LuoDeck.pro              qmake 工程
build.bat                一键构建脚本
src/
  main.cpp               入口、单实例、热键注册与降级
  mainwindow.*           面板窗口（无边框圆角、托盘、拖放、右键菜单）
  tilemodel.*            磁贴数据模型（含文件夹展开逻辑）
  tiledelegate.*         磁贴绘制（图标 + 文字 + 悬停态）
  iconprovider.*         系统图标提取与缓存
  launcher.*             启动分发（exe/lnk/url/文件夹/脚本）
  appconfig.*            配置读写、桌面导入
  theme.*                主题加载与 QSS 生成
  i18n.*                 多语言加载
  hotkey.*               RegisterHotKey 全局热键
```

## 路线图

- [ ] 设置界面（热键、主题、语言改为界面内切换）
- [ ] 图标磁盘缓存（跨重启复用，进一步降低启动耗时）
- [ ] 分页 / 分组（多屏场景）
- [ ] 搜索过滤
- [ ] 拖拽排序、跨分组移动
- [ ] 开机自启开关
- [ ] 面板位置记忆（屏幕边缘吸附）

## 许可

MIT
