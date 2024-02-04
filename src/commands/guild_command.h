#ifndef MIMIRON_COMMANDS_GUILD_H
#define MIMIRON_COMMANDS_GUILD_H

#include <dpp/coro/coroutine.h>
#include <dpp/dispatcher.h>

#include "common.h"
#include "tools/cache.h"
#include "wow/guild.h"

namespace mimiron {

class mimiron;
class mysql_database;

class guild_command {
public:
	guild_command(mimiron& bot);

	dpp::coroutine<void> operator()(const dpp::slashcommand_t &event);

	dpp::coroutine<void> add_guild(const dpp::button_click_t &event);

private:
	dpp::coroutine<void> _show_empty_menu(const dpp::slashcommand_t &event);

	mimiron *_bot;
	app_timestamp time_started;
	wow::guild::cache::value_t _guilds;
};

}

#endif
