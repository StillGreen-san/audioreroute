#include <string>

#include <Windows.h>
#include <detours/detours.h>

static_assert(std::is_same_v<WCHAR, std::wstring::value_type>);

namespace win32
{
std::string GetFullPathNameA(::LPCSTR lpFileName)
{
	std::string fullPath(MAX_PATH, 0);
	while(true)
	{
		const DWORD result = ::GetFullPathNameA(lpFileName, fullPath.size(), fullPath.data(), nullptr);
		if(result == 0)
		{
			return {};
		}
		if(result > fullPath.size())
		{
			fullPath.resize(result);
			continue;
		}
		fullPath.resize(result);
		return fullPath;
	}
}
std::wstring SearchPathW(::LPCWSTR lpFileName, ::LPCWSTR lpExtension)
{
	std::wstring filePath(MAX_PATH, 0);
	while(true)
	{
		const DWORD result = ::SearchPathW(nullptr, lpFileName, lpExtension, filePath.size(), filePath.data(), nullptr);
		if(result == 0)
		{
			return {};
		}
		if(result > filePath.size())
		{
			filePath.resize(result);
			continue;
		}
		filePath.resize(result);
		return filePath;
	}
}
struct StartupInfoW : ::STARTUPINFOW
{
	StartupInfoW() :
	    ::STARTUPINFOW{
	        sizeof(*this),
	    }
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
class Handle
{
public:
	Handle() = default;
	Handle(::HANDLE handle) noexcept : handle{handle} {};
	Handle(const Handle&) = delete;
	Handle(Handle&& other) noexcept : handle{std::exchange(other.handle, INVALID_HANDLE_VALUE)} {};
	Handle& operator=(const Handle&) = delete;
	Handle& operator=(Handle&& other) noexcept
	{
		handle = std::exchange(other.handle, INVALID_HANDLE_VALUE);
		return *this;
	}
	~Handle() noexcept
	{
		CloseHandle(handle);
	}
	operator ::HANDLE() noexcept
	{
		return handle;
	}

private:
	::HANDLE handle = INVALID_HANDLE_VALUE;
};
std::wstring GetFinalPathNameByHandleW(::HANDLE hFile, ::DWORD dwFlags)
{
	std::wstring finalPath(MAX_PATH, 0);
	while(true)
	{
		const DWORD result = ::GetFinalPathNameByHandleW(hFile, finalPath.data(), finalPath.size(), dwFlags);
		if(result == 0)
		{
			return {};
		}
		if(result > finalPath.size())
		{
			finalPath.resize(result);
			continue;
		}
		finalPath.resize(result);
		return finalPath;
	}
}
} // namespace win32

std::pair<std::wstring_view, PWSTR> splitArgs(PWSTR cmdLine)
{
	std::wstring_view firstArg(cmdLine);
	if(firstArg.empty())
	{
		return {};
	}

	if(firstArg.front() != L'"')
	{
		const size_t sepPos = firstArg.find_first_of(L' ');
		if(sepPos != std::wstring_view::npos)
		{
			firstArg.remove_suffix(firstArg.size() - sepPos);
			cmdLine += firstArg.size() + 1;
		}
		else
		{
			cmdLine = nullptr;
		}
		return {firstArg, cmdLine};
	}

	const size_t strEndPos = firstArg.find_first_of(L'"', 1);
	if(strEndPos + 1 == firstArg.size())
	{
		cmdLine = nullptr;
	}
	else
	{
		firstArg.remove_suffix(firstArg.size() - (strEndPos + 1));
		cmdLine += firstArg.size() + 1;
	}
	return {firstArg, cmdLine};
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	const std::string dllFullPath = win32::GetFullPathNameA(DLL_FILENAME);
	if(dllFullPath.empty())
	{
		return static_cast<int>(GetLastError());
	}

	const auto programArgs = splitArgs(pCmdLine);
	const std::wstring firstArg(programArgs.first);
	const std::wstring exeFullPath = win32::SearchPathW(firstArg.data(), L"exe");
	if(exeFullPath.empty())
	{
		return static_cast<int>(GetLastError());
	}

	const std::wstring exeFinalPath = [&]() -> std::wstring
	{
		win32::Handle exeFile = CreateFileW(exeFullPath.data(), 0, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if(exeFile == INVALID_HANDLE_VALUE)
		{
			return {};
		}
		return win32::GetFinalPathNameByHandleW(exeFile, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
	}();
	if(exeFinalPath.empty())
	{
		return static_cast<int>(GetLastError());
	}

	win32::StartupInfoW startupInfoW;
	win32::ProcessInformation processInformation;
	LPCSTR dllsOut = dllFullPath.data();

	const DWORD dwFlags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED;

	if(DetourCreateProcessWithDllsW(&exeFinalPath[4], programArgs.second, nullptr, nullptr, TRUE, dwFlags, nullptr,
	       nullptr, &startupInfoW, &processInformation, 1, &dllsOut, nullptr) == FALSE)
	{
		return static_cast<int>(GetLastError());
	}

#if _DEBUG
	Sleep(12000);
#endif
	ResumeThread(processInformation.hThread);

	WaitForSingleObject(processInformation.hProcess, INFINITE);

	DWORD dwResult = 0;
	if(GetExitCodeProcess(processInformation.hProcess, &dwResult) == 0)
	{
		return static_cast<int>(GetLastError());
	}

	return static_cast<int>(dwResult);
}
