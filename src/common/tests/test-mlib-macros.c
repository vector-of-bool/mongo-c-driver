#include <mlib/platform/attributes.h>
#include <mlib/pp/args.h>
#include <mlib/pp/basic.h>
#include <mlib/pp/boolean.h>
#include <mlib/pp/is-empty.h>
#include <mlib/pp/map.h>
#include <mlib/static_assert.h>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

/// Define a simple static assert that does not depend on our preprocessor machinery
#define SUPER_SIMPLE_STATIC_ASSERT(Name, ...) \
   extern int _static_assertion__##Name##_lineno_##__LINE__[(__VA_ARGS__) ? 1 : -1]

// basic.h
SUPER_SIMPLE_STATIC_ASSERT(trivial, 1);
SUPER_SIMPLE_STATIC_ASSERT(unequal, 2 != 5);
SUPER_SIMPLE_STATIC_ASSERT(nothing_is_empty, 1 == MLIB_NOTHING() 1);
SUPER_SIMPLE_STATIC_ASSERT(nothing_is_empty, 1 == MLIB_NOTHING(one_arg) 1);
SUPER_SIMPLE_STATIC_ASSERT(nothing_is_empty, 1 == MLIB_NOTHING("content", "ignored") 1);
SUPER_SIMPLE_STATIC_ASSERT(just_is_simple, 1 == MLIB_JUST(1));
SUPER_SIMPLE_STATIC_ASSERT(just_is_simple, 1 == MLIB_JUST(/* empty */) 1);
SUPER_SIMPLE_STATIC_ASSERT(first_arg, 1 == MLIB_FIRST_ARG(1, 2, 3));
SUPER_SIMPLE_STATIC_ASSERT(first_arg_empty, 1 == MLIB_FIRST_ARG() 1);

// boolean.h
SUPER_SIMPLE_STATIC_ASSERT(bool_true, MLIB_BOOLEAN(1));
SUPER_SIMPLE_STATIC_ASSERT(bool_false, !MLIB_BOOLEAN(0));
SUPER_SIMPLE_STATIC_ASSERT(bool_negate, !MLIB_NEGATE(1));
SUPER_SIMPLE_STATIC_ASSERT(bool_negate, MLIB_NEGATE(0));
SUPER_SIMPLE_STATIC_ASSERT(bool_or, MLIB_OR(0, 1));
SUPER_SIMPLE_STATIC_ASSERT(bool_or, MLIB_OR(1, 0));
SUPER_SIMPLE_STATIC_ASSERT(bool_or, MLIB_OR(1, 1));
SUPER_SIMPLE_STATIC_ASSERT(bool_or, !MLIB_OR(0, 0));
SUPER_SIMPLE_STATIC_ASSERT(bool_and, !MLIB_AND(0, 1));
SUPER_SIMPLE_STATIC_ASSERT(bool_and, !MLIB_AND(1, 0));
SUPER_SIMPLE_STATIC_ASSERT(bool_and, MLIB_AND(1, 1));
SUPER_SIMPLE_STATIC_ASSERT(bool_and, !MLIB_AND(0, 0));
SUPER_SIMPLE_STATIC_ASSERT(bool_xor, !MLIB_XOR(0, 0));
SUPER_SIMPLE_STATIC_ASSERT(bool_xor, MLIB_XOR(1, 0));
SUPER_SIMPLE_STATIC_ASSERT(bool_xor, MLIB_XOR(0, 1));
SUPER_SIMPLE_STATIC_ASSERT(bool_xor, !MLIB_XOR(1, 1));

// if-else.h
SUPER_SIMPLE_STATIC_ASSERT(if_else_1, MLIB_IF_ELSE(1)(1)(0));
SUPER_SIMPLE_STATIC_ASSERT(if_else_0, !MLIB_IF_ELSE(0)(1)(0));
SUPER_SIMPLE_STATIC_ASSERT(if, MLIB_IF(1)(1));
SUPER_SIMPLE_STATIC_ASSERT(if, 42 == MLIB_IF(0)("discarded tokens") 42);
SUPER_SIMPLE_STATIC_ASSERT(unless, 1 == MLIB_UNLESS(0)(1));
SUPER_SIMPLE_STATIC_ASSERT(unless, 1 == MLIB_UNLESS(1)() 1);

// is-empty.h
SUPER_SIMPLE_STATIC_ASSERT(is_empty, MLIB_IS_EMPTY());
SUPER_SIMPLE_STATIC_ASSERT(is_empty, !MLIB_IS_EMPTY(a));
SUPER_SIMPLE_STATIC_ASSERT(is_empty, !MLIB_IS_EMPTY(a, b));
SUPER_SIMPLE_STATIC_ASSERT(is_empty, !MLIB_IS_EMPTY(()));
SUPER_SIMPLE_STATIC_ASSERT(is_not_empty, !MLIB_IS_NOT_EMPTY());
SUPER_SIMPLE_STATIC_ASSERT(is_not_empty, MLIB_IS_NOT_EMPTY(a));
SUPER_SIMPLE_STATIC_ASSERT(is_not_empty, MLIB_IS_NOT_EMPTY(a, ));
SUPER_SIMPLE_STATIC_ASSERT(is_not_empty, MLIB_IS_NOT_EMPTY(()));
SUPER_SIMPLE_STATIC_ASSERT(if_not_empty, MLIB_IF_NOT_EMPTY(a)(1) == 1);
SUPER_SIMPLE_STATIC_ASSERT(if_not_empty, MLIB_IF_NOT_EMPTY()(1) 4 == 4);

// args.h
SUPER_SIMPLE_STATIC_ASSERT(arg_count, MLIB_ARG_COUNT() == 0);
SUPER_SIMPLE_STATIC_ASSERT(arg_count, MLIB_ARG_COUNT(a) == 1);
SUPER_SIMPLE_STATIC_ASSERT(arg_count, MLIB_ARG_COUNT(a, b) == 2);
SUPER_SIMPLE_STATIC_ASSERT(arg_count, MLIB_ARG_COUNT(, ) == 2);
SUPER_SIMPLE_STATIC_ASSERT(arg_count, MLIB_ARG_COUNT(()) == 1);
SUPER_SIMPLE_STATIC_ASSERT(arg_count, MLIB_ARG_COUNT((a, b)) == 1);

#define foo_argc_0() 1729
#define foo_argc_1(A) (42 + A)
#define foo(...) MLIB_ARGC_PICK(foo, __VA_ARGS__)
SUPER_SIMPLE_STATIC_ASSERT(pick, foo() == 1729);
SUPER_SIMPLE_STATIC_ASSERT(pick, foo(8) == 50);

mlib_extern_c_begin();
mlib_diagnostic_push();
mlib_diagnostic_pop();
mlib_extern_c_end();

#define DECLARE_VAR(Name, _nil, Counter) int Name = Counter;
MLIB_MAP_MACRO(DECLARE_VAR, @, foo, bar, baz)

#define SUM(N, _nil, _counter) +N
SUPER_SIMPLE_STATIC_ASSERT(sum, MLIB_MAP_MACRO(SUM, ~, 1, 2, 1) == 4);

/*-
 * Test: Try to use an MLIB_ARGC_PICK macro definition that ultimately expands to
 * the same identifier as the original macro expansion. Macro blue-painting rules
 * state that the final macro should not expand again, but we need to check that
 * here.
 */
int
foo_func(int a)
{
   return a;
}
#define foo_func(...) MLIB_ARGC_PICK(_foo_func, __VA_ARGS__)
#define _foo_func_argc_1(A) foo_func(A)


int
use_argc_func(void)
{
   // Force additional macro expansions, which will detect a faulty ARGC_PICK
   return MLIB_JUST(MLIB_JUST(foo_func(31) == 31));
}
