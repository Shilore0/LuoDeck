# LuoDeck

> 一个更快的 Windows 图标启动面板。图标框内直接点击、框可拖动、容量可调、桌面图标自动导入。

LuoDeck 常驻托盘，按热键弹出自由布局的图标面板。相对 Windows 开始菜单的改进：

- **文件夹不用点两次**：把若干快捷方式圈进一个"框"，框内图标缩小显示、**直接单击就能启动**，不需要先打开框；
- **布局完全自由**：框和单个图标都能拖动，位置实时写盘；
- **容量可控**：框可设 3×3、3×4、4×3 等容量，装不下时最后一格变成「打开」按钮，点开再选；
- **不只认 exe**：`.bat` / `.cmd` / `.ps1` / `.py` / `.url` / `.lnk` / 文件夹都能加，脚本启动不闪黑框；
- **桌面自动导入**：首次运行扫描桌面并收进来。

## 使用

| 操作 | 效果 |
|---|---|
| 热键（默认 `Ctrl+Space`） | 唤出 / 收起面板 |
| 单击框内图标 | 直接启动 |
| 单击框（图标以外的空白/边框区域） | 展开该框，显示名称与全部图标 |
| 按住拖动 | 移动框或单个图标，松手即保存位置 |
| 把文件拖到框上 | 加入该框，并在 `groups/<框名>/` 生成 .lnk |
| 把文件拖到空白处 | 作为独立图标加入面板 |
| 右键框 | 打开 / 重命名 / 显示容量 / 删除 |
| 右键图标 | 打开所在位置 / 移除 |
| 右键空白 | 新建文件夹框 / 刷新布局 |
| 托盘图标右键 | 显示面板 / **设置**（主题、语言、热键）/ 重新加载 / 打开配置目录 / 退出 |

框在面板上默认不显示名称，只有展开时才在弹层里显示。新建的框按「文件夹1、文件夹2…」自动命名，可随时重命名。

## 数据位置

全部数据在 **`%USERPROFILE%\Documents\LuoDeck`**：

```
Documents\LuoDeck\
├── config.json           UI 布局（框与图标的位置、容量、名称）
├── groups\
│   └── 文件夹1\           每个框一个目录，里面是该框的快捷方式副本
│       ├── 微信.lnk
│       └── ...
├── themes\               主题（改完重启或改配置即生效）
│   ├── default.json
│   └── dark.json
└── i18n\                 多语言
    ├── zh_CN.json
    └── en_US.json
```

`config.json` 结构（v2）：

```json
{
  "version": 2,
  "language": "zh_CN",
  "theme": "default",
  "hotkey": "ctrl+space",
  "iconSize": 48,
  "canvas": { "width": 720, "height": 520 },
  "items": [
    {
      "id": "uuid",
      "type": "group",
      "name": "文件夹1",
      "x": 20, "y": 20,
      "cols": 3, "rows": 3,
      "children": [
        { "id": "uuid", "type": "app", "name": "微信", "path": "C:/...", "x": 0, "y": 0 }
      ]
    },
    { "id": "uuid", "type": "app", "name": "终端", "path": "C:/...", "x": 260, "y": 24 }
  ]
}
```

- 顶层 `items` 里的 `x` / `y` 是画布坐标（像素），拖动后自动写回；
- 框内 `children` 的坐标不参与布局（按 `cols` / `rows` 自动排列），保留字段是为了将来支持框内手动排序；
- 容量 = `cols × rows`。子项数超过容量时，前 `容量-1` 个正常显示，最后一格显示「打开」。

## 自定义主题

`themes/` 下新建任意 `.json`，在托盘「设置」里选它即可。

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

颜色支持 `#RRGGBB` 或 `#AARRGGBB`（带透明度）。`window` 是面板底色，`surface` 是框的底色，`accent` 同时用于展开按钮与托盘图标。

## 添加语言

复制 `i18n/zh_CN.json` 改名翻译，再在「设置 → 语言」里选它。缺键时界面会显示键名本身，便于发现漏翻。

## 构建

依赖：Qt 6.x（msvc2022_64）、Visual Studio Build Tools 2022（MSVC）、Windows 10/11。

双击 `build.bat`（或命令行执行），产物在 `bin\LuoDeck.exe`。若 Qt 装在别处，改 `build.bat` 顶部的 `QTBIN`。

首次编译后部署 Qt 运行时：

```bat
C:\Qt\6.11.2\msvc2022_64\bin\windeployqt.exe --release --no-translations --no-system-d3d-compiler --no-opengl-sw bin\LuoDeck.exe
```

> `dxcompiler.dll` + `dxil.dll` 共约 26MB，纯 Widgets 项目用不到，可以删。

重新编译前需要先退出正在运行的程序，否则链接会报 `LNK1104`。

## 目录结构

```
LuoDeck.pro              qmake 工程
build.bat                一键构建
src/
  main.cpp               入口与单实例
  mainwindow.*           面板容器、托盘、设置入口、自动隐藏
  canvaswidget.*         自由布局画布：拖放、右键菜单、位置持久化
  itemwidget.*           可拖动图标基类（短按=激活，拖动=移动）
  appitemwidget.*        单图标绘制
  groupitemwidget.*      框：网格排布、容量与溢出
  grouppopup.*           展开的框（显示名称 + 全部图标）
  icontile.*             框内小图标
  settingsdialog.*       设置：主题 / 语言 / 热键
  appconfig.*            配置读写、分组目录与快捷方式镜像
  theme.*                主题 → QSS
  i18n.*                 多语言
  iconprovider.*         系统图标提取与缓存
  launcher.*             启动分发
  hotkey.*               RegisterHotKey 全局热键
  metrics.h              尺寸常量
```

## 路线图

- [ ] 图标磁盘缓存（跨重启复用）
- [ ] 框内手动排序 / 拖出框外
- [ ] 多面板分页
- [ ] 搜索过滤
- [ ] 开机自启开关
- [ ] 框重叠时的层级控制

## 许可

MIT
