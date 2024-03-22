#pragma once

#include <string_view>
#include <source_location>

namespace mimiron {

class exception : public std::exception {
public:
	exception() = default;
	exception(const std::string &exception);
	exception(std::string &&exception);

	const char *what() const noexcept override;

private:
	std::string _message;
};

struct internal_exception : exception {
public:
	internal_exception(std::source_location loc = std::source_location::current());
	internal_exception(const std::string &xception, std::source_location loc = std::source_location::current());
	internal_exception(std::string &&exception, std::source_location loc = std::source_location::current());

	const std::source_location &where() const noexcept;
	std::string format() const;

private:
	std::source_location _where;
};

}
