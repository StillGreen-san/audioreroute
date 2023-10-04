#include <Windows.h>
#include <detours/detours.h>

#include <mmdeviceapi.h>

#include <memory>
#include <string>

namespace win32
{
std::wstring GetModuleFileNameW(::HMODULE hModule)
{
	std::wstring modulePath(MAX_PATH, 0);
	while(true)
	{
		const DWORD charsWritten = ::GetModuleFileNameW(hModule, modulePath.data(), modulePath.size());
		if(charsWritten == 0)
		{
			modulePath.clear();
			return modulePath;
		}
		if(charsWritten == modulePath.size())
		{
			modulePath.append(1, 0);
			modulePath.resize(modulePath.capacity());
			continue;
		}
		// charsWritten < modulePath.size()
		modulePath.resize(charsWritten);
		return modulePath;
	}
}
std::wstring QueryDosDeviceW(LPCWSTR lpDeviceName)
{
	std::wstring dosDevice(MAX_PATH, 0);
	while(true)
	{
		const DWORD charsWritten = ::QueryDosDeviceW(lpDeviceName, dosDevice.data(), dosDevice.size());
		if(charsWritten == 0)
		{
			if(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				dosDevice.append(1, 0);
				dosDevice.resize(dosDevice.capacity());
				continue;
			}
			dosDevice.clear();
			return dosDevice;
		}
		// charsWritten < modulePath.size()
		dosDevice.resize(charsWritten);
		return dosDevice;
	}
}
} // namespace win32

// Target pointer for the uninstrumented Sleep API.
static HRESULT(STDAPICALLTYPE* TrueCoCreateInstance)(
    REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv) = CoCreateInstance;

struct DetourMMDeviceEnumerator : IMMDeviceEnumerator
{
	IMMDeviceEnumerator* winImpl = nullptr;
	HRESULT STDMETHODCALLTYPE EnumAudioEndpoints(EDataFlow dataFlow, DWORD dwStateMask, IMMDeviceCollection** ppDevices) override
	{
		return winImpl->EnumAudioEndpoints(dataFlow,dwStateMask, ppDevices);
	}
	HRESULT STDMETHODCALLTYPE GetDefaultAudioEndpoint(EDataFlow dataFlow, ERole role, IMMDevice** ppEndpoint) override
	{
		return winImpl->GetDefaultAudioEndpoint(dataFlow, role, ppEndpoint);
	}
	HRESULT STDMETHODCALLTYPE GetDevice(LPCWSTR pwstrId, IMMDevice** ppDevice) override
	{
		return winImpl->GetDevice(pwstrId, ppDevice);
	}
	HRESULT STDMETHODCALLTYPE RegisterEndpointNotificationCallback(IMMNotificationClient* pClient) override
	{
		return winImpl->RegisterEndpointNotificationCallback(pClient);
	}
	HRESULT STDMETHODCALLTYPE UnregisterEndpointNotificationCallback(IMMNotificationClient* pClient) override
	{
		return winImpl->UnregisterEndpointNotificationCallback(pClient);
	}
	HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** ppvObject) override
	{
		return winImpl->QueryInterface(riid, ppvObject);
	}
	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return winImpl->AddRef();
	}
	ULONG STDMETHODCALLTYPE Release() override
	{
		return winImpl->Release();
	}
};
static DetourMMDeviceEnumerator detourMmDeviceEnumerator{};

// Detour function that replaces the Sleep API.
HRESULT STDAPICALLTYPE DetouredCoCreateInstance(
    REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv)
{
	if(IsEqualGUID(rclsid, __uuidof(MMDeviceEnumerator)) && IsEqualGUID(riid, __uuidof(IMMDeviceEnumerator)))
	{
		const HRESULT result = TrueCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
		detourMmDeviceEnumerator.winImpl = static_cast<IMMDeviceEnumerator*>(*ppv);
		*ppv = &detourMmDeviceEnumerator;
		return result;
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
