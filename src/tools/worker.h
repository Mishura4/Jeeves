#ifndef MIMIRON_TOOLS_WORKER_H_
#define MIMIRON_TOOLS_WORKER_H_

#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <memory>
#include <utility>

#include <dpp/coro/awaitable.h>

namespace mimiron {

class worker {
public:
	template <typename Fun>
	requires (std::invocable<Fun>)
	dpp::awaitable<std::invoke_result_t<Fun>> queue(Fun&& work) {
		using ret = std::invoke_result_t<Fun>;
		auto promise = std::make_unique<dpp::promise<ret>>();
		dpp::awaitable<ret> awaitable = promise->get_awaitable();
		std::unique_lock lock{mutex};

		work_queue.emplace_back([fun = std::move(work), p = std::move(promise)] {
			try {
				p->set_value(std::invoke(fun));
			} catch (const std::exception &) {
				p->set_exception(std::current_exception());
			}
		});
		cv.notify_all();
		return awaitable;
	}

	dpp::awaitable<void> stop();

private:
	void _run();

  using work = std::move_only_function<void()>;
  std::mutex mutex;
	dpp::promise<void> end_promise;
  std::condition_variable cv;
  std::vector<work> work_queue;
	std::atomic<bool> running = false;
	std::jthread thread{&worker::_run, this};
};

}

#endif /* MIMIRON_TOOLS_WORKER_H_ */
