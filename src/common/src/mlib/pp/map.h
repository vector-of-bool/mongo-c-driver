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

#include <mlib/pp/if-else.h>
#include <mlib/pp/is-empty.h>

// clang-format off
#define MLIB_MAP_MACRO_(Action, Constant, ...) \
    (MLIB_IS_EMPTY(__VA_ARGS__)) \
        (MLIB_NOTHING) \
        (_mlibMapMacro_firstPass)
// clang-format on

#if 1
#define MLIB_MAP_MACRO(...) \
   MLIB_IF_ELSE(_mlibIsInMapMacroExpansion())(_mlibMapDeferred(__VA_ARGS__))(_mlibMapImmediate(__VA_ARGS__))

#define _mlibMapImmediate(Func, Constant, ...) MLIB_EVAL(_mlibMapInit(Func, Constant, __VA_ARGS__))
#define _mlibMapDeferred(Func, Constant, ...) MLIB_DEFERRED(_mlibMapInit)(Func, Constant, __VA_ARGS__)

#define _mlibIsInMapMacroExpansion() MLIB_IS_NOT_EMPTY(_mlibTryGenerateEmptyMap())
#define _mlibTryGenerateEmptyMap() _mlibMapImmediate(MLIB_NOTHING, ~, ~)

#define _mlibMapInit(Func, Constant, ...) MLIB_IF_NOT_EMPTY(__VA_ARGS__)(_mlibMapImpl(Func, Constant, 0, __VA_ARGS__))

#define _mlibMapImpl(Func, Constant, Counter, Head, ...) \
   Func(Head, Constant, (Counter))                       \
      MLIB_IF_NOT_EMPTY(__VA_ARGS__)(MLIB_DEFERRED(_mlibMapNext)()(Func, Constant, Counter + 1, __VA_ARGS__))
#define _mlibMapNext() _mlibMapImpl
#endif /// 0

#endif // MLIB_PP_MAP_H_INCLUDED
