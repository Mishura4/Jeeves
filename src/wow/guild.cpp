#include <ranges>

#include "guild.h"

#include <algorithm>

using namespace mimiron;
using namespace mimiron::wow;

guild::guild(std::string name) : _name{std::move(name)}
{}

auto guild::get_name() const noexcept -> const std::string& {
	return (_name);
}

void guild::add_player(const character &chr) {
	return (add_player(chr.name));
}

void guild::add_player(std::string_view player_name) {
	auto where = std::ranges::lower_bound(_characters, player_name);

	if (where != _characters.end() && *where == player_name)
		return;
	_characters.emplace(where, player_name);
}

std::span<std::string const> guild::get_members() const noexcept {
	return (_characters);
}
