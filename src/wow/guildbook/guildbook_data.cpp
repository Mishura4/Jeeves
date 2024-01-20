#define SOL_ALL_SAFETIES_ON 1

#include "guildbook_data.h"

#include <sol/sol.hpp>
#include <csetjmp>

using namespace mimiron;
using namespace mimiron::wow;

namespace {

void fill_guild_data(guildbook_data &guildbook, std::string guild_name, const sol::table &guild_data) {
	guild g{std::move(guild_name)};

	sol::optional<sol::table> members = guild_data["members"];
	if (members.has_value()) {
		for (const auto &[key1, value1] : *members) {
			if (key1.is<std::string>()) {
				g.add_player(key1.as<std::string>());
			}
		}
	}
	guildbook.add_guild(std::move(g));
}

}

auto guildbook_data::parse(std::string_view lua) -> guildbook_data {
	sol::state lua_state{};
	sol::environment env{lua_state, sol::create};

	lua_state.set_exception_handler([](lua_State *L, sol::optional<const std::exception &> e, sol::string_view str) -> int {
		throw *e;
	});
	env["_G"] = env;
	auto result = lua_state.script(lua, env);
	sol::optional<sol::table> data = env["GUILDBOOK_GLOBAL"];
	if (!data.has_value()) {
		throw invalid_lua_exception{"invalid guildbook data"};
	}
	guildbook_data ret;
	sol::optional<sol::table> guilds = (*data)["guilds"];
	if (guilds.has_value()) {
		for (const auto &[key, value] : *guilds) {
			if (key.is<std::string>() && value.is<sol::table>()) {
				fill_guild_data(ret, key.as<std::string>(), value.as<sol::table>());
			}
		}
	}
	sol::optional<sol::table> characters = (*data)["characterDirectory"];
	if (characters.has_value()) {
		for (const auto &[key, value] : *characters) {
			if (key.is<std::string>() && value.is<sol::table>()) {
				ret.add_character(character::from_guildbook(key.as<std::string>(), value.as<sol::table>()));
			}
		}
	}
	return (ret);
}

void guildbook_data::add_guild(const guild &g) {
	auto where = std::ranges::lower_bound(_guilds, g.get_name(), std::less{}, &guild::get_name);

	if (where->get_name() == g.get_name()) {
		*where = g;
	} else {
		_guilds.emplace(where, g);
	}
}

void guildbook_data::add_guild(guild &&g) {
	auto where = std::ranges::lower_bound(_guilds, g.get_name(), std::less{}, &guild::get_name);

	if (where != _guilds.end() && where->get_name() == g.get_name()) {
		*where = std::move(g);
	} else {
		_guilds.emplace(where, std::move(g));
	}
}

void guildbook_data::add_character(const character &g) {
	auto where = std::ranges::lower_bound(_characters, g.name, std::less{}, &character::name);

	if (where != _characters.end() && where->name == g.name) {
		*where = g;
	} else {
		_characters.emplace(where, g);
	}
}

void guildbook_data::add_character(character &&g) {
	auto where = std::ranges::lower_bound(_characters, g.name, std::less{}, &character::name);

	if (where != _characters.end() && where->name == g.name) {
		*where = std::move(g);
	} else {
		_characters.emplace(where, std::move(g));
	}
}

std::span<guild const> guildbook_data::get_guilds() const noexcept {
	return (_guilds);
}

