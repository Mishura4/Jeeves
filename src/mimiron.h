#pragma once

#include <span>

#include <dpp/cluster.h>

#include "database/database.h"

#include "commands/command_handler.h"

namespace mimiron {

class mimiron {
public:
	mimiron(std::span<char *const> args);

	~mimiron() = default;

	int run();

private:
	dpp::cluster cluster;
	command_handler _command_handler{*this};
	sql::mysql_database _database{{
		.password = "root",
		.database = "mimiron",
		.port = 3307
	}};
};

}
