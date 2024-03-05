// ReSharper disable CppClangTidyClangDiagnosticCastFunctionTypeStrict
#include "HookControl.h"

#include "error_code_exception.h"

const auto SetLangHookProcName = "SetLangHook";
const auto GetLayoutChangedMessageCodeProcName = "GetLayoutChangedMessageCode";
const auto GetLayoutChangeRequestMessageCodeProcName = "GetLayoutChangeRequestMessageCode";
const auto DestroyLangHookProcName = "DestroyLangHook";

HookControl::HookControl(const std::wstring& hookLibName, HWND messageWindow)
{
	_hookLibHandle = LoadLibrary(hookLibName.c_str());
	if (_hookLibHandle == INVALID_HANDLE_VALUE)
		throw error_code_exception("Error loading hook dll.", static_cast<int>(GetLastError()));

	const auto getLayoutChangedMessageCodeProc = reinterpret_cast<GetMsgCodeProc>
		(GetProcAddress(_hookLibHandle, GetLayoutChangedMessageCodeProcName));

	const auto getLayoutChangeRequestMessageCodeProc = reinterpret_cast<GetMsgCodeProc>
		(GetProcAddress(_hookLibHandle, GetLayoutChangeRequestMessageCodeProcName));

	const auto setLangHookProc = reinterpret_cast<SetLangHookProc>
		(GetProcAddress(_hookLibHandle, SetLangHookProcName));

	_destroyLangHookProc = reinterpret_cast<DestroyHookProc>
		(GetProcAddress(_hookLibHandle,  DestroyLangHookProcName));

	_layoutChangedMessageCode = getLayoutChangedMessageCodeProc();
	_layoutChangeRequestMessageCode = getLayoutChangeRequestMessageCodeProc();

	_hook = setLangHookProc(messageWindow);
	if (_hook == nullptr)
		throw error_code_exception("Error installing hook.", static_cast<int>(GetLastError()));
}

void HookControl::ChangeLayoutRequest(HWND hWnd, int klId, int hkl) const
{
	SendMessage(hWnd, _layoutChangeRequestMessageCode, klId, hkl);
}

UINT HookControl::getLayoutChangedMessageCode() const
{
	return _layoutChangedMessageCode;
}

HookControl::~HookControl()
{
	UnhookWindowsHookEx(_hook);
	FreeLibrary(_hookLibHandle);
}