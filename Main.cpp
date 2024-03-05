// ReSharper disable CppInconsistentNaming

// ReSharper disable CppClangTidyClangDiagnosticCastFunctionTypeStrict
#include <iostream>
#include <Windows.h>
#include "AppControl.h"
#include "error_code_exception.h"
#include "HookControl.h"
#include "MessageWindow.h"

enum Command
{
	Exit = 1,
    ChangeLayout = 2,
};

enum Response
{
	LayoutChanged = 1,
    Error = 2,
};

void MsgCaptureProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void OnDisconnect();
void OnDataReceived(const BYTE* buffer, const int len);
void SendCurrentLayout(UINT layout);
void ChangeLayoutCommand(const BYTE* buffer);
void SendOrPrintError(const char* message, int code);

const std::wstring AppId = L"NativeLangHookWrapper";
const std::wstring PipeName = AppId + L"IPC";
const std::wstring HookLibName = L"NativeLangHook_x86";

PipeServer* pPipeServer;
MessageWindow* pMessageWindow;
HookControl* pHookControl;
AppControl* pAppControl;

UINT layoutChangedMessageCode;

BYTE sendCurrentLayoutBuffer[sizeof(int) * 2];
constexpr int layoutChangedResponse = LayoutChanged;
constexpr int errorResponse = Error;

bool isRunning;

char msgBuffer[256];

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	//init layout changed buffer
	memcpy(sendCurrentLayoutBuffer, &layoutChangedResponse, sizeof(int));

	//init app
	try
	{
		AppControl appControl(AppId);
		pAppControl = &appControl;

		if (!appControl.IsUniqueInstance())
		{
			SendOrPrintError("Another app instance running. Exiting.", -1);
			return 1;
		}

		PipeServer pipeServer(PipeName);
		pPipeServer = &pipeServer;
		pipeServer.setOnReadCallback(OnDataReceived);
		pipeServer.setOnDisconnectCallback(OnDisconnect);

		pMessageWindow = new MessageWindow();
		pMessageWindow->setMsgCaptureProc(MsgCaptureProc);

		HookControl hookControl(HookLibName, pMessageWindow->getHandle());
		pHookControl = &hookControl;
		layoutChangedMessageCode = hookControl.getLayoutChangedMessageCode();

		isRunning = true;

		appControl.SetInitComplete();

		appControl.WaitForExitCommand();
	}
	catch (error_code_exception& error)
	{
		SendOrPrintError(error.what(), error.Code());

		if (pAppControl == nullptr)
			return 1;

		pAppControl->ExitApp();
	}

	return 0;
}

void MsgCaptureProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == layoutChangedMessageCode)
	{
		SendCurrentLayout(static_cast<UINT>(lParam));
	}
}

void OnDataReceived(const BYTE* buffer, const int len)
{
	const auto command = *reinterpret_cast<const int*>(buffer);
	switch (command)
	{
	case Command::ChangeLayout:
		if (len == sizeof(int) * 4)
			ChangeLayoutCommand(reinterpret_cast<const BYTE*>(buffer) + sizeof(int));
		else
			SendOrPrintError("Incorrect command.", -1);
		break;
	case Command::Exit:
		isRunning = false;
		pAppControl->ExitApp();
		break;
	default:
		SendOrPrintError("Unknown command.", -1);
	}
}

void SendCurrentLayout(const UINT layout)
{
	constexpr auto bufSize = sizeof(int) * 2;
	memcpy(sendCurrentLayoutBuffer + sizeof(int), &layout, sizeof(int));
	pPipeServer->Send(sendCurrentLayoutBuffer, bufSize);
}

void ChangeLayoutCommand(const BYTE* buffer)
{
	const auto hWnd = *reinterpret_cast<const int*>(buffer);
	const auto klId = *reinterpret_cast<const int*>(buffer + sizeof(int));
	const auto hkl = *reinterpret_cast<const int*>(buffer + sizeof(int) * 2);
	pHookControl->ChangeLayoutRequest(reinterpret_cast<HWND>(hWnd), klId, hkl);  // NOLINT(performance-no-int-to-ptr)
}

void OnDisconnect()
{
	if (!isRunning) return;

	SendOrPrintError("Abnormal pipe disconnection.", -1);
	pAppControl->ExitApp();
}

void SendOrPrintError(const char* message, int code)
{
	sprintf_s(msgBuffer, "%s Error code: %#08x", message, code);
	MessageBoxA(nullptr, msgBuffer, "Error", MB_OK);

	if (pPipeServer == nullptr || !pPipeServer->IsConnected())
		return;

	const auto msgLen = strlen(message);
	const auto bufSize = msgLen + sizeof(int) * 2 + 1; //add a byte for string zero-termination

	const auto buffer = new BYTE[bufSize];

	//send error response, error code, error message
	memcpy(buffer, &errorResponse, sizeof(int));
	memcpy(buffer + sizeof(int), &code, sizeof(int));
	memcpy(buffer + sizeof(int) * 2, message, msgLen);
	buffer[bufSize - 1] = 0;
	pPipeServer->Send(buffer, static_cast<int>(bufSize));
	delete[] buffer;
}