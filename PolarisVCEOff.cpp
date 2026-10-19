//
//  PolarisVCEOff.cpp
//  PolarisVCEOff
//
//  Turns off hardware H.264 encoding (VCE 3.4) on AMD Polaris (RX 560) under macOS.
//
//  Across all PolarisVCEFix builds the VCE stalled at the same rate (about once every
//  14,500 frames), and no macOS reset brought it back. This kext does one thing: it
//  refuses every VCE session in AMDVCEHWEngine::requestCapability, the same path that
//  in PolarisVCEFix v31 sent screen recordings to the CPU with valid video. Apps then
//  use the software H.264 encoder, or HEVC, which runs on the UVD ENC block and is not
//  touched here.
//
//  Status: ioreg -p IOService -l -n PolarisVCEOff
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_util.hpp>
#include <IOKit/IOLib.h>

static const char *pathRadeonX4000[] {
	"/System/Library/Extensions/AMDRadeonX4000.kext/Contents/MacOS/AMDRadeonX4000"
};

static KernelPatcher::KextInfo kextList[] {
	{ "com.apple.kext.AMDRadeonX4000", pathRadeonX4000, arrsize(pathRadeonX4000), {true}, {}, KernelPatcher::KextInfo::Unloaded }
};

static mach_vm_address_t orgVCEHWEngineRequestCapability {0};
static bool sHookApplied {false};
static uint32_t sRejectedVCESessions {0};

static void updateDiagProperties() {
	if (ADDPR(selfInstance)) {
		ADDPR(selfInstance)->setProperty("H264_Hardware_Encoder", "Disabled");
		ADDPR(selfInstance)->setProperty("Hook_Applied", sHookApplied);
		ADDPR(selfInstance)->setProperty("VCE_Sessions_Rejected", (unsigned long long)sRejectedVCESessions, 32);
	}
}

// AMDRadeonX4000_AMDVCEHWEngine::requestCapability(): never grants a VCE session.
static bool wrapVCEHWEngineRequestCapability(void *engine, void *cap, uint32_t *outId) {
	sRejectedVCESessions++;
	updateDiagProperties();
	IOLog("PolarisVCEOff: hardware H.264 encoder (VCE) is disabled on this GPU; session %u refused, use software H.264 or HEVC\n",
	      sRejectedVCESessions);
	return false;
}

static void processKext(void *user, KernelPatcher &patcher, size_t id, mach_vm_address_t slide, size_t size) {
	if (kextList[0].loadIndex != id) {
		return;
	}

	KernelPatcher::RouteRequest request("__ZN29AMDRadeonX4000_AMDVCEHWEngine17requestCapabilityEP12sCodecEngCapPj",
	                                    wrapVCEHWEngineRequestCapability, orgVCEHWEngineRequestCapability);
	if (patcher.routeMultiple(id, &request, 1, slide, size)) {
		sHookApplied = true;
		SYSLOG("PolarisVCEOff", "hardware H.264 encoder (VCE) disabled: requestCapability routed");
	} else {
		SYSLOG("PolarisVCEOff", "ERROR: failed to route requestCapability, code %d; VCE stays enabled", patcher.getError());
		patcher.clearError();
	}
	updateDiagProperties();
}

static void pluginStart() {
	lilu.onKextLoadForce(kextList, arrsize(kextList), processKext);
}

static const char *bootargOff[] {
	"-polarisvceoffoff"
};

static const char *bootargDebug[] {
	"-polarisvceoffdbg"
};

static const char *bootargBeta[] {
	"-polarisvceoffbeta"
};

PluginConfiguration ADDPR(config) {
	xStringify(PRODUCT_NAME),
	parseModuleVersion(xStringify(MODULE_VERSION)),
	LiluAPI::AllowNormal | LiluAPI::AllowInstallerRecovery | LiluAPI::AllowSafeMode,
	bootargOff,
	arrsize(bootargOff),
	bootargDebug,
	arrsize(bootargDebug),
	bootargBeta,
	arrsize(bootargBeta),
	KernelVersion::Sierra,
	KernelVersion::Tahoe,
	pluginStart
};
