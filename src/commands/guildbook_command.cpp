#include "guildbook_command.h"

#include <fstream>

#include <dpp/colors.h>
#include <dpp/utility.h>
#include <dpp/restresults.h>
#include <dpp/cluster.h>

#include "wow/guildbook/guildbook_data.h"

using namespace mimiron;

dpp::coroutine<> guildbook_command::upload::print_usage(const dpp::slashcommand_t &event) {
	std::string image_help = dpp::utility::read_file("data/img/guildbook_upload.png");

	co_await event.co_reply(dpp::message{}
	.add_file("guildbook_upload.png", image_help, "image/png")
	.add_embed(
		dpp::embed{}
		.set_color(dpp::colors::rust)
		.set_title(std::format("Command help: </guildbook upload:{}>", static_cast<uint64_t>(event.command.get_command_interaction().id)))
		.set_thumbnail(event.from->creator->me.get_avatar_url())
		.set_description("This command lets you update GuildBook Classic data, by providing a data file to the parameter `file`.")
		.add_field("How to", "Select the parameter `file`, then select the Guildbook data that is saved in your game installation, under `World of Warcraft\\_classic_era_\\WTF\\Account\\[account id]\\SavedVariables`.")
		.set_image("attachment://guildbook_upload.png")
	));
}

dpp::coroutine<> guildbook_command::upload::operator()(const dpp::slashcommand_t &event, const dpp::attachment&) {
	const auto &options = std::get<dpp::command_interaction>(event.command.data).options[0];
	const auto &suboptions = std::get<dpp::command_interaction>(event.command.data).options[0].options;

	if (suboptions.size() < 1 || !std::holds_alternative<dpp::snowflake>(suboptions[0].value)) {
		co_await print_usage(event);
		co_return;
	}
	dpp::snowflake id = std::get<dpp::snowflake>(suboptions[0].value);
	const auto &   attachments = event.command.resolved.attachments;
	auto           it = attachments.find(id);
	if (it == attachments.end()) {
		co_await print_usage(event);
	}
	dpp::cluster *cluster = event.from->creator;

	auto thinking = event.co_thinking(false);
	auto result = co_await cluster->co_request(it->second.url, dpp::m_get);
	if (!(result.status >= 200 && result.status < 300)) {
		cluster->log(dpp::ll_warning, std::format(
									 "could not download attachment for `/guildbook upload` for {} ({})",
									 event.command.usr.global_name, static_cast<uint64_t>(event.command.usr.id)
								 ));
		co_await thinking;
		co_await event.co_edit_original_response({"❌ There was an error in downloading the file."});
		co_return;
	}
	bool                success = false;
	wow::guildbook_data data;
	try {
		data = wow::guildbook_data::parse(result.body);
		success = true;
	} catch (const std::exception &e) {
		cluster->log(dpp::ll_warning, std::format("error during wow::guildbook_data::parse: {}", e.what()));
	}
	co_await thinking;
	if (!success) {
		co_await event.co_edit_original_response({"❌ There was an error in processing the file. Make sure you are sending a valid GuildBook save file."});
		co_return;
	}
	std::filesystem::path guild_folder = "guild_data/" + event.command.guild_id.str() + "/guildbook";
	std::error_code err;
	if (!create_directories(guild_folder, err) && err != std::error_code{}) {
		throw internal_exception{"could not create folder " + guild_folder.string() + ": " + err.message()};
	}
	auto saved_path = guild_folder / ("guildbook_" + event.command.usr.id.str() + ".lua");
	dpp::message reply = {"✅ Data updated."};
	if (std::ofstream file{saved_path, std::ios::trunc | std::ios::out}; file.good()) {
		file << result.body;
	} else {
		cluster->log(dpp::ll_error, "could not open " + saved_path.string() + " for writing");
		reply.content.append("\n\n⚠ There was an error when saving the data, please report to the bot's host");
	}
	co_await thinking;
	co_await event.co_edit_original_response(reply);
}
