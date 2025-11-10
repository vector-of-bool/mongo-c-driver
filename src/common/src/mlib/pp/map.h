/**
 * @file mlib/pp/map.h
 * @brief A MAP macro
 * @date 2025-10-31
 *
 * @copyright Copyright 2009-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MLIB_PP_MAP_H_INCLUDED
#define MLIB_PP_MAP_H_INCLUDED

#include <mlib/pp/basic.h>
#include <mlib/pp/if-else.h>
#include <mlib/pp/is-empty.h>

/**
 * @brief Perform a "macro map" expansion.
 *
 * Signature:
 *
 *    MLIB_MAP_MACRO(F, K, ...xs)
 *
 * **For each** Nth argument in `xs`, expands to approximately the following:
 *
 *    F(xs[N], K, N)
 *
 * That is, it "applies" the function (or function-like macro) `F` to each
 * argument in `xs`. The list element is passed as the first argument, the
 * argument `K` (the "constant") is always passed as the second argument, and the
 * third argument `N` is the zero-based index of the expansion (the "counter").
 *
 * If the argument `K` is not needed, it is common to pass a bogus token as a
 * placeholder, such as `~` or `@`, and simply ignore it in your definition of
 * `F`.
 *
 * @note The counter argument is not passed as a literal integer, but as a simple
 * series of `+1` that lengthens for each list item, i.e. `(0 + 1 + ... + 1)`.
 *
 * It is most common that `F` is itself another macro name that juggles the
 * argument list into some other form.
 */
#define MLIB_MAP_MACRO(Action, Constant, ...) MLIB_EVAL(_mlibMapMacro(Action, Constant, __VA_ARGS__))

// clang-format off
/**
 * @brief Like MLIB_MAP_MACRO, but does not force the recursive evaluation of
 * the mapping. Use this within other macro definitions that will already be
 * expanded with MLIB_EVAL
 */
#define _mlibMapMacro(Action, Constant, ...) \
   /*-
    * If passed no arguments, we want to immediately expand to nothing at all.
    * Otherwise, go to the first step in the map expansion
    */ \
   MLIB_IF_ELSE(MLIB_IS_EMPTY(__VA_ARGS__)) \
      (MLIB_NOTHING) \
      (_mlibMapMacroFirst) \
   MLIB_NOTHING()(Action, Constant, __VA_ARGS__)

#define _mlibMapMacroFirst(Action, Constant, ...) \
   /*-
    * We know we have at least one arg, but detect whether we are passed more
    * than one argument. We want to avoid warnings about missing argument
    * lists for variadic macros. This protects against that by immediately
    * short-circuiting to the final expansion if-and-only-if we are passed a
    * single item to map.
    */ \
   MLIB_IF_ELSE(_mlibHasComma(__VA_ARGS__)) \
      (_mlibMapMacro_A) \
      (_mlibMapMacro_final) \
   /*-
    * Invoke the first expansion helper. Pass a zero to initialize the counter.
    */ \
   MLIB_NOTHING() (Action, Constant, 0, __VA_ARGS__)

/*-
 * MAP() will now bounce between two different expansion helpers. Since neither
 * one immediately expands to itself, it allows the EVAL to repeatedly expand
 * each of the below definitions into each other. The two are identical other
 * than their name.
 */
#define _mlibMapMacro_A(Action, Constant, Counter, Head, ...) \
   /*-
    * Do the actual expansion to the user's invocation. Pass the next list
    * item as the first argument, the user's constant as the second argument,
    * and the incrementing counter as the third.
    *
    * The counter is parenthesized to make it a primary expression, preventing
    * operator precedence issues if the user attempts to do arithmetic with it.
    */ \
   Action(Head, Constant, (Counter)) \
   /*-
    * Decide what to do next. If there is more than one argument remaining,
    * bounce to the other map expansion helper. If there is only one argument
    * remaining to expand, instead go to the final expansion helper.
    */ \
   MLIB_IF_ELSE(_mlibHasComma(__VA_ARGS__)) \
      (_mlibMapMacro_B) \
      (_mlibMapMacro_final) \
   MLIB_NOTHING() (Action, Constant, Counter + 1, __VA_ARGS__)

#define _mlibMapMacro_B(Action, Constant, Counter, Head, ...) \
   Action(Head, Constant, (Counter)) \
   MLIB_IF_ELSE(_mlibHasComma(__VA_ARGS__)) \
      (_mlibMapMacro_A) \
      (_mlibMapMacro_final) \
   MLIB_NOTHING() (Action, Constant, Counter + 1, __VA_ARGS__)

#define _mlibMapMacro_final(Action, Constant, Counter, Final) \
   Action(Final, Constant, (Counter))

// clang-format on

#endif // MLIB_PP_MAP_H_INCLUDED
