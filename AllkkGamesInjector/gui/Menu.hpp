#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

class Menu
{
	friend Injector;
public:
	Menu() = default;
	~Menu() = default;

	bool initialize();

	void loop();

	// 注入是否仍在进行（主线程退出前需等待其结束）
	bool isBusy() const { return isInjecting.load(std::memory_order_acquire); }

private:
	bool createD3D9Device(HWND hWnd);

	void cleanupD3D9Device();

	static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void setupMenuStyle(bool isDarkTheme, float alpha);

	// 按当前语言取文案：zh 为中文，en 为英文
	const char* langText(const char* en, const char* zh) const;

	void renderStatusPanel();

	void renderTargetPanel();

	std::vector<std::wstring> snapshotDllPaths();

	std::vector<std::wstring> snapshotProcessNames();

	void renderInjectionPanel(const std::vector<std::wstring>& paths);

	void renderLogPanel();

	void detectGame();

	void updateFiles();

private:
	LPDIRECT3D9              pD3D = NULL;
	LPDIRECT3DDEVICE9        d3dDevice = NULL;
	D3DPRESENT_PARAMETERS    d3dpp = {};
	HWND					 hwnd{ NULL };

	std::atomic_bool isMenuOn{ false };
	std::atomic_bool isInjecting{ false };
	bool isDarkTheme{ true };
	bool isChineseLang{ false };  // initialize() 中按系统语言初始化

	std::vector<std::wstring> filePaths;
	std::vector<std::wstring> processNames;
	int selectedGame{ 0 };
	int selectedProcess{ 0 };
	int selectedDLL{ 0 };

	std::mutex mtx;

};

inline auto g_menu = std::make_unique<Menu>();
