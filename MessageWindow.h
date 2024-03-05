#pragma once
#include <functional>
#include <Windows.h>

#include "PipeServer.h"

class MessageWindow
{
public:
	MessageWindow();
	~MessageWindow();
	MessageWindow(const MessageWindow &mw) = delete;
	HWND getHandle() const;
	void setMsgCaptureProc(const std::function<void(HWND, unsigned int, WPARAM, LPARAM)>& callback);

private:
	HWND _wndHandle;
	HANDLE _initEvent;
	tagWNDCLASSEXW _wndClass;
	ATOM _wndClassHandle;
	std::thread _captureTask;
	std::function<void(HWND, UINT, WPARAM, LPARAM)> _captureCallback;

	static void s_CaptureTaskProc(void *instPtr);
	static LRESULT CALLBACK s_WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) const;
	void InitWindowClass();
};