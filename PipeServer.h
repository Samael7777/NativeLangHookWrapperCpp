#pragma once
#include <functional>
#include <thread>
#include <windows.h>


// ReSharper disable IdentifierTypo
// ReSharper disable CppInconsistentNaming

enum
{
	CONNECTING_STATE = 0,
	READING_STATE = 1,
	CLOSING_STATE = 2,

	INSTANCES = 1,
	PIPE_TIMEOUT = 5000,
	BUFSIZE = 512
};

typedef struct
{
	OVERLAPPED Overlap;
	HANDLE PipeInst;
	BYTE ReadBuffer[BUFSIZE];
	DWORD Read;
	BYTE WriteBuffer[BUFSIZE];
	DWORD ToWrite;
	DWORD State;
	BOOL IsPendingIO;
} PIPEINST, *LPPIPEINST;

class PipeServer
{
public:
	explicit PipeServer(const std::wstring& pipeName);
	PipeServer() = delete;
	PipeServer(const PipeServer &ps) = delete;
	~PipeServer();

	void Send(const void* buffer, int len);
	void setOnReadCallback(const std::function<void(const BYTE*, int)>& callback);
	void setOnDisconnectCallback(const std::function<void()>& callback);
	bool IsConnected() const;

private:
	PIPEINST _pipe;
	std::wstring _pipeName;
	std::thread _receiverThread;
	std::function<void(const BYTE*, int)> _onReadCallback;
	std::function<void()> _onDisconnectCallback;

	void InitPipe();
	void OnDisconnect() const;
	void ConnectToNewClient();
	void ReadPending();
	void ReceiveTask(PIPEINST* pipe);
	void OnRead() const;
};
