# LuoDeck

> 一个更快的 Windows 图标启动面板。文件夹框内直接点击、框可拖动、名字与边框颜色可自定义、桌面图标自动收纳。

LuoDeck 常驻托盘，按热键弹出自由布局的图标面板。相对 Windows 开始菜单的改进：

- **文件夹不用点两次**：快捷方式圈进一个「框」，框内图标缩小显示、**直接单击就能启动**，不需要先打开框；
- **框可自定义**：名字、边框颜色都能改（右键 → 文件夹设置），新建框自动分配名字与随机配色；
- **布局完全自由**：框和单个图标都能拖动，位置实时写盘；
- **容量可控**：可设 3×3、3×4、4×3 等容量，装不下时最后一格变成「打开」按钮；
- **桌面自动收纳**：桌面图标自动进入系统创建的「桌面」框，随桌面增删同步；
- **不只认 exe**：`.bat` / `.cmd` / `.ps1` / `.py` / `.url` / `.lnk` / 文件夹都能加，脚本启动不闪黑框。

## 使用

| 操作 | 效果 |
|---|---|
| 热键（默认 `Ctrl+Space`） | 唤出 / 收起面板 |
| 单击框内图标 | 直接启动 |
| 单击框（图标以外的空白/标题区域） | 展开该框，显示全部图标 |
| 按住拖动 | 移动框或单个图标，松手即保存位置 |
| 把文件拖到框上 | 加入该框，并在 `groups/<框名>/` 生成 .lnk |
| 把文件拖到空白处 | 作为独立图标加入面板 |
| **右键框 → 文件夹设置…** | 改名字、选边框颜色（系统「桌面」框这两项为灰色不可改） |
| 右键框 → 显示容量 | 2×2 / 3×3 / 3×4 / 4×3 / 4×4 |
| 右键图标 | 打开所在位置 / 移除 |
| 右键空白 | 新建文件夹 / 刷新布局 |
| 托盘图标右键 | 显示面板 / **设置** / 重新加载 / 打开配置目录 / 退出 |

## 文件夹与配色

- 框在面板上**顶部显示名字**，展开弹层里也显示；
- 新建文件夹按「文件夹 1、文件夹 2…」自动递增命名，边框颜色从内置 7 色中**随机**取一个；
- 内置配色：黄 `#F2B33D`、蓝 `#4A90D9`、绿 `#4CAF7D`、粉 `#E86A92`、紫 `#9B7EDE`、橙 `#F08A4B`、青 `#48B8C4`；
- **系统「桌面」框**：程序启动时自动创建，默认黄色边框，收纳桌面上所有快捷方式/文件夹，并随桌面增删同步。它**不能改名、不能改色、不能删除**，对应的设置项显示为灰色；
- 不想看到它：托盘 → 设置 → 勾选「隐藏「桌面」文件夹」。

## 数据位置

全部数据在 **`%USERPROFILE%\Documents\LuoDeck`**，整个目录拷走即可迁移：

```
Documents\LuoDeck\
├── config.json            布局（框/图标坐标、名字、颜色、容量、隐藏状态）
├── groups\
│   ├── 桌面\              系统框的快捷方式副本
│   │   └── 微信.lnk
│   └── 文件夹 1\
│       └── 终端.lnk
├── themes\                主题
│   ├── default.json
│   └── dark.json
└── i18n\                  多语言
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
  "hideDesktopFrame": false,
  "canvas": { "width": 720, "height": 520 },
  "items": [
    {
      "id": "uuid",
      "type": "group",
      "name": "桌面",
      "system": true,
      "color": "#F2B33D",
      "x": 20, "y": 20,
      "cols": 4, "rows": 4,
      "children": [
        { "id": "uuid", "type": "app", "name": "微信", "path": "C:/...", "x": 0, "y": 0 }
      ]
    },
    {
      "id": "uuid",
      "type": "group",
      "name": "文件夹 1",
      "color": "#4A90D9",
      "x": 285, "y": 20,
      "cols": 3, "rows": 3,
      "children": []
    }
  ]
}
```

- `system: true` 表示系统创建，界面上名字与颜色只读、禁止删除；
- `color` 是边框颜色（`#RRGGBB`）；框底色由该颜色按 10% 透明度自动生成；
- 顶层 `x` / `y` 是画布坐标（像素），拖动后自动写回；
- 容量 = `cols × rows`；子项超过容量时前 `容量-1` 个正常显示，最后一格变「打开」。

## 自定义主题

`themes/` 下新建任意 `.json`，在托盘「设置」里选它。

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

`accent` 只用于托盘图标与未配色的框；每个框的具体颜色由 `config.json` 的 `color` 决定。

## 添加语言

复制 `i18n/zh_CN.json` 改名翻译，再在「设置 → 语言」里选它。系统框的名字走翻译键 `group.desktop`，所以切语言会跟着变。

## 构建

依赖：Qt 6.x（msvc2022_64）、Visual Studio Build Tools 2022（MSVC）、Windows 10/11。

双击 `build.bat`，产物在 `bin\LuoDeck.exe`。Qt 装在别处就改 `build.bat` 顶部的 `QTBIN`。

首次编译后部署 Qt 运行时：

```bat
C:\Qt\6.11.2\msvc2022_64\bin\windeployqt.exe --release --no-translations --no-system-d3d-compiler --no-opengl-sw bin\LuoDeck.exe
```

> `dxcompiler.dll` + `dxil.dll` 约 26MB，纯 Widgets 项目用不到，可删。
> 重新编译前先退出运行中的程序，否则链接会报 `LNK1104`。

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
  groupitemwidget.*      框：标题、边框配色、网格、容量与溢出
  grouppopup.*           展开的框（显示名称 + 全部图标）
  foldersettingsdialog.* 文件夹设置：改名 + 选颜色（系统框只读）
  icontile.*             框内小图标
  settingsdialog.*       设置：主题 / 语言 / 热键 / 隐藏桌面框
  appconfig.*            配置读写、桌面同步、分组目录与快捷方式镜像
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
