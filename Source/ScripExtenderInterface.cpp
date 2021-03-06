#include "stdafx.h"
#include "ScripExtenderInterface.h"
#include "PrivateProfileRedirector.h"
#include <shlobj.h>

#pragma warning(disable: 4244) // conversion from 'x' to 'y', possible loss of data
#pragma warning(disable: 4267) // ~
#include <common/ITypes.h>

#if defined _WIN64
#include <skse64_common/skse_version.h>
#include <skse64_common/SafeWrite.h>
#include <skse64/PluginAPI.h>
#include <skse64/ScaleformCallbacks.h>
#include <skse64/ScaleformMovie.h>
#include <skse64/GameAPI.h>
#else
#include <skse/skse_version.h>
#include <skse/PluginAPI.h>
#include <skse/SafeWrite.h>
#include <skse/ScaleformCallbacks.h>
#include <skse/ScaleformMovie.h>
#include <skse/GameAPI.h>
#endif

//////////////////////////////////////////////////////////////////////////
#undef SEAPI
#define SEAPI(retType) retType __cdecl

static PluginHandle g_PluginHandle = kPluginHandle_Invalid;
static SKSEScaleformInterface* g_Scaleform = NULL;

//////////////////////////////////////////////////////////////////////////
#if 0
class SKSEScaleform_GetLibraryVersion: public GFxFunctionHandler
{
	public:
		virtual void Invoke(Args* args) override
		{
			Console_Print("%s v%s loaded", PrivateProfileRedirector::GetLibraryName(), PrivateProfileRedirector::GetLibraryVersion());
		}
};
bool RegisterScaleform(GFxMovieView* view, GFxValue* root)
{
	std::string name("Get");
	name += PrivateProfileRedirector::GetLibraryName();
	name += "Version";

	RegisterFunction<SKSEScaleform_GetLibraryVersion>(root, view, name.c_str());
	return true;
}
#endif

SEAPI(bool) SKSEPlugin_Query(const SKSEInterface* skse, PluginInfo* info)
{
	// Set info
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = PrivateProfileRedirector::GetLibraryName();
	info->version = PrivateProfileRedirector::GetLibraryVersionInt();

	// Save handle
	g_PluginHandle = skse->GetPluginHandle();

	// Get the scale form interface and query its version
	#if 0
	g_Scaleform = (SKSEScaleformInterface*)skse->QueryInterface(kInterface_Scaleform);
	if (!g_Scaleform)
	{
		//_MESSAGE("couldn't get scaleform interface");
		return false;
	}
	if (g_Scaleform->interfaceVersion < SKSEScaleformInterface::kInterfaceVersion)
	{
		//_MESSAGE("scaleform interface too old (%d expected %d)", g_scaleform->interfaceVersion, SKSEScaleformInterface::kInterfaceVersion);
		return false;
	}
	#endif

	return true;
}
SEAPI(bool) SKSEPlugin_Load(const SKSEInterface* skse)
{
	// Register scaleform callbacks
	//g_Scaleform->Register(PrivateProfileRedirector::GetLibraryName(), RegisterScaleform);

	return true;
}
