#include "exception.h"

#include <format>

using namespace mimiron;

exception::exception(const std::string &exception) : _message{exception}
{}

exception::exception(std::string &&exception) : _message{std::move(exception)}
{}

const char *exception::what() const noexcept {
	return (_message.c_str());
}

internal_exception::internal_exception(std::source_location loc) : _where{std::move(loc)}
{}


internal_exception::internal_exception(const std::string &exception, std::source_location loc) :
	exception(exception),
	_where{std::move(loc)}
{}

internal_exception::internal_exception(std::string &&exception, std::source_location loc) :
	exception(std::move(exception)),
	_where{std::move(loc)}
{}

const std::source_location& internal_exception::where() const noexcept {
	return (_where);
}

std::string internal_exception::format() const {
	return (std::format(
		"in {}\n"
		"\tat {} line {}:\n"
		"\t{}",
		_where.file_name(),
		_where.function_name(),
		_where.line(),
		what()
	));
}

