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

#include "tools/worker.h"

namespace mimiron::sql {

template <typename T, template<typename ...> class Of> 
inline constexpr bool is_specialization_v = false;

template <typename T, template<typename ...> class Of> 
inline constexpr bool is_specialization_v<const T, Of> = is_specialization_v<T, Of>;

template <template<typename ...> class Of, typename ...Ts> 
inline constexpr bool is_specialization_v<Of<Ts...>, Of> = true;

template <typename T, template<typename ...> class Of> 
using is_specialization_of = std::bool_constant<is_specialization_v<T, Of>>;

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
	query_dynamic,
	query_select
};

using enum query_type;

template <size_t N>
struct mysql_placeholders {
	std::array<MYSQL_BIND, N> data{};
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
			bind.error = &bind.error_value;
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

template <typename T>
inline constexpr bool is_optional = false;

template <typename T>
inline constexpr bool is_optional<std::optional<T>> = true;


template <typename... Args, size_t... Ns>
constexpr void stmt_bind_out(MYSQL_BIND* binds_, std::tuple<Args&...> argt, std::index_sequence<Ns...>) noexcept {
	constexpr auto impl = [&]<typename Arg>(MYSQL_BIND& b, Arg& arg) constexpr noexcept {
		mysql_bind<std::remove_cvref_t<Arg>>.bind_out(b, arg);
	};
	(impl(binds_[Ns], std::get<Ns>(argt)), ...);
}

template <typename... Args, size_t... Ns>
constexpr void stmt_bind_in(MYSQL_BIND *binds, std::tuple<const Args&...> argt, std::index_sequence<Ns...>) {
	constexpr auto impl = []<typename Arg>(MYSQL_BIND& bind, const Arg& arg) {
		if constexpr (std::ranges::contiguous_range<Arg>) {
			mysql_bind<std::decay_t<decltype(std::ranges::data(arg))>>.bind_in(bind, arg);
		} else {
			mysql_bind<std::decay_t<Arg>>.bind_in(bind, arg);
		}
	};

	(impl(binds[Ns], std::get<Ns>(argt)), ...);
}

template <typename... Args, size_t... Ns>
constexpr void stmt_fetch_texts(MYSQL_STMT* stmt, std::tuple<Args...> &argt, MYSQL_BIND (&binds)[sizeof...(Args)], std::index_sequence<Ns...>) {
	auto impl = [&]<size_t N>() constexpr {
		using arg = std::remove_cvref_t<std::tuple_element_t<N, std::tuple<Args...>>>;
		if constexpr(!is_optional<arg>) {
			assert("non-optional field cannot be null" && !binds[N].is_null_value);
		} else {
			if (binds[N].is_null_value) {
				std::get<N>(argt) = std::nullopt;
				return;
			}
		}
		if constexpr(std::ranges::contiguous_range<arg>) {
			MYSQL_BIND& bind = binds[N];
			if (*bind.error) {
				arg* field;
				if constexpr (is_optional<arg>) {
					field = &(*std::get<N>(argt));
				} else {
					field = &std::get<N>(argt);
				}
				field->resize(bind.length_value);
				bind.buffer = static_cast<void*>(field->data());
				bind.buffer_length = bind.length_value;
				if (auto result = mysql_stmt_fetch_column(stmt, &bind, N, 0); result != 0) {
					throw database_exception{mysql_stmt_error(stmt)};
				}
			}
		}
	};
	(impl.template operator()<Ns>(), ...);
}

template <typename Derived>
struct mysql_fetchable_statement {
	template <typename... Args>
	bool fetch(std::tuple<Args...>& values) {
		MYSQL_BIND binds[sizeof...(Args)];

		memset(&binds, 0, sizeof(MYSQL_BIND) * sizeof...(Args));
		stmt_bind_out(binds, values, std::make_index_sequence<sizeof...(Args)>{});
		if (auto result = mysql_stmt_bind_result(static_cast<Derived*>(this)->get(), binds); result != 0) {
			throw database_exception{mysql_stmt_error(static_cast<Derived*>(this)->get())};
		}
		return _fetch(binds, values);
	}

	template <typename T>
	requires (std::is_aggregate_v<T>)
	bool fetch(T& t) {
		return fetch(boost::pfr::structure_tie(t));
	}

	template <typename T>
	requires (std::is_scalar_v<T>)
	bool fetch(T& t) {
		return fetch(std::tuple<T&>(t));
	}

	template <typename T>
	std::optional<T> fetch() {
		std::optional<T> ret{std::in_place};

		if (!fetch(*ret)) {
			return std::nullopt;
		}
		return ret;
	}

	template <typename T>
	requires (std::is_aggregate_v<T>)
	std::vector<T> fetch_all() {
		MYSQL_BIND binds[boost::pfr::tuple_size_v<T>];

		memset(&binds, 0, sizeof(MYSQL_BIND) * boost::pfr::tuple_size_v<T>);
		std::vector<T> ret;
		T value;
		auto as_references = boost::pfr::structure_tie(value);

		stmt_bind_out(binds, as_references, std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
		if (auto result = mysql_stmt_bind_result(static_cast<Derived*>(this)->get(), binds); result != 0) {
			throw database_exception{mysql_stmt_error(static_cast<Derived*>(this)->get())};
		}
		if (auto result = mysql_stmt_store_result(static_cast<Derived*>(this)->get()); result != 0) {
			throw database_exception{mysql_stmt_error(static_cast<Derived*>(this)->get())};
		}
		while (true) {
			if (!_fetch(binds, as_references)) {
				break;
			}
			ret.push_back(std::move(value));
		}
		return ret;
	}

private:
	template <typename... Args>
	bool _fetch(MYSQL_BIND (&binds)[sizeof...(Args)], std::tuple<Args...>& values) {
		int status = mysql_stmt_fetch(static_cast<Derived*>(this)->get());
		if (status == MYSQL_NO_DATA) {
			return false;
		}
		if (status == MYSQL_DATA_TRUNCATED) {
			stmt_fetch_texts(static_cast<Derived*>(this)->get(), values, binds, std::make_index_sequence<sizeof...(Args)>{});
		}
		else if (status != 0) {
			throw database_exception{mysql_stmt_error(static_cast<Derived*>(this)->get())};
		}
		return true;
	}
};

template <typename DataType, size_t Placeholders>
class mysql_prepared_statement<query_select, DataType, Placeholders> : public managed_ptr<MYSQL_STMT, &mysql_stmt_close>, private mysql_fetchable_statement<mysql_prepared_statement<query_select, DataType, Placeholders>> {
	friend class mysql_fetchable_statement<mysql_prepared_statement>;
public:
	friend class mysql_database;

	static_assert(!std::is_same_v<DataType, empty>, "data type cannot be empty for a select");

	mysql_prepared_statement(managed_ptr<MYSQL_STMT, &mysql_stmt_close>&& ptr) noexcept :
		managed_ptr<MYSQL_STMT, &mysql_stmt_close>{std::move(ptr)}
	{}

	using managed_ptr<MYSQL_STMT, &mysql_stmt_close>::managed_ptr;
	using managed_ptr<MYSQL_STMT, &mysql_stmt_close>::operator=;

	template <typename... Args>
	requires (Placeholders == std::numeric_limits<size_t>::max() || Placeholders == sizeof...(Args))
	void bind(const Args&... args) {
		constexpr auto num = sizeof...(Args);
		if constexpr (Placeholders != std::numeric_limits<size_t>::max()) {
			_placeholders_in.data = std::vector<MYSQL_BIND>(num);
		} else {
			std::memset(_placeholders_in.data.data(), 0, sizeof(MYSQL_BIND) * num);
		}
		stmt_bind_in(_placeholders_in.data.data(), std::forward_as_tuple(args...), std::make_index_sequence<sizeof...(Args)>{});

		if (mysql_stmt_bind_param(get(), _binds_in.data()) != 0) {
			throw database_exception{mysql_stmt_error(get())};
		}
	}

	bool fetch(DataType& data) {
		return mysql_fetchable_statement<mysql_prepared_statement>::template fetch<DataType>(data);
	}

	std::optional<DataType> fetch() {
		return mysql_fetchable_statement<mysql_prepared_statement>::template fetch<DataType>();
	}

	std::vector<DataType> fetch_all() {
		return mysql_fetchable_statement<mysql_prepared_statement>::template fetch_all<DataType>();
	}

private:
	auto& _binds_in() noexcept {
		return _placeholders_in.data;
	}

	mysql_placeholders<Placeholders> _placeholders_in;
};

template <>
class mysql_prepared_statement<query_dynamic> : public managed_ptr<MYSQL_STMT, &mysql_stmt_close>, private mysql_fetchable_statement<mysql_prepared_statement<query_dynamic>> {
	friend class mysql_fetchable_statement<mysql_prepared_statement>;
public:
	friend class mysql_database;

	using managed_ptr<MYSQL_STMT, &mysql_stmt_close>::managed_ptr;
	using managed_ptr<MYSQL_STMT, &mysql_stmt_close>::operator=;

	mysql_prepared_statement(managed_ptr<MYSQL_STMT, &mysql_stmt_close>&& ptr) noexcept :
		managed_ptr<MYSQL_STMT, &mysql_stmt_close>{std::move(ptr)}
	{}

	template <typename... Args>
	void bind(const Args&... args) {
		constexpr auto num = sizeof...(Args);
		_placeholders_in.data = std::vector<MYSQL_BIND>(num);
		stmt_bind_in(_placeholders_in.data.data(), std::forward_as_tuple(args...), std::make_index_sequence<sizeof...(Args)>{});

		if (mysql_stmt_bind_param(get(), _binds_in().data()) != 0) {
			throw database_exception{mysql_stmt_error(get())};
		}
	}

private:
	auto& _binds_in() noexcept {
		return _placeholders_in.data;
	}

	mysql_placeholders<std::numeric_limits<size_t>::max()> _placeholders_in;
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

	template <typename Type, typename Table, typename Where, typename Order>
	auto prepare_sync(const sql::query<Type, Table, Where, Order>& q) {
		using query_helper = query_type_helper<sql::query<Type, Table, Where, Order>>;
		static_assert(query_helper::value != query_error, "unrecognized query");

		auto stmt = managed_ptr<MYSQL_STMT, &mysql_stmt_close>{mysql_stmt_init(this->connection.get())};
		auto str = q.to_string();
		if (mysql_stmt_prepare(stmt.get(), str.data(), str.size()) != 0) {
			throw database_exception{mysql_error(this->connection.get())};
		}
		return mysql_prepared_statement<query_helper::value, typename query_helper::data_type>{std::move(stmt)};
	}

	template <typename Type, typename Table, typename Where, typename Order>
	auto prepare(const sql::query<Type, Table, Where, Order>& q) {
		return this->_worker.queue([this, q]() {
			return this->prepare_sync(q);
		});
	}

	auto prepare_sync(std::string_view sql) -> mysql_prepared_statement<query_dynamic> {
		auto stmt = managed_ptr<MYSQL_STMT, &mysql_stmt_close>{mysql_stmt_init(this->connection.get())};
		if (mysql_stmt_prepare(stmt.get(), sql.data(), sql.size()) != 0) {
			throw database_exception{mysql_error(this->connection.get())};
		}
		return mysql_prepared_statement<query_dynamic>{std::move(stmt)};
	}

	auto prepare(std::string sql) -> dpp::awaitable<mysql_prepared_statement<query_dynamic>> {
		return this->_worker.queue([this, s = std::move(sql)] {
			return prepare_sync(s);
		});
	}

	template <query_type QueryType, typename DataType, size_t Placeholders, typename... Args>
	auto query_sync(mysql_prepared_statement<QueryType, DataType, Placeholders>& statement, const Args&... args) -> decltype(statement) {
		if constexpr (sizeof...(Args) > 0) {
			statement.bind(std::forward<Args>(args));
		}
		if (mysql_stmt_execute(statement.get()) != 0) {
			throw database_exception{mysql_stmt_error(statement.get())};
		}
		return statement;
	}

	template <query_type QueryType, typename DataType, size_t Placeholders, typename... Args>
	auto query(mysql_prepared_statement<QueryType, DataType, Placeholders>& statement, Args&&... args) {
		return this->_worker.queue([argt = std::forward_as_tuple(this, statement, std::forward<Args>(args)...)] {
			return std::apply(&mysql_database::query_sync<QueryType, DataType, Placeholders, std::remove_cvref_t<Args>...>, argt);
		});
	}

	template <typename Type, typename Table, typename Where, typename Order, typename... Args>
	auto query_sync(const sql::query<Type, Table, Where, Order>& q, const Args&... args_in) {
		auto statement = prepare_sync(q);
		if constexpr (sizeof...(Args) > 0) {
			statement.bind(args_in...);
		}
		if (mysql_stmt_execute(statement.get()) != 0) {
			throw database_exception{mysql_stmt_error(statement.get())};
		}
		return statement;
	}

	template <typename Type, typename Table, typename Where, typename Order, typename... ArgsIn>
	auto query(const sql::query<Type, Table, Where, Order>& q, ArgsIn&&... args_in) {
		return this->_worker.queue([this, argt = std::forward_as_tuple(q, args_in...)]() {
			return []<size_t... Ns>(mysql_database *self, auto&& tuple, std::index_sequence<Ns...>) {
				return self->query_sync(std::get<Ns>(tuple)...);
			}(this, argt, std::make_index_sequence<sizeof...(ArgsIn) + 1>{});
		});
	}

	template <typename... Args>
	auto query_sync(std::string_view sql, const Args&... args) {
		auto statement = prepare_sync(sql);
		if constexpr (sizeof...(Args) > 0) {
			statement.bind(args...);
		}
		if (mysql_stmt_execute(statement.get()) != 0) {
			throw database_exception{mysql_stmt_error(statement.get())};
		}
		return statement;
	}

	template <typename... Args>
	auto query(std::string sql, Args&&... args) {
		return this->_worker.queue([argt = std::forward_as_tuple(this, std::move(sql), std::forward<Args>(args))] {
			return std::apply(&mysql_database::query_sync, argt);
		});
	}

	template <query_type QueryType, typename DataType, size_t Placeholders>
	requires (QueryType != query_dynamic)
	auto fetch(mysql_prepared_statement<QueryType, DataType, Placeholders>& statement) {
		return this->_worker.queue([this, &statement]() {
			return statement.fetch();
		});
	}

	template <query_type QueryType, typename DataType, size_t Placeholders>
	requires (QueryType != query_dynamic)
	auto fetch_all(mysql_prepared_statement<QueryType, DataType, Placeholders>& statement) {
		return this->_worker.queue([this, &statement]() {
			return statement.fetch_all();
		});
	}

	template <typename Out>
	auto fetch(mysql_prepared_statement<query_dynamic>& statement) {
		return this->_worker.queue([this, &statement]() {
			if constexpr (is_specialization_v<Out, std::vector>) {
				return statement.fetch_all<std::ranges::range_value_t<Out>>();
			} else {
				return statement.template fetch<Out>();
			}
		});
	}

	template <typename Out>
	auto fetch_all(mysql_prepared_statement<query_dynamic>& statement) {
		return this->_worker.queue([this, &statement]() {
			return statement.fetch_all<Out>();
		});
	}

	template <typename Out, typename... ArgsIn>
	auto execute_sync(mysql_prepared_statement<query_dynamic>& statement, ArgsIn&&... args_in) {
		query_sync(statement, std::forward<ArgsIn>(args_in)...);

		if constexpr (is_specialization_v<Out, std::vector>) {
			return statement.fetch_all<std::ranges::range_value_t<Out>>();
		} else {
			return statement.fetch<Out>();
		}
	}

	template <typename Type, typename Table, typename Where, typename Order, typename... ArgsIn>
	auto execute(const sql::query<Type, Table, Where, Order>& q, ArgsIn&&... args_in) {
		return this->_worker.queue([this, argt = std::forward_as_tuple(q, args_in...)]() {
			return []<size_t... Ns>(mysql_database *self, auto&& tuple, std::index_sequence<Ns...>) {
				return self->execute_sync(std::get<Ns>(tuple)...);
			}(this, argt, std::make_index_sequence<sizeof...(ArgsIn) + 1>{});
		});
	}

	template <query_type QueryType, typename DataType, size_t Placeholders, typename... ArgsIn>
	requires (QueryType != query_dynamic)
	auto execute_sync(mysql_prepared_statement<QueryType, DataType, Placeholders>& statement, ArgsIn&&... args_in) {
		query_sync(statement, std::forward<ArgsIn>(args_in)...);

		return statement.fetch_all();
	}

	template <typename Out, typename... ArgsIn>
	auto execute_sync(std::string_view sql, ArgsIn&&... args_in) {
		auto statement = query_sync(sql, std::forward<ArgsIn>(args_in)...);

		if constexpr (is_specialization_v<Out, std::vector>) {
			return statement.template fetch_all<std::ranges::range_value_t<Out>>();
		} else {
			return statement.template fetch<Out>();
		}
	}

	template <typename Type, typename Table, typename Where, typename Order, typename... ArgsIn>
	auto execute_sync(const sql::query<Type, Table, Where, Order>& q, ArgsIn&&... args_in) {
		auto statement = query_sync(q, std::forward<ArgsIn>(args_in)...);

		return statement.fetch_all();
	}

	template <typename Out, typename... ArgsIn>
	auto execute(mysql_prepared_statement<query_dynamic>& statement, ArgsIn&&... args_in) {
		return this->_worker.queue([this, argt = std::forward_as_tuple(statement, std::forward<ArgsIn>(args_in)...)] {
			return []<size_t... Ns>(mysql_database *self, auto&& tuple, std::index_sequence<Ns...>) {
				return self->execute_sync<Out>(std::get<Ns>(tuple)...);
			}(this, argt, std::make_index_sequence<sizeof...(ArgsIn) + 1>{});
		});
	}

	template <query_type QueryType, typename DataType, size_t Placeholders, typename... ArgsIn>
	requires (QueryType != query_dynamic)
	auto execute(mysql_prepared_statement<QueryType, DataType, Placeholders>& statement, ArgsIn&&... args_in) {
		return this->_worker.queue([this, argt = std::forward_as_tuple(statement, std::forward<ArgsIn>(args_in)...)] {
			return []<size_t... Ns>(mysql_database *self, auto&& tuple, std::index_sequence<Ns...>) {
				return self->execute_sync(std::get<Ns>(tuple)...);
			}(this, argt, std::make_index_sequence<sizeof...(ArgsIn) + 1>{});
		});
	}

	template <typename Out, typename... ArgsIn>
	auto execute(std::string_view sql, ArgsIn&&... args_in) {
		return this->_worker.queue([this, argt = std::forward_as_tuple(std::string{sql}, std::forward<ArgsIn>(args_in)...)] {
			return []<size_t... Ns>(mysql_database *self, auto&& tuple, std::index_sequence<Ns...>) {
				return self->execute_sync<Out>(std::get<Ns>(tuple)...);
			}(this, argt, std::make_index_sequence<sizeof...(ArgsIn) + 1>{});
		});
	}

private:
	using request = std::move_only_function<void()>;

	mysql_database(const connection_info& info, std::nullopt_t);
	MYSQL* connect();

	connection_info db_info;
	managed_ptr<MYSQL, &mysql_close> connection;
	worker _worker;
};

using database = mysql_database;

}

#endif
