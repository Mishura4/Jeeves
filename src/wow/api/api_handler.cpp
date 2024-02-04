#include <concepts>

#include <boost/pfr.hpp>
#include "api_handler.h"

#include "../../database/database.h"

namespace mimiron::wow {

namespace {


template <typename T>
constexpr auto parse_json = [](const nlohmann::json& j) {
	return j.get<T>();
};

template <typename T>
constexpr auto parse_json<std::vector<T>> = [](const nlohmann::json& j) {
	std::vector<T> ret;

	for (auto sub_object : j.items()) {
		ret.emplace_back(parse_json<T>(sub_object.value()));
	}
	return ret;
};

template <typename T, size_t N>
constexpr auto parse_one = [](const nlohmann::json& j, T& value) {
	using field = boost::pfr::tuple_element_t<N, T>;
	const auto& name = boost::pfr::get_name<N, T>();
	if (auto it = j.find(name); it == j.end()) {
		if constexpr (sql::is_optional<T>) {
			boost::pfr::get<N>(value) = std::nullopt;
		} else if (std::is_default_constructible_v<T>) {
			boost::pfr::get<N>(value) = {};
		} else {
			throw dpp::parse_exception{std::format("non-optional field `{}` is not present", name)};
		}
	} else {
		if constexpr (std::is_arithmetic_v<field>) {
			if (it->is_string()) {
				std::string val = *it;
				auto [end, err] = std::from_chars(val.data(), val.data() + val.size(), boost::pfr::get<N>(value));
				if (err != std::errc{}) {
					throw dpp::parse_exception{std::format("failed to parse number in string \"{}\" for field `{}`", val, name)};
				}
				return;
			}
			boost::pfr::get<N>(value) = *it;
		} else if constexpr (std::is_same_v<field, std::string>) {
			if (it->is_number()) {
				if (it->is_number_float()) {
					boost::pfr::get<N>(value) = std::to_string(static_cast<long double>(j));
				} else {
					boost::pfr::get<N>(value) = std::to_string(static_cast<int64_t>(j));
				}
				return;
			}
			boost::pfr::get<N>(value) = *it;
		} else {
			boost::pfr::get<N>(value) = parse_json<field>(it.value());
		}
	}
};

template <typename T>
requires (std::is_aggregate_v<T>)
constexpr auto parse_json<T> = [](const nlohmann::json& j) {
	T value;

	[]<size_t... Ns>(const nlohmann::json& j, T& value, std::index_sequence<Ns...>) {
		(parse_one<T, Ns>(j, value), ...);
	}(j, value, std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
	return value;
};

}

api_handler::api_handler(dpp::cluster& cluster, std::string_view api_id, std::string_view api_token) :
	_cluster{cluster},
	_api_id{api_id},
	_api_token{api_token}
{
}

dpp::coroutine<> api_handler::start() {
	_cluster.log(dpp::ll_info, "initializing communication with the WoW API");
	auto credentials = co_await _request_access();
	_cluster.log(dpp::ll_info, "communication with the WoW API established\n");
	std::scoped_lock lock{_credentials_mutex};

	_api_credentials = std::move(credentials);
	co_return;
}


dpp::coroutine<client_credentials> api_handler::_request_access() {
	_cluster.log(dpp::ll_info, "querying the WoW API for an authorization token...");
	std::string auth = std::format("{}:{}", _api_id, _api_token);
	dpp::promise<dpp::http_request_completion_t> p;
	_cluster.request(
		"https://oauth.battle.net/token", dpp::m_post,
		[&] (const dpp::http_request_completion_t& result) { p.set_value(result); },
		"grant_type=client_credentials",
		"application/x-www-form-urlencoded", {
			{"Authorization", "Basic " + dpp::base64_encode(reinterpret_cast<unsigned char*>(auth.data()), static_cast<uint32_t>(auth.size()))}
		},
		"1.0"
	);
	auto result = co_await p.get_awaitable();
	if (result.status >= 300) {
		throw dpp::rest_exception{"bad wow api credentials"};
	}
	auto credentials = parse_json<client_credentials>(nlohmann::json::parse(result.body));

	_cluster.log(dpp::ll_info, std::format("WoW API authorization token obtained, expires in {}", std::chrono::hh_mm_ss{seconds{credentials.expires_in}}));
	co_return credentials;
}

auto api_handler::get_realms(api_region const& reg, game_version version) -> dpp::coroutine<std::vector<realm_entry>> {
	co_return co_await _queue<std::vector<realm_entry>>(reg, version, api_dynamic, "realm", "index", "realms");
}

template <typename T>
dpp::coroutine<T> api_handler::_queue(api_region const& reg, game_version version, api_namespace ns, const std::string& endpoint, const std::string& resource, const std::string& output_field) {
	co_return co_await _queue<T>(
		std::format("{}/{}/wow/{}/{}?locale=en_US", reg.url, (ns == api_profile ? "profile" : "data"), endpoint, resource),
		output_field,
		{},
		{{"Battlenet-Namespace", resource_namespace(reg, version, ns)}}
	);
}

template <typename T>
dpp::coroutine<T> api_handler::_queue(const std::string& url, const std::string& output_field, const std::string& body, std::multimap<std::string, std::string> headers, const std::string& mimetype) {
	dpp::promise<T>   promise;

	std::unique_lock lock{_queue_mutex};
	if (_requests_last_hour.size() >= 32000 || _requests_last_second.size() >= 80) {
		dpp::promise<void> queue_promise;
		_requests_queue.push(&queue_promise);
		lock.unlock();
		co_await queue_promise.get_awaitable();
		auto now = app_clock::now();
		_requests_last_hour.push(now);
		_requests_last_second.push(now);
		_queue_lock.unlock();
	} else {
		auto now = app_clock::now();
		_requests_last_hour.push(now);
		_requests_last_second.push(now);
		lock.unlock();
	}

	bool refresh_credentials = false;
	{
		std::shared_lock lock{_credentials_mutex};

		if (minutes{_api_credentials.expires_in} < 5min) {
			refresh_credentials = true;
		} else {
			headers.emplace("Authorization", "Bearer " + _api_credentials.access_token);
		}
	}
	if (refresh_credentials)
	{
		std::unique_lock lock{_credentials_mutex};

		if (minutes{_api_credentials.expires_in} < 5min) {
			_api_credentials = co_await _request_access();
		}
		headers.emplace("Authorization", "Bearer " + _api_credentials.access_token);
	}
	_cluster.request(std::string{url}, dpp::m_get, [&] (const dpp::http_request_completion_t& result) {
		try {
			if (result.status >= 300) {
				throw dpp::rest_exception("HTTP Error " + std::to_string(result.status) + (result.body.empty() ? std::string{} : ": " + result.body));
			}
			if (output_field.empty()) {
				promise.set_value(parse_json<T>(nlohmann::json::parse(result.body)));
			} else {
				promise.set_value(parse_json<T>(nlohmann::json::parse(result.body)[output_field]));
			}
		} catch (const std::exception &) {
			promise.set_exception(std::current_exception());
		}
	}, body, mimetype.empty() ? "application/x-www-form-urlencoded" : mimetype, headers, "1.0");
	co_return co_await promise.get_awaitable();
}


}
