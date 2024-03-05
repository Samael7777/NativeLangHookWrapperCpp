#pragma once
#include <exception>

class error_code_exception final : public std::exception
{
public:
	error_code_exception(const char* message, int code);
	int Code() const;
private:
	int _code;
};

inline error_code_exception::error_code_exception(const char* message, int code)
	: std::exception(message), _code{code}
{ }

inline int error_code_exception::Code() const
{
	return _code;
}
