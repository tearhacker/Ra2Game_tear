#pragma once

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace string
{
	inline std::wstring toLower(std::wstring s) {
		std::transform(s.begin(), s.end(), s.begin(), [](wchar_t character) {
			return static_cast<wchar_t>(std::towlower(character));
		});
		return s;
	}
}

namespace utils
{
	inline bool readFileToMem(const std::filesystem::path& path, std::vector<BYTE>& buffer) {
		std::ifstream file(path, std::ios::binary);
		if (file.fail()) return false;

		buffer.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		file.close();

		return true;
	}
}
