#include "presets.h"

#include <Windows.h>

const int fileVersion = 1;
std::vector<Preset> presets;
Preset emptyPreset;

const int maxFilenameLen = 1024;
wchar_t filename[maxFilenameLen];

void loadString(std::wstring& string, FILE* file) {
	int strlen = 0;
	fread(&strlen, sizeof(strlen), 1, file);
	string.resize(strlen);
	fread(&string[0], sizeof(string[0]), strlen, file);
}

void loadPreset(Preset& preset, FILE* file) {
	loadString(preset.name, file);
	fread(&preset.isEnabled, sizeof(preset.isEnabled), 1, file);

	fread(&preset.isTriggerCustom, sizeof(preset.isTriggerCustom), 1, file);
	loadString(preset.triggerEaxValue, file);
	loadString(preset.triggerCustomValue, file);

	fread(&preset.isActionCustom, sizeof(preset.isActionCustom), 1, file);
	for(int i = 0; i < 4; i++) {
		fread(&preset.actionRegisters[i], sizeof(preset.actionRegisters[i]), 1, file);
		loadString(preset.actionMasks[i], file);
		loadString(preset.actionValues[i], file);
	}
	loadString(preset.actionCustomValue, file);
}

void loadPresets(const wchar_t* inFilename) {
	for(int i = 0; i < maxFilenameLen; i++) {
		filename[i] = inFilename[i];
		if(filename[i] == L'\0')
			break;
	}
	FILE* file;
	_wfopen_s(&file, filename, L"rb");
	if(file == NULL)
		return;

	int version = 0;
	fread(&version, sizeof(version), 1, file);
	if(version == fileVersion) {
		int numPresets = 0;
		fread(&numPresets, sizeof(numPresets), 1, file);
		presets.resize(numPresets);
		for(int i = 0; i < numPresets; i++) {
			loadPreset(presets[i], file);
		}
	}
	fclose(file);
}

void saveString(const std::wstring& string, FILE* file) {
	int strlen = (int)string.length();
	fwrite(&strlen, sizeof(strlen), 1, file);
	fwrite(&string[0], sizeof(string[0]), strlen, file);
}

void savePreset(const Preset& preset, FILE* file) {
	saveString(preset.name, file);
	fwrite(&preset.isEnabled, sizeof(preset.isEnabled), 1, file);

	fwrite(&preset.isTriggerCustom, sizeof(preset.isTriggerCustom), 1, file);
	saveString(preset.triggerEaxValue, file);
	saveString(preset.triggerCustomValue, file);

	fwrite(&preset.isActionCustom, sizeof(preset.isActionCustom), 1, file);
	for(int i = 0; i < 4; i++) {
		fwrite(&preset.actionRegisters[i], sizeof(preset.actionRegisters[i]), 1, file);
		saveString(preset.actionMasks[i], file);
		saveString(preset.actionValues[i], file);
	}
	saveString(preset.actionCustomValue, file);
}

void savePresets() {
	if(filename[0] == L'\0')
		return;

	FILE* file;
	_wfopen_s(&file, filename, L"wb");
	if(file == NULL)
		return;
	fwrite(&fileVersion, sizeof(fileVersion), 1, file);
	int numPresets = (int)presets.size();
	fwrite(&numPresets, sizeof(numPresets), 1, file);
	for(const auto& preset : presets) {
		savePreset(preset, file);
	}
	fclose(file);
}

std::vector<std::wstring> getPresetNames() {
	std::vector<std::wstring> names;
	for(const auto& preset : presets) {
		names.emplace_back(preset.name);
	}
	return names;
}

const Preset& getPreset(const std::wstring& name) {
	for(const auto& preset : presets) {
		if(preset.name == name)
			return preset;
	}
	return emptyPreset;
}

void setPreset(const Preset& preset) {
	for(auto& existingPreset : presets) {
		if(existingPreset.name == preset.name) {
			existingPreset = preset;
			goto setPresetSave;
		}
	}
	presets.emplace_back(preset);
setPresetSave:
	savePresets();
}

bool deletePreset(const std::wstring& name) {
	for(auto presetIt = presets.cbegin(); presetIt != presets.cend(); ++presetIt) {
		if(presetIt->name == name) {
			presets.erase(presetIt);
			savePresets();
			return true;
		}
	}
	return false;
}

std::string wstringToString(const std::wstring& wstr) {
	int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, NULL, NULL);
	std::string str;
	str.resize(len);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &str[0], len, NULL, NULL);
	return str;
}

std::string Preset::getName() const{
	return wstringToString(this->name);
}

std::string Preset::getTrigger() const{
	return wstringToString(this->triggerCustomValue);
}

std::string Preset::getAction() const{
	return wstringToString(this->actionCustomValue);
}

std::vector<Preset> getEnabledPresets() {
	std::vector<Preset> enabledPresets;
	for(const auto& preset : presets)
		if(preset.isEnabled)
			enabledPresets.emplace_back(preset);

	return enabledPresets;
}