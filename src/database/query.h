#ifndef MIMIRON_QUERY_H_
#define MIMIRON_QUERY_H_

#include <string_view>

#include <boost/pfr.hpp>

#include "tools/string_literal.h"
#include "database/common.h"
#include "database/conditions.h"

namespace mimiron {

namespace sql {

enum class ordering {
	ascending,
	descending
};

using enum ordering;

template <typename T>
class select_t;

template <typename Field>
struct order_by_clause {
	Field field;
	ordering how = ascending;
};

struct limit_clause {
	size_t from{};
	size_t count{};
};

struct empty {};

template <typename Type, typename Table = empty, typename Where = empty, typename Order = empty>
struct query {
	template <typename T>
	constexpr auto where(T&& clause) && noexcept -> query<Type, Table, std::remove_cv_t<T>, Order> {
		return {
			std::move(type), std::move(table), std::forward<T>(clause), std::move(order), limit_clause{limit_value}
		};
	}

	template <size_t N>
	constexpr auto where(char const (&condition)[N]) && noexcept {
		auto lit = basic_string_literal{condition};
		return query<Type, Table, decltype(lit), Order> {
			std::move(type), std::move(table), lit, std::move(order), limit_clause{limit_value}
		};
	}

	template <typename Field>
	constexpr auto order_by(Field&& f, ordering how = ascending) && noexcept -> query<Type, Table, Where, order_by_clause<Field>> {
		return {
			std::move(type), std::move(table), std::move(whr), {std::forward<Field>(f), how}, limit_clause{limit_value}
		};
	}

	constexpr query limit(size_t count) && noexcept {
		return {
			std::move(type), std::move(table), std::move(whr), std::move(order), {0, count}
		};
	}

	constexpr query limit(size_t from, size_t count) && noexcept {
		return {
			std::move(type), std::move(table), std::move(whr), std::move(order), {from, count}
		};
	}

	constexpr auto to_string() const noexcept requires(!std::is_same_v<Type, empty> && !std::is_same_v<Table, empty>);

	Type type{};
	Table table{};
	Where whr{};
	Order order{};
	limit_clause limit_value{};
};

template <typename T>
class select_t {
public:
	using data_type = T;

	constexpr select_t() = default;

	template <typename T_>
	requires (std::is_same_v<std::remove_cvref_t<T_>, std::remove_cvref_t<T_>>)
	constexpr select_t(T_&&) noexcept {}

	template <typename Table>
	constexpr query<select_t, Table> from(Table&& tbl) {
		return {*this, std::forward<Table>(tbl)};
	}

	template <typename CharT, size_t N>
	constexpr query<select_t, basic_string_literal<CharT, N - 1>> from(CharT const (&table)[N]) const {
		return query<select_t, basic_string_literal<CharT, N - 1>>{
			.type = *this,
			.table = basic_string_literal{table},
			.whr = empty{},
			.order = empty{},
			.limit_value = limit_clause{0, 0}
		};
	}

	constexpr auto to_string() const noexcept {
		constexpr auto join = []<size_t... Ns>(std::index_sequence<Ns...>) constexpr noexcept {
			constexpr auto delim = []<size_t N>() constexpr -> std::string {
				if constexpr (N == 0) {
					return "";
				} else {
					return ", ";
				}
			};
			return std::string{"SELECT "} + ((delim.template operator()<Ns>() + std::string{boost::pfr::get_name<Ns, T>()}) + ...);
		};
		constexpr auto size = join(std::make_index_sequence<boost::pfr::tuple_size_v<T>>{}).size();
		return basic_string_literal<char, size>{join(std::make_index_sequence<boost::pfr::tuple_size_v<T>>{}).data()};
	}
};

template <basic_string_literal... Names, typename... Ts>
class select_t<std::tuple<field<Ts, Names>...>> {
public:
	using data_type = std::tuple<field<Ts, Names>...>;

	constexpr select_t() = default;

	template <typename... Args>
	requires (std::is_same_v<std::remove_cvref_t<Args>, std::remove_cvref_t<Ts>> && ...)
	constexpr select_t(Args&&...) noexcept {}

	template <typename Table>
	constexpr query<select_t, Table> from(Table&& tbl) const {
		return {*this, std::forward<Table>(tbl)};
	}

	template <typename CharT, size_t N>
	constexpr query<select_t, basic_string_literal<CharT, N - 1>> from(CharT const (&table)[N]) const {
		return query<select_t, basic_string_literal<CharT, N - 1>>{
			.type = *this,
			.table = basic_string_literal{table},
			.whr = empty{},
			.order = empty{},
			.limit_value = limit_clause{0, 0}
		};
	}

	constexpr auto to_string() const noexcept {
		constexpr auto join = []<size_t... Ns>(std::index_sequence<Ns...>) constexpr noexcept {
			constexpr auto delim = []<size_t N>() constexpr noexcept {
				if constexpr (N == 0) {
					return basic_string_literal{""};
				} else {
					return basic_string_literal{", "};
				}
			};
			return ((Names + delim.template operator()<Ns>()) + ...);
		};
		return "SELECT " + join(std::make_index_sequence<sizeof...(Names)>{});
	}
};

template <typename T>
inline constexpr select_t<T> select = select_t<T>{};

template <typename Type, typename Table, typename Where, typename Order>
constexpr auto query<Type, Table, Where, Order>::to_string() const noexcept
 requires(!std::is_same_v<Type, empty> && !std::is_same_v<Table, empty>) {
	auto base = type.to_string() + " FROM " + to_string_s{}(table) + "";
	constexpr auto add_where = []<typename CharT, size_t N>(basic_string_literal<CharT, N> const& base_str, Where const& value) constexpr noexcept -> decltype(auto) {
		if constexpr (std::is_same_v<Where, empty>) {
			return base_str;
		} else if constexpr (is_string_literal<Where>) {
			return base_str + " WHERE " + value;
		}
		else {
			return base_str + " WHERE " + value.to_string();
		}
	};
	return add_where(base, whr);
}

}

}

#endif
