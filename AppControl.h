// ReSharper disable CppClangTidyCppcoreguidelinesSpecialMemberFunctions
// ReSharper disable CppInconsistentNaming

#pragma once

#include <string>
#include <Windows.h>

class AppControl
{
public:
	AppControl(std::wstring appId);
	~AppControl();
	AppControl(const AppControl &ac) = delete;
	bool IsUniqueInstance() const;
	void SetInitComplete() const;
	void WaitForExitCommand() const;
	void ExitApp() const;

private:
	std::wstring _appId;
	HANDLE _exitEvent;
	HANDLE _initEvent;
	HANDLE _appMutex;
};
