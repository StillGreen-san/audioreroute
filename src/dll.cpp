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
			return {};
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
std::wstring QueryDosDeviceW(::LPCWSTR lpDeviceName)
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
			return {};
		}
		// charsWritten < modulePath.size()
		dosDevice.resize(dosDevice.find_first_of(L'\0'));
		return dosDevice;
	}
}
class HKEY
{
public:
	HKEY(::HKEY hRootKey, ::LPCWSTR lpSubKey, ::REGSAM samDesired) noexcept
	{
		const LSTATUS openStatus = RegOpenKeyExW(hRootKey, lpSubKey, 0, samDesired, &hkey);
		if(openStatus != ERROR_SUCCESS)
		{
			hkey = nullptr;
		}
	}
	HKEY(const HKEY&) = delete;
	HKEY& operator=(const HKEY&) = delete;
	HKEY(HKEY&& other) noexcept : hkey{std::exchange(other.hkey, nullptr)}
	{
	}
	HKEY& operator=(HKEY&& other) noexcept
	{
		hkey = std::exchange(other.hkey, nullptr);
		return *this;
	}
	~HKEY() noexcept
	{
		RegCloseKey(hkey);
	}
	operator ::HKEY() noexcept
	{
		return hkey;
	}

private:
	::HKEY hkey = nullptr;
};
std::wstring RegQueryValueSZW(::HKEY hKey, ::LPCWSTR lpValueName)
{
	std::wstring szData(MAX_PATH, 0);
	while(true)
	{
		DWORD szDataBytes = szData.size() * sizeof(WCHAR);
		const LSTATUS szDataStatus = RegQueryValueExW(
		    hKey, lpValueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(szData.data()), &szDataBytes);
		if(szDataStatus == ERROR_SUCCESS)
		{
			szData.resize(szDataBytes / sizeof(WCHAR));
			return szData;
		}
		if(szDataStatus == ERROR_MORE_DATA)
		{
			szData.resize(szDataBytes / sizeof(WCHAR));
			continue;
		}
		return {};
	}
}
struct CoTaskMemFreeDeleter
{
	void operator()(LPVOID data) noexcept
	{
		CoTaskMemFree(data);
	}
};
struct IUnknownDeleter
{
	void operator()(IUnknown* itf) noexcept
	{
		if(itf)
		{
			itf->Release();
		}
	}
};
template<typename TDataType, typename TDeleter>
class Resource
{
public:
	using DataType = std::remove_pointer_t<TDataType>;
	Resource() = default;
	Resource(DataType* data) noexcept : data{data}
	{
	}
	Resource(const Resource&) = delete;
	Resource(Resource&& other) noexcept : data{std::exchange(other.data, nullptr)}
	{
	}
	Resource& operator=(const Resource&) = delete;
	Resource& operator=(Resource&& other) noexcept
	{
		data = std::exchange(other.data, nullptr);
		return *this;
	}
	~Resource()
	{
		TDeleter{}(data);
	}
	DataType* operator->() noexcept
	{
		return data;
	}
	DataType* get() noexcept
	{
		return data;
	}
	DataType** get_ptr() noexcept
	{
		return &data;
	}
	DataType* release() noexcept
	{
		return std::exchange(data, nullptr);
	}

private:
	DataType* data = nullptr;
};
template<typename TDataType>
using CoTaskResource = Resource<TDataType, CoTaskMemFreeDeleter>;
template<typename TDataType>
using IUnknownResource = Resource<TDataType, IUnknownDeleter>;
} // namespace win32

// Target pointer for the uninstrumented Sleep API.
static HRESULT(STDAPICALLTYPE* TrueCoCreateInstance)(
    REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv) = CoCreateInstance;

std::wstring extractAudioEndpointId(std::wstring endpointPath)
{
	endpointPath.erase(0, endpointPath.find_first_of('{'));
	endpointPath.resize(endpointPath.find_first_of('}', endpointPath.find_first_of('}') + 1) + 1);
	return endpointPath;
}

struct DetourMMDeviceEnumerator : IMMDeviceEnumerator
{
	IMMDeviceEnumerator* winImpl = nullptr;
	std::wstring outputId;
	std::wstring inputId;
	HRESULT STDMETHODCALLTYPE EnumAudioEndpoints(
	    EDataFlow dataFlow, DWORD dwStateMask, IMMDeviceCollection** ppDevices) override
	{
		return winImpl->EnumAudioEndpoints(dataFlow,dwStateMask, ppDevices);
	}
	HRESULT STDMETHODCALLTYPE GetDefaultAudioEndpoint(EDataFlow dataFlow, ERole role, IMMDevice** ppEndpoint) override
	{
		if(dataFlow == EDataFlow::eAll)
		{
			return winImpl->GetDefaultAudioEndpoint(dataFlow, role, ppEndpoint);
		}
		IMMDeviceCollection* pDevices;
		HRESULT hr = EnumAudioEndpoints(dataFlow, DEVICE_STATE_ACTIVE, &pDevices);
		bool bFind = false;
		if(SUCCEEDED(hr))
		{
			UINT count;
			hr = pDevices->GetCount(&count);
			if(SUCCEEDED(hr))
			{
				for(UINT i = 0; i < count; i++)
				{
					IMMDevice* pDevice;
					hr = pDevices->Item(i, &pDevice);
					if(SUCCEEDED(hr))
					{
						LPWSTR wstrID = NULL;
						hr = pDevice->GetId(&wstrID);
						if(SUCCEEDED(hr))
						{
							switch(dataFlow)
							{
							case eRender:
								if(lstrcmpW(wstrID, outputId.data()) == 0)
								{
									*ppEndpoint = pDevice;
									bFind = true;
								}
								break;
							case eCapture:
								if(lstrcmpW(wstrID, inputId.data()) == 0)
								{
									*ppEndpoint = pDevice;
									bFind = true;
								}
								break;
							}
						}
						if(!bFind)
						{
							pDevice->Release();
						}
					}
					if(bFind)
					{
						break;
					}
				}
			}
			pDevices->Release();
		}
		if(bFind)
		{
			return S_OK;
		}
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
	void reset() noexcept
	{
		outputId = std::wstring{};
		inputId = std::wstring{};
	}
	void queryDefaultId()
	{
		constexpr const LPCWSTR DefaultValue = L"";
		constexpr const LPCWSTR OutputValue = L"000_000";
		constexpr const LPCWSTR InputValue = L"000_001";
		constexpr const LPCWSTR DefaultEndpointKey = L"SOFTWARE\\Microsoft\\Multimedia\\Audio\\DefaultEndpoint";

		std::wstring exePath = win32::GetModuleFileNameW(nullptr);
		exePath[2] = 0;
		std::wstring devicePath = win32::QueryDosDeviceW(exePath.data());
		exePath[2] = '\\';
		devicePath.append(exePath, 2);

		win32::HKEY defaultEndpoints(HKEY_CURRENT_USER, DefaultEndpointKey, KEY_ENUMERATE_SUB_KEYS);

		for(DWORD endpointIndex = 0;; ++endpointIndex)
		{
			std::array<WCHAR, 16> enpointName{}; // optional
			const LSTATUS keyStatus =
			    RegEnumKeyW(defaultEndpoints, endpointIndex, enpointName.data(), enpointName.size());
			if(keyStatus == ERROR_NO_MORE_ITEMS)
			{
				break;
			}
			if(keyStatus == ERROR_SUCCESS)
			{
				win32::HKEY endpoint(defaultEndpoints, enpointName.data(), KEY_QUERY_VALUE);

				std::wstring appDevicePath = win32::RegQueryValueSZW(endpoint, DefaultValue);
				appDevicePath.resize(appDevicePath.find_first_of(L'\0'));
				if(appDevicePath != devicePath)
				{
					continue;
				}

				outputId = extractAudioEndpointId(win32::RegQueryValueSZW(endpoint, OutputValue));
				inputId = extractAudioEndpointId(win32::RegQueryValueSZW(endpoint, InputValue));

				break;
			}
		}
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

		detourMmDeviceEnumerator.queryDefaultId();
	}
	else if(dwReason == DLL_PROCESS_DETACH)
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(&(PVOID&)TrueCoCreateInstance, DetouredCoCreateInstance);
		DetourTransactionCommit();

		detourMmDeviceEnumerator.reset();
	}
	return TRUE;
}
