#ifndef MIMIRON_STRING_LITERAL_H_
#define MIMIRON_STRING_LITERAL_H_

#include <algorithm>
#include <utility>
#include <span>

namespace mimiron {

template <typename CharT, size_t N>
struct basic_string_literal {
	using char_type = CharT;

	basic_string_literal() = default;

	template <size_t N2>
	requires (N2 >= N)
	constexpr basic_string_literal(CharT const (&arr)[N2]) noexcept {
		if (arr[N2 - 1] == 0) {
			std::copy_n(std::begin(arr), std::min(N2 - 1, N), std::begin(str));
		} else {
			std::copy_n(std::begin(arr), std::min(N2, N), std::begin(str));
		}
		str[N] = 0;
	}

	constexpr basic_string_literal(CharT const* arr) noexcept {
		std::copy_n(arr, N, std::begin(str));
		str[N] = 0;
	}

	constexpr std::add_lvalue_reference_t<CharT const[N + 1]> data() const noexcept {
		return str;
	}

	static constexpr std::size_t size() noexcept {
		return N;
	}

	template <size_t N2>
	friend constexpr basic_string_literal<CharT, N + N2> operator+(basic_string_literal const& lhs, basic_string_literal<CharT, N2> const& rhs) noexcept {
		basic_string_literal<CharT, N + N2> ret{};
		std::copy_n(std::begin(lhs.str), N, std::begin(ret.str));
		std::copy_n(std::begin(rhs.str), N2, std::begin(ret.str) + N);
		ret.str[N + N2] = 0;
		return ret;
	}

	template <size_t N2>
	friend constexpr basic_string_literal<CharT, N + N2 - 1> operator+(basic_string_literal const& lhs, CharT const (&rhs)[N2]) noexcept {
		return lhs + basic_string_literal<CharT, N2 - 1>{rhs};
	}

	template <size_t N2>
	friend constexpr basic_string_literal<CharT, N + N2 - 1> operator+(CharT const (&lhs)[N2], basic_string_literal const& rhs) noexcept {
		return basic_string_literal<CharT, N2 - 1>{lhs} + rhs;
	}

	template <size_t N2>
	constexpr friend auto operator<=>(basic_string_literal const& lhs, basic_string_literal<CharT, N2> const& rhs) noexcept {
		return std::ranges::lexicographical_compare(lhs.str, rhs.str);
	}

	template <size_t N2>
	constexpr friend auto operator==(basic_string_literal const& lhs, basic_string_literal<CharT, N2> const& rhs) noexcept {
		return N == N2 && std::ranges::equal(lhs.str, rhs.str);
	}

	template <size_t N2>
	constexpr friend auto operator<=>(basic_string_literal const& lhs, CharT const (&rhs)[N2]) noexcept {
		return std::ranges::lexicographical_compare(lhs.str, rhs);
	}

	template <size_t N2>
	constexpr friend auto operator==(basic_string_literal const& lhs, CharT const (&rhs)[N2]) noexcept {
		return N == N2 && std::ranges::equal(lhs.str, rhs);
	}

	template <size_t N2>
	constexpr friend auto operator<=>(CharT const (&lhs)[N2], basic_string_literal const& rhs) noexcept {
		return std::ranges::lexicographical_compare(lhs, rhs.str);
	}

	template <size_t N2>
	constexpr friend auto operator==(CharT const (&lhs)[N2], basic_string_literal const& rhs) noexcept {
		return N == N2 && std::ranges::equal(lhs, rhs.str);
	}

	template <typename T>
	constexpr friend decltype(auto) operator<<(T&& lhs, basic_string_literal const& rhs) noexcept(noexcept(std::declval<T>() << std::declval<char const (&)[N + 1]>())) {
		return lhs << rhs.str;
	}

	constexpr operator std::string_view() const noexcept {
		return {str, N};
	}

	constexpr operator std::span<CharT const, N>() const noexcept {
		return {str, N};
	}

	constexpr operator std::span<CharT, N>() noexcept {
		return {str, N};
	}

	constexpr auto begin() const noexcept {
		return std::ranges::begin(str);
	}

	constexpr auto end() const noexcept {
		return std::ranges::end(str);
	}

	constexpr auto begin() noexcept {
		return std::ranges::begin(str);
	}

	constexpr auto end() noexcept {
		return std::ranges::end(str);
	}

	CharT str[N + 1];
};

template <typename CharT, size_t N>
basic_string_literal(CharT const (&)[N]) -> basic_string_literal<CharT, N - 1>;

template <size_t N>
struct string_literal : basic_string_literal<char, N> {
	using basic_string_literal<char, N>::basic_string_literal;
	using basic_string_literal<char, N>::operator=;
	using basic_string_literal<char, N>::operator<=>;
	using basic_string_literal<char, N>::operator==;
	using basic_string_literal<char, N>::operator<<;
};

template <typename CharT>
inline constexpr auto make_string_literal = []<size_t N>(CharT const (&arr)[N]) {
	return basic_string_literal<CharT, N - 1>{arr};
};

template <typename T>
inline constexpr bool is_string_literal = false;

template <typename CharT, size_t N>
inline constexpr bool is_string_literal<basic_string_literal<CharT, N>> = true;

template <typename CharT, size_t N>
inline constexpr bool is_string_literal<CharT const[N]> = true;

template <typename CharT, size_t N>
inline constexpr bool is_string_literal<CharT const (&)[N]> = true;

}

#endif
