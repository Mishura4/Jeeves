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

using std::chrono::system_clock;

using discord_duration = std::chrono::duration<double>;
using discord_timestamp = std::chrono::time_point<system_clock, discord_duration>;

inline constexpr auto discord_epoch = std::chrono::time_point_cast<discord_duration>(std::chrono::sys_days{std::chrono::year{2015}/1/1});

constexpr discord_timestamp discord_time(double time) {
	return discord_timestamp{discord_duration{time}};
}

constexpr double discord_time(discord_timestamp const& time) {
	return time.time_since_epoch().count();
}

constexpr discord_timestamp discord_time() {
	return std::chrono::time_point_cast<discord_duration>(system_clock::now());
}

using std::chrono::nanoseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::minutes;
using std::chrono::hours;

}

#endif /* MIMIRON_COMMON_H_ */
