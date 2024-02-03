#pragma once

#include <string>
#include <vector>
#include <span>

#include <dpp/managed.h>
#include "tools/tools.h"
#include "wow/character.h"

namespace mimiron::wow {

class guild {
public:
	using cache = cache<dpp::snowflake, std::vector<guild>, std::hash<uint64_t>>;

	guild(dpp::snowflake snowflake, uint8_t wow_id, std::string name);

	const std::string &name() const noexcept;
	std::span<std::string const> members() const noexcept;
	uint8_t wow_id() const noexcept;
	dpp::snowflake discord_guild() const noexcept;

	void add_player(std::string_view player_name);
	void add_player(const character &chr);

private:
	dpp::snowflake _discord_guild;
	uint8_t _wow_guild_id;
	std::string _name;
	std::vector<std::string> _characters;
};

}
