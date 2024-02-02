#ifndef MIMIRON_DATABASE_H_
#define MIMIRON_DATABASE_H_

#include <cassert>
#include <expected>
#include <functional>
#include <memory>
#include <type_traits>
#include <string>
#include <optional>
#include <shared_mutex>

#include <dpp/coro/task.h>
#include <dpp/coro/async.h>
#include <mysql.h>

#include "query.h"

namespace mimiron::sql {

template <typename T, auto Deleter>
struct unique_ptr_deleter {
	void operator()(T* ptr) const noexcept(std::is_nothrow_invocable_v<decltype(Deleter), T*>) {
		Deleter(ptr);
	}
};

template <typename T, auto Deleter, template <typename, typename> typename PtrType = std::unique_ptr>
using managed_ptr = PtrType<T, unique_ptr_deleter<T, Deleter>>;

class database_exception : public std::exception {
public:
	database_exception() = default;
	template <typename... Args>
	database_exception(std::basic_format_string<Args...> fmt, Args&&... args) :
		message{std::format(fmt, std::forward<Args>(args)...)}
	{}

	database_exception(std::string msg);
	database_exception(const database_exception&) = default;
	database_exception(database_exception&&) = default;

	database_exception& operator=(const database_exception&) = default;
	database_exception& operator=(database_exception&&) = default;

	const char *what() const noexcept;

private:
	std::string message{};
};

enum class query_type {
	query_error,
	query_select
};

using enum query_type;

template <size_t N>
struct mysql_placeholders {
	std::array<MYSQL_BIND, N> data{};
	std::array<bool, N> is_set{};
};

template <>
struct mysql_placeholders<std::numeric_limits<size_t>::max()> {
	std::vector<MYSQL_BIND> data{};
};

template <typename T>
inline constexpr auto mysql_type_in = empty{};

template <> inline constexpr auto mysql_type_in<signed char> = MYSQL_TYPE_TINY;
template <> inline constexpr auto mysql_type_in<char> = MYSQL_TYPE_TINY;
template <> inline constexpr auto mysql_type_in<int> = MYSQL_TYPE_LONG;
template <> inline constexpr auto mysql_type_in<long long> = MYSQL_TYPE_LONGLONG;
template <> inline constexpr auto mysql_type_in<float> = MYSQL_TYPE_FLOAT;
template <> inline constexpr auto mysql_type_in<double> = MYSQL_TYPE_DOUBLE;
template <> inline constexpr auto mysql_type_in<char*> = MYSQL_TYPE_STRING;
template <> inline constexpr auto mysql_type_in<std::byte*> = MYSQL_TYPE_BLOB;
template <size_t N> inline constexpr auto mysql_type_in<char[N]> = MYSQL_TYPE_STRING;

template <typename T>
inline constexpr auto mysql_type_out = empty{};

template <> inline constexpr auto mysql_type_out<signed char> = MYSQL_TYPE_TINY;
template <> inline constexpr auto mysql_type_out<char> = MYSQL_TYPE_TINY;
template <> inline constexpr auto mysql_type_out<short> = MYSQL_TYPE_SHORT;
template <> inline constexpr auto mysql_type_out<int> = MYSQL_TYPE_LONG;
template <> inline constexpr auto mysql_type_out<long long> = MYSQL_TYPE_LONGLONG;
template <> inline constexpr auto mysql_type_out<float> = MYSQL_TYPE_FLOAT;
template <> inline constexpr auto mysql_type_out<double> = MYSQL_TYPE_DOUBLE;
template <> inline constexpr auto mysql_type_out<char*> = MYSQL_TYPE_BLOB;
template <> inline constexpr auto mysql_type_out<std::byte*> = MYSQL_TYPE_BLOB;
template <size_t N> inline constexpr auto mysql_type_out<char[N]> = MYSQL_TYPE_STRING;

template <typename T>
struct mysql_binder_helper;

template <>
struct mysql_binder_helper<empty> {
};

template <typename T>
struct mysql_binder_helper {
	void bind_in(MYSQL_BIND& bind, const T& arg) const noexcept {
		bind.buffer = &const_cast<T&>(arg);
		bind.length = &bind.buffer_length;
		bind.is_null = nullptr;
		bind.buffer_length = sizeof(T);
		if constexpr (std::is_integral_v<T>) {
			bind.buffer_type = mysql_type_in<std::make_signed_t<T>>;
		} else {
			bind.buffer_type = mysql_type_in<T>;
		}
		bind.is_unsigned = std::is_unsigned_v<T>;
	}

	void bind_in(MYSQL_BIND& bind, const std::nullopt_t&) const noexcept {
		bind.buffer_type = mysql_type_in<T>;
		bind.is_null = &bind.is_null_value;
		bind.is_null_value = true;
	}

	void bind_out(MYSQL_BIND &bind, T& arg) const noexcept {
		bind.buffer = &arg;
		bind.is_null = &bind.is_null_value;
		bind.buffer_length = sizeof(T);
		if constexpr (std::is_integral_v<T>) {
			bind.buffer_type = mysql_type_out<std::make_signed_t<T>>;
		} else {
			bind.buffer_type = mysql_type_out<T>;
		}
		bind.is_unsigned = std::is_unsigned_v<T>;
	}
};

template <std::ranges::contiguous_range T>
struct mysql_binder_helper<T> {
	void bind_in(MYSQL_BIND& bind, const T& arg) const noexcept {
		bind.buffer = const_cast<std::remove_const_t<std::decay_t<decltype(std::ranges::data(arg))>>>(std::ranges::data(arg));
		bind.length = &bind.buffer_length;
		bind.is_null = nullptr;
		bind.buffer_length = static_cast<unsigned long>(std::ranges::size(arg));
		bind.buffer_type = mysql_type_in<char*>;
		bind.is_unsigned = false;
	}

	void bind_in(MYSQL_BIND& bind, const std::nullopt_t&) const noexcept {
		bind.buffer_type = mysql_type_in<char*>;
		bind.is_null = &bind.is_null_value;
		bind.is_null_value = true;
	}

	void bind_out(MYSQL_BIND &bind, T& arg) const noexcept {
		if constexpr (requires (T t, size_t n) { t.resize(n); }) {
			bind.buffer = nullptr;
			bind.buffer_length = 0;
			bind.length = &bind.length_value;
		} else {
			bind.buffer = &arg;
			bind.buffer_length = std::tuple_size_v<T> * sizeof(std::ranges::range_value_t<T>);
		}
		bind.is_null = &bind.is_null_value;
		bind.buffer_type = mysql_type_out<char*>;
		bind.is_unsigned = false;
	}
};

template <typename T>
struct mysql_binder_helper<std::optional<T>> {
	void bind_out(MYSQL_BIND& bind, std::optional<T>& arg) const noexcept {
		arg.emplace();
		mysql_binder_helper<T>{}.bind_out(bind, *arg);
	}

	void bind_in(MYSQL_BIND& bind, const std::optional<T>& arg) const noexcept {
		if (arg.has_value()) {
			mysql_binder_helper<T>{}.bind_in(bind, *arg);
		} else {
			if constexpr (std::ranges::contiguous_range<T>) {
				bind.buffer_type = mysql_type_in<char*>;
			} else {
				bind.buffer_type = mysql_type_in<T>;
			}
			bind.is_null = &bind.is_null_value;
			bind.is_null_value = true;
		}
	}
};

template <typename T>
inline constexpr auto mysql_bind = mysql_binder_helper<T>{};

template <query_type Type, typename DataType = empty, size_t Placeholders = std::numeric_limits<size_t>::max()>
class mysql_prepared_statement;

template <typename DataType, size_t Placeholders>
class mysql_prepared_statement<query_select, DataType, Placeholders> : public managed_ptr<MYSQL_STMT, &mysql_stmt_close> {
public:
	friend class mysql_database;

	static_assert(!std::is_same_v<DataType, empty>, "data type cannot be empty for a select");

	mysql_prepared_statement(managed_ptr<MYSQL_STMT, &mysql_stmt_close>&& ptr) noexcept :
		managed_ptr<MYSQL_STMT, &mysql_stmt_close>{std::move(ptr)}
	{}

	using managed_ptr<MYSQL_STMT, &mysql_stmt_close>::managed_ptr;
	using managed_ptr<MYSQL_STMT, &mysql_stmt_close>::operator=;

	template <size_t N, typename Arg>
	requires (N < Placeholders)
	void bind(const Arg& arg) noexcept(Placeholders != std::numeric_limits<size_t>::max()) {
		if constexpr (Placeholders == std::numeric_limits<size_t>::max()) {
			if (N >= _placeholders_in.data.size()) {
				_placeholders_in.data.resize(N);
			}
		}
		if constexpr (std::ranges::contiguous_range<Arg>) {
			mysql_bind<std::decay_t<decltype(std::ranges::data(arg))>>.bind_in(_placeholders_in.data[N], arg);
		} else {
			mysql_bind<std::decay_t<Arg>>.bind_in(_placeholders_in.data[N], arg);
		}
	}

	template <typename... Args>
	void bind(const Args&... args) {
		constexpr auto num = sizeof...(Args);
		if constexpr (Placeholders == std::numeric_limits<size_t>::max()) {
			if (num >= _placeholders_in.data.size()) {
				_placeholders_in.data.resize(num);
			}
		}
		[this]<size_t... Ns>(std::tuple<const Args&...> argt, std::index_sequence<Ns...>) {
			constexpr auto impl = []<size_t N, typename Arg>(mysql_prepared_statement& me, const Arg& arg) {
				if constexpr (std::ranges::contiguous_range<Arg>) {
					mysql_bind<std::decay_t<decltype(std::ranges::data(arg))>>.bind_in(me._placeholders_in.data[N], arg);
				} else {
					mysql_bind<std::decay_t<Arg>>.bind_in(me._placeholders_in.data[N], arg);
				}
			};

			(impl.template operator()<Ns>(*this, std::get<Ns>(argt)), ...);
		}({args...}, std::make_index_sequence<num>{});
	}

private:
	mysql_placeholders<Placeholders> _placeholders_in;
};

template <typename T>
struct query_type_helper {
	constexpr static inline auto value = query_error;
	using data_type = empty;
};

template <typename DataType, typename Table, typename Where, typename Order>
struct query_type_helper<query<select_t<DataType>, Table, Where, Order>> {
	constexpr static inline auto value = query_select;
	using data_type = DataType;
};

template <query_type QueryType, typename DatasType, size_t Placeholders = std::numeric_limits<size_t>::max()>
struct mysql_stmt_helper;

template <typename T>
inline constexpr bool is_optional = false;

template <typename T>
inline constexpr bool is_optional<std::optional<T>> = true;

template <typename DataType, size_t Placeholders>
requires (boost::pfr::tuple_size_v<DataType> > 0)
struct mysql_stmt_helper<query_select, DataType, Placeholders> {
	static constexpr auto num_fields = boost::pfr::tuple_size_v<DataType>;

	constexpr void bind(mysql_prepared_statement<query_select, DataType, Placeholders>& stmt, DataType &storage, std::span<MYSQL_BIND, num_fields> binds) const {
		[&]<size_t... Ns>(std::index_sequence<Ns...>) constexpr noexcept {
			constexpr auto impl = [&]<typename Arg>(MYSQL_BIND& b, Arg& arg) constexpr noexcept {
				mysql_bind<std::remove_cvref_t<Arg>>.bind_out(b, arg);
			};
			(impl(binds[Ns], boost::pfr::get<Ns>(storage)), ...);
		}(std::make_index_sequence<num_fields>{});
	}

	constexpr void fetch(mysql_prepared_statement<query_select, DataType, Placeholders>& stmt, DataType &storage, std::span<MYSQL_BIND, num_fields> binds) const {
		[&]<size_t... Ns>(std::index_sequence<Ns...>) constexpr {
			auto impl = [&]<size_t N>() constexpr {
				using arg = boost::pfr::tuple_element_t<N, DataType>;
				if constexpr(!is_optional<arg>) {
					assert("non-optional field cannot be null" && !binds[N].is_null_value);
				}
				if constexpr(std::ranges::contiguous_range<arg>) {
					if (binds[N].buffer_length < binds[N].length_value) {
						arg* field;
						if constexpr (is_optional<arg>) {
							if (binds[N].is_null_value) {
								boost::pfr::get<N>(storage) = std::nullopt;
								return;
							}
							field = &(*boost::pfr::get<N>(storage));
						} else {
							field = &boost::pfr::get<N>(storage);
						}
						field->resize(binds[N].length_value);
						binds[N].buffer = static_cast<void*>(field->data());
						binds[N].buffer_length = binds[N].length_value;
						if (auto result = mysql_stmt_fetch_column(stmt.get(), &binds[N], N, 0); result != 0) {
							throw database_exception{mysql_stmt_error(stmt.get())};
						}
					}
				}
			};
			(impl.template operator()<Ns>(), ...);
		}(std::make_index_sequence<num_fields>{});
	}
};

template <typename T>
struct data_type_helper {
	using execute_type = std::vector<T>;
	using fetch_type = std::optional<T>;
};

template <>
struct data_type_helper<void> {
	using execute_type = void;
	using fetch_type = bool;
};

template <typename T>
using data_type = typename data_type_helper<T>::execute_type;

template <typename T>
using fetch_type = typename data_type_helper<T>::fetch_type;

class mysql_database {
public:
	struct connection_info {
		std::string host = "localhost";
		std::string username = "root";
		std::string password = {};
		std::string database = {};
		uint16_t port = 3306;
	};

	mysql_database(const connection_info& info = {});

	~mysql_database();

	template <typename Type, typename Table, typename Where, typename Order>
	auto prepare(const query<Type, Table, Where, Order>& q) {
		using query_helper = query_type_helper<query<Type, Table, Where, Order>>;
		static_assert(query_helper::value != query_error, "unrecognized query");

		return this->queue([this, q]() -> mysql_prepared_statement<query_helper::value, typename query_helper::data_type> {
			auto stmt = managed_ptr<MYSQL_STMT, &mysql_stmt_close>{mysql_stmt_init(this->connection.get())};
			auto str = q.to_string();
			if (mysql_stmt_prepare(stmt.get(), str.data(), str.size()) != 0) {
				throw database_exception{mysql_error(this->connection.get())};
			}
			return {std::move(stmt)};
		});
	}

	template <query_type QueryType, typename DataType, size_t Placeholders, typename... Args>
	requires (sizeof...(Args) == Placeholders)
	dpp::task<void> execute(mysql_prepared_statement<QueryType, DataType, Placeholders>& statement, Args&&... args) {
		if constexpr (Placeholders > 0) {
			constexpr auto bind = []<size_t...Ns>(mysql_prepared_statement<QueryType, DataType, Placeholders>& stmt, std::tuple<Args...> argt, std::index_sequence<Ns...>) {
				((stmt.bind<Ns>(std::get<Ns>(argt))), ...);
			}(statement, std::tuple<Args...>{std::forward<Args>(args)...}, std::make_index_sequence<Placeholders>{});
		}
		co_return;
	}

	template <query_type QueryType, typename DataType, size_t Placeholders>
		requires (!std::is_void_v<DataType>)
	auto fetch_sync(mysql_prepared_statement<QueryType, DataType, Placeholders>& statement) -> std::optional<DataType> {
		constexpr auto helper = mysql_stmt_helper<QueryType, DataType>{};
		MYSQL_BIND     binds[boost::pfr::tuple_size_v<DataType>]{};
		auto           ret = std::optional<DataType>{std::in_place};
		helper.bind(statement, *ret, binds);
		if (auto result = mysql_stmt_bind_result(statement.get(), binds); result != 0) {
			throw database_exception{mysql_stmt_error(statement.get())};
		}
		int status = mysql_stmt_fetch(statement.get());
		if (status == MYSQL_NO_DATA) {
			return std::nullopt;
		}
		if (status != 0 && status != MYSQL_DATA_TRUNCATED) {
			throw database_exception{mysql_stmt_error(statement.get())};
		}
		helper.fetch(statement, *ret, binds);
		return ret;
	}

	template <query_type QueryType, typename DataType, size_t Placeholders>
	requires (!std::is_void_v<DataType>)
	auto fetch(mysql_prepared_statement<QueryType, DataType, Placeholders>& statement) {
		return this->queue([this, &statement]() -> std::optional<DataType> {
			return this->fetch_sync(statement);
		});
	}

	template <query_type QueryType, typename DataType, typename... Args>
	auto execute(mysql_prepared_statement<QueryType, DataType>& statement, Args&&... args) {
		return this->queue([this, argt = std::forward_as_tuple(args...), &statement]() -> data_type<DataType> {
			if constexpr (sizeof...(Args) > 0) {
				constexpr auto bind = []<size_t...Ns>(mysql_prepared_statement<QueryType, DataType>& stmt, std::tuple<Args...> argt, std::index_sequence<Ns...>) {
					((stmt.bind<Ns>(std::get<Ns>(argt))), ...);
				}(statement, argt, std::make_index_sequence<sizeof...(Args)>{});
			}
			if (!statement._placeholders_in.data.empty()) {
				if (mysql_stmt_bind_param(statement.get(), statement._placeholders_in.data.data()) != 0) {
					throw database_exception{mysql_stmt_error(statement.get())};
				}
			}
			if (mysql_stmt_execute(statement.get()) != 0) {
				throw database_exception{mysql_stmt_error(statement.get())};
			}
			if constexpr (!std::is_same_v<DataType, void>) {
				std::vector<DataType> ret;
				constexpr auto        helper = mysql_stmt_helper<QueryType, DataType>{};
				DataType              value;
				MYSQL_BIND            binds[boost::pfr::tuple_size_v<DataType>]{};
				helper.bind(statement, value, binds);
				if (auto result = mysql_stmt_bind_result(statement.get(), binds); result != 0) {
					throw database_exception{mysql_stmt_error(statement.get())};
				}
				int status;
				if (status = mysql_stmt_store_result(statement.get()); status != 0) {
					throw database_exception{mysql_stmt_error(statement.get())};
				}
				status = 0;
				while (status = mysql_stmt_fetch(statement.get()), true) {
					if (status == MYSQL_NO_DATA) {
						break;
					}
					if (status == MYSQL_DATA_TRUNCATED) {
						helper.fetch(statement, value, binds);
					} else if (status != 0) {
						throw database_exception{mysql_stmt_error(statement.get())};
					}
					ret.push_back(std::move(value));
				}
				return ret;
			} else {
				return;
			}
		});
	}

private:
	using request = std::move_only_function<void()>;

	mysql_database(const connection_info& info, std::nullopt_t);
	void run_queue();
	MYSQL* connect();

	template <typename Fun>
	requires (std::invocable<Fun>)
	dpp::awaitable<std::invoke_result_t<Fun>> queue(Fun&& work) {
		using ret = std::invoke_result_t<Fun>;
		auto promise = std::make_unique<dpp::promise<ret>>();
		dpp::awaitable<ret> awaitable = promise->get_awaitable();

		request_queue.emplace_back([fun = std::move(work), p = std::move(promise)] {
			try {
				p->set_value(std::invoke(fun));
			} catch (const std::exception &) {
				p->set_exception(std::current_exception());
			}
		});
		request_cv.notify_all();
		return awaitable;
	}

	connection_info db_info;
	managed_ptr<MYSQL, &mysql_close> connection;
	std::mutex request_mutex;
	std::condition_variable request_cv;
	std::vector<request> request_queue;
	std::atomic<bool> running;
	std::jthread request_thread;
};

using database = mysql_database;

}

#endif
