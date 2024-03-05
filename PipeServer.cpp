#include <format>

#include "PipeServer.h"

#include "error_code_exception.h"

// ReSharper disable IdentifierTypo
// ReSharper disable CppInconsistentNaming
// ReSharper disable CommentTypo

const std::wstring PipeNamePrefix = L"\\\\.\\pipe\\";

PipeServer::PipeServer(const std::wstring& pipeName)
{
	_pipeName = PipeNamePrefix + pipeName;

	InitPipe();
	ConnectToNewClient();
	_receiverThread = std::thread(&PipeServer::ReceiveTask, this, &_pipe);
}

void PipeServer::InitPipe()
{
	_pipe.Overlap.Offset = 0;
	_pipe.Overlap.OffsetHigh = 0;

	_pipe.Overlap.hEvent = CreateEvent(
		nullptr, // default security attribute 
		TRUE, // manual-reset event 
		TRUE, // initial state = signaled 
		nullptr); // unnamed event object 

	if (_pipe.Overlap.hEvent == nullptr)
		throw error_code_exception("CreateEvent failed.", static_cast<int>(GetLastError()));

	_pipe.PipeInst = CreateNamedPipe(
		_pipeName.c_str(),						// pipe name 
		PIPE_ACCESS_DUPLEX |					// read/write access 
		FILE_FLAG_OVERLAPPED,					// overlapped mode 
		PIPE_TYPE_MESSAGE |						// message-type pipe 
		PIPE_READMODE_MESSAGE |					// message-read mode 
		PIPE_WAIT,								// blocking mode 
		INSTANCES,								// number of instances 
		BUFSIZE,								// output buffer size 
		BUFSIZE,								// input buffer size 
		PIPE_TIMEOUT,							// client time-out 
		nullptr);				// default security attributes 

	if (_pipe.PipeInst == INVALID_HANDLE_VALUE)
		throw error_code_exception("CreateNamedPipe failed.", static_cast<int>(GetLastError()));
}

// This function is called to start an overlapped connect operation. 
// It returns TRUE if an operation is pending or FALSE if the 
// connection has been completed. 
void PipeServer::ConnectToNewClient()
{
	_pipe.IsPendingIO = false;

	// Start an overlapped connection for this pipe instance
	// Overlapped ConnectNamedPipe should return zero. 
	if (ConnectNamedPipe(_pipe.PipeInst, &_pipe.Overlap))
		throw error_code_exception("ConnectNamedPipe failed.", static_cast<int>(GetLastError()));

	switch (GetLastError())
	{
	// The overlapped connection in progress. 
	case ERROR_IO_PENDING:
		_pipe.IsPendingIO = true;
		break;

	// Client is already connected, so signal an event. 
	case ERROR_PIPE_CONNECTED:
		if (SetEvent(_pipe.Overlap.hEvent))	break;
		_FALLTHROUGH;
	// If an error occurs during the connect operation...
	default:
		throw error_code_exception("ConnectNamedPipe failed.", static_cast<int>(GetLastError()));
	}

	_pipe.State = _pipe.IsPendingIO
		? CONNECTING_STATE // still connecting
		: READING_STATE; // ready to read
}

void PipeServer::Send(const void* buffer, const int len)
{
	DWORD bytesTransfered;

	if (_pipe.State != READING_STATE) return;

	reinterpret_cast<int*>(_pipe.WriteBuffer)[0] = len;
	memcpy(&_pipe.WriteBuffer[sizeof(int)], buffer, len);
	_pipe.ToWrite = len + sizeof(int);

	const auto isSuccess = WriteFile(
				_pipe.PipeInst,
				_pipe.WriteBuffer,
				_pipe.ToWrite,
				&bytesTransfered,
				nullptr);

	if (!isSuccess)
		throw error_code_exception("Send data failed.", static_cast<int>(GetLastError()));
}

void PipeServer::setOnReadCallback(const std::function<void(const BYTE*, int)>& callback)
{
	_onReadCallback = callback;
}

void PipeServer::setOnDisconnectCallback(const std::function<void()>& callback)
{
	_onDisconnectCallback = callback;
}

bool PipeServer::IsConnected() const
{
	return _pipe.State != CONNECTING_STATE;
}

// DisconnectAndReconnect(DWORD) 
// This function is called when an error occurs or when the client 
// closes its handle to the pipe.
void PipeServer::OnDisconnect() const
{
	// Disconnect the pipe instance. 
	if (!DisconnectNamedPipe(_pipe.PipeInst))
		throw error_code_exception("DisconnectNamedPipe failed.", static_cast<int>(GetLastError()));

	if (_onDisconnectCallback != nullptr)
		_onDisconnectCallback();
}

// The pipe state determines which operation to do next.
void PipeServer::ReadPending()
{
	const BOOL isSuccess = ReadFile(
		_pipe.PipeInst,
		_pipe.ReadBuffer,
		BUFSIZE * sizeof(TCHAR),
		&_pipe.Read,
		&_pipe.Overlap);

	// The read operation completed successfully. 
	if (isSuccess && _pipe.Read != 0)
	{
		_pipe.IsPendingIO = false;
		_pipe.State = READING_STATE;
		return;
	}

	// The read operation is still pending. 
	if (!isSuccess && (GetLastError() == ERROR_IO_PENDING))
	{
		_pipe.IsPendingIO = true;
		return;
	}

	// An error occurred; disconnect from the client. 
	OnDisconnect();
}

void PipeServer::ReceiveTask(PIPEINST* pipe)
{
	DWORD bytesTransfered;
	bool isRunning = true;

	while (isRunning)
	{
		// Wait for the event object to be signaled, indicating 
		// completion of an overlapped read, write, or 
		// connect operation. 

		WaitForSingleObject(pipe->Overlap.hEvent,INFINITE);

		// Get the result if the operation was pending. 

		if (pipe->IsPendingIO)
		{
			const auto isSuccess = GetOverlappedResult(
				pipe->PipeInst, // handle to pipe 
				&pipe->Overlap, // OVERLAPPED structure 
				&bytesTransfered, // bytes transferred 
				false); // do not wait 

			switch (pipe->State)
			{

			case CLOSING_STATE:
				isRunning = false;
				continue;

			// Pending connect operation 
			case CONNECTING_STATE:
				if (!isSuccess)	
				{
					throw error_code_exception("Receive failed.", static_cast<int>(GetLastError()));
				}
				pipe->State = READING_STATE;
				break;

			// Pending read operation 
			case READING_STATE:
				if (!isSuccess || bytesTransfered == 0)
				{
					OnDisconnect();
					isRunning = false;
					continue;
				}
				pipe->Read = bytesTransfered;
				pipe->State = READING_STATE;
				OnRead();
				break;

			default:
				throw error_code_exception("Invalid pipe state.", -1);
			}
		}

		ReadPending();
	}
}

void PipeServer::OnRead() const
{
	const auto msgLen = *reinterpret_cast<const int*>(_pipe.ReadBuffer);
	const auto request = _pipe.ReadBuffer + sizeof(int);
	if (_onReadCallback != nullptr)
		_onReadCallback(request, msgLen);
}

PipeServer::~PipeServer()
{
	_pipe.State = CLOSING_STATE;

	if (_pipe.Overlap.hEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_pipe.Overlap.hEvent);
	}

	if (_pipe.PipeInst != INVALID_HANDLE_VALUE)
	{
		CancelIoEx(_pipe.PipeInst, &_pipe.Overlap);
		_receiverThread.join(); //Wait for receiver thread exits.
		CloseHandle(_pipe.PipeInst);
	}	
}