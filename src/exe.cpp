#include <stdio.h>
#include <strsafe.h>

#include <string>

#include <Windows.h>
#include <detours/detours.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	const DWORD requiredBufferSize = GetFullPathNameA(DLL_FILENAME, 0, nullptr, nullptr);
	if(requiredBufferSize == 0)
	{
		return static_cast<int>(GetLastError());
	}

	std::string dllFullPath(requiredBufferSize, '\0');

	if(GetFullPathNameA(DLL_FILENAME, dllFullPath.size(), dllFullPath.data(), nullptr) == 0)
	{
		return static_cast<int>(GetLastError());
	}

	//////////////////////////////////////////////////////////////////////////
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	CHAR szCommand[2048];
	CHAR szExe[1024];
	CHAR szFullExe[1024] = "\0";
	PCHAR pszFileExe = NULL;
	LPCSTR rpszDllsOut[1] = {dllFullPath.data()};

	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);

	szCommand[0] = L'\0';

	StringCchCopyA(szExe, sizeof(szExe), "C:/Windows/notepad.exe");

	DWORD dwFlags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED;

	SetLastError(0);
	SearchPathA(NULL, szExe, ".exe", ARRAYSIZE(szFullExe), szFullExe, &pszFileExe);
	if (!DetourCreateProcessWithDllsA(szFullExe, szCommand, NULL, NULL, TRUE, dwFlags, NULL, NULL,
	       &si, &pi, ARRAYSIZE(rpszDllsOut), rpszDllsOut, NULL)) {
		DWORD dwError = GetLastError();
		printf("withdll.exe: DetourCreateProcessWithDllEx failed: %ld\n", dwError);
		if (dwError == ERROR_INVALID_HANDLE) {
#if DETOURS_64BIT
			printf("withdll.exe: Can't detour a 32-bit target process from a 64-bit parent process.\n");
#else
			printf("withdll.exe: Can't detour a 64-bit target process from a 32-bit parent process.\n");
#endif
		}
		ExitProcess(9009);
	}

	ResumeThread(pi.hThread);

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD dwResult = 0;
	if (!GetExitCodeProcess(pi.hProcess, &dwResult)) {
		printf("withdll.exe: GetExitCodeProcess failed: %ld\n", GetLastError());
		return 9010;
	}

	return dwResult;
}
