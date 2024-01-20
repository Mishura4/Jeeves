#include "mimiron.h"

#include <fstream>
#include <atomic>
#include <memory>

#include <dpp/once.h>

#include "wow/guildbook/guildbook_data.h"
#include "commands/guildbook_command.h"


namespace mimiron {

namespace {

std::string get_token(const std::filesystem::path &file_path) {
	std::ifstream fs{file_path};

	if (!fs.good()) {
		throw dpp::invalid_token_exception{"could not read " + file_path.string()};
	}
	std::string token;

	std::getline(fs, token);
	return (token);
}

}

mimiron::mimiron(std::span<char *const> args) :
	cluster{get_token(args.size() < 2 ? "token.txt" : args[1]), dpp::i_default_intents, 0, 0, 1, true, dpp::cache_policy::cpol_balanced} {
	/* Output simple log messages to stdout */
	cluster.on_log(dpp::utility::cout_logger());
}

int mimiron::run() {
	auto data = wow::guildbook_data::parse(dpp::utility::read_file("Guildbook_ClassicEra.lua"));

	try {
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
	} catch (const std::exception &e) {
		cluster.log(dpp::ll_error, e.what());
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

	/* Handle slash command with the most recent addition to D++ features, coroutines! */
	cluster.on_slashcommand(_command_handler);

	cluster.start(dpp::st_wait);

	return (0);
}

}
