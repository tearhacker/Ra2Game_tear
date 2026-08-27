# Ra2Overlay — 红色警戒 2：尤里的复仇 单机辅助工具

> RA2: Yuri's Revenge (YR 1.001) single-player trainer overlay — Win32/x86 Dear ImGui overlay.
> 基于 OpenGL 渲染链路的游戏内悬浮菜单，全部功能仅限**单机**使用。

[![Platform](https://img.shields.io/badge/platform-Win32%20x86-blue)](https://github.com/tearhacker/Ra2Game_tear)
[![Language](https://img.shields.io/badge/language-C%2B%2B17-orange)](https://github.com/tearhacker/Ra2Game_tear)
[![License](https://img.shields.io/badge/license-MIT-green)](https://github.com/tearhacker/Ra2Game_tear)

## 简介 (Introduction)

Ra2Overlay 是一个注入 RA2 尤里的复仇（YR 1.001）进程的 ASI 辅助模块，使用 Dear ImGui 绘制游戏内悬浮菜单，通过内存读写与代码钩子实现各类单机作弊功能。

- 注入方式：ASI 模块（`.asi`），由游戏自动加载
- 渲染方式：复用游戏当前 OpenGL 上下文，不创建第二个窗口
- 挂钩点：`GDI32!SwapBuffers`（MinHook）
- 按键：`Insert` 显示/隐藏菜单，`End` 卸载辅助

> ⚠️ **重要声明 / Disclaimer**：本项目的所有写入类功能都会导致联机不同步（desync），**仅限单机（SP）使用**。请勿在联机对战中使用。

## 功能一览 (Features)

### 顶层菜单 (Top Tabs)

| 英文 (English) | 中文 (Chinese) | 说明 (Description) |
|---|---|---|
| `Status` | 状态 | 帧率、窗口/显卡信息、卸载按钮 |
| `Log` | 日志 | 查看辅助运行日志（Ra2Overlay.log） |
| `Configuration` | 配置 | 按键操作说明 |
| `ESP` | 透视 | 实体方框 / 血条 / 名称显示（只读，联机安全） |
| `Reveal Map` | 全图视野 | 永久清除战争迷雾 |
| `Memory Features` | 内存功能 | 全部作弊功能入口（4 个子页） |
| `About` | 关于 | 作者信息、QQ 群、官网、GitHub |

### 内存功能子页 (Memory Features Sub-Tabs)

| 英文 (English) | 中文 (Chinese) | 说明 (Description) |
|---|---|---|
| `Economy` | 经济 | 金钱 / 电力 / 采矿 / 建造 / 科技类 |
| `Superweapons` | 超级武器 | 超武充能、核弹发射 |
| `Combat` | 战斗 | 火力 / 生命 / 射程 / 命中类 |
| `Strategy` | 策略 | 游戏速度 / 胜负判定 / 单位移除 |

### 全部功能 (All Features, SP = Single Player 单机)

| 英文 (English) | 中文 (Chinese) | 说明 (Description) |
|---|---|---|
| `Unlimited Money (SP)` | 无限金钱 | 每帧增加 1000 资金 |
| `Unlimited Power (SP)` | 无限电力 | 电力输出恒为 99999，永不断电 |
| `Instant Mining (SP)` | 立即采矿 | 矿石精炼厂立即产出 |
| `Instant Build (SP)` | 快速建造 | 生产瞬间完成 |
| `Build Everywhere (SP)` | 随处建造 | 任意地形（含水面）可建造 |
| `Unlock All Tech (SP)` | 解锁全部科技 | 解锁所有建筑/单位建造权限 |
| `Unlimited Superweapons (SP)` | 无限超级武器 | 所有己方超武随时可放 |
| `Launch Nuke (At Cursor)` | 鼠标处发射核弹 | 在鼠标所指位置发射核弹 |
| `Unlimited Firepower (SP)` | 无限火力 | 装填无冷却、弹药无限 |
| `Instant Turret Turn (SP)` | 炮塔瞬间转向 | 炮塔/炮管对准鼠标 |
| `Auto-Repair (SP)` | 自动维修 | 受损建筑立即修复 |
| `Infinite Health (SP)` | 无限生命 | 己方单位/建筑打不死 |
| `Max Veterancy (SP)` | 满级精英 | 所有己方单位升为精英级 |
| `Max Range (SP)` | 最大射程 | 武器射程全图 |
| `Force Fire (SP)` | 强制命中 | 攻击必定命中 |
| `Speed Up All Units (SP)` | 全部单位加速 | 载具 5x / 步兵 2x / 飞机 3x |
| `Pause Game (SP)` | 暂停游戏 | 保持暂停状态 |
| `Game Speed (SP)` | 游戏速度 | 滑块 0（最慢）~ 5（最快） |
| `I Win` | 我方获胜 | 立即判定当前玩家胜利 |
| `Make AI Lose` | 让 AI 失败 | 立即判定 AI 失败 |
| `Remove Selected Units` | 移除选中单位 | 删除选中的己方单位/建筑 |

### ESP 透视设置 (ESP Options)

| 英文 (English) | 中文 (Chinese) | 说明 (Description) |
|---|---|---|
| `Enable ESP` | 开启透视 | 绘制实体方框 |
| `Show health bar` | 显示血条 | 头顶血量条（绿→黄→红） |
| `Show name` | 显示名称 | 实体下方单位类型名 |
| `Enemies only` | 仅显示敌人 | 只标敌方 |
| `Box size` | 方框大小 | 滑块 10 ~ 60 |

> ESP 为只读绘制，不写入游戏状态，**联机安全**。

## 构建 (Build)

要求环境：

- Visual Studio 2022（MSVC 平台工具集 `v143`）
- Windows 10/11 SDK
- C++17

步骤：

1. 打开 `GL-BaseHook/Ra2Overlay.sln`
2. 选择 `Release | x86` 配置（工程无 x64 配置）
3. 直接生成，依赖（Dear ImGui 1.92.9 / MinHook 1.3.4）已内联在 `third_party/`，无需预编译库

输出产物：

```text
GL-BaseHook\build\Win32\Release\Ra2Overlay.asi
```

也可以直接运行 `GL-BaseHook/build_ra2.bat` 一键构建。

## 使用 (Usage)

1. 将 `Ra2Overlay.asi` 放入 RA2 游戏目录（ASI 加载器会自动加载）
2. 进入游戏（单机）
3. 按 `Insert` 打开/关闭菜单，按 `End` 卸载辅助
4. 在 `Memory Features` 下按需勾选功能（带 `(SP)` 的功能仅限单机）

## 目录结构 (Repository Layout)

```text
Ra2Game_tear/
├─ GL-BaseHook/            主工程（源码）
│  ├─ src/                 功能模块源码（ESP / 内存功能 / UI 壳）
│  ├─ third_party/         Dear ImGui、MinHook 内联依赖
│  ├─ Ra2Overlay.sln       VS2022 工程
│  └─ build_ra2.bat        一键构建脚本
└─ README.md               本文件
```

## 联系 (Contact)

| 项目 | 信息 |
|---|---|
| Author | tearhacker |
| QQ Group | 435539500 |
| Website | http://teargamestorem.top/ |
| GitHub | https://github.com/tearhacker/Ra2Game_tear |

## 免责声明 (Disclaimer)

本工具仅用于学习与研究目的（游戏逆向、内存操作、图形覆盖层技术）。使用本工具产生的任何后果由使用者自行承担。请勿在联机对战中使用本工具的任何写入类功能。
