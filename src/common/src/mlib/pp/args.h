/**
 * @file mlib/pp/args.h
 * @brief Preprocessor macros for working with variadic argument lists
 * @date 2025-07-29
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
 *
 */

#ifndef MLIB_PP_ARGS_H_INCLUDED
#define MLIB_PP_ARGS_H_INCLUDED

#include "./if-else.h"
#include "./is-empty.h"

// clang-format off

/**
 * @brief Expands to an integer literal corresponding to the number of macro
 * arguments. Supports up to 15 arguments.
 *
 * This may be replaced someday in the future with a possible __VA_NARG__()
 * preprocessor intrinsic.
 *
 * @note The preprocessor only considers parentheses for token grouping, meaning
 * that braces {} or square brackets [] will not "group" a token sequence that
 * contains a comma. This can cause surprising results if the macro is invoked
 * with a compound literal or braced initializer. To avoid this, compound literals
 * should be wrapped in parentheses for such function-like macros.
 */
#define MLIB_ARG_COUNT(...) \
   _mlibPickSixteenth( \
        /*-
         * Expand the argument list. For every top-level comma, shifts the
         * argument list over by 1.
         */ \
        __VA_ARGS__ \
        /*-
         * If given no arguments, don't insert the first comma, preventing us
         * from incorrectly yielding `1` for zero arguments:
         */ \
        MLIB_OPT_COMMA(__VA_ARGS__) \
        /*-
         * Argument counts, up to 15. For each top-level comment in __VA_ARGS__,
         * these are shifted along, with `0` in the sixteenth position to start.
         * To support more than 15 arguments, more integers must be added,
         * and `_mlibPickSixteenth` needs to be extended to a higher number.
         */ \
        15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, ~)

/**
 * @brief Expand to a different function-like macro invocation based on
 * the number of arguments passed to the macro:
 *
 *      MLIB_ARGC_PICK(<prefix>, <args>)
 *
 * Where `<prefix>` is an arbitrary identifier and `<args>` is zero or more
 * arguments. When given `<N>` argments in `<args>`, this will expand to the
 * following:
 *
 *      <prefix>_argc_<N>(<args>)
 *
 * @warning The preprocessor only counts parentheses when looking at token
 * grouping. This means that things like C99 compound literals will count as
 * more than one argument if they have more than one initializer (and thus
 * contain a comma). Compound literals used with a macro like this requires that
 * the compound literal be wrapped in parentheses in order to count as a single
 * argument:
 *
 *      MLIB_ARGC_PICK(foo,  (my_type){1, 2, 3} ) // ← Expands to foo_argc_3(...)
 *      MLIB_ARGC_PICK(foo, ((my_type){1, 2, 3})) // ← Expands to foo_argc_1(...)
 */
#define MLIB_ARGC_PICK \
    MLIB_IF_ELSE(_mlibIsInPickExpansion()) \
        (_mlibArgcPickDeferred) \
        (_mlibArgcPickImmediate)
#define _mlibArgcPickImmediate(Prefix, ...) _mlibArgcPickImpl(Prefix, __VA_ARGS__)
#define _mlibArgcPickDeferred(Prefix, ...) MLIB_DEFERRED(_mlibArgcPickImpl)(Prefix, __VA_ARGS__)

#define _mlibArgcPickImpl(Prefix, ...) MLIB_ARGC_PASTE(Prefix, __VA_ARGS__)(__VA_ARGS__)

// This macro expands to `1` iff `_mlibArgcPickImmediate` is currently expanding, otherwise `0`
#define _mlibIsInPickExpansion() MLIB_IS_NOT_EMPTY(_mlibArgcPickImmediate(_mlibPickDetect, ~))
#define _mlibPickDetect_argc_1(_nil)

/**
 * @brief Perform the token-paste done by `MLIB_ARGC_PICK` without invoking the
 * result as a function-like macro.
 *
 * This is useful to control the expansion order of variadic macros used with
 * `ARGC_PICK`
 */
#define MLIB_ARGC_PASTE(Prefix, ...) \
    MLIB_PASTE_3(Prefix, _argc_, MLIB_ARG_COUNT(__VA_ARGS__))

/*-
 * This helper macro expands to its sixteenth argument, no matter how many
 * arguments it is given. You must pass at least sixteen arguments to this macro
 */
#define _mlibPickSixteenth(...) _mlibPickSixteenth1(__VA_ARGS__, ~)
#define _mlibPickSixteenth1(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, ...) _16

#endif // MLIB_PP_ARGS_H_INCLUDED
