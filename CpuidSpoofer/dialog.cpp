#include <Windows.h>
#include <stdio.h>
#include "resource.h"

#include "presets.h"

#include <string>

std::wstring getWindowTextString(HWND hWnd) {
	std::wstring text;
	text.resize(GetWindowTextLengthW(hWnd));
	GetWindowTextW(hWnd, (LPWSTR)text.data(), (int)text.size() + 1);
	return text;
}

void enableSetRegister(HWND hDialog, UINT set, UINT mask, UINT value, BOOL enable) {
	if(enable)
		enable = IsDlgButtonChecked(hDialog, set);
	EnableWindow(GetDlgItem(hDialog, mask), enable);
	EnableWindow(GetDlgItem(hDialog, value), enable);
}

#pragma warning( disable : 4002 )

#define ENABLE_SET_INPUTS(register, enable) \
	enableSetRegister(hDialog, IDC_SET_ ## register, IDC_SET_ ## register ## _MASK, IDC_SET_ ## register ## _VALUE, enable)

#define ENABLE_SET_GROUP(register, enable) \
	do { \
		EnableWindow(GetDlgItem(hDialog, IDC_SET_ ## register), enable); \
		ENABLE_SET_INPUTS(register, enable); \
	} while(false)

#define ENABLE_ON_CLICKED_SET(register) \
	case IDC_SET_ ## register: \
		ENABLE_SET_INPUTS(register, TRUE); \
		return

#define FOR_EACH_REGISTER(statement, ...) \
	do { \
		statement(EAX, __VA_ARGS__); \
		statement(EBX, __VA_ARGS__); \
		statement(ECX, __VA_ARGS__); \
		statement(EDX, __VA_ARGS__); \
	} while(false)

void enableControls(HWND hDialog, UINT message, UINT control) {
	switch(message) {
	case BN_CLICKED:
		switch(control) {
		case IDC_TRIGGER_EAX:
		case IDC_TRIGGER_CUSTOM:
		{
			bool custom = control == IDC_TRIGGER_CUSTOM;
			EnableWindow(GetDlgItem(hDialog, IDC_TRIGGER_EAX_VALUE), custom ? FALSE : TRUE);
			EnableWindow(GetDlgItem(hDialog, IDC_TRIGGER_CUSTOM_VALUE), custom ? TRUE : FALSE);
			return;
		}
		case IDC_ACTION_VALUES:
		case IDC_ACTION_CUSTOM:
		{
			bool custom = control == IDC_ACTION_CUSTOM;
			FOR_EACH_REGISTER(ENABLE_SET_GROUP, custom ? FALSE : TRUE);
			EnableWindow(GetDlgItem(hDialog, IDC_ACTION_CUSTOM_VALUE), custom ? TRUE : FALSE);
			return;
		}
		FOR_EACH_REGISTER(ENABLE_ON_CLICKED_SET);
		}
	}
}

template<int maxLength>
class StringBuilder {
	WCHAR string[maxLength] = {};
	int position = 0;

public:
	void putChar(WCHAR c) {
		if(position < maxLength)
			string[position++] = c;
	}

	void putString(LPCWSTR str) {
		for(int i = 0; str[i] != L'\0'; i++)
			putChar(str[i]);
	}

	void putWindowText(HWND hWnd) {
		position += GetWindowTextW(hWnd, string + position, maxLength - position);
	}

	LPCWSTR getString() const {
		return string;
	}
};

void fillCustomTrigger(HWND hDialog) {
	StringBuilder<32> customValue;

	HWND hValue = GetDlgItem(hDialog, IDC_TRIGGER_EAX_VALUE);

	if(GetWindowTextLengthW(hValue) > 0) {
		customValue.putString(L"eax == ");
		customValue.putWindowText(GetDlgItem(hDialog, IDC_TRIGGER_EAX_VALUE));
	}
	SetWindowTextW(GetDlgItem(hDialog, IDC_TRIGGER_CUSTOM_VALUE), customValue.getString());
}

template<int maxLength>
void putCustomAction(StringBuilder<maxLength>* customValue, HWND hDialog, LPCWSTR name, UINT set, UINT mask, UINT value) {
	if(!IsDlgButtonChecked(hDialog, set))
	   return;
	HWND hMask = GetDlgItem(hDialog, mask);
	HWND hValue = GetDlgItem(hDialog, value);
	int maskLength = GetWindowTextLengthW(hMask);
	int valueLength = GetWindowTextLengthW(hValue);
	if(maskLength == 0 && valueLength == 0)
		return;
	customValue->putString(name);
	customValue->putString(L" = ");
	if(maskLength > 0) {
		if(valueLength > 0)
			customValue->putChar(L'(');

		customValue->putString(name);
		customValue->putString(L" & ~");
		customValue->putWindowText(hMask);

		if(valueLength > 0)
			customValue->putString(L") | ");
	}
	customValue->putWindowText(hValue);
	customValue->putString(L"; ");
}

#define PUT_CUSTOM_ACTION(register, registerName) \
	putCustomAction(&customValue, hDialog, registerName, IDC_SET_ ## register, IDC_SET_ ## register ## _MASK, IDC_SET_ ## register ## _VALUE)

void fillCustomAction(HWND hDialog) {
	StringBuilder<256> customValue;
	PUT_CUSTOM_ACTION(EAX, L"eax");
	PUT_CUSTOM_ACTION(EBX, L"ebx");
	PUT_CUSTOM_ACTION(ECX, L"ecx");
	PUT_CUSTOM_ACTION(EDX, L"edx");
	SetWindowTextW(GetDlgItem(hDialog, IDC_ACTION_CUSTOM_VALUE), customValue.getString());
}

#define FILL_ON_CHANGED(register) \
	case IDC_SET_ ## register ## _MASK: \
	case IDC_SET_ ## register ## _VALUE: \
		fillCustomAction(hDialog); \
		return

#define FILL_ON_CLICKED_SET(register) \
	case IDC_SET_ ## register: \
		fillCustomAction(hDialog); \
		return

void fillCustoms(HWND hDialog, UINT message, UINT control) {
	switch(message) {
	case EN_CHANGE:
		switch(control) {
		case IDC_TRIGGER_EAX_VALUE:
			fillCustomTrigger(hDialog);
			return;
			FOR_EACH_REGISTER(FILL_ON_CHANGED);
		}
	case BN_CLICKED:
		switch(control) {
		case IDC_TRIGGER_EAX:
			fillCustomTrigger(hDialog);
			return;
		case IDC_ACTION_VALUES:
			fillCustomAction(hDialog);
			FOR_EACH_REGISTER(FILL_ON_CLICKED_SET);
		}
	}
}

bool dirty = false;
int listIndex = CB_ERR;

std::wstring getSelectedPresetName(HWND hDialog) {
	std::wstring presetName;
	if(listIndex == CB_ERR)
		return presetName;
	HWND list = GetDlgItem(hDialog, IDC_LIST);
	presetName.resize(SendMessageW(list, LB_GETTEXTLEN, listIndex, 0));
	SendMessageW(list, LB_GETTEXT, listIndex, (LPARAM)presetName.data());
	return presetName;
}

#define LOAD_PRESET_REGISTERS(register) \
	do { \
		SendMessageW(GetDlgItem(hDialog, IDC_SET_ ## register), BM_SETCHECK, preset.actionRegisters[regIndex] ? BST_CHECKED : BST_UNCHECKED, 0); \
		SetWindowTextW(GetDlgItem(hDialog, IDC_SET_ ## register ## _MASK), preset.actionMasks[regIndex].c_str()); \
		SetWindowTextW(GetDlgItem(hDialog, IDC_SET_ ## register ## _VALUE), preset.actionValues[regIndex].c_str()); \
		regIndex++; \
	} while(false)

void fillFromPreset(HWND hDialog, const Preset& preset) {
	SetWindowTextW(GetDlgItem(hDialog, IDC_NAME), preset.name.c_str());
	SendMessageW(GetDlgItem(hDialog, IDC_ENABLED), BM_SETCHECK, preset.isEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

	SetWindowTextW(GetDlgItem(hDialog, IDC_TRIGGER_EAX_VALUE), preset.triggerEaxValue.c_str());
	SetWindowTextW(GetDlgItem(hDialog, IDC_TRIGGER_CUSTOM_VALUE), preset.triggerCustomValue.c_str());

	int regIndex = 0;
	FOR_EACH_REGISTER(LOAD_PRESET_REGISTERS);
	SetWindowTextW(GetDlgItem(hDialog, IDC_ACTION_CUSTOM_VALUE), preset.actionCustomValue.c_str());

	SendMessageW(GetDlgItem(hDialog, preset.isTriggerCustom ? IDC_TRIGGER_CUSTOM : IDC_TRIGGER_EAX), BM_CLICK, 0, 0);
	SendMessageW(GetDlgItem(hDialog, preset.isActionCustom ? IDC_ACTION_CUSTOM : IDC_ACTION_VALUES), BM_CLICK, 0, 0);
	SetFocus(GetDlgItem(hDialog, IDC_LIST));

	dirty = false;
}

#define SAVE_PRESET_REGISTERS(register) \
	do { \
		preset.actionRegisters[regIndex] = SendMessageW(GetDlgItem(hDialog, IDC_SET_ ## register), BM_GETCHECK, 0, 0) == BST_CHECKED; \
		preset.actionMasks[regIndex] = getWindowTextString(GetDlgItem(hDialog, IDC_SET_ ## register ## _MASK)); \
		preset.actionValues[regIndex] = getWindowTextString(GetDlgItem(hDialog, IDC_SET_ ## register ## _VALUE)); \
		regIndex++; \
	} while(false)

void storeDialogToPreset(HWND hDialog) {
	Preset preset;
	preset.name = getWindowTextString(GetDlgItem(hDialog, IDC_NAME));
	if(preset.name.empty()) {
		MessageBoxW(hDialog, L"Name must be set!", L"Empty name", MB_OK);
		return;
	}
	preset.isEnabled = SendMessageW(GetDlgItem(hDialog, IDC_ENABLED), BM_GETCHECK, 0, 0) == BST_CHECKED;

	preset.isTriggerCustom = SendMessageW(GetDlgItem(hDialog, IDC_TRIGGER_CUSTOM), BM_GETCHECK, 0, 0) == BST_CHECKED;
	preset.triggerEaxValue = getWindowTextString(GetDlgItem(hDialog, IDC_TRIGGER_EAX_VALUE));
	preset.triggerCustomValue = getWindowTextString(GetDlgItem(hDialog, IDC_TRIGGER_CUSTOM_VALUE));

	preset.isActionCustom = SendMessageW(GetDlgItem(hDialog, IDC_ACTION_CUSTOM), BM_GETCHECK, 0, 0) == BST_CHECKED;
	int regIndex = 0;
	FOR_EACH_REGISTER(SAVE_PRESET_REGISTERS);
	preset.actionCustomValue = getWindowTextString(GetDlgItem(hDialog, IDC_ACTION_CUSTOM_VALUE));
	setPreset(preset);

	HWND list = GetDlgItem(hDialog, IDC_LIST);
	listIndex = (int)SendMessageW(list, LB_FINDSTRINGEXACT, -1, (LPARAM)preset.name.c_str());
	if(listIndex == LB_ERR) {
		listIndex = (int)SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)preset.name.c_str());
	}
	SendMessageW(list, LB_SETCURSEL, listIndex, 0);

	dirty = false;
}

void deletePresetFromList(HWND hDialog) {
	if(listIndex == LB_ERR)
		return;

	if(MessageBoxW(hDialog, L"Are you sure you want to delete this preset?", L"Delete this preset", MB_YESNO) == IDNO)
		return;

	if(!deletePreset(getSelectedPresetName(hDialog)))
		return;

	SendMessageW(GetDlgItem(hDialog, IDC_LIST), LB_DELETESTRING, listIndex, 0);
	listIndex = CB_ERR;

	//clear the form
	fillFromPreset(hDialog, Preset());

	dirty = false;
}

bool checkForChanges(HWND hDialog) {
	if(!dirty)
		return true;
	return MessageBoxW(hDialog, L"This preset contains unsaved changes. Do you want to discard these changes?", L"Discard changes?", MB_OKCANCEL) == IDOK;
}

void onControlMessage(HWND hDialog, UINT message, UINT control) {
	//cancel button
	if(control == IDCANCEL) {
		if(checkForChanges(hDialog))
			EndDialog(hDialog, NULL);
		return;
	}
	
	//ok button
	if(control == IDOK) {
		storeDialogToPreset(hDialog);
		EndDialog(hDialog, NULL);
		return;
	}

	//apply button
	if(control == IDC_APPLY) {
		storeDialogToPreset(hDialog);
		return;
	}

	//delete button
	if(control == IDC_DELETE) {
		deletePresetFromList(hDialog);
		return;
	}

	//list control action
	if(control == IDC_LIST) {
		if(message == LBN_SELCHANGE) {
			HWND list = GetDlgItem(hDialog, IDC_LIST);
			if(!checkForChanges(hDialog)) {
				SendMessageW(list, LB_SETCURSEL, listIndex, 0);
				return;
			}

			listIndex = (int)SendMessageW(list, LB_GETCURSEL, 0, 0);
			auto preset = getPreset(getSelectedPresetName(hDialog));
			fillFromPreset(hDialog, preset);
		}
		return;
	}

	if(message == BN_CLICKED || message == EN_CHANGE) {
		dirty = true;
	}

	//other control action
	enableControls(hDialog, message, control);
	fillCustoms(hDialog, message, control);
}

void initDialog(HWND hDialog) {
	//center the dialog
	{
		RECT dialogRect;
		GetWindowRect(hDialog, &dialogRect);
		HWND parent = GetParent(hDialog);
		RECT parentRect;
		GetWindowRect(parent, &parentRect);

		int x = ((parentRect.right - parentRect.left) - (dialogRect.right - dialogRect.left)) / 2 + parentRect.left;
		int y = ((parentRect.bottom - parentRect.top) - (dialogRect.bottom - dialogRect.top)) / 2 + parentRect.top;
		SetWindowPos(hDialog, 0, x, y, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
	}

	{
		auto font = (HFONT)GetStockObject(ANSI_FIXED_FONT);
		HWND ignoreControl = GetDlgItem(hDialog, IDC_NAME);
		HWND hControl = NULL;
		while(true) {
			hControl = FindWindowExW(hDialog, hControl, L"Edit", NULL);
			if(hControl == NULL)
				break;
			if(hControl == ignoreControl)
				continue;
			SendMessageW(hControl, WM_SETFONT, (WPARAM)font, 0);
		}
	}

	HWND list = GetDlgItem(hDialog, IDC_LIST);
	for(const auto& presetName : getPresetNames()) {
		SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)presetName.c_str());
	}
	listIndex = (int)SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)L"(New preset)");
	SendMessageW(list, LB_SETCURSEL, listIndex, 0);

	//clear the form
	fillFromPreset(hDialog, Preset());
}

INT_PTR CALLBACK onDialogMessage(HWND hDialog, UINT message, WPARAM wParam, LPARAM lParam) {
	switch(message) {
	case WM_INITDIALOG:
		initDialog(hDialog);
		return TRUE;
	case WM_COMMAND:
		onControlMessage(hDialog, HIWORD(wParam), LOWORD(wParam));
		return TRUE;
	case WM_CLOSE:
		if(checkForChanges(hDialog))
			EndDialog(hDialog, NULL);
		return TRUE;
	}
	return FALSE;
}

HINSTANCE hInst;

void showPresetsDialog(HWND hWndParent) {
	dirty = false;
	DialogBoxW(hInst, MAKEINTRESOURCE(IDD_PRESETS), hWndParent, &onDialogMessage);
	;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
	if(fdwReason == DLL_PROCESS_ATTACH) {
		hInst = hinstDLL;

		WCHAR pluginPath[MAX_PATH];
		DWORD pluginPathLen = GetModuleFileNameW(hInst, pluginPath, MAX_PATH);
		if(pluginPathLen == 0)
			return FALSE;

		WCHAR* presetsFile = pluginPath;
		int presetsFileLen = pluginPathLen;
		while(presetsFile[presetsFileLen] != L'.' && presetsFileLen)
			presetsFileLen--;

		if(!presetsFileLen)
			return FALSE;

		presetsFile[presetsFileLen++] = L'.';
		presetsFile[presetsFileLen++] = L'd';
		presetsFile[presetsFileLen++] = L'a';
		presetsFile[presetsFileLen++] = L't';
		presetsFile[presetsFileLen++] = L'\0';
		loadPresets(presetsFile);
	}
	return TRUE;
}
