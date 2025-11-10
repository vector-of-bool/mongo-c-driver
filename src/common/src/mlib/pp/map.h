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

#define MLIB_MAP_MACRO(Action, Constant, ...) \
   MLIB_EVAL( \
      MLIB_IF_ELSE(MLIB_IS_EMPTY(__VA_ARGS__)) \
         (MLIB_NOTHING) \
         (_mlibMapMacroFirst) \
      MLIB_NOTHING()(Action, Constant, __VA_ARGS__))

#define _mlibMapMacroFirst(Action, Constant, ...) \
   MLIB_IF_ELSE(_mlibHasComma(__VA_ARGS__)) \
      (_mlibMapMacro_A) \
      (_mlibMapMacro_final) \
   MLIB_NOTHING() (Action, Constant, 0, __VA_ARGS__)

#define _mlibMapMacro_final(Action, Constant, Counter, Final) \
   Action(Final, Constant, (Counter))

#define _mlibMapMacro_A(Action, Constant, Counter, Head, ...) \
   Action(Head, Constant, (Counter)) \
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

// clang-format on

#endif // MLIB_PP_MAP_H_INCLUDED
