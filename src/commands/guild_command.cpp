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

    embed.set_thumbnail(std::format("{} - {} wizard", cluster.me.username, event.command.get_command_interaction().get_mention()));
    embed.set_description("It looks like you haven't associated any guild with this discord server.\nWould you like to get started? This will require the \"Manage Server\" permission.");
    dpp::component button;
    button.set_id("setup");
    button.set_label("Add a guild");
    button.set_emoji("⚙");
    m.embeds.push_back(std::move(embed));
    dpp::component &row = m.components.emplace_back();
    row.set_type(dpp::cot_action_row);
    row.components.push_back(std::move(button));
    throw_if_error(co_await thinking);
    throw_if_error(co_await event.co_edit_original_response(
      std::move(m)
    ));
    auto result = co_await event.co_get_original_response();
    throw_if_error(result);
    m = std::move(std::get<dpp::message>(result.value));
  }

  auto action = co_await dpp::when_any{
    cluster.on_button_click.when([message_id = m.id](const dpp::button_click_t& e) {
      return e.command.msg.id == message_id;
    }),
    cluster.co_sleep(std::chrono::floor<std::chrono::seconds>(10s - (app_clock::now() - time_started)).count())
  };

  switch (action.index()) {
    case 0: {
      dpp::button_click_t const& click = action.get<0>();
      m.embeds[0].set_description("woo");
      click.reply(dpp::ir_update_message, m);
      co_return;
    }

    case 1: {
      for (dpp::component &c : m.components) {
        c.set_disabled(true);
      }
      auto& footer = *m.embeds[0].footer;
      footer.text = footer.text + " **- this interaction has expired!**";
      event.edit_original_response(m);
      co_return;
    }
  }
}

} // namespace mimiron
