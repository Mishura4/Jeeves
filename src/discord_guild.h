#ifndef MIMIRON_DISCORD_GUILD_H_
#define MIMIRON_DISCORD_GUILD_H_

#include <chrono>
#include <shared_mutex>

#include <dpp/coro/coroutine.h>
#include <dpp/managed.h>
#include <dpp/guild.h>
#include <dpp/cluster.h>

#include "common.h"
#include "database/tables/discord_guild.h"

namespace mimiron {

class discord_guild {
public:
	discord_guild(dpp::snowflake id) noexcept;
	discord_guild(const tables::discord_guild_entry& db_entry);
	discord_guild(const discord_guild& other);

	discord_guild& operator=(const discord_guild& other);

	dpp::snowflake id() const noexcept;

	app_timestamp last_updated_bot_member() const;
	dpp::guild_member bot_member() const;
	uint32_t bot_color() const;

	void update_bot_member(dpp::guild_member const& member, dpp::role_map const& roles = {});

private:
	mutable std::shared_mutex mutex;
	dpp::snowflake _id;
	dpp::guild_member _bot_member;
	app_timestamp _bot_member_last_updated;
	uint64_t _bot_color;
};

}

#endif /* MIMIRON_DISCORD_GUILD_H_ */
