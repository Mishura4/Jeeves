#include "commands/guild_command.h"
#include "tools/cache.h"
#include "mimiron.h"

namespace mimiron {

guild_command::guild_command(mimiron& bot) :
  _bot(&bot)
{}

dpp::coroutine<void> guild_command::operator()(const dpp::slashcommand_t &event) {
  time_started = app_clock::now();
  _guilds = _bot->wow_guild_cache().find(event.command.guild_id);
  if (!_guilds || _guilds->second.empty()) {
    co_await _show_empty_menu(event);
  }
  co_return;
}

dpp::coroutine<void> guild_command::_show_empty_menu(const dpp::slashcommand_t &event) {
  dpp::guild_member const* member = event.command.guild_id ? &event.command.member : nullptr;
  dpp::cluster &cluster = *event.from->creator;

  auto thinking = event.co_thinking();

  dpp::confirmation_callback_t result;
  std::string interaction_id = event.command.id.str();
  dpp::message m;
  {
    auto embed = co_await _bot->make_default_embed(event.command.guild_id, &event.command.usr, member);

    embed.set_title(std::format("{} - {} wizard", cluster.me.username, event.command.get_command_interaction().get_mention()));
    embed.set_description("It looks like you haven't associated any guild with this discord server.\nWould you like to get started? This will require the \"Manage Server\" permission.");
    dpp::component button;
    button.set_id("guild_add");
    button.set_label("Add a guild");
    button.set_emoji("🛠");
    m.embeds.push_back(std::move(embed));
    dpp::component &row = m.components.emplace_back();
    row.set_type(dpp::cot_action_row);
    row.components.push_back(std::move(button));
    throw_if_error(co_await thinking);
    throw_if_error(co_await event.co_edit_original_response(
      std::move(m)
    ));
  }
}

dpp::coroutine<void> guild_command::add_guild(const dpp::button_click_t& event) {
  const dpp::message &event_msg = event.command.msg;
  dpp::cluster& cluster = *event.from->creator;

  if (auto time_since = system_clock::now() - discord_time(event_msg.get_creation_time()); time_since > 10min) {
    event.reply(dpp::message{"This button has expired!"}.set_flags(dpp::m_ephemeral));
    dpp::message msg = event_msg;
    bool edit = false;
    for (dpp::component &row : msg.components) {
      if (row.disabled == false)
        edit = true;
      row.disabled = true;
      for (dpp::component &button : row.components) {
        if (button.disabled == false)
          edit = true;
        button.disabled = true;
      }
    }
    if (edit) {
      cluster.message_edit(msg);
    }
    co_return;
  }

  auto permissions = event.command.get_resolved_permission(event.command.usr.id);

  if (!permissions.can(dpp::p_manage_guild)) {
    event.reply(dpp::message{"You need the \"Manage Server\" permission to add a guild."}.set_flags(dpp::m_ephemeral));
    co_return;
  }

  auto realms = co_await _bot->wow_api().get_realms(wow::api_regions::north_america, wow::wow_classic_era);

  for (const auto& realm : realms) {
    cluster.log(dpp::ll_info, realm.name);
  }

  /*event.dialog(dpp::interaction_modal_response{
    "guild_add",
    "Link a guild to the server", {
      dpp::component{}
      .set_type(dpp::cot_text)
      .set_placeholder("Guild name (must match in-game name!)")
      .set_max_length(63)
      .set_text_style(dpp::text_short)
      .set_id("name"),

      dpp::component{}
      .set_type(dpp::cot_selectmenu)
      .set_id("region")
      .add_select_option({"United States", "0"})
      .add_select_option({"Europe", "1"})
      .add_select_option({"South Korea", "2"})
      .add_select_option({"Taiwan", "3"})
      .add_select_option({"China", "4"}),
    }
  });*/
}


} // namespace mimiron
