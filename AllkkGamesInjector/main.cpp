#include "pch.h"

int main()
{
	if (!g_menu->initialize())
		return 1;

	g_menu->loop();

	// 窗口关闭后等待注入线程结束，避免退出时访问已销毁的全局对象
	while (g_menu->isBusy())
		std::this_thread::sleep_for(100ms);

	return 0;
}
