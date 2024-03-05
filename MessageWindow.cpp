// ReSharper disable StringLiteralTypo
// ReSharper disable IdentifierTypo
// ReSharper disable CppClangTidyPerformanceNoIntToPtr
// ReSharper disable CppInconsistentNaming

#include "MessageWindow.h"
#include "error_code_exception.h"
#include <format>

constexpr auto WndClassName = L"{3EEEDD77}_MsgWindowClass";

MessageWindow::MessageWindow()
{
	_initEvent = CreateEvent(nullptr, true, false, nullptr);
	if (_initEvent == INVALID_HANDLE_VALUE)
		throw error_code_exception("Error creating event.", static_cast<int>(GetLastError()));

	InitWindowClass();

	_captureTask = std::thread(&MessageWindow::s_CaptureTaskProc, this);
}

void MessageWindow::InitWindowClass()
{
	_wndClass.cbSize = sizeof(tagWNDCLASSEXW);
	_wndClass.lpfnWndProc = s_WndProc;
	_wndClass.lpszClassName = WndClassName;
	_wndClass.hInstance = GetModuleHandle(nullptr);

	_wndClassHandle = RegisterClassEx(&_wndClass);
	if (_wndClassHandle == 0)
		throw error_code_exception("Error registering window class", static_cast<int>(GetLastError()));
}

void MessageWindow::s_CaptureTaskProc(void* instPtr)
{
	int error;
	MSG msg;
	const auto pThis = static_cast<MessageWindow*>(instPtr);

	pThis->_wndHandle = CreateWindow(
		WndClassName,
		nullptr,
		0,
		0, 0, 0, 0,
		HWND_MESSAGE,
		nullptr,
		pThis->_wndClass.hInstance,
		pThis);

	if (pThis->_wndHandle == INVALID_HANDLE_VALUE)
		throw error_code_exception("Error creating window.", static_cast<int>(GetLastError()));

	SetEvent(pThis->_initEvent);

	while ((error = GetMessage(&msg, nullptr, 0, 0)) != 0)
	{
		if (error == -1)
		{
			//Process error
			break;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	CloseHandle(pThis->_initEvent);
	UnregisterClass(WndClassName, pThis->_wndClass.hInstance);
}

LRESULT MessageWindow::s_WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MessageWindow *pThis;
	if (uMsg == WM_NCCREATE)
	{
		// Recover the "this" pointer which was passed as a parameter
		// to CreateWindow(Ex).
		const auto lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
		pThis = static_cast<MessageWindow*>(lpcs->lpCreateParams);

		// Put the value in a safe place for future use
		SetWindowLongPtr(hwnd, GWLP_USERDATA,  reinterpret_cast<LONG_PTR>(pThis));
	}
	else
	{
		pThis = reinterpret_cast<MessageWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}

	return pThis && pThis->_wndHandle
			? pThis->WndProc(hwnd, uMsg, wParam, lParam)
			: DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT MessageWindow::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) const
{
	switch (uMsg)
	{
	case WM_QUIT:
		DestroyWindow(_wndHandle);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		if (_captureCallback)
		{
			_captureCallback(hwnd, uMsg, wParam, lParam);	
		}
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


HWND MessageWindow::getHandle() const
{
	WaitForSingleObject(_initEvent, INFINITE);
	return _wndHandle;
}

void MessageWindow::setMsgCaptureProc(const std::function<void(HWND, unsigned, WPARAM, LPARAM)>& callback)
{
	_captureCallback = callback;
}

MessageWindow::~MessageWindow()
{
	SendMessage(_wndHandle, WM_QUIT, 0, 0);
	_captureTask.join(); // Wait for exiting message loop.
}