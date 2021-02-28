#pragma once

#include "pluginmain.h"

//plugin data
#define PLUGIN_NAME "CPUID Spoofer"
#define PLUGIN_VERSION 1

//functions
bool pluginInit(PLUG_INITSTRUCT* initStruct);
void pluginStop();
void pluginSetup();

#ifdef _WIN64
#define DUINT_FMT "0x%016llX"
#else
#define DUINT_FMT "0x%016lX"
#endif //_WIN64