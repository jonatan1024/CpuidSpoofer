#pragma once
#include <string>
#include <vector>

// when modyfing this struct, make sure you also modify:
// > the dialog, if necessary
// > loadPreset()
// > savePreset()
// > increment fileVersion

struct Preset {
	std::wstring name;
	bool isEnabled = true;

	bool isTriggerCustom = false;
	std::wstring triggerEaxValue;
	std::wstring triggerCustomValue;

	bool isActionCustom = false;
	bool actionRegisters[4] = {};
	std::wstring actionMasks[4];
	std::wstring actionValues[4];
	std::wstring actionCustomValue;

	std::string getName() const;
	std::string getTrigger() const;
	std::string getAction() const;
};

void loadPresets(const wchar_t* filename);
void savePresets();

std::vector<std::wstring> getPresetNames();
const Preset& getPreset(const std::wstring& name);
void setPreset(const Preset& preset);
bool deletePreset(const std::wstring& name);

std::vector<Preset> getEnabledPresets();
