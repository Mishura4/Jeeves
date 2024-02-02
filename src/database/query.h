#ifndef MIMIRON_QUERY_H_
#define MIMIRON_QUERY_H_

#include <string_view>

#include <boost/pfr.hpp>

#include "tools/string_literal.h"

namespace mimiron {

namespace sql {

enum class op {
	equal,
	less,
	less_eq,
	greater,
	greater_eq,
	not_equal,
	like,
};

enum class ordering {
	ascending,
	descending
};

using enum ordering;

struct placeholder{};

template <typename T>
class select_t;

template <typename T, basic_string_literal Name>
struct field {
	T value;
};

template <typename T>
struct unrecognized_type {};

template <typename T>
inline constexpr auto field_name = unrecognized_type<T>{};

template <typename T, basic_string_literal Name>
inline constexpr auto field_name<field<T, Name>> = "`" + Name + "`";

struct to_string_s {
	template <typename T, typename CharT, size_t N, basic_string_literal<CharT, N> Name>
	constexpr auto operator()(field<T, Name> const& field) const noexcept {
		return Name;
	}

	template <typename CharT, size_t N>
	constexpr auto operator()(basic_string_literal<CharT, N> const& value) const noexcept {
		return value;
	}

	template <typename CharT, size_t N>
	constexpr auto operator()(CharT const (&value)[N]) const noexcept {
		return basic_string_literal{value};
	}

	constexpr auto operator()(placeholder) const noexcept {
		return "?";
	}
};

struct to_query_identifier_s {
	template <typename T, typename CharT, size_t N, basic_string_literal<CharT, N> Name>
	constexpr auto operator()(field<T, Name> const& field) const noexcept {
		return "`" + Name + "`";
	}

	template <typename CharT, size_t N>
	constexpr auto operator()(basic_string_literal<CharT, N> const& value) const noexcept {
		return "\"" + value + "\"";
	}

	template <typename CharT, size_t N>
	constexpr auto operator()(CharT const (&value)[N]) const noexcept {
		return "\"" + basic_string_literal{value} + "\"";
	}

	constexpr auto operator()(placeholder) const noexcept {
		return "?";
	}
};

inline constexpr auto to_query_identifier = to_query_identifier_s{};

template <typename T>
constexpr inline bool is_field = !std::is_same_v<decltype(field_name<T>), unrecognized_type<T>>;

using enum op;

constexpr std::string_view get_operand(op operand) {
	using namespace std::string_view_literals;

	switch (operand) {
		case equal:
			return "="sv;

		case less:
			return "<"sv;

		case less_eq:
			return "<="sv;
		case greater:
			return ">"sv;

		case greater_eq:
			return ">="sv;

		case not_equal:
			return "<>"sv;

		case like:
			return "LIKE"sv;

		default:
			std::unreachable();
	}
}

template <typename Lhs, typename Rhs>
struct where {
	Lhs lhs;
	op operand;
	Rhs rhs;

	constexpr auto to_string() const noexcept {
		auto lhs_translated = to_query_identifier(lhs);
		auto rhs_translated = to_query_identifier(rhs);
		using lhs_t = decltype(lhs_translated);
		using rhs_t = decltype(rhs_translated);
		auto operand_sv = get_operand(operand);
		basic_string_literal<typename lhs_t::char_type, lhs_t::size() + rhs_t::size() + 6> ret;

		auto out_it = std::copy(std::begin(lhs_translated.str), std::end(lhs_translated.str), std::begin(ret.str));
		*out_it = ' ';
		++out_it;
		out_it = std::copy(std::begin(operand_sv), std::end(operand_sv), out_it);
		*out_it = ' ';
		++out_it;
		out_it = std::copy(std::begin(rhs_translated.str), std::end(rhs_translated.str), out_it);
		std::fill(out_it, std::end(ret.str), 0);
		return ret;
	}
};

template <typename Field>
struct order_by_clause {
	Field field;
	ordering how = ascending;
};

template <typename Lhs, typename Rhs>
where(Lhs&& lhs, op operand, Rhs&& rhs) -> where<Lhs, Rhs>;

struct limit_clause {
	size_t from{};
	size_t count{};
};

struct empty {};

template <typename Type, typename Table = empty, typename Where = empty, typename Order = empty>
struct query {
	template <typename Lhs, typename Rhs>
	constexpr auto where(Lhs&& lhs, op operand, Rhs&& rhs) && noexcept -> query<Type, Table, sql::where<Lhs, Rhs>, Order> {
		return {
			std::move(type), std::move(table), sql::where<Lhs, Rhs>{std::forward<Lhs>(lhs), operand, std::forward<Rhs>(rhs)}, std::move(order), limit_clause{limit_value}
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
	constexpr auto sanitize = []<typename CharT, size_t N>(basic_string_literal<CharT, N> const& base_str) constexpr noexcept {
		basic_string_literal<CharT, N> ret;

		auto out_it = std::copy_if(std::begin(base_str.str), std::end(base_str.str), std::begin(ret.str), [](CharT c) constexpr noexcept { return c != CharT{}; });
		std::fill(out_it, std::end(ret.str), CharT{});
		return ret;
	};
	return sanitize(add_where(base, whr));
}

}

}

#endif
