#include <mlib/pp/is-empty.h>
#include <mlib/pp/map.h>

#define DECLARE_VAR(Name, _nil, Counter) int Name = Counter;

// MLIB_IS_EMPTY(42);
MLIB_MAP_MACRO(DECLARE_VAR, ~, foo, bar, baz)
