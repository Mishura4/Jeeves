#pragma once

#include <string_view>
#include <vector>
#include <memory>
#include <span>

#include "exception.h"
#include "wow/guild.h"
#include "wow/character.h"

namespace mimiron {

class invalid_lua_exception : public exception {
	using exception::exception;
	using exception::operator=;
};

namespace wow {

class guildbook_data {
public:
	static guildbook_data parse(std::string_view lua);

	void add_guild(guild const &g);
	void add_guild(guild &&g);

	void add_character(character const &g);
	void add_character(character &&g);

	std::span<guild const> get_guilds() const noexcept;

private:
	std::vector<character> _characters;
	std::vector<guild> _guilds;
};

}

} // namespace mimiron
