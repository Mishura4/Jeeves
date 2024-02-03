#ifndef MIMIRON_TOOLS_TOOLS_H_
#define MIMIRON_TOOLS_TOOLS_H_

#include <utility>
#include <functional>

namespace mimiron {

template <typename Key, typename Value, typename Hasher = std::hash<Key>, typename Equal = std::equal_to<>>
class cache;

}

#endif MIMIRON_TOOLS_TOOLS_H_