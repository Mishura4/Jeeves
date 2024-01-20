#pragma once

#include <string>
#include <vector>

#include <sol/table.hpp>

namespace mimiron::wow {

struct character {
	struct profession {
		using recipe_id = uint32_t;

		int id = -1;
		int level = -1;
		std::vector<recipe_id> recipes {};
	};

	static character from_guildbook(std::string name, const sol::table &data);

	std::string name = {};
	std::string guild = {};
	int class_id = -1;
	int race = -1;
	int gender = -1;
	profession prof_main{};
	profession prof_secondary{};
	profession prof_cooking{};
	profession prof_firstaid{};
	int fishing_level = -1;
};

}
