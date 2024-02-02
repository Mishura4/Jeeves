#pragma once

#include <expected>

#include <dpp/dispatcher.h>
#include <dpp/coro/task.h>
#include <dpp/coro/coroutine.h>

#include "command_handler.h"

namespace mimiron {

class mimiron;

namespace detail {

template <typename>
struct callable_helper {};

template <typename Ret, typename... Args>
struct callable_helper<Ret (*)(Args...)> {
	using ret = Ret;
	using args_as_tuple = std::tuple<Args...>;
};

template <typename Obj, typename Ret, typename... Args>
struct callable_helper<Ret (Obj::*)(Args...)> {
	using ret = Ret;
	using args_as_tuple = std::tuple<Args...>;
};

template <typename Obj, typename Ret, typename... Args>
struct callable_helper<Ret (Obj::*)(Args...) &> {
	using ret = Ret;
	using args_as_tuple = std::tuple<Args...>;
};

template <typename Obj, typename Ret, typename... Args>
struct callable_helper<Ret (Obj::*)(Args...) &&> {
	using ret = Ret;
	using args_as_tuple = std::tuple<Args...>;
};

template <typename Obj, typename Ret, typename... Args>
struct callable_helper<Ret (Obj::*)(Args...) const> {
	using ret = Ret;
	using args_as_tuple = std::tuple<Args...>;
};

template <typename Obj, typename Ret, typename... Args>
struct callable_helper<Ret (Obj::*)(Args...) const&> {
	using ret = Ret;
	using args_as_tuple = std::tuple<Args...>;
};

template <typename Obj, typename Ret, typename... Args>
struct callable_helper<Ret (Obj::*)(Args...) const&&> {
	using ret = Ret;
	using args_as_tuple = std::tuple<Args...>;
};

template <typename Callable>
requires std::is_member_function_pointer_v<decltype(&Callable::operator())>
struct callable_helper<Callable> : callable_helper<decltype(&Callable::operator())> {
	using ret = typename callable_helper<decltype(&Callable::operator())>::ret;
	using args_as_tuple = typename callable_helper<decltype(&Callable::operator())>::args_as_tuple;
};

template <typename T>
using callable_ret = typename callable_helper<std::remove_cvref_t<T>>::ret;
template <typename T>
using callable_args = typename callable_helper<std::remove_cvref_t<T>>::args_as_tuple;

template <typename T>
struct unrecognized_param {
	operator dpp::command_option_type() = delete;
	void operator()(auto&&...) const = delete;
};

template <typename T>
constexpr inline unrecognized_param<T> command_param_to_option_type;

template <std::ranges::range StringLike>
requires (std::same_as<std::remove_cvref_t<std::ranges::range_value_t<StringLike>>, char>)
constexpr inline dpp::command_option_type command_param_to_option_type<StringLike> = dpp::co_string;

template <> constexpr inline dpp::command_option_type command_param_to_option_type<int64_t> = dpp::co_integer;
template <> constexpr inline dpp::command_option_type command_param_to_option_type<bool> = dpp::co_boolean;
template <> constexpr inline dpp::command_option_type command_param_to_option_type<dpp::user> = dpp::co_user;
template <> constexpr inline dpp::command_option_type command_param_to_option_type<dpp::guild_member> = dpp::co_user;
template <> constexpr inline dpp::command_option_type command_param_to_option_type<std::pair<const dpp::user*, const dpp::guild_member*>> = dpp::co_user;
template <> constexpr inline dpp::command_option_type command_param_to_option_type<dpp::channel> = dpp::co_channel;
template <> constexpr inline dpp::command_option_type command_param_to_option_type<dpp::role> = dpp::co_role;
template <> constexpr inline dpp::command_option_type command_param_to_option_type<std::variant<const dpp::role*, const dpp::user*>> = dpp::co_mentionable;
template <> constexpr inline dpp::command_option_type command_param_to_option_type<double> = dpp::co_number;
template <> constexpr inline dpp::command_option_type command_param_to_option_type<dpp::attachment> = dpp::co_attachment;
template <typename T>
constexpr inline auto parse_param = unrecognized_param<T>{};

template <std::ranges::range StringLike>
requires (std::same_as<std::remove_cvref_t<std::ranges::range_value_t<StringLike>>, char>)
constexpr inline auto parse_param<StringLike> = [](const dpp::interaction& interaction [[maybe_unused]], const dpp::command_data_option& opt) -> StringLike {
	if constexpr (std::convertible_to<std::string, StringLike>) {
		return std::get<std::string>(opt.value);
	} else {
		return std::get<std::string>(opt.value).c_str();
	}
};

template <>
constexpr inline auto parse_param<int64_t> = [](const dpp::interaction& interaction [[maybe_unused]], const dpp::command_data_option& opt) {
	return std::get<int64_t>(opt.value);
};

template <>
constexpr inline auto parse_param<bool> = [](const dpp::interaction& interaction [[maybe_unused]], const dpp::command_data_option& opt) {
	return std::get<bool>(opt.value);
};

template <>
constexpr inline auto parse_param<dpp::user> = [](const dpp::interaction& interaction [[maybe_unused]], const dpp::command_data_option& opt) -> const dpp::user& {
	dpp::snowflake id = std::get<dpp::snowflake>(opt.value);
	if (auto it = interaction.resolved.users.find(id); it != interaction.resolved.users.end()) {
		return it->second;
	}
	throw dpp::parse_exception{"could not find user " + id.str() + " in the resolved list of users for command parameter `" + opt.name + "`"};
};

template <>
constexpr inline auto parse_param<dpp::guild_member> = [](const dpp::interaction& interaction [[maybe_unused]], const dpp::command_data_option& opt) -> const dpp::guild_member& {
	dpp::snowflake id = std::get<dpp::snowflake>(opt.value);
	if (auto it = interaction.resolved.members.find(id); it != interaction.resolved.members.end()) {
		return it->second;
	}
	throw dpp::parse_exception{"could not find guild member " + id.str() + " in the resolved list of guild members for command parameter `" + opt.name + "`"};
};

template <>
constexpr inline auto parse_param<std::pair<const dpp::user*, const dpp::guild_member*>> = [](const dpp::interaction& interaction [[maybe_unused]], const dpp::command_data_option& opt) -> std::pair<const dpp::user*, const dpp::guild_member*> {
	dpp::snowflake id = std::get<dpp::snowflake>(opt.value);
	if (auto user_it = interaction.resolved.users.find(id); user_it != interaction.resolved.users.end()) {
		auto member_it = interaction.resolved.members.find(id);
		return {&user_it->second, member_it == interaction.resolved.members.end() ? nullptr : &member_it->second};
	}
	throw dpp::parse_exception{"could not find user " + id.str() + " in the resolved list of users for command parameter `" + opt.name + "`"};
};

template <>
constexpr inline auto parse_param<dpp::channel> = [](const dpp::interaction& interaction [[maybe_unused]], const dpp::command_data_option& opt) -> const dpp::channel& {
	dpp::snowflake id = std::get<dpp::snowflake>(opt.value);
	if (auto it = interaction.resolved.channels.find(id); it != interaction.resolved.channels.end()) {
		return it->second;
	}
	throw dpp::parse_exception{"could not find channel " + id.str() + " in the resolved list of channels for command parameter `" + opt.name + "`"};
};

template <>
constexpr inline auto parse_param<dpp::role> = [](const dpp::interaction& interaction [[maybe_unused]], const dpp::command_data_option& opt) -> const dpp::role& {
	dpp::snowflake id = std::get<dpp::snowflake>(opt.value);
	if (auto it = interaction.resolved.roles.find(id); it != interaction.resolved.roles.end()) {
		return it->second;
	}
	throw dpp::parse_exception{"could not find role " + id.str() + " in the resolved list of roles for command parameter `" + opt.name + "`"};
};

template <>
constexpr inline auto parse_param<std::variant<const dpp::role*, const dpp::user*>> = [](const dpp::interaction& interaction [[maybe_unused]], const dpp::command_data_option& opt) -> std::variant<const dpp::role*, const dpp::user*> {
	dpp::snowflake id = std::get<dpp::snowflake>(opt.value);
	if (auto it = interaction.resolved.roles.find(id); it != interaction.resolved.roles.end()) {
		return &(it->second);
	}
	if (auto it = interaction.resolved.users.find(id); it != interaction.resolved.users.end()) {
		return &(it->second);
	}
	throw dpp::parse_exception{"could not find mentionable " + id.str() + " in the resolved roles and users for command parameter `" + opt.name + "`"};
};

template <>
constexpr inline auto parse_param<double> = [](const dpp::interaction& interaction [[maybe_unused]], const dpp::command_data_option& opt) {
	return std::get<double>(opt.value);
};

template <>
constexpr inline auto parse_param<dpp::attachment> = [](const dpp::interaction& interaction [[maybe_unused]], const dpp::command_data_option& opt) -> const dpp::attachment& {
	dpp::snowflake id = std::get<dpp::snowflake>(opt.value);
	if (auto it = interaction.resolved.attachments.find(id); it != interaction.resolved.attachments.end()) {
		return it->second;
	}
	throw dpp::parse_exception{"could not find attachment " + id.str() + " in the resolved list of attachments for command parameter `" + opt.name + "`"};
};

template <typename>
struct optional_type_s {};

template <typename T>
struct optional_type_s<std::optional<T>> {
	using type = T;
};

template <typename T>
using optional_type = typename optional_type_s<T>::type;

template <typename T>
constexpr inline bool is_optional = false;

template <typename T>
constexpr inline bool is_optional<std::optional<T>> = true;

template <typename Ret>
using command_resolver = std::function<Ret(const dpp::slashcommand_t&, std::span<const dpp::command_data_option>)>;

}

class command_handler {
public:
	using command_fun = std::variant<detail::command_resolver<dpp::coroutine<>>, detail::command_resolver<void>>;

	class subcommand {
		friend class command_handler;
	public:
		template <typename Callable>
		subcommand(std::string name_, std::string desc_, Callable&& callable, std::vector<dpp::command_option> options_ = {});

	private:
		std::string name;
		std::string description;
		std::vector<dpp::command_option> options;
		command_fun fun;
	};

	struct subcommand_group {
		std::string name;
		std::string description;
		std::vector<subcommand> subcommands;
	};

	command_handler(mimiron &bot);

	dpp::task<> operator()(const dpp::slashcommand_t &event);

	dpp::coroutine<std::expected<size_t, dpp::error_info>> fetch_commands(dpp::cluster &bot);
	dpp::coroutine<std::expected<size_t, dpp::error_info>> register_commands(dpp::cluster &bot);

	template <typename Callable>
	command_handler& add_command(const dpp::slashcommand& cmd, Callable&& callable);

	command_handler& add_command(const dpp::slashcommand& cmd, std::vector<subcommand> subcommands);

	command_handler& add_command(const dpp::slashcommand& cmd, std::vector<subcommand_group> subcommand_groups);

private:
	struct final_subcommand_node {
		std::string name;
		command_fun handler;
	};

	struct subcommand_node {
		std::string name;
		std::variant<command_fun, std::vector<final_subcommand_node>> body;
	};

	struct slashcommand_node {
		slashcommand_node(const std::string& name, const std::string& description);

		friend class command_handler;

		const std::string& get_name() const noexcept;

		const dpp::slashcommand& get_slashcommand() const noexcept;

		const dpp::command_option &get_option(std::string_view name) const;

		dpp::slashcommand command;
		std::variant<command_fun, std::vector<subcommand_node>> body;
	};

	command_fun* find_command(std::string_view name, const dpp::command_data_option* subcommand,  const dpp::command_data_option* subcommand_group);

	dpp::snowflake app_id;
	std::vector<slashcommand_node> commands;
	mimiron &_bot;
};

namespace detail {

template <typename Ret, typename... Args, size_t... Ns>
command_handler::command_fun make_command_handler(std::function<Ret(const dpp::slashcommand_t&, Args...)> fun, std::span<dpp::command_option> options, std::index_sequence<Ns...>) {
	using args = std::tuple<const dpp::slashcommand_t&, Args...>;
	/* this below is executed at command creation */
	if (auto num_opts = std::ranges::size(options); num_opts != sizeof...(Args)) {
		throw dpp::logic_exception{"wrong number of parameters for slashcommand (" + std::to_string(num_opts) + "), command handler function has " + std::to_string(sizeof...(Args))};
	}
	/* this below is executed at command execution */
	return command_handler::command_fun{
		std::in_place_index<std::is_same_v<Ret, dpp::coroutine<>> ? 0 : 1>,
		[fun = std::move(fun), my_options = options | std::views::transform(&dpp::command_option::name) | std::ranges::to<std::vector>()](const dpp::slashcommand_t& command, std::span<const dpp::command_data_option> options) mutable -> Ret {
			constexpr auto parse_arg = []<size_t N>(const dpp::interaction& interaction, std::span<const dpp::command_data_option> options, const std::string& expected_opt) -> std::tuple_element_t<N + 1, args> {
				auto provided_it = std::ranges::find(options, expected_opt, &dpp::command_data_option::name);
				constexpr bool required = !is_optional<std::tuple_element_t<N + 1, args>>;
				if (provided_it == options.end()) { // option not provided
					if constexpr (required) {
						throw dpp::parse_exception{"could not find required argument `" + expected_opt + "` in command"};
					} else {
						return std::nullopt;
					}
				}
				if constexpr (required) {
					return parse_param<std::remove_cvref_t<std::tuple_element_t<N + 1, args>>>(interaction, *provided_it);
				} else {
					return parse_param<std::remove_cvref_t<optional_type<std::tuple_element_t<N + 1, args>>>>(interaction, *provided_it);
				}
			};
			return std::invoke(fun, command, parse_arg.template operator()<Ns>(command.command, options, my_options[Ns])...);
	}};
};

}

template <typename Callable>
command_handler& command_handler::add_command(const dpp::slashcommand& cmd, Callable &&callable) {
	using args_tuple = detail::callable_args<Callable>;
	auto it = std::ranges::lower_bound(commands, cmd.name, std::less{}, [](const slashcommand_node& node) { return node.get_slashcommand().name; });

	if (it != commands.end() && it->command.name == cmd.name) {
		throw dpp::logic_exception("command " + cmd.name + " already exists");
	}
	it = commands.emplace(it, cmd.name, cmd.description);
	if constexpr (std::tuple_size_v<args_tuple> > 1) {
		constexpr auto set_options = []<size_t... Ns>(std::vector<dpp::command_option>& parameters, std::index_sequence<Ns...>) {
			constexpr auto make_option = []<size_t N>(std::vector<dpp::command_option>& parameters_) {
				constexpr dpp::command_option_type option_type = detail::command_param_to_option_type<std::remove_cvref_t<std::tuple_element_t<N + 1, args_tuple>>>;
				constexpr bool required = !detail::is_optional<std::tuple_element_t<N + 1, args_tuple>>;
				parameters_[N].type = option_type;
				parameters_[N].required = required;
			};
			(make_option.template operator()<Ns>(parameters), ...);
		};
		set_options(it->command.options, std::make_index_sequence<std::tuple_size_v<args_tuple> - 1>());
	}
	it->body = detail::make_command_handler(std::function{std::forward<Callable>(callable)}, it->command.options, std::make_index_sequence<std::tuple_size_v<args_tuple> - 1>{});
	return *this;
}

template <typename Callable>
command_handler::subcommand::subcommand(std::string name_, std::string desc_, Callable&& callable, std::vector<dpp::command_option> options_) :
	name{std::move(name_)},
	description{std::move(desc_)},
	options{std::move(options_)},
	fun{} {
	using args_tuple = detail::callable_args<Callable>;
	if constexpr (std::tuple_size_v<args_tuple> > 1) {
		constexpr auto set_options = []<size_t... Ns>(std::vector<dpp::command_option>& parameters, std::index_sequence<Ns...>) {
			constexpr auto make_option = []<size_t N>(std::vector<dpp::command_option>& parameters_) {
				constexpr dpp::command_option_type option_type = detail::command_param_to_option_type<std::remove_cvref_t<std::tuple_element_t<N + 1, args_tuple>>>;
				constexpr bool required = !detail::is_optional<std::tuple_element_t<N + 1, args_tuple>>;
				parameters_[N].type = option_type;
				parameters_[N].required = required;
			};
			(make_option.template operator()<Ns>(parameters), ...);
		};
		set_options(options, std::make_index_sequence<std::tuple_size_v<args_tuple> - 1>());
	}
	fun = detail::make_command_handler(std::function{std::forward<Callable>(callable)}, options, std::make_index_sequence<std::tuple_size_v<detail::callable_args<Callable>> - 1>{});
}


inline command_handler::slashcommand_node::slashcommand_node(const std::string& name, const std::string& description) :
	command{name, description, 0},
	body{std::in_place_index<0>}
{
}

}
