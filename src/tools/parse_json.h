#ifndef MIMIRON_PARSE_JSON_H_
#define MIMIRON_PARSE_JSON_H_

#include <vector>
#include <format>
#include <type_traits>
#include <charconv>

#include <boost/pfr.hpp>

#include <dpp/json.h>

#include "exception.h"
#include "localized_string.h"

namespace mimiron {

template <typename T>
constexpr inline auto parse_json = [](const nlohmann::json& j) {
	return j.get<T>();
};

template <typename T>
constexpr inline auto parse_json<std::optional<T>> = [](const nlohmann::json& j) {
	return j.get<T>();
};

template <typename T>
constexpr inline auto parse_json<std::vector<T>> = [](const nlohmann::json& j) {
	std::vector<T> ret;

	for (auto sub_object : j.items()) {
		ret.emplace_back(parse_json<T>(sub_object.value()));
	}
	return ret;
};

template <typename T, size_t N>
constexpr inline auto parse_one = [](const nlohmann::json& j, T& value) {
	using field = boost::pfr::tuple_element_t<N, T>;
	const auto& name = boost::pfr::get_name<N, T>();

	try {
		if (auto it = j.find(name); it == j.end()) {
			if constexpr (is_optional<boost::pfr::tuple_element_t<N, T>>) {
				boost::pfr::get<N>(value) = std::nullopt;
			} else {
				throw dpp::parse_exception{std::format("non-optional field `{}` is not present", name)};
			}
		} else {
			if constexpr (std::is_same_v<field, bool>) {
				if (it->is_string()) {
					boost::pfr::get<N>(value) = it->template get<std::string>() == "true";
					return;
				}
				boost::pfr::get<N>(value) = *it;
			} else if constexpr (std::is_arithmetic_v<field>) {
				if (it->is_string()) {
					std::string val = *it;
					auto [end, err] = std::from_chars(val.data(), val.data() + val.size(), boost::pfr::get<N>(value));
					if (err != std::errc{}) {
						throw dpp::parse_exception{std::format("failed to parse number in string \"{}\" for field `{}`", val, name)};
					}
					return;
				}
				boost::pfr::get<N>(value) = *it;
			} else if constexpr (std::is_same_v<field, std::string>) {
				if (it->is_number()) {
					if (it->is_number_float()) {
						boost::pfr::get<N>(value) = std::to_string(static_cast<long double>(j));
					} else {
						boost::pfr::get<N>(value) = std::to_string(static_cast<int64_t>(j));
					}
					return;
				}
				boost::pfr::get<N>(value) = *it;
			} else if constexpr (requires { { field::from_json(j) } -> std::convertible_to<field>; }) {
				boost::pfr::get<N>(value) = field::from_json(*it);
			} else {
				boost::pfr::get<N>(value) = parse_json<field>(it.value());
			}
		}
	} catch (const nlohmann::detail::exception &e) {
		throw exception{std::format("failed to parse field `{}`: {}", name, e.what())};
	}
};

template <typename T>
requires (std::is_aggregate_v<T>)
constexpr inline auto parse_json<T> = [](const nlohmann::json& j) {
	T value;

	[]<size_t... Ns>(const nlohmann::json& j, T& value, std::index_sequence<Ns...>) {
		(parse_one<T, Ns>(j, value), ...);
	}(j, value, std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
	return value;
};

}

#endif
