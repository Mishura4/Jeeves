#ifndef MIMIRON_RESOURCE_MANAGER_H_
#define MIMIRON_RESOURCE_MANAGER_H_

#include <dpp/cluster.h>
#include <string_view>
#include <shared_mutex>
#include <filesystem>

#include "common.h"
#include "wow/api/api_handler.h"
#include "tools/cache.h"

#include <unordered_map>

namespace mimiron::wow {

namespace stdfs = std::filesystem;

class resource_manager {
public:
	template <typename T>
	using resource = cached_resource<std::string, T>;

	template <typename T>
	using awaitable = dpp::awaitable<resource<T>>;

	template <typename T>
	using coroutine = dpp::coroutine<resource<T>>;

	resource_manager(dpp::cluster &cluster, std::string_view api_id, std::string_view api_token);

	dpp::coroutine<void> start();

	void set_disk_cache(stdfs::path path) noexcept;
	stdfs::path const& disk_cache() const;

	coroutine<realm> get_realm(const resource_location& location, std::string name);
	//coroutine<realm> get_realm(const resource_location& location, int64_t id);

	coroutine<std::vector<realm_entry>> get_realms(const resource_location& location);

private:
	template <typename T>
	coroutine<T> _get(const resource_location& location, std::string name);

	template <typename T>
	coroutine<T> _get(const resource_location& location, int64_t id);

	dpp::cluster& _cluster;
	api_handler _api_handler;
	std::filesystem::path _fs_path;
};

}

#endif
