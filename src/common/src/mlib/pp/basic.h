/**
 * @file mlib/pp/basic.h
 * @brief Very basic preprocessor macros and utilities
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

#ifndef MLIB_PP_BASIC_H_INCLUDED
#define MLIB_PP_BASIC_H_INCLUDED

/**
 * @brief A function-like macro that expands to nothing
 *
 * Takes any number of arguments, and will always expand to no tokens.
 *
 * This macro is primarily useful to prevent or defer to the expansion of other
 * macros.
 *
 * ## Supressing function-like macros
 *
 * You can place this between a function-like macro name and the opening
 * parenthesis of its argument list to prevent/defer the expansion of that
 * function-like macro:
 *
 *      SOME_FUNC_MACRO MLIB_NOTHING("separator") (arg1, arg2, arg3)
 *
 * this causes the above to expand to
 *
 *      SOME_FUNC_MACRO(arg1, arg2, arg3)
 *
 * even if `SOME_FUNC_MACRO` is a defined. Note that applying another expansion
 * *will* expand `SOME_FUNC_MACRO`.
 */
#define MLIB_NOTHING(...)

/**
 * @brief Expand to the argument list directly when invoked as a function-like macro
 *
 * This can be used to force another expansion pass of the arguments, since using
 * a function-like macro will cause the preprocessor to expands the argument list.
 */
#define MLIB_JUST(...) __VA_ARGS__

/**
 * @brief Expand to the first argument in the given argument list. If given no
 * arguments, expands to nothing.
 */
#define MLIB_FIRST_ARG(...) _mlibFirstArgJust(_mlibFirstArg MLIB_NOTHING("MSVC deferral") (__VA_ARGS__, ~))
#define _mlibFirstArg(X, ...) X
#define _mlibFirstArgJust(...) __VA_ARGS__

/**
 * @brief Perform token-pasting after expanding the macro arguments
 *
 * This is needed instead of direct token-pasting, because token pasting within
 * a macro definition will paste the immediately passed argument tokens without
 * expanding them. Passing it through this macro will cause the expected behavior:
 *
 *      #define FOO lhs
 *      #define BAR _rhs
 *
 *      #define BAD_PASTE(A, B) A ## B
 *
 *      BAD_PASTE(FOO, BAR)   // ← Bad:  Expands to `FOOBAR`
 *      MLIB_PASTE(FOO, BAR)  // ← Good: Expands to `lhs_rhs`
 *
 * Additionally, `MLIB_PASTE_<n>` is also defined for pasting 3, 4, and 5 tokens
 */
#define MLIB_PASTE(A, ...) _mlibPaste(A, __VA_ARGS__)
// Paste three tokens
#define MLIB_PASTE_3(A, B, ...) MLIB_PASTE(A, MLIB_PASTE(B, __VA_ARGS__))
// Paste four tokens
#define MLIB_PASTE_4(A, B, C, ...) MLIB_PASTE(A, MLIB_PASTE_3(B, C, __VA_ARGS__))
// Paste five tokens
#define MLIB_PASTE_5(A, B, C, D, ...) MLIB_PASTE(A, MLIB_PASTE_4(B, C, D, __VA_ARGS__))
#define _mlibPaste(A, ...) A##__VA_ARGS__

/**
 * @brief String-ify the given set of preprocessor tokens after macro expansion
 *
 * This is needed for the same reason that `MLIB_PASTE` is needed: string-ifying
 * a macro parameter directly will string-ify the unexpanded argument:
 *
 *      #define FOO the_expanded_macro
 *
 *      #define BAD_STR(...) #__VA_ARGS__
 *
 *      BAD_STR(FOO)  // ← Bad:  Expands to "FOO"
 *      MLIB_STR(FOO) // ← Good: Expands to "the_expanded_macro"
 */
#define MLIB_STR(...) _mlibStr(__VA_ARGS__)
#define _mlibStr(...) "" #__VA_ARGS__

/**
 * @brief Force macro expansion on the argument list to occur at least 32 times
 *
 * This is useful in the case that macro expansion causes additional macro forms
 * to appear, which will need their own macro expansions.
 */
#define MLIB_EVAL(...) _mlibEvalSixteenTimes(__VA_ARGS__)
#define _mlibEvalOnce(...) __VA_ARGS__
#define _mlibEvalFourTimes(...) _mlibEvalOnce(_mlibEvalOnce(_mlibEvalOnce(_mlibEvalOnce(__VA_ARGS__))))
#define _mlibEvalSixteenTimes(...) \
   _mlibEvalFourTimes(_mlibEvalFourTimes(_mlibEvalFourTimes(_mlibEvalFourTimes(__VA_ARGS__))))

/**
 * @brief Pass a function-like macro name, inhibiting its expansion until the
 * next pass:
 *
 *      #define func_macro(x) x
 *
 *      MLIB_DEFERRED(func_macro)(foo)  // Expands to "func_macro(foo)", not "foo"
 */
#define MLIB_DEFERRED(MacroName)                                                                  \
   /* Expand to the macro name: */                                                                \
   MacroName /*-                                                                                  \
              * Place a separator between the function macro name and whatever comes next         \
              * in the file. Presumably, the next token will be the parens to invoke "MacroName", \
              * but this separator inhibits its expansion unless something else comes             \
              * along to do another expansion pass (e.g. MLIB_VAL())                              \
              */                                                                                  \
      MLIB_NOTHING("[separator]")

#endif // MLIB_PP_BASIC_H_INCLUDED
