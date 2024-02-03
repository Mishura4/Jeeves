#pragma once

#include <span>
#include <chrono>

#include <dpp/cluster.h>
#include <dpp/message.h>
#include <dpp/user.h>

#include "database/database.h"
#include "database/tables/discord_guild.h"
#include "commands/command_handler.h"
#include "tools/cache.h"
#include "wow/guild.h"
#include "discord_guild.h"

namespace mimiron {

class mimiron {
public:
	mimiron(std::span<char *const> args);

	~mimiron() = default;

	int run();

	void log(dpp::loglevel level, std::string_view message) const;

	template <typename T, typename... Args>
	void log(dpp::loglevel level, std::format_string<T, Args...> fmt, T&& arg, Args&&... args) const {
		if (level < log_min)
			return;
		log(level, std::format(fmt, std::forward<T>(arg), std::forward<Args>(args)...));
	}

	wow::guild::cache& wow_guild_cache() noexcept {
		return _wow_guild_cache;
	}

	wow::guild::cache const& wow_guild_cache() const noexcept {
		return _wow_guild_cache;
	}

	auto& discord_guild_cache() noexcept {
		return _discord_guild_cache;
	}

	auto const& discord_guild_cache() const noexcept {
		return _discord_guild_cache;
	}

	dpp::coroutine<dpp::guild_member> get_bot_member(dpp::snowflake guild);

	dpp::coroutine<dpp::embed> make_default_embed(dpp::snowflake guild_for = {}, dpp::user const* user_for = nullptr, dpp::guild_member const* member_for = nullptr);

private:
	void _log(dpp::log_t const &log_event) const;

	void _init_commands();

	void _init_database();
	void _load_guilds();

	uint64_t log_min = 0;
	dpp::cluster cluster;
	command_handler _command_handler{*this};
	sql::mysql_database _database{{
		.password = "root",
		.database = "mimiron",
		.port = 3307
	}};

	cache<dpp::snowflake, discord_guild> _discord_guild_cache;
	wow::guild::cache _wow_guild_cache;
};

}
