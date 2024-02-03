#ifndef MIMIRON_DATABASE_COMMON_H_
#define MIMIRON_DATABASE_COMMON_H_

#include "tools/string_literal.h"

namespace mimiron::sql {

struct placeholder_t{};

inline constexpr placeholder_t placeholder = placeholder_t{};

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

	constexpr auto operator()(placeholder_t) const noexcept {
		return basic_string_literal{"?"};
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

	constexpr auto operator()(placeholder_t) const noexcept {
		return basic_string_literal{"?"};
	}
};

inline constexpr auto to_query_identifier = to_query_identifier_s{};

template <typename T>
constexpr inline bool is_field = !std::is_same_v<decltype(field_name<T>), unrecognized_type<T>>;

}

#endif
