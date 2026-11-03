#ifndef SIR_CORE_CONTAINERS_H
#define SIR_CORE_CONTAINERS_H

#include <EASTL/array.h>
#include <EASTL/deque.h>
#include <EASTL/fixed_string.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/hash_map.h>
#include <EASTL/hash_set.h>
#include <EASTL/optional.h>
#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>

#include <stddef.h>

namespace sir
{
template <typename T>
using SIRList = eastl::vector<T>;

template <typename K, typename V>
using SIRHashMap = eastl::hash_map<K, V>;

using SIRString = eastl::string;

template <typename T, size_t nCapacity>
using SIRArray = eastl::array<T, nCapacity>;

template <typename T, size_t nCapacity>
using SIRFixedList = eastl::fixed_vector<T, nCapacity, false>;

template <typename K>
using SIRHashSet = eastl::hash_set<K>;

template <typename T>
using SIRDeque = eastl::deque<T>;

using SIRStringView = eastl::string_view;

template <typename T, size_t nExtent = eastl::dynamic_extent>
using SIRSpan = eastl::span<T, nExtent>;

template <typename T>
using SIROptional = eastl::optional<T>;

template <typename A, typename B>
using SIRPair = eastl::pair<A, B>;

template <int nCapacity>
using SIRFixedString = eastl::fixed_string<char, nCapacity, false>;
}

#endif
