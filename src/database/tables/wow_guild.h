#ifndef MIMIRON_DATABASE_TABLES_WOW_GUILD
#define MIMIRON_DATABASE_TABLES_WOW_GUILD

#include "database/table.h"

namespace mimiron::tables {

struct wow_guild_entry {
	uint64_t discord_guild_id;
  uint8_t wow_guild_id;
  uint16_t server_id;
  uint16_t region_id;
  std::string name;
};

using wow_guild = sql::table<"wow_guild", wow_guild_entry>;

}

#endif /* MIMIRON_DATABASE_TABLES_WOW_GUILD */