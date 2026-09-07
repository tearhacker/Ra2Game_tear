# Allkk Games Injector

通用的 Win32 (x86) 游戏 DLL 注入器，ImGui 界面，静态链接无依赖。

## 支持的游戏（预设）

| 游戏 | 进程名 |
| --- | --- |
| 红警2 尤里复仇 | gamemd.exe |
| 红警2 | ra2.exe |
| 魔兽争霸3 | war3.exe |
| CS 1.6 | hl.exe |

进程名不一致时，可在界面 "Running process" 下拉中选择实际运行中的进程。

## 注入原理

标准 `LoadLibraryW + CreateRemoteThread`：

1. 在本进程内校验 DLL 为 Win32 (x86) PE
2. 等待目标进程出现（最长 30 秒）
3. `VirtualAllocEx` + `WriteProcessMemory` 写入 DLL 绝对路径
4. `CreateRemoteThread` 调用 `kernel32!LoadLibraryW` 加载

## 使用

1. 运行 `build_injector.bat` 编译（VS2026 x86 环境 + Hikari ollvm 混淆，输出 `bin\potatoInjector.exe`；自研代码全量混淆，imgui 用普通 cl 编译）
2. 把要注入的 x86 DLL 放入 `dlls` 目录（首次运行自动创建）
3. 启动游戏 → 启动注入器 → 选择 DLL → 点击 Inject

## 注意

- 注入器与游戏必须同为 x86；游戏以管理员运行时，注入器也需以管理员运行
- 被注入的 DLL 建议静态链接运行库（/MT），不依赖目标机器的 VC 运行库
