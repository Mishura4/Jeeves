#ifndef MIMIRON_WOW_API_WOW_API_H_
#define MIMIRON_WOW_API_WOW_API_H_

#include <string>

#include "common.h"
#include "tools/string_literal.h"

namespace mimiron::wow {

struct api_region;

enum class game_version {
	retail,
	progression,
	classic_era
};

using enum game_version;

enum class api_namespace {
	api_static,
	api_dynamic,
	api_profile
};

using enum api_namespace;

struct resource_location {
	std::string     host;
	string_literal<2> region_code;
	game_version    version;
};

struct api_region {
	int64_t id;
	fixed_string<16> name;
	string_literal<2> code;
	fixed_string<40> url;

	constexpr resource_location operator[](game_version version) const {
		return {
			.host = std::string{url},
			.region_code = code,
			.version = version
		};
	}
};

constexpr fixed_string foo = "foo";

constexpr api_region bar = { 0, "hi", "ee", "baba"};

struct api_regions {
	static constexpr auto all = std::to_array<api_region>({
		api_region{ 0, "North America", "us", "https://us.api.blizzard.com" },
		api_region{ 1, "Europe", "eu", "https://eu.api.blizzard.com" },
		api_region{ 2, "South Korea", "kr", "https://kr.api.blizzard.com" },
		api_region{ 3, "Taiwan", "tw", "https://tw.api.blizzard.com" },
		api_region{ 4, "China", "cn", "https://gateway.battlenet.com.cn" }
	});

	static constexpr api_region const& north_america = all[0];
	static constexpr api_region const& europe = all[1];
	static constexpr api_region const& south_korea = all[2];
	static constexpr api_region const& taiwan = all[3];
	static constexpr api_region const& china = all[4];
};

struct client_credentials {
	std::string access_token;
	std::string token_type;
	uint64_t expires_in;
	std::optional<std::string> scope;
};

struct api_link {
	std::string href;
};

}

#endif /* MIMIRON_WOW_API_WOW_API_H_ */
