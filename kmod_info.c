#include <mach/mach_types.h>

extern kern_return_t PolarisVCEOff_kern_start(kmod_info_t *ki, void *data);
extern kern_return_t PolarisVCEOff_kern_stop(kmod_info_t *ki, void *data);

__attribute__((visibility("default"))) KMOD_EXPLICIT_DECL(com.honeyforge.PolarisVCEOff, "1.0.0", PolarisVCEOff_kern_start, PolarisVCEOff_kern_stop)
__private_extern__ kmod_start_func_t *_realmain = 0;
__private_extern__ kmod_stop_func_t *_antimain = 0;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
