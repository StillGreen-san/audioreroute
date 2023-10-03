#include <Windows.h>
#include <detours/detours.h>

#include <mmdeviceapi.h>

// Target pointer for the uninstrumented Sleep API.
static HRESULT(STDAPICALLTYPE* TrueCoCreateInstance)(
    REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID FAR* ppv) = CoCreateInstance;

// Detour function that replaces the Sleep API.
HRESULT STDAPICALLTYPE DetouredCoCreateInstance(
    REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID FAR* ppv)
{
	if(IsEqualGUID(rclsid, __uuidof(MMDeviceEnumerator)) && IsEqualGUID(riid, __uuidof(IMMDeviceEnumerator)))
	{
		return TrueCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
	}

	return TrueCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
}

// DllMain function attaches and detaches the TimedSleep detour to the
// Sleep target function.  The Sleep target function is referred to
// through the TrueSleep target pointer.
//
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
	if(DetourIsHelperProcess())
	{
		return TRUE;
	}

	if(dwReason == DLL_PROCESS_ATTACH)
	{
		DetourRestoreAfterWith();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)TrueCoCreateInstance, DetouredCoCreateInstance);
		DetourTransactionCommit();
	}
	else if(dwReason == DLL_PROCESS_DETACH)
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(&(PVOID&)TrueCoCreateInstance, DetouredCoCreateInstance);
		DetourTransactionCommit();
	}
	return TRUE;
}
