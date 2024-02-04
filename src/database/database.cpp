#include "database.h"
#include "query.h"

namespace mimiron::sql {

database_exception::database_exception(std::string msg)
    : message{std::move(msg)} {}

const char *database_exception::what() const noexcept {
  return message.c_str();
}

mysql_database::mysql_database(const connection_info &info)
    : db_info{info}, connection{mysql_init(nullptr)} {
  if (MYSQL *mysql = connect(); !mysql) {
    throw database_exception{mysql_error(connection.get())};
  } else {
    mysql_options(mysql, MYSQL_READ_DEFAULT_GROUP, "mimiron");
  }
}

mysql_database::mysql_database(const connection_info &info, std::nullopt_t)
    : db_info{info}, connection{mysql_init(nullptr)} {}

MYSQL *mysql_database::connect() {
  return mysql_real_connect(
      connection.get(), db_info.host.empty() ? nullptr : db_info.host.c_str(),
      db_info.username.empty() ? nullptr : db_info.username.c_str(),
      db_info.password.empty() ? nullptr : db_info.password.c_str(),
      db_info.database.empty() ? nullptr : db_info.database.c_str(),
      db_info.port, nullptr, 0);
}

} // namespace mimiron::sql
