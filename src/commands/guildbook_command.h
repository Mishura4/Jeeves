#pragma once

#include <dpp/coro/coroutine.h>
#include <dpp/dispatcher.h>

namespace mimiron {

class mimiron;

struct guildbook_command {
	struct upload {
		dpp::coroutine<> print_usage(const dpp::slashcommand_t &event);
		dpp::coroutine<> operator()(const dpp::slashcommand_t &event, const dpp::attachment &attachment);

		mimiron &bot;
	};
};

}
