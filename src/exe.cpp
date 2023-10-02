#include <strsafe.h>

#include <string>
#include <string_view>

#include <Windows.h>
#include <detours/detours.h>

static_assert(std::is_same_v<WCHAR, std::wstring::value_type>);

namespace win32
{
struct StartupInfoW : ::STARTUPINFOW
{
	StartupInfoW() : ::STARTUPINFOW{sizeof(*this),}
	{
	}
};
static_assert(sizeof(StartupInfoW) == sizeof(::STARTUPINFOW));
struct ProcessInformation : ::PROCESS_INFORMATION
{
	ProcessInformation() : ::PROCESS_INFORMATION{}
	{
	}
};
static_assert(sizeof(ProcessInformation) == sizeof(::PROCESS_INFORMATION));
} // namespace win32

std::wstring_view getFirstArg(std::wstring_view str)
{
	if(str.empty())
	{
		return {};
	}

	if(str.front() != L'"')
	{
		const size_t sepPos = str.find_first_of(L' ');
		if(sepPos != std::wstring_view::npos)
		{
			str.remove_suffix(str.size() - sepPos);
		}
		return str;
	}

	const size_t strEndPos = str.find_first_of(L'"', 1);
	str.remove_suffix(str.size() - (strEndPos + 1));
	return str;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	const DWORD requiredDllBufferSize = GetFullPathNameA(DLL_FILENAME, 0, nullptr, nullptr);
	if(requiredDllBufferSize == 0)
	{
		return static_cast<int>(GetLastError());
	}

	std::string dllFullPath(requiredDllBufferSize, '\0');

	if(GetFullPathNameA(DLL_FILENAME, dllFullPath.size(), dllFullPath.data(), nullptr) == 0)
	{
		return static_cast<int>(GetLastError());
	}

	std::wstring firstArg(getFirstArg(pCmdLine));
	const DWORD requiredExeBufferSize = SearchPathW(nullptr, firstArg.data(), L".exe", 0, nullptr, nullptr);
	if(requiredExeBufferSize == 0)
	{
		return static_cast<int>(GetLastError());
	}

	std::wstring exeFullPath(requiredExeBufferSize, L'\0');

	if(SearchPathW(nullptr, firstArg.data(), L".exe", exeFullPath.size(), exeFullPath.data(), nullptr) == 0)
	{
		return static_cast<int>(GetLastError());
	}

	win32::StartupInfoW startupInfoW;
	win32::ProcessInformation processInformation;
	LPCSTR dllsOut = dllFullPath.data();

	const DWORD dwFlags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED;

	if(DetourCreateProcessWithDllsW(exeFullPath.data(), pCmdLine, nullptr, nullptr, TRUE, dwFlags, nullptr, nullptr,
	       &startupInfoW, &processInformation, 1, &dllsOut, nullptr) == FALSE)
	{
		return static_cast<int>(GetLastError());
	}

	ResumeThread(processInformation.hThread);

	WaitForSingleObject(processInformation.hProcess, INFINITE);

	DWORD dwResult = 0;
	if(GetExitCodeProcess(processInformation.hProcess, &dwResult) == 0)
	{
		return static_cast<int>(GetLastError());
	}

	return static_cast<int>(dwResult);
}
