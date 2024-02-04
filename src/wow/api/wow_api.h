#ifndef MIMIRON_WOW_API_WOW_API_H_
#define MIMIRON_WOW_API_WOW_API_H_

#include <string>

namespace mimiron::wow {

struct api_region {
	int64_t id;
	std::string_view name;
	std::string_view code;
	std::string_view url;
};

struct api_regions {
	static constexpr auto all = std::to_array<api_region>({
		{ 0, "North America", "us", "https://us.api.blizzard.com" },
		{ 1, "Europe", "eu", "https://eu.api.blizzard.com" },
		{ 2, "South Korea", "kr", "https://kr.api.blizzard.com" },
		{ 3, "Taiwan", "tw", "https://tw.api.blizzard.com" },
		{ 4, "China", "cn", "https://gateway.battlenet.com.cn" }
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
	std::string scope;
};

struct api_link {
	std::string href;
};

enum class game_version {
	wow_retail,
	wow_progression,
	wow_classic_era
};

using enum game_version;

enum class api_namespace {
	api_static,
	api_dynamic,
	api_profile
};

using enum api_namespace;

constexpr std::string resource_namespace(api_region const& region, game_version version, api_namespace n) {
	std::string str;

	str.reserve(64);
	switch (n) {
		case api_static:
			str = "static";
			break;

		case api_dynamic:
			str = "dynamic";
			break;

		case api_profile:
			str = "profile";
			break;
	}

	switch (version) {
		case wow_retail:
			break;

		case wow_progression:
			str += "-classic";
			break;

		case wow_classic_era:
			str += "-classic1x";
			break;
	}

	str.push_back('-');
	str += region.code;
	return str;
}

}

#endif /* MIMIRON_WOW_API_WOW_API_H_ */
