#include <ranges>

#include "guild.h"

#include <algorithm>

using namespace mimiron;
using namespace mimiron::wow;

guild::guild(dpp::snowflake snowflake, uint8_t wow_id, std::string name)
    : _discord_guild{snowflake}, _wow_guild_id{wow_id}, _name{std::move(name)} {
}

auto guild::name() const noexcept -> const std::string & { return (_name); }

void guild::add_player(const character &chr) { return (add_player(chr.name)); }

void guild::add_player(std::string_view player_name) {
  auto where = std::ranges::lower_bound(_characters, player_name);

  if (where != _characters.end() && *where == player_name)
    return;
  _characters.emplace(where, player_name);
}

std::span<std::string const> guild::members() const noexcept {
  return (_characters);
}

dpp::snowflake guild::discord_guild() const noexcept { return _discord_guild; }

uint8_t guild::wow_id() const noexcept { return _wow_guild_id; }
