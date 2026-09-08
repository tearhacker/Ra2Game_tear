#pragma once

// ATL 桩头文件：本机 VS 未安装 ATL 组件时兜底。
// YRpp/Interfaces.h 仅 #include <atlbase.h> 但未使用任何 ATL 符号
// （_COM_SMARTPTR_TYPEDEF 实际来自随后包含的 <comdef.h>），
// 因此空实现即可满足编译。
