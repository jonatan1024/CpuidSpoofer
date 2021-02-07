#include "plugin.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "localdecoder.h"
#include "dialog.h"
#include "presets.h"

enum class MenuEntries {
	OPTIONS_DIALOG,
	SET_BPS,
	SET_BP_HERE,
	REMOVE_BPS,
};

const unsigned char cpuidBytes[2] = {0x0F, 0xA2};

bool checkCpuidAt(duint addr) {
	BASIC_INSTRUCTION_INFO info;
	DbgDisasmFastAt(addr, &info);
	return strcmp("cpuid", info.instruction) == 0;
}

std::unordered_map<duint, std::string> actions;

bool onCpuidSpooferBegin(int argc, char** argv) {
	duint cip = GetContextData(UE_CIP);
	if(!checkCpuidAt(cip)) {
		dprintf("Not a CPUID instruction on current address 0x%016llX!\n", cip);
		return false;
	}

	auto actionIt = actions.find(cip);
	if(actionIt != actions.cend()) {
		dprintf("Overwriting previous stored action at address 0x%016llX!\n", cip);
		actions.erase(cip);
	}

	DbgCmdExecDirect("$breakpointcondition=0");

	std::string action;
	for(const auto& preset : getEnabledPresets()) {
		auto trigger = preset.getTrigger();
		bool triggerOk;
		if(DbgEval(trigger.c_str(), &triggerOk) && triggerOk) {
			action.append(";");
			action.append(preset.getAction());
		}
		else if(!triggerOk) {
			dprintf("Failed to evaluate trigger condition of preset %s!\n", preset.getName().c_str());
			DbgCmdExecDirect("$breakpointcondition=1");
		}
	}

	actions.emplace(cip, action);
	return true;
}

bool onCpuidSpooferEnd(int argc, char** argv) {
	duint prevCip = GetContextData(UE_CIP) - sizeof(cpuidBytes);
	if(!checkCpuidAt(prevCip)) {
		dprintf("Not a CPUID instruction on previous address 0x%016llX!\n", prevCip);
		return false;
	}
	auto actionIt = actions.find(prevCip);
	if(actionIt == actions.cend()) {
		dprintf("No action stored on previous address 0x%016llX!\n", prevCip);
		return false;
	}

	DbgCmdExecDirect("$breakpointcondition=0");
	const auto& action = actionIt->second;
	if(!DbgCmdExecDirect(action.c_str())) {
		dprintf("Failed to execute an action: %s!\n", action.c_str());
		DbgCmdExecDirect("$breakpointcondition=1");
	}
	actions.erase(actionIt);

	return true;
}

#define COMMAND_BEGIN "CpuidSpooferBegin"
#define COMMAND_END "CpuidSpooferEnd"

bool pluginInit(PLUG_INITSTRUCT* initStruct) {
	_plugin_registercommand(pluginHandle, COMMAND_BEGIN, onCpuidSpooferBegin, true);
	_plugin_registercommand(pluginHandle, COMMAND_END, onCpuidSpooferEnd, true);
	return true;
}

void pluginStop() {}

void pluginSetup() {
	_plugin_menuaddentry(hMenu, (int)MenuEntries::OPTIONS_DIALOG, "&Options");
	_plugin_menuaddentry(hMenu, (int)MenuEntries::SET_BPS, "Find CPUIDs and &set breakpoints");
	_plugin_menuaddentry(hMenu, (int)MenuEntries::SET_BP_HERE, "Set breakpoint &here");
	_plugin_menuaddentry(hMenu, (int)MenuEntries::REMOVE_BPS, "&Remove all Breakpoints");
}

void setBreakpoint(duint addr) {
	char cmdBuffer[256];
	//Create "begin" breakpoint
	sprintf_s(cmdBuffer, "SetBPX 0x%016llX, \"" PLUGIN_NAME " 0x%016llX begin\"", addr, addr);
	if(!DbgCmdExecDirect(cmdBuffer)) {
		dprintf("Breakpoint on address 0x%016llX already set!\n", addr);
		return;
	}
	sprintf_s(cmdBuffer, "SetBreakpointCommand 0x%016llX, \"" COMMAND_BEGIN "\"", addr);
	DbgCmdExecDirect(cmdBuffer);

	//Create "end" breakpoint
	duint endAddr = addr + sizeof(cpuidBytes);
	sprintf_s(cmdBuffer, "SetBPX 0x%016llX, \"" PLUGIN_NAME " 0x%016llX end\"", endAddr, addr);
	if(!DbgCmdExecDirect(cmdBuffer)) {
		dprintf("Breakpoint on address 0x%016llX already set!\n", endAddr);
		//Delete "begin" breakpoint
		sprintf_s(cmdBuffer, "DeleteBPX 0x%016llX", addr);
		DbgCmdExecDirect(cmdBuffer);
		return;
	}
	sprintf_s(cmdBuffer, "SetBreakpointCommand 0x%016llX, \"" COMMAND_END "\"", endAddr);
	DbgCmdExecDirect(cmdBuffer);
}

void maybeSetBreakpoint(duint addr, const std::unordered_set<duint>& knownBPs) {
	if(knownBPs.find(addr) != knownBPs.cend())
		return;
	LocalDecoder localDecoder(addr, sizeof(cpuidBytes));
	if(!localDecoder.IsDesired())
		return;
	setBreakpoint(addr);
}

std::unordered_set<duint> getKnownBreakpoints() {
	std::unordered_set<duint> knownBPs;

	BPMAP bpMap;
	memset(&bpMap, 0, sizeof(bpMap));
	DbgGetBpList(bp_normal, &bpMap);
	for(int bpIdx = 0; bpIdx < bpMap.count; bpIdx++) {
		const auto& bp = bpMap.bp[bpIdx];
		if(strncmp(PLUGIN_NAME, bp.name, sizeof(PLUGIN_NAME) - 1) == 0) {
			knownBPs.emplace(bp.addr);
		}
	}
	if(bpMap.count > 0)
		BridgeFree(bpMap.bp);
	return knownBPs;
}

const int pageBufferSize = PAGE_SIZE;
unsigned char pageBuffer[pageBufferSize];

void setBreakpoints() {
	MEMMAP memMap;
	memset(&memMap, 0, sizeof(memMap));
	auto memMapOk = DbgMemMap(&memMap);
	if(!memMapOk) {
		dprintf("Failed to get memory map!\n");
		return;
	}

	const auto knownBPs = getKnownBreakpoints();

	duint firstByteMatch = 0;
	for(int pageIdx = 0; pageIdx < memMap.count; pageIdx++) {
		const auto& page = memMap.page[pageIdx];
		if((page.mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) == 0)
			continue;
		duint baseAddr = (duint)page.mbi.BaseAddress;
		for(duint pageAddr = baseAddr; pageAddr < baseAddr + page.mbi.RegionSize; pageAddr += pageBufferSize) {
			auto memReadOk = DbgMemRead(pageAddr, pageBuffer, pageBufferSize);
			if(!memReadOk) {
				dprintf("Failed to read memory chunk 0x%016llX - 0x%016llX!\n", pageAddr, pageAddr + pageBufferSize);
				continue;
			}
			if(firstByteMatch + 1 == pageAddr && pageBuffer[0] == cpuidBytes[1]) {
				maybeSetBreakpoint(firstByteMatch, knownBPs);
			}
			for(int byteIdx = 0; byteIdx < pageBufferSize; byteIdx++) {
				if(pageBuffer[byteIdx] == cpuidBytes[0]) {
					duint byteAddr = pageAddr + byteIdx;
					if(byteIdx + 1 == pageBufferSize) {
						firstByteMatch = byteAddr;
					}
					else if(pageBuffer[byteIdx + 1] == cpuidBytes[1]) {
						maybeSetBreakpoint(byteAddr, knownBPs);
					}
				}
			}
		}
	}
	BridgeFree(memMap.page);
}

void removeBreakpoints() {
	char cmdBuffer[256];
	for(duint addr : getKnownBreakpoints()) {
		sprintf_s(cmdBuffer, "DeleteBPX 0x%016llX", addr);
		DbgCmdExecDirect(cmdBuffer);
	}
}

PLUG_EXPORT void CBMENUENTRY(CBTYPE cbType, PLUG_CB_MENUENTRY* info) {
	switch((MenuEntries)info->hEntry) {
	case MenuEntries::OPTIONS_DIALOG:
		showPresetsDialog(hwndDlg);
		return;
	case MenuEntries::SET_BPS:
		setBreakpoints();
		return;
	case MenuEntries::SET_BP_HERE:
		SELECTIONDATA selection;
		GuiSelectionGet(GUI_DISASSEMBLY, &selection);
		maybeSetBreakpoint(selection.start, getKnownBreakpoints());
		return;
	case MenuEntries::REMOVE_BPS:
		removeBreakpoints();
		return;
	}
	assert(false);
}
