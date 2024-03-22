#ifndef MIMIRON_TOOLS_TOOLS_H_
#define MIMIRON_TOOLS_TOOLS_H_

#include <utility>
#include <functional>

namespace mimiron {

template <typename T, template<typename ...> class Of>
inline constexpr bool is_specialization_v = false;

template <typename T, template<typename ...> class Of>
inline constexpr bool is_specialization_v<const T, Of> = is_specialization_v<T, Of>;

template <template<typename ...> class Of, typename ...Ts>
inline constexpr bool is_specialization_v<Of<Ts...>, Of> = true;

template <typename T, template<typename ...> class Of>
using is_specialization_of = std::bool_constant<is_specialization_v<T, Of>>;

template <typename T, auto Deleter>
struct unique_ptr_deleter {
	void operator()(T* ptr) const noexcept(std::is_nothrow_invocable_v<decltype(Deleter), T*>) {
		Deleter(ptr);
	}
};

template <typename T, auto Deleter, template <typename, typename> typename PtrType = std::unique_ptr>
using managed_ptr = PtrType<T, unique_ptr_deleter<T, Deleter>>;

template <typename T>
inline constexpr bool is_optional = false;

template <typename T>
inline constexpr bool is_optional<std::optional<T>> = true;

template <typename Key, typename Value, typename Hasher = std::hash<Key>, typename Equal = std::equal_to<>>
class cache;

}

#endif MIMIRON_TOOLS_TOOLS_H_