#include "stdafx.h"
#include "PrivateProfileRedirector.h"
#include <strsafe.h>
#include <detours.h>
#include <detver.h>
#pragma comment(lib, "detours.lib")

#define AttachFunctionN(name)	AttachFunction(m_##name, On_##name, L#name)
#define DetachFunctionN(name)	DetachFunction(m_##name, On_##name, L#name)

//////////////////////////////////////////////////////////////////////////
bool INIObject::LoadFile()
{
	FILE* file = _wfopen(m_Path, L"rb");
	if (file)
	{
		m_ExistOnDisk = true;
		m_INIMap.LoadFile(file);
		fclose(file);
		return true;
	}
	return false;
}
bool INIObject::SaveFile(bool fromOnWrite)
{
	if (fromOnWrite)
	{
		PrivateProfileRedirector::GetInstance().Log(L"Saving file on write: '%s'", m_Path.data());
	}

	if (m_INIMap.SaveFile(m_Path, false) == SI_OK)
	{
		m_IsChanged = false;
		m_ExistOnDisk = true;
		return true;
	}
	return false;
}
void INIObject::OnWrite()
{
	m_IsChanged = true;
	if (PrivateProfileRedirector::GetInstance().ShouldSaveOnWrite())
	{
		SaveFile(true);
	}
}

//////////////////////////////////////////////////////////////////////////
PrivateProfileRedirector* PrivateProfileRedirector::ms_Instance = NULL;
const int PrivateProfileRedirector::ms_VersionMajor = 0;
const int PrivateProfileRedirector::ms_VersionMinor = 1;
const int PrivateProfileRedirector::ms_VersionPatch = 1;

PrivateProfileRedirector& PrivateProfileRedirector::CreateInstance()
{
	DestroyInstance();
	ms_Instance = new PrivateProfileRedirector();

	return *ms_Instance;
}
void PrivateProfileRedirector::DestroyInstance()
{
	delete ms_Instance;
	ms_Instance = NULL;
}

const char* PrivateProfileRedirector::GetLibraryName()
{
	return "PrivateProfileRedirector";
}
const char* PrivateProfileRedirector::GetLibraryVersion()
{
	static char ms_Version[16] = {0};
	if (*ms_Version == '\000')
	{
		sprintf_s(ms_Version, "%d.%d.%d", ms_VersionMajor, ms_VersionMinor, ms_VersionPatch);
	}
	return ms_Version;
}
int PrivateProfileRedirector::GetLibraryVersionInt()
{
	// 1.2.3 -> 1 * 100 + 2 * 10 + 3 * 1 = 123
	// 0.1 -> (0 * 100) + (1 * 10) + (0 * 1) = 10
	return (ms_VersionMajor * 100) + (ms_VersionMinor * 10) + (ms_VersionPatch * 1);
}

//////////////////////////////////////////////////////////////////////////
void PrivateProfileRedirector::InitFunctions()
{
	m_GetPrivateProfileStringA = ::GetPrivateProfileStringA;
	m_GetPrivateProfileStringW = ::GetPrivateProfileStringW;

	m_WritePrivateProfileStringA = ::WritePrivateProfileStringA;
	m_WritePrivateProfileStringW = ::WritePrivateProfileStringW;

	m_GetPrivateProfileIntA = ::GetPrivateProfileIntA;
	m_GetPrivateProfileIntW = ::GetPrivateProfileIntW;
}
void PrivateProfileRedirector::LogAttachDetachStatus(LONG status, const wchar_t* operation, const FunctionInfo& info)
{
	switch (status)
	{
		case NO_ERROR:
		{
			Log(L"[%s]: %s -> NO_ERROR", operation, info.Name);
			break;
		}
		case ERROR_INVALID_BLOCK:
		{
			Log(L"[%s]: %s -> ERROR_INVALID_BLOCK", operation, info.Name);
			break;
		}
		case ERROR_INVALID_HANDLE:
		{
			Log(L"[%s]: %s -> ERROR_INVALID_HANDLE", operation, info.Name);
			break;
		}
		case ERROR_INVALID_OPERATION:
		{
			Log(L"[%s]: %s -> ERROR_INVALID_OPERATION", operation, info.Name);
			break;
		}
		case ERROR_NOT_ENOUGH_MEMORY:
		{
			Log(L"[%s]: %s -> ERROR_NOT_ENOUGH_MEMORY", operation, info.Name);
			break;
		}
		default:
		{
			Log(L"[%s]: %s -> <Unknown>", operation, info.Name);
			break;
		}
	};
}

void PrivateProfileRedirector::OverrideFunctions()
{
	// 1
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());

		AttachFunctionN(GetPrivateProfileStringW);
		AttachFunctionN(GetPrivateProfileStringA);
		AttachFunctionN(WritePrivateProfileStringA);
		AttachFunctionN(WritePrivateProfileStringW);

		DetourTransactionCommit();
	}

	// 2
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());

		AttachFunctionN(GetPrivateProfileIntA);
		AttachFunctionN(GetPrivateProfileIntW);

		DetourTransactionCommit();
	}
}
void PrivateProfileRedirector::RestoreFunctions()
{
	// 1
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());

		DetachFunctionN(GetPrivateProfileStringW);
		DetachFunctionN(GetPrivateProfileStringA);
		DetachFunctionN(WritePrivateProfileStringA);
		DetachFunctionN(WritePrivateProfileStringW);

		DetourTransactionCommit();
	}

	// 2
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());

		DetachFunctionN(GetPrivateProfileIntA);
		DetachFunctionN(GetPrivateProfileIntW);

		DetourTransactionCommit();
	}
}

const wchar_t* PrivateProfileRedirector::GetConfigOption(const wchar_t* section, const wchar_t* key, const wchar_t* defaultValue) const
{
	return m_Config.GetValue(section, key, defaultValue);
}
int PrivateProfileRedirector::GetConfigOptionInt(const wchar_t* section, const wchar_t* key, int defaultValue) const
{
	const wchar_t* value = GetConfigOption(section, key, NULL);
	if (value)
	{
		int valueInt = defaultValue;
		swscanf(value, L"%d", &valueInt);
		return valueInt;
	}
	return defaultValue;
}

PrivateProfileRedirector::PrivateProfileRedirector()
	:m_ThreadID(GetCurrentThreadId()), m_Config(false, false, false, false)
{
	// Load config
	m_Config.LoadFile(L"Data\\SKSE\\Plugins\\PrivateProfileRedirector.ini");
	m_ShouldSaveOnWrite = GetConfigOptionBool(L"General", L"SaveOnWrite", true);
	m_ShouldSaveOnThreadDetach = GetConfigOptionBool(L"General", L"SaveOnThreadDetach", false);

	// Open log
	if (GetConfigOptionBool(L"General", L"EnableLog", false))
	{
		m_Log = fopen("Data\\SKSE\\Plugins\\PrivateProfileRedirector.log", "wb+");
	}
	Log(L"Log opened");

	// Save function pointers
	InitFunctions();

	// Initialize detour
	DetourRestoreAfterWith();
	OverrideFunctions();
}
PrivateProfileRedirector::~PrivateProfileRedirector()
{
	// Uninitializing
	RestoreFunctions();

	// Save files
	SaveChnagedFiles(L"On process detach");

	// Close log
	Log(L"Log closed");
	if (m_Log)
	{
		fclose(m_Log);
	}
}

INIObject& PrivateProfileRedirector::GetOrLoadFile(const KxDynamicString& path)
{
	auto it = m_INIMap.find(path);
	if (it != m_INIMap.end())
	{
		return *it->second;
	}
	else
	{
		KxCriticalSectionLocker lock(m_INIMapCS);

		auto& ini = m_INIMap.insert_or_assign(path, std::make_unique<INIObject>(path)).first->second;
		ini->LoadFile();

		Log(L"Attempt to access file: '%s' -> file object initialized. Exist on disk: %d", path.data(), ini->IsExistOnDisk());
		return *ini;
	}
}
void PrivateProfileRedirector::SaveChnagedFiles(const wchar_t* message) const
{
	Log(L"Saving files: %s", message);

	size_t changedCount = 0;
	for (const auto& v: m_INIMap)
	{
		const auto& iniObject = v.second;
		if (iniObject->IsChanged())
		{
			changedCount++;
			iniObject->SaveFile();
			Log(L"File saved: '%s'", v.first.data());
		}
		else
		{
			Log(L"File wasn't changed: '%s'", v.first.data());
		}
	}
	Log(L"All changed files saved. Total: %zu, Changed: %zu", m_INIMap.size(), changedCount);
}

//////////////////////////////////////////////////////////////////////////
#undef PPR_API
#define PPR_API(retType) retType WINAPI

namespace
{
	// Zero Separated STRing Zero Zero
	size_t KeysSectionsToZSSTRZZ(const INIFile::TNamesDepend& valuesList, KxDynamicString& zsstrzz, size_t maxSize)
	{
		if (!valuesList.empty())
		{
			for (const auto& v: valuesList)
			{
				zsstrzz.append(v.pItem);
			}
			zsstrzz.append(L'\000');
		}
		else
		{
			zsstrzz.append(2, L'\000');
		}

		size_t length = zsstrzz.size();
		zsstrzz.resize(maxSize);
		if (length >= maxSize)
		{
			if (zsstrzz.size() >= 2)
			{
				zsstrzz[zsstrzz.size() - 2] = L'\000';
			}
			if (zsstrzz.size() >= 1)
			{
				zsstrzz[zsstrzz.size() - 1] = L'\000';
			}
		}
		return zsstrzz.length();
	}
}

PPR_API(DWORD) On_GetPrivateProfileStringA(LPCSTR appName, LPCSTR keyName, LPCSTR defaultValue, LPSTR lpReturnedString, DWORD nSize, LPCSTR lpFileName)
{
	auto appNameW = KxDynamicString::to_utf16(appName);
	auto keyNameW = KxDynamicString::to_utf16(keyName);
	auto defaultValueW = KxDynamicString::to_utf16(defaultValue);
	auto lpFileNameW = KxDynamicString::to_utf16(lpFileName);

	PrivateProfileRedirector::GetInstance().Log(L"[GetPrivateProfileStringA]: Redirecting to 'GetPrivateProfileStringW'");
	
	KxDynamicString lpReturnedStringW;
	lpReturnedStringW.resize(nSize + 1);

	DWORD length = On_GetPrivateProfileStringW(appNameW, keyNameW, defaultValueW, lpReturnedStringW.data(), nSize, lpFileNameW);
	if (length != 0)
	{
		std::string result = KxDynamicString::to_utf8(lpReturnedStringW.data());
		StringCchCopyNA(lpReturnedString, nSize, result.data(), result.length());
	}
	return length;
}
PPR_API(DWORD) On_GetPrivateProfileStringW(LPCWSTR appName, LPCWSTR keyName, LPCWSTR defaultValue, LPWSTR lpReturnedString, DWORD nSize, LPCWSTR lpFileName)
{
	PrivateProfileRedirector::GetInstance().Log(L"[GetPrivateProfileStringW]: Section: %s, Key: %s, Default: %s, Path: %s", appName, keyName, defaultValue, lpFileName);

	if (lpFileName)
	{
		if (lpReturnedString == NULL || nSize == 0)
		{
			SetLastError(ERROR_INSUFFICIENT_BUFFER);
			return 0;
		}

		KxDynamicString pathL(lpFileName);
		pathL.make_lower();

		const INIObject& iniObject = PrivateProfileRedirector::GetInstance().GetOrLoadFile(pathL);
		const INIFile& iniFile = iniObject.GetFile();

		// Enum all sections
		if (appName == NULL)
		{
			PrivateProfileRedirector::GetInstance().Log(L"[GetPrivateProfileStringW]: Enum all sections of '%s'", lpFileName);

			KxDynamicString sectionsList;
			INIFile::TNamesDepend sections;
			iniFile.GetAllSections(sections);
			sections.sort(INIFile::Entry::LoadOrder());

			size_t length = KeysSectionsToZSSTRZZ(sections, sectionsList, nSize);
			StringCchCopyNW(lpReturnedString, nSize, sectionsList.data(), sectionsList.length());
			return static_cast<DWORD>(length);
		}

		// Enum all keys in section
		if (keyName == NULL)
		{
			PrivateProfileRedirector::GetInstance().Log(L"[GetPrivateProfileStringW]: Enum all keys is '%s' section of '%s'", appName, lpFileName);

			KxDynamicString keysList;
			INIFile::TNamesDepend keys;
			iniFile.GetAllKeys(appName, keys);
			keys.sort(INIFile::Entry::LoadOrder());

			size_t length = KeysSectionsToZSSTRZZ(keys, keysList, nSize);
			StringCchCopyNW(lpReturnedString, nSize, keysList.data(), keysList.length());
			return static_cast<DWORD>(length);
		}

		if (lpReturnedString)
		{
			LPCWSTR value = iniFile.GetValue(appName, keyName, defaultValue);
			if (value)
			{
				PrivateProfileRedirector::GetInstance().Log(L"[GetPrivateProfileStringW]: Value: %s", value);

				StringCchCopyNW(lpReturnedString, nSize, value, nSize);
				
				size_t length = 0;
				StringCchLengthW(value, nSize, &length);
				return static_cast<DWORD>(length);
			}
			else
			{
				PrivateProfileRedirector::GetInstance().Log(L"[GetPrivateProfileStringW]: Value: <none>");

				StringCchCopyNW(lpReturnedString, nSize, L"", 1);
				return 0;
			}
		}
	}

	SetLastError(ERROR_FILE_NOT_FOUND);
	return 0;
}

PPR_API(UINT) On_GetPrivateProfileIntA(LPCSTR appName, LPCSTR keyName, INT defaultValue, LPCSTR lpFileName)
{
	PrivateProfileRedirector::GetInstance().Log(L"[GetPrivateProfileIntA]: Redirecting to 'GetPrivateProfileIntW'");

	auto appNameW = KxDynamicString::to_utf16(appName);
	auto keyNameW = KxDynamicString::to_utf16(keyName);
	auto lpFileNameW = KxDynamicString::to_utf16(lpFileName);

	return On_GetPrivateProfileIntW(appNameW, keyNameW, defaultValue, lpFileNameW);
}
PPR_API(UINT) On_GetPrivateProfileIntW(LPCWSTR appName, LPCWSTR keyName, INT defaultValue, LPCWSTR lpFileName)
{
	PrivateProfileRedirector::GetInstance().Log(L"[GetPrivateProfileIntW]: Section: %s, Key: %s, Default: %d, Path: %s", appName, keyName, defaultValue, lpFileName);
	
	if (lpFileName && appName && keyName)
	{
		KxDynamicString pathL(lpFileName);
		pathL.make_lower();

		INIObject& ini = PrivateProfileRedirector::GetInstance().GetOrLoadFile(pathL);
		LPCWSTR value = ini.GetFile().GetValue(appName, keyName);
		if (value)
		{
			INT intValue = defaultValue;
			swscanf(value, L"%d", &intValue);

			PrivateProfileRedirector::GetInstance().Log(L"[GetPrivateProfileIntW]: ValueString: %s, ValueInt: %d", value, intValue);
			return intValue;
		}

		PrivateProfileRedirector::GetInstance().Log(L"[GetPrivateProfileIntW]: ValueString: <none>, ValueInt: %d", defaultValue);
		return defaultValue;
	}

	SetLastError(ERROR_FILE_NOT_FOUND);
	return defaultValue;
}

PPR_API(BOOL) On_WritePrivateProfileStringA(LPCSTR appName, LPCSTR keyName, LPCSTR lpString, LPCSTR lpFileName)
{
	PrivateProfileRedirector::GetInstance().Log(L"[WritePrivateProfileStringA]: Redirecting to 'WritePrivateProfileStringW'");

	auto appNameW = KxDynamicString::to_utf16(appName);
	auto keyNameW = KxDynamicString::to_utf16(keyName);
	auto lpStringW = KxDynamicString::to_utf16(lpString);
	auto lpFileNameW = KxDynamicString::to_utf16(lpFileName);

	return On_WritePrivateProfileStringW(appNameW, keyNameW, lpStringW, lpFileNameW);
}
PPR_API(BOOL) On_WritePrivateProfileStringW(LPCWSTR appName, LPCWSTR keyName, LPCWSTR lpString, LPCWSTR lpFileName)
{
	PrivateProfileRedirector::GetInstance().Log(L"[WritePrivateProfileStringW]: Section: %s, Key: %s, Value: %s, Path: %s", appName, keyName, lpString, lpFileName);

	if (appName)
	{
		KxDynamicString pathL(lpFileName);
		pathL.make_lower();

		INIObject& iniObject = PrivateProfileRedirector::GetInstance().GetOrLoadFile(pathL);
		KxCriticalSectionLocker lock(iniObject.GetLock());
		INIFile& ini = iniObject.GetFile();

		// Delete section
		if (keyName == NULL)
		{
			iniObject.OnWrite();
			return ini.Delete(appName, NULL, true);
		}

		// Delete value
		if (lpString == NULL)
		{
			iniObject.OnWrite();
			return ini.DeleteValue(appName, keyName, NULL, true);
		}

		// Set value
		SI_Error ret = ini.SetValue(appName, keyName, lpString, NULL, true);
		if (ret == SI_INSERTED || ret == SI_UPDATED)
		{
			iniObject.OnWrite();
			return TRUE;
		}
	}

	SetLastError(ERROR_FILE_NOT_FOUND);
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
BOOL APIENTRY DllMain(HMODULE module, DWORD event, LPVOID lpReserved)
{
    switch (event)
    {
		case DLL_PROCESS_ATTACH:
		{
			PrivateProfileRedirector::CreateInstance();
			break;
		}
		case DLL_THREAD_DETACH:
		{
			if (PrivateProfileRedirector* instance = PrivateProfileRedirector::GetInstancePtr())
			{
				if (instance->ShouldSaveOnThreadDetach())
				{
					instance->SaveChnagedFiles(L"On thread detach");
				}
			}
			break;
		}
		case DLL_PROCESS_DETACH:
		{
			PrivateProfileRedirector::DestroyInstance();
			break;
		}
    }
    return TRUE;
}

void DummyFunction()
{
}
