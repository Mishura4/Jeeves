#ifndef MIMIRON_LOCALIZED_STRING_H_
#define MIMIRON_LOCALIZED_STRING_H_

#include <string>
#include <string_view>
#include <array>

#include <dpp/json.h>

#include "exception.h"

namespace mimiron::wow {

struct locale_info {
	std::string_view code;
	std::u8string_view language;
	std::u8string_view country;
};

enum class locale {
	en_us = 0,
	es_mx,
	pt_br,
	de_de,
	en_gb,
	es_es,
	fr_fr,
	it_it,
	ru_ru,
	ko_kr,
	zh_tw,

	american = en_us,
	mexican = es_mx,
	brazilian = pt_br,
	german = de_de,
	english = en_gb,
	spanish = es_es,
	french = fr_fr,
	russian = ru_ru,
	korean = ko_kr,
	taiwanese = zh_tw,

	unknown = std::numeric_limits<std::underlying_type_t<locale>>::max(),
	count = zh_tw + 1
};

inline constexpr auto locales = std::to_array<locale_info, std::to_underlying(locale::count)>({
	{"en_US", u8"english", u8"United States"},
	{"es_MX", u8"mexicano", u8"México"},
	{"pt_BR", u8"brasileiro", u8"Brazil"},
	{"de_DE", u8"deutsch", u8"Deutschland"},
	{"en_GB", u8"english", u8"United Kingdoms"},
	{"es_ES", u8"español", u8"España"},
	{"fr_FR", u8"français", u8"France"},
	{"it_IT", u8"italiano", u8"Italia"},
	{"ru_RU", u8"русский", u8"Россия"},
	{"ko_KR", u8"한국어", u8"대한민국"},
	{"zh_TW", u8"華語", u8"台灣"}
});

inline constexpr locale_info unknown_locale = {"unknown", u8"unknown", u8"unknown"};

struct localized_string {
	static localized_string from_json(dpp::json const& j);

	constexpr std::string_view operator[](locale loc) const noexcept;
	constexpr std::string_view operator[](std::string_view loc) const;

	std::array<std::string, locales.size()> values;
};

class locale_exception : public exception {
	using exception::exception;
};

constexpr std::string_view localized_string::operator[](locale loc) const noexcept {
	assert(std::to_underlying(loc) < values.size());

	return values[std::to_underlying(loc)];
}

constexpr std::string_view localized_string::operator[](std::string_view loc) const {
	auto it = std::ranges::find(locales, loc, &locale_info::code);

	if (it == locales.end()) {
		throw locale_exception{"invalid locale " + std::string{loc}};
	}
	return values[it - locales.begin()];
}

}

#endif
