#include "database.h"
#include "query.h"

namespace mimiron::sql {

database_exception::database_exception(std::string msg) :
	message{std::move(msg)}
{}

const char *database_exception::what() const noexcept {
	return message.c_str();
}

mysql_database::mysql_database(const connection_info& info) :
	db_info{info},
	connection{mysql_init(nullptr)} {
	if (MYSQL* mysql = connect(); !mysql) {
		throw database_exception{mysql_error(connection.get())};
	} else {
		mysql_options(mysql, MYSQL_READ_DEFAULT_GROUP, "mimiron");
	}
	request_thread = std::jthread{&mysql_database::run_queue, this};
}

mysql_database::mysql_database(const connection_info& info, std::nullopt_t) :
	db_info{info},
	connection{mysql_init(nullptr)} {
}

mysql_database::~mysql_database() {
	running = false;
	request_cv.notify_all();
}

void mysql_database::run_queue() {
	std::unique_lock lock{request_mutex, std::defer_lock};

	running = true;
	while (running) {
		lock.lock();
		request_cv.wait(lock, [this]() { return !running || !request_queue.empty(); });
		if (!running) {
			return;
		}
		std::vector<request> requests = std::move(request_queue);
		lock.unlock();
		for (request& fun : requests) {
			fun();
		}
	}
}

MYSQL *mysql_database::connect() {
	return mysql_real_connect(
		connection.get(),
		db_info.host.empty() ? nullptr : db_info.host.c_str(),
		db_info.username.empty() ? nullptr : db_info.username.c_str(),
		db_info.password.empty() ? nullptr : db_info.password.c_str(),
		db_info.database.empty() ? nullptr : db_info.database.c_str(),
		db_info.port,
		nullptr,
		0
	);
}

}
