#include "tools/worker.h"

namespace mimiron {

void worker::_run() {
	std::unique_lock lock{mutex, std::defer_lock};

	running = true;
	while (true) {
		lock.lock();
		cv.wait(lock, [this]() { return !running.load(std::memory_order_acquire) || !work_queue.empty(); });
		if (!running.load(std::memory_order_relaxed)) {
			break;
		}
		std::vector<work> requests = std::move(work_queue);
		lock.unlock();
		for (work &fun : requests) {
			fun();
		}
	}
	end_promise.set_value();
}

dpp::awaitable<void> worker::stop() {
	running.store(false, std::memory_order_release);
	cv.notify_all();
	return end_promise.get_awaitable();
}


}
