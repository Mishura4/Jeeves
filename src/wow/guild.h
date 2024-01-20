#pragma once

#include <string>
#include <vector>
#include <span>

#include "wow/character.h"

namespace mimiron::wow {

class guild {
public:
	guild(std::string name);

	const std::string &get_name() const noexcept;
	std::span<std::string const> get_members() const noexcept;

	void add_player(std::string_view player_name);
	void add_player(const character &chr);

private:
	std::string _name;
	std::vector<std::string> _characters;
};

}
