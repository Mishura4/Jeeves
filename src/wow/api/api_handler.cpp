#include <concepts>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <memory>

#include <boost/pfr.hpp>
#include "api_handler.h"

#include "database/database.h"
#include "tools/parse_json.h"
#include "tools/tuple.h"

namespace dpp {

/**
 * @brief Generic awaitable class, represents a future value that can be co_await-ed on.
 *
 * Roughly equivalent of std::future for coroutines, with the crucial distinction that the future does not own a reference to a "shared state".
 * It holds a non-owning reference to the promise, which must be kept alive for the entire lifetime of the awaitable.
 *
 * @tparam T Type of the asynchronous value
 * @see promise
 */
template <typename T>
class shared_awaitable : public basic_awaitable<shared_awaitable<T>> {
protected:
	friend class detail::promise::promise_base<T>;

	struct state {
		std::shared_mutex mutex;

		/**
		 * @brief Variant representing one of either 3 states of the result value : empty, result, exception.
	 	 */
		using storage_type = std::variant<std::monostate, std::conditional_t<std::is_void_v<T>, detail::promise::empty, T>, std::exception_ptr>;

		storage_type stored_value;

		std::vector<std::coroutine_handle<>> awaiters;
	};

	std::shared_ptr<state> shared_state;

	/**
	 * @brief Awaiter returned by co_await.
	 *
	 * Contains the await_ready, await_suspend and await_resume functions required by the C++ standard.
	 * This class is CRTP-like, in that it will refer to an object derived from awaitable.
	 *
	 * @tparam Derived Type of reference to refer to the awaitable.
	 */
	template <typename Derived>
	struct awaiter {
		Derived &awaitable_obj;

		/**
		 * @brief First function called by the standard library when co_await-ing this object.
		 *
		 * @throws dpp::logic_exception If the awaitable's valid() would return false.
		 * @return bool Whether the result is ready, in which case we don't need to suspend
		 */
		bool await_ready() const;

		/**
		 * @brief Second function called by the standard library when co_await-ing this object.
		 *
		 * @throws dpp::logic_exception If the awaitable's valid() would return false.
		 * At this point the coroutine frame was allocated and suspended.
		 *
		 * @return bool Whether we do need to suspend or not
		 */
		bool await_suspend(detail::std_coroutine::coroutine_handle<> handle);

		/**
		 * @brief Third and final function called by the standard library when co_await-ing this object, after resuming.
		 *
		 * @throw ? Any exception that occured during the retrieval of the value will be thrown
		 * @return T The result.
		 */
		std::add_const_t<std::add_lvalue_reference_t<T>> await_resume();
	};

public:
	/**
	 * @brief Construct an empty awaitable.
	 *
	 * Such an awaitable must be assigned a promise before it can be awaited.
	 */
	shared_awaitable() = default;

	/**
	 * @brief Move from another awaitable.
	 *
	 * @param rhs The awaitable to move from, left in an unspecified state after this.
	 */
	shared_awaitable(awaitable<T>&& rhs) : shared_state(std::make_shared<state>()) {
		[](awaitable<T> awaitable, std::shared_ptr<state> state) -> job {
			if constexpr (std::is_void_v<T>) {
				co_await awaitable;
			} else {
				state->stored_value = co_await awaitable;
			}

			std::vector<std::coroutine_handle<>> awaiters;
			{
				std::unique_lock lock{state->mutex};

				awaiters = std::exchange(state->awaiters, {});
			}
			for (std::coroutine_handle<> handle : awaiters) {
				handle.resume();
			}
		}(std::move(rhs), shared_state);
	}

	/**
	 * @brief Check whether this awaitable refers to a valid promise.
	 *
	 * @return bool Whether this awaitable refers to a valid promise or not
	 */
	bool valid() const noexcept;

	/**
	 * @brief Check whether or not co_await-ing this would suspend the caller, i.e. if we have the result or not
	 *
	 * @return bool Whether we already have the result or not
	 */
	bool await_ready() const;

	/**
	 * @brief Overload of the co_await operator.
	 *
	 * @return Returns an @ref awaiter referencing this awaitable.
	 */
	template <typename Derived>
	requires (std::is_base_of_v<shared_awaitable, std::remove_cv_t<Derived>>)
	friend awaiter<Derived&> operator co_await(Derived& obj) noexcept {
		return {obj};
	}

	/**
	 * @brief Overload of the co_await operator. Returns an @ref awaiter referencing this awaitable.
	 *
	 * @return Returns an @ref awaiter referencing this awaitable.
	 */
	template <typename Derived>
	requires (std::is_base_of_v<shared_awaitable, std::remove_cv_t<Derived>>)
	friend awaiter<Derived&&> operator co_await(Derived&& obj) noexcept {
		return {std::move(obj)};
	}
};

template <typename T>
bool shared_awaitable<T>::valid() const noexcept {
	return shared_state != nullptr;
}

template <typename T>
bool shared_awaitable<T>::await_ready() const {
	if (!this->valid()) {
		throw dpp::logic_exception("cannot co_await an empty awaitable");
	}
	std::shared_lock lock{shared_state->mutex};

	return shared_state->stored_value.index() > 0;
}

template <typename T>
template <typename Derived>
bool shared_awaitable<T>::awaiter<Derived>::await_suspend(detail::std_coroutine::coroutine_handle<> handle) {
	std::unique_lock lock{awaitable_obj.shared_state->mutex};

	if (shared_state->stored_value.index() > 0) {
		return false;
	}
	awaitable_obj.shared_state->awaiters.push_back(handle);
	return true;
}

template <typename T>
template <typename Derived>
std::add_const_t<std::add_lvalue_reference_t<T>> shared_awaitable<T>::awaiter<Derived>::await_resume() {
	std::shared_lock lock{awaitable_obj.shared_state->mutex};
	auto &state = awaitable_obj.shared_state;

	if (std::holds_alternative<std::exception_ptr>(state.stored_value)) {
		std::rethrow_exception(std::get<2>(state.stored_value));
	}
	if constexpr (!std::is_void_v<T>) {
		return std::get<1>(std::move(state.stored_value));
	} else {
		return;
	}
}

}

namespace stdfs = std::filesystem;

namespace mimiron::wow {

namespace {

template <typename T>
std::shared_mutex cache_mutex;

template <typename T>
cache<std::string, T> resource_cache;

constexpr fixed_string<32> resource_namespace(resource_location const& loc, api_namespace n) noexcept {
	fixed_string<32> ret;
	fixed_string<32>::iterator_t it = ret.begin();

	switch (n) {
		case api_static:
			it = std::copy_n("static", 6, it);
			break;

		case api_dynamic:
			it = std::copy_n("dynamic", 7, it);
			break;

		case api_profile:
			it = std::copy_n("profile", 7, it);
			break;
	}

	switch (loc.version) {
		case retail:
			break;

		case progression:
			it = std::copy_n("-classic", 8, it);
			break;

		case classic_era:
			it = std::copy_n("-classic1x", 10, it);
			break;
	}
	*it = '-';
	++it;
	*it = loc.region_code[0];
	++it;
	*it = loc.region_code[1];
	++it;
	ret.str_size = std::distance(ret.begin(), it);
	std::fill(it, ret.str.end(), 0);
	return {ret};
}

constexpr std::pair<std::string, std::string> namespace_header(resource_location const& loc, api_namespace n) {
	return {"Battlenet-Namespace", std::string{resource_namespace(loc, n)}};
}

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


	/*auto cache_path = r.ns + ':' + r.path;

	{
		std::shared_lock shared_lock{cache_mutex<T>};

		if (auto resource = resource_cache<T>.find(cache_path); resource) {
			co_return resource;
		}
	}

	auto dir = stdfs::current_path() / "cache" / r.ns / std::string_view{r.path.data(), pathlen};

	auto path = dir / std::string_view{buf, end};
	{
		std::shared_lock fs_lock{_fs_mutex};

		if (std::ifstream fs{path, std::ios::in | std::ios::binary}; fs.good()) {
			cache_file_header header;

			serialize<cache_file_header>.out(fs, header);
			if (header.last_updated + header.cache_for_directive >= system_clock::now()) {
				fs.seekg(cache_file_header_padding, std::ios::cur);

				T val;
				serialize<T>.out(fs, val);
				fs.close();
				fs_lock.unlock();
				std::unique_lock shared_lock{cache_mutex<T>};

				auto [_, res] = resource_cache<T>.try_emplace(cache_path, val);
				co_return res;
			}
		}
	}*/

	/*
	resource<T> res;
	{
		std::unique_lock shared_lock{cache_mutex<T>};

		std::tie(std::ignore, res) = resource_cache<T>.try_emplace(cache_path, val);
	}

	{
		std::unique_lock fs_lock{_fs_mutex};

		if (std::ofstream fs{path, std::ios::out | std::ios::binary | std::ios::trunc}; fs.good()) {
			cache_file_header header;

			// TODO: cache for directive
			serialize<cache_file_header>.in(fs, header);
			if (header.last_updated + header.cache_for_directive >= system_clock::now()) {
				char zero[cache_file_header_padding] = {};

				fs.write(zero, cache_file_header_padding);

				serialize<T>.in(fs, val);
				fs.close();
				fs_lock.unlock();
			}
		}
	}*/

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

/*auto api_handler::get_realms(const resource_location& location) -> async_request<std::vector<realm_entry>> {
	co_return co_await _fetch<std::vector<realm_entry>>(request{
		.host = location.host,
		.path = "/data/wow/realm/",
		.ns = std::string{resource_namespace(location, api_dynamic)},
		.output_field = "realms",
	}, "index");
}

auto api_handler::get_realm(const resource_location& location, std::string_view realm_slug) -> async_request<realm> {
	co_return co_await _fetch<realm>(request{
		.host = location.host,
		.path = "/data/wow/realm/",
		.ns = std::string{resource_namespace(location, api_dynamic)}
	}, realm_slug);
}

auto api_handler::get_realm(const resource_location& location, uint64_t id) -> async_request<realm> {
	co_return co_await _fetch<realm>(request{
		.host = location.host,
		.path = "/data/wow/realm/",
		.ns = std::string{resource_namespace(location, api_dynamic)}
	}, id);
}*/

auto api_handler::get(std::string_view path, std::string_view resource_namespace) -> async_request {
	return _queue(path, resource_namespace);
}

namespace {

rest_resource _parse_response(dpp::http_request_completion_t& result) {
	rest_resource ret;

	auto           [begin, end] = result.headers.equal_range("cache-control");
	constexpr auto max_age_field = "max-age="sv;
	for (const auto& [key, value] : std::ranges::subrange{begin, end}) {
		for (auto node : value | std::views::split(',')) {
			std::string_view str{node};
			size_t           word_begin = str.find_first_not_of(' ');

			if (word_begin == std::string_view::npos)
				break;

			size_t           word_end = str.find_last_of(' ');
			str = str.substr(word_begin, word_end == std::string_view::npos ? word_end : word_end - word_begin);
			if (str.starts_with(max_age_field)) {
				seconds::rep max_age;

				if (auto [_, errc] = from_chars(str.substr(max_age_field.size()), max_age); errc == std::errc{}) {
					ret.cache_control.max_age = std::chrono::duration_cast<system_clock::duration>(seconds{max_age});
				}
			}
		}
	}
	mimiron::tie(begin, end) = result.headers.equal_range("age");
	for (const auto& [key, value] : std::ranges::subrange{begin, end}) {
		seconds::rep age;

		if (auto [_, errc] = from_chars(value, age); errc == std::errc{}) {
			ret.cache_control.age = std::chrono::duration_cast<system_clock::duration>(seconds{age});
		}
	}
	ret.data = std::vector<std::byte>{reinterpret_cast<std::byte const*>(result.body.data()), reinterpret_cast<std::byte const*>(result.body.data()) + result.body.size()};
	return ret;
}

}

auto api_handler::_do_request(dpp::http_method method, std::string_view path, std::string_view resource_namespace) -> async_request {
	std::multimap<std::string, std::string> headers{
		{ "Battlenet-Namespace", std::string{resource_namespace} }
	};

	{
		std::shared_lock lock{_credentials_mutex};

		if (minutes{_api_credentials.expires_in} < 5min) {
			lock.unlock();

			std::unique_lock lock_{_credentials_mutex};

			if (minutes{_api_credentials.expires_in} < 5min) {
				_api_credentials = co_await _request_access();
			}
		}
		headers.emplace("Authorization", "Bearer " + _api_credentials.access_token);
	}

	dpp::http_request_completion_t result = co_await _cluster.co_request(std::string{path}, method, {}, "application/x-www-form-urlencoded", headers, "1.0");

	if (result.status >= 300) {
		co_return dpp::error_info{result.status, {}, {}, {}};
	}
	co_return _parse_response(result);
}


auto api_handler::_queue(std::string_view path, std::string_view resource_namespace) -> async_request {
	std::unique_lock lock{_queue_mutex};
	if (_requests_last_hour.size() >= 32000 || _requests_last_second.size() >= 80) {
		dpp::promise<void> queue_promise;
		_requests_queue.push(&queue_promise);
		lock.unlock();
		co_await queue_promise.get_awaitable();
	} else {
		auto now = app_clock::now();
		_requests_last_hour.push(now);
		_requests_last_second.push(now);
		lock.unlock();
	}
	co_return co_await _do_request(dpp::m_get, path, resource_namespace);
}

}
