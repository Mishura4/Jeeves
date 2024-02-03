#ifndef MIMIRON_DATABASE_TABLE_H_
#define MIMIRON_DATABASE_TABLE_H_

#include <utility>
#include <tuple>

#include <boost/pfr.hpp>

#include "database/query.h"
#include "tools/string_literal.h"

namespace mimiron::sql {

template <typename T, auto V>
inline constexpr bool is_member_of = false;

template <typename T, typename U, U T::* ptr>
inline constexpr bool is_member_of<T, ptr> = true;

template <typename T, basic_string_literal Name>
constexpr size_t index_of_member_f() {
	size_t ret = []<size_t... Ns>(std::index_sequence<Ns...>) constexpr {
		size_t i = 0;

		return ((++i, Name == boost::pfr::get_name<Ns, T>()) || ...), i - 1;
	}(std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
	if (ret >= boost::pfr::tuple_size_v<T>)
		throw std::invalid_argument{"field not found"};
	return ret;
}

template <typename T, basic_string_literal Name>
inline constexpr size_t index_of_member = index_of_member_f<T, Name>();

template <basic_string_literal Name, typename T>
class table {
public:
	template <basic_string_literal... Fields>
	constexpr static inline auto select = sql::select<std::tuple<field<boost::pfr::tuple_element_t<index_of_member<T, Fields>, T>, Fields>...>>.from(Name);

private:
};

}

#endif
