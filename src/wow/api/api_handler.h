#ifndef MIMIRON_WOW_API_HANDLER
#define MIMIRON_WOW_API_HANDLER

#include <any>

#include <dpp/cluster.h>
#include <dpp/coro/awaitable.h>

#include "tools/cache.h"

#include "common.h"
#include "tools/worker.h"
#include "wow/api/region.h"
#include "wow/api/realm.h"

namespace mimiron::wow {

struct cache_settings {
	enum flags {
		no_store         = 1 << 0,
		must_revalidate  = 1 << 1,
		public_resource  = 1 << 2,
		private_resource = 1 << 3,
	};

	std::optional<system_clock::duration> max_age;
	std::optional<system_clock::duration> age;
};

struct rest_resource {
	std::vector<std::byte> data;
	cache_settings         cache_control;
};

class api_handler {
public:
	using async_request = dpp::coroutine<std::variant<rest_resource, dpp::error_info>>;

	api_handler(dpp::cluster &cluster, std::string_view api_id, std::string_view api_token);

	dpp::coroutine<void> start();

	async_request get(std::string_view path, std::string_view resource_namespace);

private:
	dpp::coroutine<client_credentials> _request_access();

	async_request _queue(std::string_view path, std::string_view resource_namespace);

	async_request _do_request(dpp::http_method method, std::string_view path, std::string_view resource_namespace);

	dpp::cluster& _cluster;
	std::mutex _queue_mutex;
	std::shared_mutex _requests_mutex;
	std::shared_mutex _credentials_mutex;
	std::queue<app_timestamp> _requests_last_hour;
	std::queue<app_timestamp> _requests_last_second;
	std::queue<dpp::promise<void>*> _requests_queue;
	std::string_view _api_id;
	std::string_view _api_token;
	client_credentials _api_credentials;
};

}

#endif /* MIMIRON_WOW_API_HANDLER */
