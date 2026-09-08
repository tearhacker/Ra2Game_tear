#pragma once

#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace logger
{
	enum class Level { Info, Ok, Warn, Err };

	struct Entry
	{
		std::wstring time;   // HH:MM:SS
		Level level;
		std::wstring message;
	};

	// 线程安全日志：环形缓冲供界面线程读取，同时追加写入 exe 目录的 injector_log.txt。
	// 注入线程写、界面线程读，写入频率极低，互斥锁开销可忽略。
	class Logger
	{
	public:
		Logger()
		{
			wchar_t exePath[MAX_PATH]{};
			if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0)
				m_filePath = (std::filesystem::path(exePath).parent_path() / L"injector_log.txt").wstring();
			else
				m_filePath = L"injector_log.txt";
		}

		void write(Level level, const std::wstring& message)
		{
			SYSTEMTIME st{};
			GetLocalTime(&st);
			wchar_t timeText[16]{};
			swprintf_s(timeText, L"%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);

			std::scoped_lock lock(m_mutex);
			m_entries.push_back({ timeText, level, message });
			if (m_entries.size() > kMaxEntries)
				m_entries.pop_front();
			appendToFileLocked(st, m_entries.back());
		}

		// 界面线程持锁遍历渲染；回调内不得再调用 Logger 的写接口
		template <typename Fn>
		void forEach(Fn&& fn) const
		{
			std::scoped_lock lock(m_mutex);
			for (const auto& entry : m_entries)
				fn(entry);
		}

		void clear()
		{
			std::scoped_lock lock(m_mutex);
			m_entries.clear();
		}

		const std::wstring& filePath() const { return m_filePath; }

	private:
		static constexpr std::size_t kMaxEntries = 500;

		static std::string toUtf8(const std::wstring& text)
		{
			if (text.empty()) return {};
			const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
				static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
			if (size <= 0) return {};
			std::string result(static_cast<std::size_t>(size), '\0');
			WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
				result.data(), size, nullptr, nullptr);
			return result;
		}

		// 调用方必须已持有 m_mutex；文件损坏/被占用时静默放弃，界面缓冲不受影响
		void appendToFileLocked(const SYSTEMTIME& st, const Entry& entry)
		{
			if (m_fileBroken) return;

			std::error_code ec;
			bool writeBom = true;
			if (std::filesystem::exists(m_filePath, ec) && !ec)
				writeBom = std::filesystem::file_size(m_filePath, ec) == 0 && !ec;

			std::ofstream file(m_filePath, std::ios::binary | std::ios::app);
			if (!file)
			{
				m_fileBroken = true;
				return;
			}
			if (writeBom)
				file.write("\xEF\xBB\xBF", 3);

			static constexpr const wchar_t* levelTags[] = { L"INFO", L"OK", L"WARN", L"ERR" };
			wchar_t dateText[24]{};
			swprintf_s(dateText, L"%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
			const std::wstring line = std::wstring(L"[") + dateText + L" " + entry.time
				+ L"][" + levelTags[static_cast<int>(entry.level)] + L"] " + entry.message;
			file << toUtf8(line) << "\n";
		}

		std::deque<Entry> m_entries;
		std::wstring m_filePath;
		mutable std::mutex m_mutex;
		bool m_fileBroken{ false };
	};

	inline Logger g_logger;
}
