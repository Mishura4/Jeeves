#ifndef MIMIRON_DATABASE_TABLES_DISCORD_GUILD
#define MIMIRON_DATABASE_TABLES_DISCORD_GUILD

#include "database/table.h"

namespace mimiron::tables {

struct discord_guild_entry {
	uint64_t snowflake;
};

}

#endif /* MIMIRON_DATABASE_TABLES_DISCORD_GUILD */