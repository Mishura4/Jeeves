#ifndef MIMIRON_WOW_API_HANDLER
#define MIMIRON_WOW_API_HANDLER

#include <dpp/cluster.h>
#include <dpp/coro/awaitable.h>

#include "common.h"
#include "tools/worker.h"
#include "wow/api/region.h"
#include "wow/api/realm.h"

namespace mimiron::wow {

class api_handler {
public:
	api_handler(dpp::cluster &cluster, std::string_view api_id, std::string_view api_token);

	auto get_realms(api_region const& reg, game_version version) -> dpp::coroutine<std::vector<realm_entry>>;

	dpp::coroutine<void> start();

private:
	dpp::coroutine<client_credentials> _request_access();

	template <typename T>
	dpp::coroutine<T> _queue(api_region const& reg, game_version version, api_namespace ns, const std::string& endpoint, const std::string& resource = {}, const std::string& output_field = {});

	template <typename T>
	dpp::coroutine<T> _queue(
		const std::string& url, const std::string& output_field = {}, const std::string& body = {}, std::multimap<std::string, std::string> headers = {}, const std::string& mimetype = {}
	);

	dpp::cluster& _cluster;
	std::mutex _queue_mutex;
	std::unique_lock<std::mutex> _queue_lock{_queue_mutex, std::defer_lock};
	std::queue<app_timestamp> _requests_last_hour;
	std::queue<app_timestamp> _requests_last_second;
	std::queue<dpp::promise<void>*> _requests_queue;
	std::string_view _api_id;
	std::string_view _api_token;
	std::shared_mutex _credentials_mutex;
	client_credentials _api_credentials;
	worker _worker;
};

}

#endif /* MIMIRON_WOW_API_HANDLER */
