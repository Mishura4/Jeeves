#include "resource_manager.h"

#include <source_location>
#include <fstream>
#include <chrono>

#include "exception.h"

#include "realm.h"
#include "region.h"
#include "tools/parse_json.h"

namespace mimiron::wow {

namespace {

using empty = struct {};

using file_duration = std::chrono::file_clock::duration;
using file_time = std::chrono::file_time<file_duration>;

template <typename T>
struct disk_resource {
	file_time last_updated;
	file_time expiration_time;
	uint64_t build;
	std::variant<std::monostate, T, std::vector<std::byte>> data;
};

enum cache_resource_type {
	resource,
	json,
	table
};

struct cache_file_header {
	std::array<char, 8> magic_number;
	uint64_t cache_format{};
	file_time last_updated{};
	seconds valid_for{};
	cache_resource_type type;
	uint64_t build;
};

constexpr inline auto zero = std::array<char, 128>{};

template <typename T>
struct serializer_t {
	template <typename S>
	void in(S &stream, const T& value) const {
		stream.write(reinterpret_cast<char const*>(&value), sizeof(value));
	}

	template <typename S>
	void out(S &stream, T& value) const {
		stream.read(reinterpret_cast<char*>(&value), sizeof(value));
	}
};;

template <typename T, size_t N>
struct serializer_t<T[N]> {
	template <typename S>
	void in(S &stream, const T (&value)[N]) const {
		[]<size_t... Ns>(S &s, const T (&v)[N], std::index_sequence<Ns...>) {
			(serializer_t<T>{}.in(s, v[Ns]), ...);
		}(stream, value, std::make_index_sequence<N>{});
	}

	template <typename S>
	void out(S &stream, T (&value)[N]) const {
		[]<size_t... Ns>(S &s, T (&v)[N], std::index_sequence<Ns...>) {
			(serializer_t<T>{}.out(s, v[Ns]), ...);
		}(stream, value, std::make_index_sequence<N>{});
	}
};

template <typename T, size_t N>
struct serializer_t<std::array<T, N>> {
	template <typename S>
	void in(S &stream, const std::array<T, N>& value) const {
		[]<size_t... Ns>(S &s, std::array<T, N> const& v, std::index_sequence<Ns...>) {
			(serializer_t<T>{}.in(s, v[Ns]), ...);
		}(stream, value, std::make_index_sequence<N>{});
	}

	template <typename S>
	void out(S &stream, std::array<T, N>& value) const {
		[]<size_t... Ns>(S &s, std::array<T, N>& v, std::index_sequence<Ns...>) {
			(serializer_t<T>{}.out(s, v[Ns]), ...);
		}(stream, value, std::make_index_sequence<N>{});
	}
};

template <size_t N>
struct serializer_t<char[N]> {
	template <typename S>
	void in(S &stream, const char (&value)[N]) const {
		stream.write(value, N);
	}

	template <typename S>
	void out(S &stream, char (&value)[N]) const {
		stream.read(value, N);
	}
};

template <size_t N>
struct serializer_t<std::array<char, N>> {
	template <typename S>
	void in(S &stream, const std::array<char, N>& value) const {
		stream.write(value.data(), value.size());
	}

	template <typename S>
	void out(S &stream, std::array<char, N>& value) const {
		stream.read(value.data(), value.size());
	}
};

template <typename Clock, typename Duration>
struct serializer_t<std::chrono::time_point<Clock, Duration>> {
	template <typename S>
	void in(S &stream, const std::chrono::time_point<Clock, Duration>& value) const {
		serializer_t<Duration>{}.in(stream, value.time_since_epoch());
	}

	template <typename S>
	void out(S &stream, std::chrono::time_point<Clock, Duration>& value) const {
		Duration dur;
		serializer_t<Duration>{}.out(stream, dur);
		value = std::chrono::time_point<Clock, Duration>{dur};
	}
};

template <typename T, size_t Num, size_t Den>
struct serializer_t<std::chrono::duration<T, std::ratio<Num, Den>>> {
	template <typename S>
	void in(S &stream, const std::chrono::duration<T, std::ratio<Num, Den>>& value) const {
		serializer_t<T>{}.in(stream, value.count());
	}

	template <typename S>
	void out(S &stream, std::chrono::duration<T, std::ratio<Num, Den>>& value) const {
		T val;
		serializer_t<T>{}.out(stream, val);
		value = std::chrono::duration<T, std::ratio<Num, Den>>{val};
	}
};

template <size_t N>
struct serialize_one_t {
	template <typename S, typename T>
	void in(S &stream, const T& value) const {
		serializer_t<boost::pfr::tuple_element_t<N, T>>{}.in(stream, boost::pfr::get<N>(value));
	}

	template <typename S, typename T>
	void out(S &stream, T& value) const {
		serializer_t<boost::pfr::tuple_element_t<N, T>>{}.out(stream, boost::pfr::get<N>(value));
	}
};

template <typename T>
requires std::is_aggregate_v<T>
struct serializer_t<T> {
	template <typename S>
	void in(S &stream, const T& value) const {
		[]<size_t... Ns>(S &f, const T& v, std::index_sequence<Ns...>) {
			(serialize_one_t<Ns>{}.in(f, v), ...);
		}(stream, value, std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
	}

	template <typename S>
	void out(S &stream, T& value) const {
		[]<size_t... Ns>(S &f, T& v, std::index_sequence<Ns...>) {
			(serialize_one_t<Ns>{}.out(f, v), ...);
		}(stream, value, std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
	}
};

template <typename T>
requires (std::ranges::sized_range<T> && !std::convertible_to<T, std::string_view>)
struct serializer_t<T> {
	template <typename S>
	void in(S &stream, const T& value) const {
		serializer_t<size_t>{}.in(stream, std::ranges::size(value));
		for (auto& val : value) {
			serializer_t<std::ranges::range_value_t<T>>{}.in(stream, val);
		}
	}

	template <typename S>
	void out(S &stream, T& value) const {
		std::size_t size;

		serializer_t<size_t>{}.out(stream, size);
		if constexpr (requires (T t) { t.resize(size); }) {
			value.resize(size);
			for (auto& v : value) {
				serializer_t<std::ranges::range_value_t<T>>{}.out(stream, v);
			};
		} else {
			for (auto& v : value) {
				serializer_t<std::ranges::range_value_t<T>>{}.out(stream, v);
				--size;
				if (size == 0)
					return;
			}
		}
	}
};

template <std::convertible_to<std::string_view> T>
struct serializer_t<T> {
	template <typename S>
	void in(S &stream, const T &value) {
		auto sv = std::string_view{value};

		stream << sv;
		stream.write(zero.data(), 1);
	}
};

template <>
struct serializer_t<std::string> : serializer_t<std::string_view> {
	using serializer_t<std::string_view>::in;

	template <typename S>
	void out(S &stream, std::string &value) {
		std::getline(stream, value, char{0});
	}
};

template <typename T>
inline constexpr serializer_t<std::remove_cvref_t<T>> serialize;

template <typename T>
class disk_cache {
public:
	auto load(const resource_location& location, std::string_view name) -> std::optional<disk_resource<T>>;
	void save(const resource_location& location, const disk_resource<T>& resource, std::string_view name);
	void remove(const resource_location& location, std::string_view name);
};

template <typename T>
class disk_cache_indexed : private disk_cache<T> {
public:
	using disk_cache<T>::load;
	using disk_cache<T>::remove;

	auto load(const resource_location& location, uint32_t uid) -> std::optional<disk_resource<T>>;
	void save(const resource_location& location, const disk_resource<T>& resource, std::string_view name, uint32_t uid);
	void remove(const resource_location& location, uint32_t uid);

private:
	std::unordered_map<std::int64_t, fixed_string<64>> index;
};

template <typename T>
constexpr auto resource_folder = empty{};

template <>
constexpr auto resource_folder<realm> = string_literal{"/realm"};

template <>
constexpr auto resource_folder<std::vector<realm_entry>> = string_literal{"/realm"};

template <typename T>
std::shared_mutex disk_mutex;

template <typename T>
auto get_path = [](const resource_location& location, std::string_view name) -> stdfs::path {
	static constexpr auto path = string_literal{"data/blizzard/{}/{}"} + resource_folder<realm> + string_literal{"/{}"};
	return {std::format(path.str, std::string_view{location.region_code}, std::to_underlying(location.version), name)};
};

template <typename T>
auto disk_cache<T>::load(const resource_location& location, std::string_view name) -> std::optional<disk_resource<T>> {
	stdfs::path path = get_path<T>(location, name);

	std::shared_lock lock{disk_mutex<T>};
	std::ifstream fs{path, std::ios::in | std::ios::binary};

	if (!fs.good()) {
		return std::nullopt;
	}

	std::size_t file_size;
	fs.seekg(0, std::ios::end);

	if (file_size = fs.tellg(); file_size < 128) {
		return std::nullopt;
	}

	cache_file_header header;
	
	fs.seekg(0, std::ios::beg);
	serialize<cache_file_header>.out(fs, header);
	file_time expiration_time = header.last_updated + header.valid_for;
	if (expiration_time != expiration_time.max() && expiration_time <= std::chrono::file_clock::now()) {
		return std::nullopt;
	}

	if (std::memcmp(header.magic_number.data(), "mimiron~", 8) != 0 || header.cache_format > 0) {
		return std::nullopt;
	}

	fs.seekg(128, std::ios::beg);

	switch (header.type) {
		case resource: {
			disk_resource<T> resource {
				.last_updated = header.last_updated,
				.expiration_time = expiration_time,
				.build = header.build
			};
			resource.data.template emplace<T>();
			serialize<T>.out(fs, std::get<T>(resource.data));
			return resource;
		}

		case json: {
			disk_resource<T> resource {
				.last_updated = header.last_updated,
				.expiration_time = expiration_time,
				.build = header.build
			};

			std::vector<std::byte> &vec = resource.data.template emplace<std::vector<std::byte>>();
			vec.reserve(file_size - 128);
			char buf[1024];
			while (fs.good()) {
				fs.read(buf, 1024);
				auto sz = static_cast<size_t>(fs.gcount());
				vec.append_range(std::span{reinterpret_cast<std::byte*>(buf), sz});
				if (sz < 1024) {
					break;
				}
			}

			return resource;
		}

		default:
			throw exception(std::format("unknown disk resource type #{}", std::to_underlying(header.type)));
	}
};

template <typename T>
void disk_cache<T>::save(const resource_location& location, const disk_resource<T>& resource, std::string_view name) {
	stdfs::path path = get_path<T>(location, name);

	std::unique_lock lock{disk_mutex<T>};
	std::ofstream fs{path, std::ios::out | std::ios::binary | std::ios::trunc};

	if (!fs.good()) {
		std::error_code err{};
		create_directories(path.parent_path(), err); // MAY OFTEN THROW
		fs = std::ofstream{path, std::ios::out | std::ios::binary | std::ios::trunc};
		if (!fs.good()) {
			throw std::runtime_error{"could not open file " + path.string() + " for writing"};
		}
	}

	cache_file_header header {
		.magic_number = {'m', 'i', 'm', 'i', 'r', 'o', 'n', '~'},
		.cache_format = 0,
		.last_updated = resource.last_updated,
		.valid_for = std::chrono::floor<seconds>(resource.expiration_time - resource.last_updated),
		.type = std::holds_alternative<std::vector<std::byte>>(resource.data) ? json : cache_resource_type::resource,
		.build = 0
	};

	serialize<cache_file_header>.in(fs, header);
	std::streampos pos = fs.tellp();
	fs.write(zero.data(), 128 - pos);

	if (auto const* data = std::get_if<std::vector<std::byte>>(&resource.data); data != nullptr) {
		fs.write(reinterpret_cast<char const*>(data->data()), data->size());
		return;
	}

	if (std::holds_alternative<T>(resource.data)) {
		serialize<T>.in(fs, std::get<T>(resource.data));
	}
}

template <typename T>
using promise = dpp::promise<T>;

template <typename T>
class promise_cache {
	using value_t = std::pair<std::string, promise<T>>;
public:
	std::optional<dpp::awaitable<T>> emplace(std::string key) {
		std::lock_guard lock{mutex};

		auto [begin, end] = std::ranges::equal_range(promises, key, std::less{}, &value_t::first);
		auto it = promises.emplace(end, std::move(key), promise<T>{});

		if (begin == end) {
			return std::nullopt;
		}
		return it->second.get_awaitable();
	}

	template <typename U>
	void fulfill(std::string_view key, U&& value) {
		std::unique_lock lock{mutex};

		auto eq_range = std::ranges::equal_range(promises, key, std::less{}, &value_t::first);
		std::vector<promise<T>> out = eq_range | std::views::transform(&value_t::second) | std::views::as_rvalue | std::ranges::to<std::vector>();
		promises.erase(eq_range.begin(), eq_range.end());
		lock.unlock();

		if constexpr (!std::is_same_v<std::decay_t<U>, std::exception_ptr>) {
			if (out.size() == 1) {
				out[0].set_value(std::forward<U>(value));
			} else {
				for(promise<T>& promise : out) {
					promise.set_value(std::as_const(value));
				}
			}
		} else {
			for(promise<T>& promise : out) {
				promise.set_exception(std::forward<U>(value));
			}
		}
	}

private:
	std::mutex mutex;
	std::vector<std::pair<std::string, dpp::promise<T>>> promises;
};

template <typename T>
promise_cache<T> s_promise_list;

template <typename T>
disk_cache<T> s_disk_cache;

template <typename T>
std::shared_mutex s_cache_mutex;

template <typename T>
cache<std::string, T, std::hash<std::string_view>> s_resource_cache;

constexpr fixed_string<32> location_str(resource_location const& loc, api_namespace n) noexcept {
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

template <typename T>
struct resource_api_info {
	api_namespace ns;
	const char *path;
	const char *output_field = nullptr;
	int64_t T::*idx_field = nullptr;
};

template <typename T>
auto resource_info = empty{};

template <>
auto resource_info<realm> = resource_api_info<realm> {
	api_dynamic,
	"/data/wow/realm/",
	{},
	&realm::id
};

template <>
auto resource_info<std::vector<realm_entry>> = resource_api_info<std::vector<realm_entry>> {
	api_dynamic,
	"/data/wow/realm/",
	"realms"
};

}

resource_manager::resource_manager(dpp::cluster& cluster, std::string_view api_id, std::string_view api_token) :
	_cluster{cluster},
	_api_handler{cluster, api_id, api_token} {

}

dpp::coroutine<> resource_manager::start() {
	return _api_handler.start();
}

template <typename T>
auto resource_manager::_get(resource_location const& location, std::string name) -> coroutine<T> {
	constexpr resource_api_info<T>& resource_inf = resource_info<T>;
	constexpr promise_cache<resource<T>>& promise_cache = s_promise_list<resource<T>>;
	auto namespace_str = location_str(location, resource_inf.ns);
	std::string cache_path = std::format("{}:{}", std::string_view{namespace_str}, name);

	std::optional<awaitable<T>> awaitable = promise_cache.emplace(cache_path);
	if (awaitable.has_value()) {
		co_return co_await *awaitable;
	}

	auto do_thing = [&]() -> coroutine<T> {
		if (auto resource = s_resource_cache<T>.find(name); resource) {
			co_return resource;
		}

		if (std::optional<disk_resource<T>> disk_resource = s_disk_cache<T>.load(location, name); disk_resource) {
			if (auto const* data = std::get_if<std::vector<std::byte>>(&disk_resource->data); data != nullptr) {
				try {
					auto [it, inserted] = s_resource_cache<T>.try_emplace(cache_path, parse_json<T>(nlohmann::json::parse(data->begin(), data->end())));

					co_return it;
				} catch (const std::exception &e) {
					_cluster.log(dpp::ll_warning, "exception while parsing stored json for resource " + cache_path + ": " + e.what());
				}
			} else if (std::holds_alternative<T>(disk_resource->data)) {
				auto [it, inserted] = s_resource_cache<T>.try_emplace(cache_path, std::move(std::get<T>(disk_resource->data)));

				co_return it;
			}
		}
		std::variant<rest_resource, dpp::error_info> result = co_await _api_handler.get(location.host + resource_inf.path + name, namespace_str);

		if (dpp::error_info const* info = std::get_if<1>(&result); info != nullptr) {
			throw dpp::rest_exception{!info->human_readable.empty() ? info->human_readable : std::format("REST request produced HTTP error {}", info->code)};
		}

		rest_resource&   resource = std::get<0>(result);
		disk_resource<T> res {
			.last_updated = std::chrono::file_clock::now() - resource.cache_control.age.value_or(file_duration::zero()),
			.expiration_time = resource.cache_control.max_age.has_value() ? res.last_updated + *resource.cache_control.max_age : file_time::max(),
			.build = 0
		};

		try {
			nlohmann::json j = nlohmann::json::parse(resource.data.begin(), resource.data.end());
			res.data.template emplace<T>(parse_json<T>(resource_inf.output_field == nullptr ? j : j[resource_inf.output_field]));

			auto [it, _] = s_resource_cache<T>.try_emplace(cache_path, std::get<T>(res.data));
			s_disk_cache<T>.save(location, res, name);
			co_return it;
		} catch (const std::exception &e) {
			_cluster.log(dpp::ll_warning, "exception while parsing received json for resource " + cache_path + ": " + e.what());
			res.data.template emplace<std::vector<std::byte>>(std::move(resource.data));
			s_disk_cache<T>.save(location, res, name);
			throw;
		} catch (...) {
			_cluster.log(dpp::ll_warning, "exception while parsing received json for resource " + cache_path);
			res.data.template emplace<std::vector<std::byte>>(std::move(resource.data));
			s_disk_cache<T>.save(location, res, name);
			throw;
		}
	};

	try {
		auto res = co_await do_thing();
		promise_cache.fulfill(cache_path, res);
		co_return res;
	} catch (...) {
		promise_cache.fulfill(cache_path, std::current_exception());
		throw;
	}
}

auto resource_manager::get_realm(resource_location const& location, std::string name) -> coroutine<realm> {
	co_return co_await _get<realm>(location, std::move(name));
}

auto resource_manager::get_realms(resource_location const& location) -> coroutine<std::vector<realm_entry>> {
	co_return co_await _get<std::vector<realm_entry>>(location, "index");
}


void resource_manager::set_disk_cache(stdfs::path path) noexcept {
	_fs_path = std::move(path);
}

stdfs::path const& resource_manager::disk_cache() const {
	return _fs_path;
}


}