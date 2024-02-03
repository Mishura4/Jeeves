#ifndef MIMIRON_DATABASE_CONDITIONS_H_
#define MIMIRON_DATABASE_CONDITIONS_H_

#include <utility>
#include <type_traits>
#include <concepts>

#include "tools/string_literal.h"

namespace mimiron::sql {

template <typename Lhs, typename Rhs>
struct and_condition;

template <typename Lhs, typename Rhs>
struct or_condition;

template <typename T>
struct brackets_helper_s;


template <typename Lhs, typename Rhs>
struct brackets_helper_s<and_condition<Lhs, Rhs>> {
	constexpr auto operator()(const Lhs &lhs, const Rhs& rhs) const noexcept {
		return lhs.to_string() + " AND " + rhs.to_string();
	}
};


template <typename Lhs, typename Rhs>
struct brackets_helper_s<or_condition<Lhs, Rhs>> {
	constexpr auto operator()(const Lhs &lhs, const Rhs& rhs) const noexcept {
		return lhs.to_string() + " OR " + rhs.to_string();
	}
};


template <typename Lhs, typename Lhs2, typename Rhs2>
struct brackets_helper_s<and_condition<Lhs, or_condition<Lhs2, Rhs2>>> {
	constexpr auto operator()(const Lhs &lhs, const or_condition<Lhs2, Rhs2> &rhs) const noexcept {
		return lhs.to_string() + " AND (" + rhs.to_string() + ")";
	}
};

template <typename Lhs, typename Lhs2, typename Rhs2>
struct brackets_helper_s<and_condition<or_condition<Lhs2, Rhs2>, Lhs>> {
	constexpr auto operator()(const or_condition<Lhs2, Rhs2> &lhs, const Lhs &rhs) const noexcept {
		return "(" + lhs.to_string() + ") AND " + rhs.to_string();
	}
};


template <typename Lhs, typename Lhs2, typename Rhs2>
struct brackets_helper_s<or_condition<Lhs, and_condition<Lhs2, Rhs2>>> {
	constexpr auto operator()(const Lhs &lhs, const and_condition<Lhs2, Rhs2> &rhs) const noexcept {
		return lhs.to_string() + " OR (" + rhs.to_string() + ")";
	}
};

template <typename Lhs1, typename Rhs1, typename Rhs>
struct brackets_helper_s<or_condition<and_condition<Lhs1, Rhs1>, Rhs>> {
	constexpr auto operator()(const and_condition<Lhs1, Rhs1> &lhs, Rhs &rhs) const noexcept {
		return "(" + lhs.to_string() + ") OR " + rhs.to_string();
	}
};


template <typename Lhs1, typename Rhs1, typename Lhs2, typename Rhs2>
struct brackets_helper_s<or_condition<and_condition<Lhs1, Rhs1>, and_condition<Lhs2, Rhs2>>> {
	constexpr auto operator()(const and_condition<Lhs1, Rhs1>& lhs, const and_condition<Lhs2, Rhs2> &rhs) const noexcept {
		return "(" + lhs.to_string() + ") OR (" + rhs.to_string() + ")";
	}
};


template <typename Lhs1, typename Rhs1, typename Lhs2, typename Rhs2>
struct brackets_helper_s<and_condition<or_condition<Lhs1, Rhs1>, or_condition<Lhs2, Rhs2>>> {
	constexpr auto operator()(const or_condition<Lhs1, Rhs1>& lhs, const or_condition<Lhs2, Rhs2> &rhs) const noexcept {
		return "(" + lhs.to_string() + ") AND (" + rhs.to_string() + ")";
	}
};


template <typename Lhs, typename Rhs>
struct or_condition {
	Lhs lhs;
	Rhs rhs;

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) const& {
		return and_condition<or_condition, std::remove_cvref_t<Rhs_>>{{lhs, rhs}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) && noexcept {
		return and_condition<or_condition, std::remove_cvref_t<Rhs_>>{{std::move(lhs), std::move(rhs)}, std::forward<Rhs_>(rhs_)};
	}
	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) const& noexcept {
		return or_condition<or_condition, std::remove_cvref_t<Rhs_>>{{lhs, rhs}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) && noexcept {
		return or_condition<or_condition, std::remove_cvref_t<Rhs_>>{{std::move(lhs), std::move(rhs)}, std::forward<Rhs_>(rhs_)};
	}

	constexpr auto to_string() const noexcept {
		return brackets_helper_s<or_condition>{}(lhs, rhs);
	}
};

template <typename Lhs, typename Rhs>
struct and_condition {
	Lhs lhs;
	Rhs rhs;

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) const& {
		return and_condition<and_condition, std::remove_cvref_t<Rhs_>>{{lhs, rhs}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) && noexcept {
		return and_condition<and_condition, std::remove_cvref_t<Rhs_>>{{std::move(lhs), std::move(rhs)}, std::forward<Rhs_>(rhs_)};
	}
	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) const& noexcept {
		return or_condition<and_condition, std::remove_cvref_t<Rhs_>>{{lhs, rhs}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) && noexcept {
		return or_condition<and_condition, std::remove_cvref_t<Rhs_>>{{std::move(lhs), std::move(rhs)}, std::forward<Rhs_>(rhs_)};
	}

	constexpr auto to_string() const noexcept {
		return brackets_helper_s<and_condition>{}(lhs, rhs);
	}
};

template <typename Lhs, typename Rhs>
struct not_equal {
	Lhs field;
	Rhs value;

	constexpr auto to_string() const noexcept {
		return basic_string_literal{field} + " != " + to_string_s{}(value);
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) const& {
		return and_condition<not_equal, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) && noexcept {
		return and_condition<not_equal, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) const& noexcept {
		return or_condition<not_equal, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) && noexcept {
		return or_condition<not_equal, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
};

template <typename CharT, size_t N, typename Rhs>
not_equal(CharT const (&)[N], Rhs) -> not_equal<basic_string_literal<CharT, N - 1>, Rhs>;

template <typename Lhs, typename Rhs>
struct equal {
	Lhs field;
	Rhs value;

	constexpr auto to_string() const noexcept {
		return basic_string_literal{field} + " = " + to_string_s{}(value);
	}

	constexpr not_equal<Lhs, Rhs> operator!() const& noexcept {
		return {value};
	};

	constexpr not_equal<Lhs, Rhs> operator!() && noexcept {
		return {std::move(value)};
	};

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) const& {
		return and_condition<equal, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) && noexcept {
		return and_condition<equal, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) const& noexcept {
		return or_condition<equal, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) && noexcept {
		return or_condition<equal, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
};

template <typename CharT, size_t N, typename Rhs>
equal(CharT const (&)[N], Rhs) -> equal<basic_string_literal<CharT, N - 1>, Rhs>;

template <typename Lhs, typename Rhs>
struct greater {
	Lhs field;
	Rhs value;

	constexpr auto to_string() const noexcept {
		return basic_string_literal{field} + " > " + to_string_s{}(value);
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) const& {
		return and_condition<greater, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) && noexcept {
		return and_condition<greater, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) const& noexcept {
		return or_condition<greater, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) && noexcept {
		return or_condition<greater, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
};

template <typename CharT, size_t N, typename Rhs>
greater(CharT const (&)[N], Rhs) -> greater<basic_string_literal<CharT, N - 1>, Rhs>;

template <typename Lhs, typename Rhs>
struct greater_equal {
	Lhs field;
	Rhs value;

	constexpr auto to_string() const& noexcept {
		return basic_string_literal{field} + " >= " + to_string_s{}(value);
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) const& {
		return and_condition<greater_equal, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) && noexcept {
		return and_condition<greater_equal, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) const& noexcept {
		return or_condition<greater_equal, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) && noexcept {
		return or_condition<greater_equal, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
};

template <typename CharT, size_t N, typename Rhs>
greater_equal(CharT const (&)[N], Rhs) -> greater_equal<basic_string_literal<CharT, N - 1>, Rhs>;

template <typename Lhs, typename Rhs>
struct less {
	Lhs field;
	Rhs value;

	constexpr auto to_string() const noexcept {
		return basic_string_literal{field} + " < " + to_string_s{}(value);
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) const& {
		return and_condition<less, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) && noexcept {
		return and_condition<less, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) const& noexcept {
		return or_condition<less, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) && noexcept {
		return or_condition<less, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
};

template <typename CharT, size_t N, typename Rhs>
less(CharT const (&)[N], Rhs) -> less<basic_string_literal<CharT, N - 1>, Rhs>;

template <typename Lhs, typename Rhs>
struct less_equal {
	Lhs field;
	Rhs value;

	constexpr auto to_string() const noexcept {
		return basic_string_literal{field} + " <= " + to_string_s{}(value);
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) const& {
		return and_condition<less_equal, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) && noexcept {
		return and_condition<less_equal, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) const& noexcept {
		return or_condition<less_equal, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) && noexcept {
		return or_condition<less_equal, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
};

template <typename CharT, size_t N, typename Rhs>
less_equal(CharT const (&)[N], Rhs) -> less_equal<basic_string_literal<CharT, N - 1>, Rhs>;

template <typename Lhs, typename Rhs>
struct not_like {
	Lhs field;
	Rhs value;

	constexpr auto to_string() const noexcept {
		return basic_string_literal{field} + " NOT LIKE " + to_string_s{}(value);
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) const& {
		return and_condition<not_like, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) && noexcept {
		return and_condition<not_like, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) const& noexcept {
		return or_condition<not_like, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) && noexcept {
		return or_condition<not_like, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
};

template <typename CharT, size_t N, typename Rhs>
not_like(CharT const (&)[N], Rhs) -> not_like<basic_string_literal<CharT, N - 1>, Rhs>;

template <typename Lhs, typename Rhs>
struct like {
	Lhs field;
	Rhs value;

	constexpr auto to_string() const noexcept {
		return basic_string_literal{field} + " LIKE " + to_string_s{}(value);
	}

	constexpr not_like<Lhs, Rhs> operator!() const& noexcept {
		return {value};
	};

	constexpr not_like<Lhs, Rhs> operator!() && noexcept {
		return {std::move(value)};
	};

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) const& {
		return and_condition<like, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator&&(Rhs_&& rhs_) && noexcept {
		return and_condition<like, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) const& noexcept {
		return or_condition<like, std::remove_cvref_t<Rhs_>>{{field, value}, std::forward<Rhs_>(rhs_)};
	}

	template <typename Rhs_>
	constexpr auto operator||(Rhs_&& rhs_) && noexcept {
		return or_condition<like, std::remove_cvref_t<Rhs_>>{{std::move(field), std::move(value)}, std::forward<Rhs_>(rhs_)};
	}
};

template <typename CharT, size_t N, typename Rhs>
like(CharT const (&)[N], Rhs) -> like<basic_string_literal<CharT, N - 1>, Rhs>;

template <typename Lhs, typename Rhs>
using eq = equal<Lhs, Rhs>;

template <typename Lhs, typename Rhs>
using gt = greater<Lhs, Rhs>;

template <typename Lhs, typename Rhs>
using gt_eq = greater_equal<Lhs, Rhs>;

template <typename Lhs, typename Rhs>
using lt = less<Lhs, Rhs>;

template <typename Lhs, typename Rhs>
using lt_eq = less_equal<Lhs, Rhs>;

}

#endif
