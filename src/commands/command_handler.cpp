#include <dpp/cluster.h>

#include "command_handler.h"
#include "guildbook_command.h"

using namespace mimiron;

namespace {

std::string format_user(const dpp::user &user) {
  return "`" + user.username + "` (" + user.id.str() + ")";
}

} // namespace

const std::string &
command_handler::slashcommand_node::get_name() const noexcept {
  return command.name;
}

const dpp::slashcommand &
command_handler::slashcommand_node::get_slashcommand() const noexcept {
  return command;
}

const dpp::command_option &
command_handler::slashcommand_node::get_option(std::string_view name) const {
  auto it =
      std::ranges::find(command.options, name, &dpp::command_option::name);

  if (it == command.options.end()) {
    throw dpp::logic_exception("option " + std::string{name} +
                               " does not exist in slashcommand " +
                               command.name);
  }
  return *it;
}

command_handler::command_handler(mimiron &bot) : _bot{bot} {}

command_handler::command_fun *command_handler::find_command(
    std::string_view name, const dpp::command_data_option *subcommand,
    const dpp::command_data_option *subcommand_group) {
  auto root_node =
      std::ranges::find(commands, name, &slashcommand_node::get_name);
  if (root_node == commands.end()) {
    return nullptr;
  }

  if (subcommand_group) {
    if (root_node->body.index() != 1) {
      return nullptr;
    }

    std::span groups = std::get<1>(root_node->body);
    auto group_node = std::ranges::find(groups, subcommand_group->name,
                                        &subcommand_node::name);
    if (group_node == groups.end() || group_node->body.index() != 1) {
      return nullptr;
    }

    std::span subcommands = std::get<1>(group_node->body);
    auto subcommand_node = std::ranges::find(subcommands, subcommand->name,
                                             &final_subcommand_node::name);
    if (subcommand_node == subcommands.end()) {
      return nullptr;
    }

    return &subcommand_node->handler;
  } else if (subcommand) {
    if (root_node->body.index() != 1) {
      return nullptr;
    }

    std::span subcommands = std::get<1>(root_node->body);
    auto subcommand_node = std::ranges::find(subcommands, subcommand->name,
                                             &subcommand_node::name);
    if (subcommand_node == subcommands.end() ||
        subcommand_node->body.index() != 0) {
      return nullptr;
    }

    return &std::get<0>(subcommand_node->body);
  } else {
    if (root_node->body.index() != 0) {
      return nullptr;
    }
    return &std::get<0>(root_node->body);
  }
}

auto command_handler::operator()(const dpp::slashcommand_t &event)
    -> dpp::task<void> {
  dpp::cluster &cluster = *event.from->creator;
  const auto &command = std::get<dpp::command_interaction>(event.command.data);
  const auto &issuer = event.command.get_issuing_user();
  const dpp::command_data_option *subcommand = nullptr;
  const dpp::command_data_option *subcommandgroup = nullptr;
  std::span bottom_options = command.options;

  // first look if a subcommand group was requested
  auto option_it = std::ranges::find(bottom_options, dpp::co_sub_command_group,
                                     &dpp::command_data_option::type);
  if (option_it != bottom_options.end()) {
    subcommandgroup = &(*option_it);
    bottom_options = subcommandgroup->options;
  }
  // then look for subcommand
  option_it = std::ranges::find(bottom_options, dpp::co_sub_command,
                                &dpp::command_data_option::type);
  if (option_it != bottom_options.end()) {
    subcommand = &(*option_it);
    bottom_options = subcommand->options;
  }

  if (subcommandgroup) {
    if (!subcommand) {
      cluster.log(dpp::ll_error,
                  "user " + format_user(issuer) + " sent an invalid command");
      co_return;
    }
  }

  std::stringstream full_command;
  full_command << command.name;
  if (subcommandgroup) {
    full_command << " " << subcommandgroup->name;
  }
  if (subcommand) {
    full_command << " " << subcommand->name;
  }
  if (command_fun *fun =
          find_command(command.name, subcommand, subcommandgroup);
      fun) {
    try {
      if (fun->index() == 0) {
        co_await std::invoke(std::get<0>(*fun), event, bottom_options);
      } else {
        std::invoke(std::get<1>(*fun), event, bottom_options);
      }
    } catch (const std::exception &e) {
      cluster.log(dpp::ll_error,
                  "exception in command `" + std::move(full_command).str() +
                      "` sent by " + format_user(issuer) + ": " + e.what());
    }
  } else {
    cluster.log(dpp::ll_error, "user " + format_user(issuer) +
                                   " sent unknown command `" +
                                   std::move(full_command).str() + "`");
  }

  /*
  if (event.command.get_command_name() == "guildbook") {
          const auto &options = event.command.get_command_interaction().options;

          if (options.empty()) {
                  cluster.log(dpp::ll_error, std::format("{} ({}) sent command
  /guildbook with no option", issuer.username, uint64_t{issuer.id})); co_return;
          }

          if (options[0].name == "upload") {
                  //co_await guildbook_command::upload{_bot}(event);
          }
  } else if (event.command.get_command_name() == "register") {
          std::vector<dpp::slashcommand> commands {
                  dpp::slashcommand{"register", "Register commands",
  cluster.me.id}.set_default_permissions(dpp::p_administrator),
                  dpp::slashcommand{"guildbook", "Guildbook data interaction",
  cluster.me.id} .add_option( dpp::command_option{dpp::co_sub_command, "upload",
  "Upload guildbook data"} .add_option({dpp::co_attachment, "file",
  "Guildbook_ClassicEra.lua data"})
                  )
          };

          auto thinking = event.co_thinking();

          auto result = co_await
  cluster.co_global_bulk_command_create(commands);

          if (result.is_error()) {
                  cluster.log(dpp::ll_error, "error while registering commands:
  " + result.get_error().human_readable); } else { cluster.log(dpp::ll_info,
  "successfully registered " +
  std::to_string(std::get<dpp::slashcommand_map>(result.value).size()) + "
  commands");
          }

          co_await thinking;
          co_await event.co_edit_original_response({"✅"});
  }
  co_return;*/
}

dpp::coroutine<std::expected<size_t, dpp::error_info>>
command_handler::fetch_commands(dpp::cluster &bot) {
  auto result = co_await bot.co_global_commands_get();
  if (result.is_error()) {
    auto error = result.get_error();
    bot.log(dpp::ll_error, "could not fetch commands: " + error.human_readable);
    co_return std::unexpected{std::move(error)};
  }
  for (const auto &[_, slashcommand] :
       std::get<dpp::slashcommand_map>(result.value)) {
    auto my_command = std::ranges::find(commands, slashcommand.name,
                                        &slashcommand_node::get_name);
    if (my_command == commands.end()) {
      bot.log(dpp::ll_warning,
              "registered slashcommand " + slashcommand.name +
                  " is unknown, it will not be recognized by the bot");
      continue;
    }
    my_command->command = slashcommand;
  }
  co_return std::expected<size_t, dpp::error_info>{
      std::get<dpp::slashcommand_map>(result.value).size()};
}

dpp::coroutine<std::expected<size_t, dpp::error_info>>
command_handler::register_commands(dpp::cluster &bot) {
  auto result = co_await bot.co_global_bulk_command_create(
      commands | std::views::transform(&slashcommand_node::command) |
      std::ranges::to<std::vector>());
  if (result.is_error()) {
    auto error = result.get_error();
    bot.log(dpp::ll_error, "could not fetch commands: " + error.human_readable);
    co_return std::unexpected{std::move(error)};
  }
  for (const auto &[_, slashcommand] :
       std::get<dpp::slashcommand_map>(result.value)) {
    auto my_command = std::ranges::find(commands, slashcommand.name,
                                        &slashcommand_node::get_name);
    if (my_command == commands.end()) {
      bot.log(dpp::ll_warning,
              "registered slashcommand " + slashcommand.name +
                  " is unknown, it will not be recognized by the bot");
      continue;
    }
    my_command->command = slashcommand;
  }
  co_return std::expected<size_t, dpp::error_info>{
      std::get<dpp::slashcommand_map>(result.value).size()};
}

command_handler &
command_handler::add_command(const dpp::slashcommand &command,
                             std::vector<subcommand> subcommands) {
  auto it = std::ranges::lower_bound(commands, command.name, std::less{},
                                     [](const slashcommand_node &node) {
                                       return node.get_slashcommand().name;
                                     });

  if (it != commands.end() && it->command.name == command.name) {
    throw dpp::logic_exception("command " + command.name + " already exists");
  }
  it = commands.emplace(it, command.name, command.description);
  it->body.emplace<1>(
      subcommands |
      std::views::transform([](subcommand &cmd) -> subcommand_node {
        return subcommand_node{cmd.name, std::move(cmd.fun)};
      }) |
      std::ranges::to<std::vector>());
  for (const auto &[sub_name, sub_desc, sub_options, _] : subcommands) {
    dpp::command_option sub_opt = {dpp::co_sub_command, sub_name, sub_desc};
    for (const dpp::command_option &opt : sub_options) {
      sub_opt.add_option(opt);
    }
    it->command.add_option(sub_opt);
  }
  return *this;
}

command_handler &
command_handler::add_command(const dpp::slashcommand &command,
                             std::vector<subcommand_group> subcommand_groups) {
  auto it = std::ranges::lower_bound(commands, command.name, std::less{},
                                     [](const slashcommand_node &node) {
                                       return node.get_slashcommand().name;
                                     });

  if (it != commands.end() && it->command.name == command.name) {
    throw dpp::logic_exception("command " + command.name + " already exists");
  }
  it = commands.emplace(it, command.name, command.description);
  it->body.emplace<1>(
      subcommand_groups |
      std::views::transform([](subcommand_group &cmd) -> subcommand_node {
        return subcommand_node{
            cmd.name,
            cmd.subcommands |
                std::views::transform([](subcommand &cmd)
                                          -> final_subcommand_node {
                  return final_subcommand_node{cmd.name, std::move(cmd.fun)};
                }) |
                std::ranges::to<std::vector>()};
      }) |
      std::ranges::to<std::vector>());
  for (const auto &[group_name, group_desc, subcommands] : subcommand_groups) {
    dpp::command_option group_opt = {dpp::co_sub_command_group, group_name,
                                     group_desc};
    for (const auto &[sub_name, sub_desc, sub_options, _] : subcommands) {
      dpp::command_option sub_opt = {dpp::co_sub_command, sub_name, sub_desc};
      for (const dpp::command_option &opt : sub_options) {
        sub_opt.add_option(opt);
      }
      group_opt.add_option(sub_opt);
    }
    it->command.add_option(group_opt);
  }
  return *this;
}
