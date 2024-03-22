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

	constexpr CharT& operator[](size_t idx) noexcept {
		return str[idx];
	}

	constexpr CharT operator[](size_t idx) const noexcept {
		return str[idx];
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
};

template <size_t N>
string_literal(char const (&)[N]) -> string_literal<N - 1>;

string_literal() -> string_literal<0>;

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



template <typename CharT, size_t N>
struct basic_fixed_string {
	std::array<CharT, N> str;
	std::size_t str_size;

	using value_t = CharT;
	using iterator_t = std::ranges::iterator_t<decltype(str)>;
	using const_iterator_t = std::ranges::iterator_t<std::add_const_t<decltype(str)>>;

	constexpr basic_fixed_string() noexcept = default;

	template <size_t N_>
	explicit(N_ > N) constexpr basic_fixed_string(const CharT (&other)[N_]) noexcept {
		auto other_size = N_;
		while (other_size > 0 && other[other_size - 1] == 0)
			--other_size;
		if constexpr (N_ <= N) {
			str_size = other_size;
			auto [_, end] = std::ranges::copy_n(other, N_, str.data());
			if constexpr (N != N_) {
				std::ranges::fill_n(end, N - N_, 0);
			}
		} else if constexpr (N_ > N) {
			str_size = std::min(other_size, N);
			auto [_, end] = std::ranges::copy_n(other, str_size, str.data());
			if (str_size < N) {
				std::ranges::fill_n(end, N - str_size, 0);
			}
		}
	};

	template <size_t N_>
	explicit(N_ > N) constexpr basic_fixed_string(const basic_fixed_string<CharT, N_> &other) noexcept :
		str_size{std::min(N, other.str_size())} {
		if constexpr (N_ <= N) {
			auto [_, end] = std::ranges::copy_n(other.str, N_, str);
			if constexpr (N != N_) {
				std::ranges::fill_n(end, N - N_, 0);
			}
		} else if constexpr (N_ > N) {
			str_size = std::min(other.size(), N);
			auto [_, end] = std::ranges::copy_n(other.str, str_size, str);
			std::ranges::fill(end, std::ranges::end(str), 0);
		}
	};

	constexpr basic_fixed_string(std::string_view other) noexcept :
		str_size{std::min(N, other.size())} {
		auto [_, end] = std::ranges::copy_n(other.data(), str_size, str.data());
		std::ranges::fill_n(end, N - str_size, 0);
	}

	template <size_t N_>
	requires (N_ <= N)
	constexpr basic_fixed_string& operator=(const CharT (&other)[N_]) noexcept {
		auto str_size = N_;
		while (str_size > 0 && other[str_size - 1] == 0)
			--str_size;
		auto [_, end] = std::ranges::copy_n(other, N, str.data());
		if constexpr (N_ < N) {
			std::ranges::fill_n(end, N - N_, 0);
		}
		return *this;
	};

	template <size_t N_>
	requires (N_ <= N)
	constexpr basic_fixed_string& operator=(const basic_fixed_string<CharT, N_> &other) noexcept {
		str_size = other.str_size();
		auto [_, end] = std::ranges::copy_n(other.str, N, str);
		if constexpr (N_ < N) {
			std::ranges::fill_n(end, N - N_, 0);
		}
		return *this;
	};

	constexpr basic_fixed_string& operator=(std::string_view other) noexcept {
		str_size = other.size();
		auto [_, end] = std::ranges::copy_n(other, std::min(N, other.size()), str);
		std::ranges::fill(end, std::ranges::end(str), 0);
		return *this;
	}

	size_t size() const noexcept {
		return str_size;
	}

	static consteval size_t capacity() noexcept {
		return N;
	}

	constexpr CharT* data() noexcept {
		return str.data();
	}

	constexpr CharT const* data() const noexcept {
		return str.data();
	}

	constexpr iterator_t begin() noexcept {
		return str.begin();
	}

	constexpr const_iterator_t begin() const noexcept {
		return str.begin();
	}

	constexpr iterator_t end() noexcept {
		return str.begin();
	}

	constexpr const_iterator_t end() const noexcept {
		return str.begin();
	}

	constexpr operator std::string_view() const noexcept {
		return {str.data(), str_size};
	}

	constexpr CharT& operator[](size_t idx) noexcept {
		return str[idx];
	}

	constexpr CharT operator[](size_t idx) const noexcept {
		return str[idx];
	}

	template <size_t N2>
	friend constexpr basic_fixed_string<CharT, N + N2> operator+(basic_fixed_string const& lhs, basic_fixed_string<CharT, N2> const& rhs) noexcept {
		basic_fixed_string<CharT, N + N2> ret{};
		auto it = std::begin(ret.str);
		it = std::copy_n(std::begin(lhs.str), size(), it);
		it = std::copy_n(std::begin(rhs.str), rhs.size(), it);
		ret.str_size = size() + rhs.size();
		it = std::fill(it, str.end(), CharT{});
		return ret;
	}

	template <size_t N2>
	friend constexpr basic_fixed_string<CharT, N + N2> operator+(basic_fixed_string const& lhs, basic_string_literal<CharT, N2> const& rhs) noexcept {
		basic_fixed_string<CharT, N + N2> ret{};
		auto it = std::begin(ret.str);
		it = std::copy_n(std::begin(lhs.str), size(), it);
		it = std::copy_n(std::begin(rhs.str), N2, it);
		ret.str_size = size() + N2;
		it = std::fill(it, str.end(), CharT{});
		return ret;
	}

	template <size_t N2>
	friend constexpr basic_fixed_string<CharT, N + N2> operator+(basic_string_literal<CharT, N2> const& lhs,  const basic_fixed_string& rhs) noexcept {
		basic_fixed_string<CharT, N + N2> ret{};
		auto it = std::begin(ret.str);
		it = std::copy_n(std::begin(lhs.str), N2, it);
		it = std::copy_n(std::begin(rhs.str), size(), it);
		ret.str_size = size() + N2;
		it = std::fill(it, str.end(), CharT{});
		return ret;
	}

	template <size_t N2>
	friend constexpr basic_fixed_string<CharT, N + N2 - 1> operator+(basic_fixed_string const& lhs, CharT const (&rhs)[N2]) noexcept {
		return lhs + basic_string_literal<CharT, N2 - 1>{rhs};
	}

	template <size_t N2>
	friend constexpr basic_fixed_string<CharT, N + N2 - 1> operator+(CharT const (&lhs)[N2], basic_fixed_string const& rhs) noexcept {
		return basic_string_literal<CharT, N2 - 1>{lhs} + rhs;
	}

	friend constexpr std::string operator+(basic_fixed_string const& lhs, std::basic_string_view<CharT> rhs) noexcept {
		return std::string{lhs.str.data(), lhs.size()} + rhs;
	}

	friend constexpr std::string operator+(std::basic_string_view<CharT> lhs, basic_fixed_string const& rhs) noexcept {
		return lhs + std::string{rhs.str.data(), rhs.size()};
	}

	template <size_t N_>
	friend constexpr auto operator<=>(const basic_fixed_string& lhs, const basic_fixed_string<CharT, N_> &rhs) noexcept {
		return (std::string_view{lhs} <=> std::string_view{rhs});
	}

	template <size_t N_>
	friend constexpr auto operator<=>(const basic_fixed_string& lhs, const CharT (&rhs)[N_]) noexcept {
		return (std::string_view{lhs} <=> std::string_view{rhs});
	}

	friend constexpr auto operator<=>(const basic_fixed_string& lhs, std::string_view rhs) noexcept {
		return (std::string_view{lhs} <=> rhs);
	}

	template <size_t N_>
	friend constexpr auto operator<=>(const basic_fixed_string<CharT, N_>& lhs, const basic_fixed_string& rhs) noexcept {
		return (std::string_view{lhs} <=> std::string_view{rhs});
	}

	template <size_t N_>
	friend constexpr auto operator<=>(const CharT (&lhs)[N_], const basic_fixed_string& rhs) noexcept {
		return (std::string_view{lhs} <=> std::string_view{rhs});
	}

	template <size_t N_>
	friend constexpr auto operator<=>(std::string_view lhs, const basic_fixed_string& rhs) noexcept {
		return (lhs <=> std::string_view{rhs});
	}

	template <size_t N_>
	friend constexpr auto operator==(const basic_fixed_string& lhs, const basic_fixed_string<CharT, N_> &rhs) noexcept {
		return (std::string_view{lhs} == std::string_view{rhs});
	}

	template <size_t N_>
	friend constexpr auto operator==(const basic_fixed_string& lhs, const CharT (&rhs)[N_]) noexcept {
		return (std::string_view{lhs} == std::string_view{rhs});
	}

	friend constexpr auto operator==(const basic_fixed_string& lhs, std::string_view rhs) noexcept {
		return (std::string_view{lhs} == rhs);
	}

	template <size_t N_>
	friend constexpr auto operator==(const basic_fixed_string<CharT, N_>& lhs, const basic_fixed_string& rhs) noexcept {
		return (std::string_view{lhs} == std::string_view{rhs});
	}

	template <size_t N_>
	friend constexpr auto operator==(const CharT (&lhs)[N_], const basic_fixed_string& rhs) noexcept {
		return (std::string_view{lhs} == std::string_view{rhs});
	}

	template <size_t N_>
	friend constexpr auto operator==(std::string_view lhs, const basic_fixed_string& rhs) noexcept {
		return (lhs == std::string_view{rhs});
	}
};

template <typename CharT, size_t N>
basic_fixed_string(const CharT (&str)[N]) -> basic_fixed_string<CharT, N>;

template <size_t N>
using fixed_string = basic_fixed_string<char, N>;

}

#endif
