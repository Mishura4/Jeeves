#ifndef MIMIRON_COMMON_H_
#define MIMIRON_COMMON_H_

#include <dpp/colors.h>

#include <chrono>

namespace mimiron {

using app_clock = std::chrono::steady_clock;
using app_timestamp = std::chrono::time_point<app_clock>;

inline constexpr auto mimiron_color = dpp::colors::rust;

inline void throw_if_error(dpp::confirmation_callback_t const& result) {
	if (result.is_error()) {
		throw dpp::rest_exception{result.get_error().human_readable};
	}
}

using namespace std::chrono_literals;

using std::chrono::nanoseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::minutes;
using std::chrono::hours;

}

#endif /* MIMIRON_COMMON_H_ */
