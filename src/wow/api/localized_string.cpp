#include "localized_string.h"

namespace mimiron::wow {


namespace {

template <size_t N>
std::string find_locale(const dpp::json& json) {
	if (auto it = json.find(locales[N].code); it != json.end()) {
		return *it;
	}
	return {};
};

template <size_t... Ns>
std::array<std::string, sizeof...(Ns)> find_locales(const dpp::json& json, std::index_sequence<Ns...>) {
	return {find_locale<Ns>(json)...};
}

}

localized_string localized_string::from_json(const dpp::json& j) {
	if (j.is_string()) {
		auto ret = localized_string{};
		ret.values[0] = j;
		return ret;
	}
	return {.values = find_locales(j, std::make_index_sequence<locales.size()>{})};
}

}


