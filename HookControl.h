#pragma once
#include <string>
#include <Windows.h>

class HookControl
{
public:
	HookControl(const std::wstring& hookLibName, HWND messageWindow);
	~HookControl();
	HookControl(const HookControl &hc) = delete;
	void ChangeLayoutRequest(HWND hWnd, int klId, int hkl) const;
	UINT getLayoutChangedMessageCode() const;

private:
	typedef HHOOK (*SetLangHookProc)(HWND);
	typedef UINT (*GetMsgCodeProc)();
	typedef bool (*DestroyHookProc)();
		
	UINT _layoutChangedMessageCode;
	UINT _layoutChangeRequestMessageCode;
	HHOOK _hook;
	HMODULE _hookLibHandle;
	DestroyHookProc _destroyLangHookProc;
};
