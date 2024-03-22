#include "mimiron.h"

#include <fstream>
#include <atomic>
#include <memory>

#include <dpp/once.h>
#include <termcolor/termcolor.hpp>

#include "wow/guildbook/guildbook_data.h"
#include "commands/guildbook_command.h"
#include "commands/guild_command.h"

#include "database/tables/wow_guild.h"

namespace mimiron {

namespace {

nlohmann::json load_config(const std::filesystem::path &file_path) {
	std::ifstream fs{file_path};

	if (!fs.good()) {
		throw dpp::invalid_token_exception{"could not read " + file_path.string()};
	}

	nlohmann::json j;

	fs >> j;
	return j;
}

}

mimiron::mimiron(std::span<char *const> args) :
	config{load_config(args.size() < 2 ? "config.json" : args[1])},
	cluster{config["discord_token"], dpp::i_default_intents, 0, 0, 1, true, dpp::cache_policy::cpol_balanced},
	_resource_manager{cluster, config["wow_api_id"], config["wow_api_key"]} {
	log_min = 0;
	cluster.on_log([this]( dpp::log_t const& log) { _log(log); });
}
void mimiron::_log(dpp::log_t const& log_event) const {
	log(log_event.severity, log_event.message);
}

void mimiron::log(dpp::loglevel severity, std::string_view message) const {
	if (severity < log_min)
		return;
	switch (severity) {
		case dpp::ll_warning:
			std::cerr << termcolor::yellow << "[WARNING] " << message << termcolor::reset << std::endl;
			return;

		case dpp::ll_error:
			std::cerr << termcolor::red << "[ERROR] " << message << termcolor::reset << std::endl;
			return;

		case dpp::ll_critical:
			std::cerr << termcolor::bold << termcolor::red << "[CRITICAL] " << message << termcolor::reset << std::endl;
			return;

		case dpp::ll_info:
			std::cout << termcolor::white << "[INFO] " << message << termcolor::reset << std::endl;
			return;

		case dpp::ll_trace:
			std::cout << termcolor::bright_blue << "[TRACE] " << message << termcolor::reset << std::endl;
			return;

		case dpp::ll_debug:
			std::cout << termcolor::bright_cyan << "[DEBUG] " << message << termcolor::reset << std::endl;
			return;
	}
	std::cout << termcolor::bright_white << "[UNKNOWN] " << message << termcolor::reset << std::endl;
}

dpp::coroutine<dpp::embed> mimiron::make_default_embed(dpp::snowflake guild_for, dpp::user const* user_for, dpp::guild_member const* member_for) {
	auto [guild_settings, _] = discord_guild_cache().try_emplace(guild_for, guild_for);
	dpp::embed ret{};
	std::string nickname;
	std::string url;

	if (member_for) {
		url = member_for->get_avatar_url();
		nickname = member_for->get_nickname();
		if (url.empty()) {
			url = user_for->get_avatar_url();
		}
		if (nickname.empty()) {
			nickname = user_for->username;
		}
	} else if (user_for) {
		url = user_for->get_avatar_url();
		nickname = user_for->username;
	}
	ret.set_footer(nickname, url);
	uint32_t color = mimiron_color;

	url = {};
	if (guild_for) {
		dpp::guild_member me = co_await get_bot_member(guild_for);
		url = me.get_avatar_url();
		color = guild_settings->second.bot_color();
	}
	if (url.empty()) {
		url = cluster.me.get_avatar_url();
	}
	ret.set_color(color);
	ret.set_thumbnail(url);

	co_return std::move(ret);
}

dpp::coroutine<dpp::guild_member> mimiron::get_bot_member(dpp::snowflake guild) {
	auto [guild_settings, _] = discord_guild_cache().try_emplace(guild, guild);

	if (dpp::guild* g = dpp::find_guild(guild)) {
		if (auto it = g->members.find(cluster.me.id); it != g->members.end()) {
			dpp::guild_member const& member = it->second;
			dpp::role_map roles;

			for (dpp::snowflake role_id : member.get_roles()) {
				dpp::role const* role = dpp::find_role(role_id);

				if (!role) {
					break;
				}
				roles.try_emplace(role_id, *role);
			}

			if (roles.size() != member.get_roles().size()) {
				auto result = co_await cluster.co_roles_get(guild);
				if (result.is_error()) {
					roles.clear();
				} else {
					roles = std::get<dpp::role_map>(std::move(result.value));
				}
			}

			guild_settings->second.update_bot_member(it->second, roles);
			co_return it->second;
		}
	}
	auto last_update = guild_settings->second.last_updated_bot_member();
	if (!(last_update.time_since_epoch() == app_clock::duration{}) && app_clock::now() - last_update < std::chrono::minutes(30)) {
		co_return guild_settings->second.bot_member();
	}
	auto result = co_await cluster.co_guild_get_member(guild, cluster.me.id);
	if (result.is_error()) {
		throw dpp::rest_exception{result.get_error().human_readable};
	}
	dpp::guild_member member = std::get<dpp::guild_member>(std::move(result.value));
	dpp::role_map role_map;

	result = co_await cluster.co_roles_get(guild);
	if (!result.is_error()) {
		role_map = std::get<dpp::role_map>(std::move(result.value));
	}
	guild_settings->second.update_bot_member(member, role_map);
	co_return member;
}


void mimiron::_init_commands() {
	_command_handler.add_command(
		dpp::slashcommand{"guildbook", "test", 0},
		std::vector<command_handler::subcommand>{{
				"upload", "upload a thing", guildbook_command::upload{*this}, {
					dpp::command_option{{}, "thing", "the thing"}
				}
			}
		}
	);

	_command_handler.add_command(
		dpp::slashcommand{"guild", "Bring up a menu for everything guild related", 0},
		[this](const dpp::slashcommand_t &event) -> dpp::coroutine<void> {
			co_await guild_command{*this}(event);
		}
	);

	_command_handler.add_command(
		dpp::slashcommand{}.set_name("register").set_description("Register commands"),
		[this](const dpp::slashcommand_t& event) -> dpp::coroutine<> {
			auto thinking = event.co_thinking();

			auto result = co_await _command_handler.register_commands(cluster);

			co_await thinking;
			if (result)
				co_await event.co_edit_original_response({"✅"});
			else
				co_await event.co_edit_original_response({"❌ error; please refer to logs"});
		}
	);
}

void mimiron::_init_database() {
	try {
		_load_guilds();
	} catch (const std::exception &e) {
		cluster.log(dpp::ll_critical, std::format("error while loading guilds: {}", e.what()));
		throw;
	}
}

void mimiron::_load_guilds() {
	{
		cluster.log(dpp::ll_info, "loading discord guilds...");
		auto discord_guilds = _database.execute_sync(sql::select<tables::discord_guild_entry>.from("discord_guild"));
		for (const tables::discord_guild_entry& entry : discord_guilds) {
			auto [guild, inserted] = _discord_guild_cache.try_emplace(entry.snowflake, entry);
			if (!inserted) {
				guild->second = discord_guild{entry};
			}
			log(dpp::ll_trace, "loaded guild {}", static_cast<uint64_t>(guild->second.id()));
		}
		cluster.log(dpp::ll_info, std::format("loaded {} guilds\n", discord_guilds.size()));
	}

	{
		cluster.log(dpp::ll_info, "loading wow guilds...");
		auto wow_guilds = _database.execute_sync(sql::select<tables::wow_guild_entry>.from("wow_guild"));
		for (const tables::wow_guild_entry& entry : wow_guilds) {
			auto cache_entry = _wow_guild_cache[entry.discord_guild_id];
			auto& [_, guilds] = *cache_entry;
			wow::guild* this_guild;
			if (auto it = std::ranges::find(guilds, entry.wow_guild_id, &wow::guild::wow_id); it != guilds.end()) {
				this_guild = &(*it);
			}
			else {
				this_guild = &guilds.emplace_back(entry.discord_guild_id, entry.wow_guild_id, entry.name);
			}
			log(dpp::ll_trace, "loaded guild <{}> with id {}:{}", this_guild->name(), static_cast<uint64_t>(this_guild->discord_guild()), this_guild->wow_id());
		}
		cluster.log(dpp::ll_info, std::format("loaded {} guilds\n", wow_guilds.size()));
	}
}



int mimiron::run() {
	auto data = wow::guildbook_data::parse(dpp::utility::read_file("Guildbook_ClassicEra.lua"));

	try {
		_init_commands();
	} catch (const std::exception &e) {
		cluster.log(dpp::ll_critical, std::format("fatal error while initializing command handler: {}", e.what()));
		return -1;
	}

	try {
		_init_database();
	} catch (const std::exception &e) {
		cluster.log(dpp::ll_critical, std::format("fatal error while initializing database, exiting: {}", e.what()));
		return -1;
	}

	try {
		auto result = _resource_manager.start().sync_wait_for(1min);
		if (!result) {
			throw exception{"WoW API timed out"};
		}
	} catch (const std::exception &e) {
		cluster.log(dpp::ll_critical, std::format("fatal error while initializing WoW API communication, exiting: {}", e.what()));
		return -1;
	}

	/* Register slash command here in on_ready */
	cluster.on_ready([this](const dpp::ready_t& event) {
		/* Wrap command registration in run_once to make sure it doesnt run on every full reconnection */
		if (dpp::run_once<struct register_bot_commands>()) {
			static std::atomic c = &cluster;

			signal(SIGINT, [](int sig) {
				if (dpp::cluster *cluster = c.exchange(nullptr); cluster != nullptr) {
					cluster->shutdown();
				}
			});
		}
	});

	cluster.on_button_click([this](const dpp::button_click_t &event) -> dpp::task<> {
		try {
			if (event.custom_id == "guild_add") {
				co_await guild_command{*this}.add_guild(event);
			}
		} catch (const std::exception &e) {
			log(dpp::ll_error, "exception in button handler: {}", e.what());
		}
		co_return;
	});

	cluster.on_slashcommand(_command_handler);

	cluster.start(dpp::st_wait);

	return (0);
}

}
