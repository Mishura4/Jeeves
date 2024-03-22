#ifndef MIMIRON_WOW_API_REALM
#define MIMIRON_WOW_API_REALM

#include <string>

#include "wow/api/wow_api.h"
#include "wow/api/localized_string.h"

namespace mimiron::wow {

/**
 * Entry in a list of realms.
 */
struct realm_entry {
  api_link key;
  localized_string name;
  int64_t id;
  std::string slug;
};

/**
 * Full realm object.
 */
struct realm {
  struct region_t {
    api_link key;
    localized_string name;
    int64_t id;
  };

  struct type_t {
    std::string type;
    localized_string name;
  };

  int64_t id;
  region_t region;
  api_link connected_realm;
  localized_string name;
  localized_string category;
  std::string locale;
  std::string timezone;
  type_t type;
  bool is_tournament;
  std::string slug;
};

};

#endif /* MIMIRON_WOW_API_REALM */

