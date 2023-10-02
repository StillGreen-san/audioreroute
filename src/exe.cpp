#include <stdio.h>
#include <strsafe.h>

#include <Windows.h>
#include <detours/detours.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	LPCSTR rpszDllsOut;
	DWORD nDlls = 1;

	/////////////////////////////////////////////////////////// Validate DLLs.
	for (DWORD n = 0; n < nDlls; n++) {
		CHAR szDllPath[1024];
		PCHAR pszFilePart = NULL;
		LPSTR szDllName = "arrdll.dll";

		DWORD len = GetFullPathNameA(szDllName, ARRAYSIZE(szDllPath), szDllPath, &pszFilePart);
		if (len == 0) {
			printf("withdll.exe: Error: %s is not a valid path name..\n", szDllName);
			return 9002;
		}

		PCHAR psz = new CHAR [len];
		StringCchCopyA(psz, len, szDllPath);
		rpszDllsOut = psz;
	}

	//////////////////////////////////////////////////////////////////////////
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	CHAR szCommand[2048];
	CHAR szExe[1024];
	CHAR szFullExe[1024] = "\0";
	PCHAR pszFileExe = NULL;

	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);

	szCommand[0] = L'\0';

	StringCchCopyA(szExe, sizeof(szExe), "C:/Windows/notepad.exe");

	DWORD dwFlags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED;

	SetLastError(0);
	SearchPathA(NULL, szExe, ".exe", ARRAYSIZE(szFullExe), szFullExe, &pszFileExe);
	if (!DetourCreateProcessWithDllsA(szFullExe[0] ? szFullExe : NULL, szCommand,
	       NULL, NULL, TRUE, dwFlags, NULL, NULL,
	       &si, &pi, nDlls, &rpszDllsOut, NULL)) {
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

	delete rpszDllsOut;
	rpszDllsOut = NULL;

	return dwResult;
}
