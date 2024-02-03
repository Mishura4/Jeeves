#include "character.h"

#include <ranges>

#include <sol/sol.hpp>

namespace mimiron::wow {

auto character::from_guildbook(std::string name, const sol::table &data)
    -> character {
  character ret{std::move(name)};

  if (sol::optional<int> v = data["class"]; v.has_value())
    ret.class_id = *v;
  if (sol::optional<int> v = data["race"]; v.has_value())
    ret.race = *v;
  if (sol::optional<int> v = data["gender"]; v.has_value())
    ret.gender = *v;
  if (sol::optional<int> v = data["profession1"]; v.has_value())
    ret.prof_main.id = *v;
  if (sol::optional<int> v = data["profession1Level"]; v.has_value())
    ret.prof_main.level = *v;
  if (sol::optional<int> v = data["profession2"]; v.has_value())
    ret.prof_secondary.id = *v;
  if (sol::optional<int> v = data["profession2Level"]; v.has_value())
    ret.prof_secondary.level = *v;
  if (sol::optional<int> v = data["cookingLevel"]; v.has_value())
    ret.prof_cooking.level = *v;
  if (sol::optional<int> v = data["firstAidLevel"]; v.has_value())
    ret.prof_cooking.level = *v;
  if (sol::optional<int> v = data["fishingLevel"]; v.has_value())
    ret.fishing_level = *v;
  if (sol::optional<sol::table> v = data["profession1Recipes"];
      v.has_value() && !v->empty()) {
    ret.prof_main.recipes.reserve(v->size());
    for (const auto &[key, value] : *v) {
      if (value.is<int>()) {
        ret.prof_main.recipes.push_back(value.as<int>());
      }
    }
  }
  if (sol::optional<sol::table> v = data["profession2Recipes"];
      v.has_value() && !v->empty()) {
    ret.prof_secondary.recipes.reserve(v->size());
    for (const auto &[key, value] : *v) {
      if (value.is<int>()) {
        ret.prof_secondary.recipes.push_back(value.as<int>());
      }
    }
  }
  return (ret);
}

} // namespace mimiron::wow