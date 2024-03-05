// ReSharper disable CppClangTidyClangDiagnosticReservedIdentifier
// ReSharper disable CppClangTidyBugproneReservedIdentifier
#include "AppControl.h"

const std::wstring _appMutexNameSuffix= L"Mutex";
const std::wstring _exitEventNameSuffix = L"ExitEvent";
const std::wstring _initEventNameSuffix = L"InitEvent";

AppControl::AppControl(std::wstring appId)
{
	_appId = std::move(appId);

	_initEvent = CreateEvent(nullptr, true, false, 
		(_appId + _initEventNameSuffix).c_str());
	if (_initEvent == INVALID_HANDLE_VALUE)
		throw std::exception("Error creating init event.", static_cast<int>(GetLastError()));

	_exitEvent = CreateEvent(nullptr, true, false, 
		(_appId + _exitEventNameSuffix).c_str());
	if (_initEvent == INVALID_HANDLE_VALUE)
		throw std::exception("Error creating exit event.", static_cast<int>(GetLastError()));

	_appMutex = CreateMutex(nullptr, true, (_appId + _appMutexNameSuffix).c_str());
	if (_appMutex == INVALID_HANDLE_VALUE)
		throw std::exception("Error creating mutex.", static_cast<int>(GetLastError()));
}

bool AppControl::IsUniqueInstance() const
{
	return WaitForSingleObject(_appMutex, 0) == WAIT_OBJECT_0;
}

void AppControl::SetInitComplete() const
{
	SetEvent(_initEvent);
}

void AppControl::WaitForExitCommand() const
{
	WaitForSingleObject(_exitEvent, INFINITE);
}

void AppControl::ExitApp() const
{
	SetEvent(_exitEvent);
}

AppControl::~AppControl()
{
	CloseHandle(_appMutex);
	CloseHandle(_exitEvent);
}