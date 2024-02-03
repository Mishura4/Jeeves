#include <dpp/colors.h>

#include "discord_guild.h"

namespace mimiron {

discord_guild::discord_guild(dpp::snowflake id) noexcept :
	_id{id} {
}

discord_guild::discord_guild(const tables::discord_guild_entry& db_entry) :
	_id(db_entry.snowflake) {
}

discord_guild::discord_guild(const discord_guild& other) :
	_id{other._id},
	_bot_member{other._bot_member},
	_bot_member_last_updated{other._bot_member_last_updated} {
}

discord_guild& discord_guild::operator=(const discord_guild& other) {
	_id = other._id;
	_bot_member = other._bot_member;
	_bot_member_last_updated = other._bot_member_last_updated;
	return *this;
}

dpp::snowflake discord_guild::id() const noexcept {
	return _id;
}

app_timestamp discord_guild::last_updated_bot_member() const {
	std::shared_lock lock{mutex};

	return _bot_member_last_updated;
}

dpp::guild_member discord_guild::bot_member() const {
	std::shared_lock lock{mutex};

	return _bot_member;
}

uint32_t discord_guild::bot_color() const {
	std::shared_lock lock{mutex};

	return _bot_color;
}

void discord_guild::update_bot_member(dpp::guild_member const& member, dpp::role_map const& roles) {
	auto color = mimiron_color;
	uint8_t role_position = std::numeric_limits<uint8_t>::max();
	if (!roles.empty()) {
		for (const dpp::snowflake &role_id : member.get_roles()) {
			if (auto it = roles.find(role_id); it != roles.end()) {
				const auto& [_, role] = *it;

				if (role.colour != 0) {
					if (role.position < role_position) {
						role_position = role.position;
						color = role.colour;
					}
				}
			}
		}
	}
	std::unique_lock lock{mutex};

	_bot_member_last_updated = app_clock::now();
	_bot_member = member;
	_bot_color = color;
}

}
