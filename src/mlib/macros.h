#ifndef MLIB_MACROS_H
#define MLIB_MACROS_H

#include "./user-check.h"

// clang-format off
#if defined(__GNUC__) || defined(__clang__) || (defined(_MSC_VER) && _MSC_VER >= 1900)
    #define mlib_kw_inline inline
#elif defined(_MSC_VER)
    #define mlib_kw_inline __inline
#else
    #define mlib_kw_inline _Do_not_know_how_to_say_inline_on_this_compiler
#endif

/**
 * @brief Declare a function to be inline-defined, but with external linkage.
 *
 * This has the effect that there will be only one definiton if the final
 * program, even if the function is defined in multiple translation units
 * (e.g as the result of #inclusion). This mimicks the linkage behavior of
 * C++ 'inline' keyword, which is not shared by C99's 'inline'.
 */
#ifdef __INTELLISENSE__
    // Give a pretty definition for intellisense readers
    #define mlib_inline inline
#elif _WIN32
    // On MSVC/Windows, inline functions are implicitly in COMDAT, even in C
    #define mlib_inline extern mlib_kw_inline
#else
    // On other platforms, declare the symbol "weak" to cause symbol merging,
    // and "hidden" to disable the ability for the symbol to be overridden via
    // interposition
    #define mlib_inline extern mlib_kw_inline __attribute__((weak, visibility("hidden")))
#endif

// clang-format on

#endif // MLIB_MACROS_H
