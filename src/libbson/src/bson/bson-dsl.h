#ifndef BSON_BSON_DSL_H_INCLUDED
#define BSON_BSON_DSL_H_INCLUDED

/**
 * @file bson-dsl.h
 * @brief Define a C-preprocessor DSL for working with BSON objects
 *
 * This file defines an embedded DSL for working with BSON objects consisely and
 * correctly. The main macros are:
 *
 * - @see bsonBuild(Ptr, ...)
 *       Construct a new document and assign the result to `Ptr`.
 *       bson_destroy(Ptr) will be called before the assignment, but after the
 *       construction.
 * - @see bsonBuildAppend(Ptr, ...)
 *       Append new items to the bson_t at `Ptr`. `Ptr` must be an initialized
 *       bson_t object.
 * - @see bsonBuildDecl(Var, ...)
 *       Declare a new bson_t `Var` and construct a new document into it.
 *
 * The remaining arguments are the same for all three macros, and the same as
 * the `doc()` element. See below.
 */

#include "bson.h"

enum { BSON_DSL_DEBUG = 1 };

typedef struct _bsonBuildContext_t {
   bson_t *doc;
   const char *key;
   int key_len;
   int index;
   char strbuf64[64];
   struct _bsonBuildContext_t *parent;
} _bsonBuildContext_t;

struct _bsonVisitContext_t {
   const bson_t *doc;
   bson_iter_t iter;
   const struct _bsonVisitContext_t *parent;
};

BSON_EXPORT (_bsonBuildContext_t **)
_bsonBuildContextPtr ();

BSON_EXPORT (struct _bsonVisitContext_t const **)
_bsonVisitContextPtr ();

BSON_EXPORT (char *)
bsonParse_createPathString ();

#define bsonBuildContext (**_bsonBuildContextPtr ())
#define bsonVisitContext (**_bsonVisitContextPtr ())

static int _bson_dsl_indent = 0;

static inline void BSON_GNUC_PRINTF (4, 5) _bson_dsl_debug (
   const char *file, int line, const char *func, const char *string, ...)
{
   if (BSON_DSL_DEBUG) {
      fprintf (stderr, "%s:%d: [%s] bson_dsl: ", file, line, func);
      for (int i = 0; i < _bson_dsl_indent; ++i) {
         fputs ("  ", stderr);
      }
      va_list va;
      va_start (va, string);
      vfprintf (stderr, string, va);
      va_end (va);
      fputc ('\n', stderr);
      fflush (stderr);
   }
}

#define _bsonDSLDebug(...) \
   _bson_dsl_debug (__FILE__, __LINE__, __func__, __VA_ARGS__)

static inline bool
_bson_dsl_key_is_anyof (const char *key, const int keylen, ...)
{
   va_list va;
   va_start (va, keylen);
   const char *str;
   while ((str = va_arg (va, const char *))) {
      size_t str_len = strlen (str);
      if (str_len != keylen) {
         continue;
      }
      if (memcmp (key, str, str_len) == 0) {
         va_end (va);
         return true;
      }
   }
   return false;
}

// clang-format off

/// Convert the given argument into a string without inhibitting macro expansion
#define _bsonDSL_str(...) _bsonDSL_str_1(__VA_ARGS__)
#define _bsonDSL_str_1(...) "" #__VA_ARGS__

/* Expands to nothing. Used to defer a function-like macro and ignore arguments */
#define _bsonDSL_nothing(...)

/// Paste two tokens:
#define _bsonDSL_paste(a, ...) _bsonDSL_paste_IMPL(a, __VA_ARGS__)
#define _bsonDSL_paste_IMPL(a, ...) a##__VA_ARGS__

/// Paste three tokens:
#define _bsonDSL_paste3(a, b, c) _bsonDSL_paste(a, _bsonDSL_paste(b, c))
/// Paste four tokens:
#define _bsonDSL_paste4(a, b, c, d) _bsonDSL_paste(a, _bsonDSL_paste3(b, c, d))

/// Expand to the 64th argument. See below for why this is useful.
#define _bsonDSL_pick64th(\
                    _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, \
                    _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, \
                    _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, \
                    _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, \
                    _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, \
                    _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, \
                    _61, _62, _63, ...) \
    _63

/**
 * @brief Expands to 1 if the given arguments contain a top-level comma, zero otherwise.
 *
 * There is an expansion of __VA_ARGS__, followed by 62 '1' arguments, followed
 * by single '0'. If __VA_ARGS__ contains no commas, pick64th() will return the
 * single zero. If __VA_ARGS__ contains any top-level commas, the series of ones
 * will shift to the right and pick64th will return one of those ones. This only
 * works __VA_ARGS__ contains fewer than 62 commas, which is a somewhat reasonable
 * limit. The _bsonDSL_nothing() is a workaround for MSVC's bad preprocessor that
 * forwards __VA_ARGS__ incorrectly.
 *
 * If we have __VA_OPT__, this can be a lot simpler.
 */
#define _bsonDSL_hasComma(...) \
    _bsonDSL_pick64th \
    _bsonDSL_nothing() (__VA_ARGS__, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, ~)

/**
 * Expands to a comma if the token to the right-hand side is an opening
 * parenthesis. This will make sense, I promise.
 */
#define _bsonDSL_commaIfRHSHasParens(...) ,

/**
 * A helper for isEmpty(): If given (0, 0, 0, 1), expands as:
 *    - _bsonDSL_hasComma(_bsonDSL_isEmpty_CASE_0001)
 *    - _bsonDSL_hasComma(,)
 *    - 1
 * Given any other aruments:
 *    - _bsonDSL_hasComma(_bsonDSL_isEmpty_CASE_<somethingelse>)
 *    - 0
 */
#define _bsonDSL_isEmpty_1(_1, _2, _3, _4) \
    _bsonDSL_hasComma(_bsonDSL_paste(_bsonDSL_isEmpty_CASE_, _bsonDSL_paste4(_1, _2, _3, _4)))
#define _bsonDSL_isEmpty_CASE_0001 ,

/**
 * @brief Expand to 1 if given no arguments, otherwise 0.
 *
 * This could be done much more simply using __VA_OPT__, but we need to work on
 * older compilers.
 */
#define _bsonDSL_isEmpty(...) \
    _bsonDSL_isEmpty_1(\
        /* Expands to '1' if __VA_ARGS__ contains any top-level comma */ \
        _bsonDSL_hasComma(__VA_ARGS__), \
        /* Expands to '1' if __VA_ARGS__ begins with a parenthesis */ \
        _bsonDSL_hasComma(_bsonDSL_commaIfRHSHasParens __VA_ARGS__), \
        /* Expands to '1' if __VA_ARGS__ expands to a function-like macro name \
         * that then expands to anything containing a top-level comma */ \
        _bsonDSL_hasComma(__VA_ARGS__ ()), \
        /* Expands to '1' if __VA_ARGS__ expands to nothing. */ \
        _bsonDSL_hasComma(_bsonDSL_commaIfRHSHasParens __VA_ARGS__ ()))

/**
 * @brief Expand to the first argument if `Cond` is 1, the second argument if `Cond` is 0
 */
#define _bsonDSL_ifElse(Cond, IfTrue, IfFalse) \
    /* Suppress expansion of the two branches by using the '#' operator */ \
    _bsonDSL_nothing(#IfTrue, #IfFalse)  \
    /* Concat the cond 1/0 with a prefix macro: */ \
    _bsonDSL_paste(_bsonDSL_ifElse_PICK_, Cond)(IfTrue, IfFalse)

#define _bsonDSL_ifElse_PICK_1(IfTrue, IfFalse) \
   /* Expand the first operand, throw away the second */ \
   IfTrue _bsonDSL_nothing(#IfFalse)
#define _bsonDSL_ifElse_PICK_0(IfTrue, IfFalse) \
   /* Expand to the second operand, throw away the first */ \
   IfFalse _bsonDSL_nothing(#IfTrue)

#ifdef _MSVC_TRADITIONAL
// MSVC's "traditional" preprocessor requires many more expansion passes,
// but GNU and Clang are very slow when evaluating hugely nested expansions
// and generate massive macro expansion backtraces.
#define _bsonDSL_eval_1(...) __VA_ARGS__
#define _bsonDSL_eval_2(...) _bsonDSL_eval_1(_bsonDSL_eval_1(_bsonDSL_eval_1(_bsonDSL_eval_1(_bsonDSL_eval_1(__VA_ARGS__)))))
#define _bsonDSL_eval_4(...) _bsonDSL_eval_2(_bsonDSL_eval_2(_bsonDSL_eval_2(_bsonDSL_eval_2(_bsonDSL_eval_2(__VA_ARGS__)))))
#define _bsonDSL_eval_8(...) _bsonDSL_eval_4(_bsonDSL_eval_4(_bsonDSL_eval_4(_bsonDSL_eval_4(_bsonDSL_eval_4(__VA_ARGS__)))))
#define _bsonDSL_eval_16(...) _bsonDSL_eval_8(_bsonDSL_eval_8(_bsonDSL_eval_8(_bsonDSL_eval_8(_bsonDSL_eval_8(__VA_ARGS__)))))
#define _bsonDSL_eval(...) _bsonDSL_eval_16(_bsonDSL_eval_16(_bsonDSL_eval_16(_bsonDSL_eval_16(_bsonDSL_eval_16(__VA_ARGS__)))))
#else
#define _bsonDSL_eval_1(...) __VA_ARGS__
#define _bsonDSL_eval_2(...) _bsonDSL_eval_1(_bsonDSL_eval_1(__VA_ARGS__))
#define _bsonDSL_eval_4(...) _bsonDSL_eval_2(_bsonDSL_eval_2(__VA_ARGS__))
#define _bsonDSL_eval_8(...) _bsonDSL_eval_4(_bsonDSL_eval_4(__VA_ARGS__))
#define _bsonDSL_eval_16(...) _bsonDSL_eval_8(_bsonDSL_eval_8(__VA_ARGS__))
#define _bsonDSL_eval_32(...) _bsonDSL_eval_16(_bsonDSL_eval_16(__VA_ARGS__))
#define _bsonDSL_eval(...) _bsonDSL_eval_32(__VA_ARGS__)
#endif

/**
 * Finally, the Map() macro that allows us to do the magic, which we've been
 * building up to all along.
 *
 * The dance with mapMacro_first, mapMacro_final, and _bsonDSL_nothing
 * conditional on argument count is to prevent warnings from pre-C99 about
 * passing no arguments to the '...' parameters. Yet again, if we had C99 and
 * __VA_OPT__ this would be somewhat simpler.
 */
#define _bsonDSL_mapMacro(Action, Constant, ...) \
   /* Pick our first action based on the content of '...': */ \
    _bsonDSL_ifElse( \
      /* If given no arguments: */\
      _bsonDSL_isEmpty(__VA_ARGS__), \
         /* expand to _bsonDSL_nothing */ \
         _bsonDSL_nothing, \
         /* Otherwise, expand to mapMacro_first: */ \
         _bsonDSL_mapMacro_first) \
   /* Now "invoke" the chosen macro: */ \
   _bsonDSL_nothing() (Action, Constant, __VA_ARGS__)

#define _bsonDSL_mapMacro_first(Action, Constant, ...) \
   /* Select our next step based on whether we have one or more arguments: */ \
   _bsonDSL_ifElse( \
      /* If '...' contains more than one argument (has a top-level comma): */ \
      _bsonDSL_hasComma(__VA_ARGS__), \
         /* Begin the mapMacro loop with mapMacro_A: */ \
         _bsonDSL_mapMacro_A, \
         /* Otherwise skip to the final step of the loop: */ \
         _bsonDSL_mapMacro_final) \
   /* Invoke the chosen macro, setting the counter to zero: */ \
   _bsonDSL_nothing() (Action, Constant, 0, __VA_ARGS__)

/// Handle the last expansion in a mapMacro sequence.
#define _bsonDSL_mapMacro_final(Action, Constant, Counter, FinalElement) \
    Action(FinalElement, Constant, Counter)

/**
 * mapMacro_A and mapMacro_B are identical and just invoke each other.
 */
#define _bsonDSL_mapMacro_A(Action, Constant, Counter, Head, ...) \
   /* First evaluate the action once: */ \
   Action(Head, Constant, Counter) \
   /* Pick our next step: */ \
   _bsonDSL_ifElse( \
      /* If '...' contains more than one argument (has a top-level comma): */ \
      _bsonDSL_hasComma(__VA_ARGS__), \
         /* Jump to the other mapMacro: */ \
         _bsonDSL_mapMacro_B, \
         /* Otherwise go to mapMacro_final */ \
         _bsonDSL_mapMacro_final) \
   /* Invoke the next step of the map: */ \
   _bsonDSL_nothing() (Action, Constant, Counter + 1, __VA_ARGS__)

#define _bsonDSL_mapMacro_B(Action, Constant, Counter, Head, ...) \
    Action(Head, Constant, Counter) \
    _bsonDSL_ifElse(_bsonDSL_hasComma(__VA_ARGS__), _bsonDSL_mapMacro_A, _bsonDSL_mapMacro_final) \
    _bsonDSL_nothing() (Action, Constant, Counter + 1, __VA_ARGS__)

// clang-format on

/// Begin any function-like macro by opening a new scope and writing a debug
/// message.
#define _bsonDSL_begin(Str, ...)        \
   do {                                 \
      _bsonDSLDebug (Str, __VA_ARGS__); \
   ++_bson_dsl_indent

/// End a function-like macro scope.
#define _bsonDSL_end   \
   --_bson_dsl_indent; \
   }                   \
   while (0)

/**
 * @brief Expands to a call to bson_append_{Kind}, with the three first
 * arguments filled in by the DSL context variables.
 */
#define _bsonBuildAppendArgs \
   bsonBuildContext.doc, bsonBuildContext.key, bsonBuildContext.key_len
#define _bsonDSL_append(Kind, ...) \
   bson_append_##Kind (_bsonBuildAppendArgs, __VA_ARGS__)

/// We must defer expansion of the nested doc() to allow "recursive" evaluation
#define _bsonBuild_docAppendValue_doc \
   _bsonBuild_docAppendValueDeferred_doc _bsonDSL_nothing ()
#define _bsonBuild_arrAppendValue_doc _bsonBuild_docAppendValue_doc

#define _bsonBuild_docAppendValueDeferred_doc(...)                  \
   _bsonDSL_begin ("doc(%s)", _bsonDSL_str (__VA_ARGS__));          \
   /* Write to this variable as the child: */                       \
   bson_t _bbChildDoc = BSON_INITIALIZER;                           \
   bson_append_document_begin (_bsonBuildAppendArgs, &_bbChildDoc); \
   _bsonBuildAppend (&_bbChildDoc, __VA_ARGS__);                    \
   bson_append_document_end (bsonBuildContext.doc, &_bbChildDoc);   \
   _bsonDSL_end

/// We must defer expansion of the nested array() to allow "recursive"
/// evaluation
#define _bsonBuild_docAppendValue_array \
   _bsonBuild_docAppendValueDeferred_array _bsonDSL_nothing ()
#define _bsonBuild_arrAppendValue_array _bsonBuild_docAppendValue_array

#define _bsonBuild_docAppendValueDeferred_array(...)                 \
   _bsonDSL_begin ("array(%s)", _bsonDSL_str (__VA_ARGS__));         \
   /* Write to this variable as the child array: */                  \
   bson_t _array_ = BSON_INITIALIZER;                                \
   _bsonDSL_append (array_begin, &_array_);                          \
   /* Set the child as the new context doc to modify: */             \
   /* Remember the parent: */                                        \
   struct _bsonBuildContext_t _bbCtx = (struct _bsonBuildContext_t){ \
      .doc = &_array_,                                               \
      .parent = *_bsonBuildContextPtr (),                            \
      .index = 0,                                                    \
   };                                                                \
   *_bsonBuildContextPtr () = &_bbCtx;                               \
   _bsonDSL_mapMacro (_bsonDSL_arrayAppend, ~, __VA_ARGS__);         \
   /* Restore the prior context document and finish the append: */   \
   *_bsonBuildContextPtr () = _bbCtx.parent;                         \
   bson_append_array_end (bsonBuildContext.doc, &_array_);           \
   _bsonDSL_end

/// Append a "cstr" as UTF-8
#define _bsonBuild_docAppendValue_cstr(String) \
   _bsonDSL_append (utf8, String, strlen (String))
#define _bsonBuild_arrAppendValue_cstr _bsonBuild_docAppendValue_cstr

/// Append a UTF-8 string with an explicit length
#define _bsonBuild_docAppendValue_utf8_w_len(String, Len) \
   _bsonDSL_append (utf8, (String), (Len))
#define _bsonBuild_arrAppendValue_utf8_w_len \
   _bsonBuild_docAppendValue_utf8_w_len

/// Append an int32
#define _bsonBuild_docAppendValue_i32(Integer) \
   _bsonDSL_append (int32, (Integer))
#define _bsonBuild_arrAppendValue_i32 _bsonBuild_docAppendValue_i32

/// Append an int64
#define _bsonBuild_docAppendValue_i64(Integer) \
   _bsonDSL_append (int64, (Integer))
#define _bsonBuild_arrAppendValue_i64 _bsonBuild_docAppendValue_i64

/// Append the value referenced by a given iterator
#define _bsonBuild_docAppendValue_iter(Iter) _bsonDSL_append (iter, &(Iter))
#define _bsonBuild_arrAppendValue_iter _bsonBuild_docAppendValue_iter

/// Append the BSON document referenced by the given pointer
#define _bsonBuild_docAppendValue_bson(Ptr) _bsonDSL_append (document, (Ptr))
#define _bsonBuild_arrAppendValue_bson _bsonBuild_docAppendValue_bson

/// Append the BSON document referenced by the given pointer as an array
#define _bsonBuild_docAppendValue_bsonArray(Ptr) _bsonDSL_append (array, (Ptr))
#define _bsonBuild_arrAppendValue_bsonArray _bsonBuild_docAppendValue_bsonArray

#define _bsonBuild_docAppendValue_bool(b) _bsonDSL_append (bool, (b))
#define _bsonBuild_arrAppendValue_bool _bsonBuild_docAppendValue_bool
#define _bsonBuild_docAppendValue__Bool(b) _bsonDSL_append (bool, (b))
#define _bsonBuild_arrAppendValue__Bool _bsonBuild_docAppendValue_bool

#define _bsonBuild_docAppendValue_null \
   bson_append_null (                  \
      bsonBuildContext.doc, bsonBuildContext.key, bsonBuildContext.key_len)
#define _bsonBuild_arrAppendValue_null _bsonBuild_docAppendValue_null

// DSL items for doc() appending (and the top-level bsonBuild):

/// key-value pair with explicit key length
#define _bsonBuild_docElement_kvl(String, Len, Element)                     \
   _bsonDSL_begin ("Append: '%s' => [%s]", String, _bsonDSL_str (Element)); \
   _bbCtx.key = (String);                                                   \
   _bbCtx.key_len = (Len);                                                  \
   _bsonBuild_docAppendValue_##Element;                                     \
   _bsonDSL_end

/// Key-value pair with a C-string
#define _bsonBuild_docElement_kv(String, Element) \
   _bsonBuild_docElement_kvl ((String), strlen ((String)), Element)

#define _bsonDSL_insertFilter_all _filter_drop_ = false
#define _bsonDSL_insertFilter_excluding(...)                         \
   _filter_drop_ =                                                   \
      _bson_dsl_key_is_anyof (bson_iter_key (&_bbDocInsertIter),     \
                              bson_iter_key_len (&_bbDocInsertIter), \
                              __VA_ARGS__,                           \
                              NULL)

#define _bsonDSL_insertFilter(F, _nil, _count) \
   if (!_filter_drop_) {                       \
      _bsonDSL_insertFilter_##F;               \
   }

/// Insert the given BSON document into the parent document in-place
#define _bsonBuild_docElement_insert(OtherDoc, ...)                         \
   _bsonDSL_begin ("Insert other document: [%s]", _bsonDSL_str (OtherDoc)); \
   bson_iter_t _bbDocInsertIter;                                            \
   bson_t *_bbInsertDoc = (bson_t *) (OtherDoc);                            \
   bson_iter_init (&_bbDocInsertIter, _bbInsertDoc);                        \
   while (bson_iter_next (&_bbDocInsertIter)) {                             \
      bool _filter_drop_ = false;                                           \
      _bsonDSL_mapMacro (_bsonDSL_insertFilter, ~, __VA_ARGS__);            \
      if (_filter_drop_) {                                                  \
         continue;                                                          \
      }                                                                     \
      _bsonBuild_docElement_kvl (bson_iter_key (&_bbDocInsertIter),         \
                                 bson_iter_key_len (&_bbDocInsertIter),     \
                                 iter (_bbDocInsertIter));                  \
   }                                                                        \
   _bsonDSL_end

/// Insert the given BSON document into the parent array. Keys of the given
/// document are discarded and it is treated as an array of values.
#define _bsonBuild_arrAppendValue_insert(OtherArr)                           \
   _bsonDSL_begin ("Insert other array: [%s]", _bsonDSL_str (OtherArr));     \
   bson_iter_t iter;                                                         \
   bson_iter_init (&iter, (bson_t *) (OtherArr));                            \
   while (bson_iter_next (&iter)) {                                          \
      _bsonDSL_setKeyToArrayIndex (bsonBuildContext.index);                  \
      _bsonBuild_arrAppendValue_iter (iter);                                 \
      ++_bbCtx.index;                                                        \
   }                                                                         \
   /* Decrement once to counter the outer increment that will be applied: */ \
   --_bbCtx.index;                                                           \
   _bsonDSL_end

#define _bsonBuild_docElementIfThen_then _bsonBuild_docAppendValue_doc
#define _bsonBuild_docElementIfElse_else _bsonBuild_docAppendValue_doc

#define _bsonBuild_docElement_if(Condition, Then, Else)                     \
   _bsonDSL_begin ("Conditional append on [%s]", _bsonDSL_str (Condition)); \
   if ((Condition)) {                                                       \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then));      \
      _bsonBuild_docElementIfThen_##Then;                                   \
   } else {                                                                 \
      _bsonDSLDebug ("Taking FALSE branch: [%s]", _bsonDSL_str (Else));     \
      _bsonBuild_docElementIfElse_##Else;                                   \
   }                                                                        \
   _bsonDSL_end

#define _bsonBuild_arrAppendValueIfThen_then _bsonBuild_docAppendValue_array
#define _bsonBuild_arrAppendValueIfElse_else _bsonBuild_docAppendValue_array

#define _bsonBuild_arrAppendValue_if(Condition, Then, Else)                \
   _bsonDSL_begin ("Conditional value on [%s]", _bsonDSL_str (Condition)); \
   if ((Condition)) {                                                      \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then));     \
      _bsonBuild_arrAppendValueIfThen_##Then;                              \
   } else {                                                                \
      _bsonDSLDebug ("Taking FALSE branch: [%s]", _bsonDSL_str (Else));    \
      _bsonBuild_arrAppendValueIfElse_##Else;                              \
   }                                                                       \
   _bsonDSL_end
#define _bsonBuild_docAppendValue_if _bsonBuild_arrAppendValue_if

#define _bsonDSL_setKeyToArrayIndex(Idx)                           \
   _bbCtx.key_len = sprintf (_bbCtx.strbuf64, "%d", _bbCtx.index); \
   _bbCtx.key = _bbCtx.strbuf64

// Handle an element of doc()
#define _bsonBuild_docElement(Elem, _nil, _count) _bsonBuild_docElement_##Elem;

/// Handle an element of array()
#define _bsonDSL_arrayAppend(Element, _nil, _count)      \
   _bsonDSL_begin ("Array append [%d]: [%s]",            \
                   bsonBuildContext.index,               \
                   _bsonDSL_str (Element));              \
   /* Set the doc key to the array index as a string: */ \
   _bsonDSL_setKeyToArrayIndex (bsonBuildContext.index); \
   /* Append a value: */                                 \
   _bsonBuild_arrAppendValue_##Element;                  \
   /* Increment the array index: */                      \
   ++_bbCtx.index;                                       \
   _bsonDSL_end;

#define _bsonDSL_documentBuild(...) \
   _bsonDSL_mapMacro (_bsonBuild_docElement, ~, __VA_ARGS__)

#define _bsonDSL_iterType_array BSON_TYPE_ARRAY
#define _bsonDSL_iterType_bool BSON_TYPE_BOOL
#define _bsonDSL_iterType__Bool BSON_TYPE_BOOL
#define _bsonDSL_iterType_doc BSON_TYPE_DOCUMENT

#define _bsonVisitOneApply_ifType(T, ...)                \
   if (bson_iter_type_unsafe (&bsonVisitContext.iter) == \
       _bsonDSL_iterType_##T) {                          \
      _bsonVisitOneApplyOps (__VA_ARGS__);               \
   }

#define _bsonVisitOneApply_halt _bwHalt = true

#define _bsonVisitOneApply_ifTruthy(...)                        \
   _bsonDSL_begin ("ifTruthy(%s)", _bsonDSL_str (__VA_ARGS__)); \
   if (bson_iter_as_bool (&bsonVisitContext.iter)) {            \
      _bsonVisitOneApplyOps (__VA_ARGS__);                      \
   }                                                            \
   _bsonDSL_end;

#define _bsonVisitOneApply_ifStrEqual(S, ...)                              \
   _bsonDSL_begin ("ifStrEqual(%s)", _bsonDSL_str (S));                    \
   if (bson_iter_type_unsafe (&bsonVisitContext.iter) == BSON_TYPE_UTF8) { \
      uint32_t len;                                                        \
      const char *str = bson_iter_utf8 (&bsonVisitContext.iter, &len);     \
      if (len == strlen (S) && (0 == memcmp (str, S, len))) {              \
         _bsonVisitOneApplyOps (__VA_ARGS__);                              \
      }                                                                    \
   }                                                                       \
   _bsonDSL_end

#define _bsonVisitOneApply_storeBool(Dest)                 \
   _bsonDSL_begin ("storeBool(%s)", _bsonDSL_str (Dest));  \
   if (BSON_ITER_HOLDS_BOOL (&bsonVisitContext.iter)) {    \
      (Dest) = bson_iter_as_bool (&bsonVisitContext.iter); \
   }                                                       \
   _bsonDSL_end

#define _bsonVisitOneApply_found(IterDest)                \
   _bsonDSL_begin ("found(%s)", _bsonDSL_str (IterDest)); \
   (IterDest) = bsonVisitContext.iter;                    \
   _bsonDSL_end

#define _bsonVisitOneApply_do(...)                            \
   _bsonDSL_begin ("do: { %s }", _bsonDSL_str (__VA_ARGS__)); \
   do {                                                       \
      __VA_ARGS__;                                            \
   } while (0);                                               \
   _bsonDSL_end

#define _bsonVisitOneApply_append \
   _bsonVisitOneApplyDeferred_append _bsonDSL_nothing ()
#define _bsonVisitOneApplyDeferred_append(Doc, ...)                           \
   _bsonDSL_begin (                                                           \
      "append to [%s] : %s", _bsonDSL_str (Doc), _bsonDSL_str (__VA_ARGS__)); \
   _bsonBuildAppend (Doc, __VA_ARGS__);                                       \
   _bsonDSL_end

#define _bsonVisitEach(Doc, ...)                                            \
   _bsonDSL_begin (                                                         \
      "visitEach(%s, %s)", _bsonDSL_str (Doc), _bsonDSL_str (__VA_ARGS__)); \
   do {                                                                     \
      /* Reset the context */                                               \
      struct _bsonVisitContext_t _bpCtx = {                                 \
         .doc = &(Doc),                                                     \
         .parent = *_bsonVisitContextPtr (),                                \
      };                                                                    \
      *_bsonVisitContextPtr () = &_bpCtx;                                   \
      /* Iterate over each element of the document */                       \
      bson_iter_init (&_bpCtx.iter, &(Doc));                                \
      bool _bvBreak = false;                                                \
      bool _bvContinue = false;                                             \
      while (bson_iter_next (&_bpCtx.iter) && !_bwHalt && !_bvBreak) {      \
         _bvContinue = false;                                               \
         _bsonVisitOneApplyOps (__VA_ARGS__);                               \
      }                                                                     \
      /* Restore the dsl context */                                         \
      *_bsonVisitContextPtr () = bsonVisitContext.parent;                   \
   } while (0);                                                             \
   _bsonDSL_end

#define _bsonVisitOneApply_visitEach \
   _bsonVisitOneApply_visitEachDeferred _bsonDSL_nothing ()
#define _bsonVisitOneApply_visitEachDeferred(...)                              \
   _bsonDSL_begin ("visitEach(%s)", _bsonDSL_str (__VA_ARGS__));               \
   do {                                                                        \
      const uint8_t *data;                                                     \
      uint32_t len;                                                            \
      bson_type_t typ = bson_iter_type_unsafe (&bsonVisitContext.iter);        \
      if (typ == BSON_TYPE_ARRAY)                                              \
         bson_iter_array (&bsonVisitContext.iter, &len, &data);                \
      else if (typ == BSON_TYPE_DOCUMENT)                                      \
         bson_iter_document (&bsonVisitContext.iter, &len, &data);             \
      else {                                                                   \
         _bsonDSLDebug ("(Skipping visitEach() of non-array/document value)"); \
         break;                                                                \
      }                                                                        \
      bson_t inner;                                                            \
      bson_init_static (&inner, data, len);                                    \
      _bsonVisitEach (inner, __VA_ARGS__);                                     \
   } while (0);                                                                \
   _bsonDSL_end

#define _bsonParseDeferred _bsonParse _bsonDSL_nothing ()

#define _bsonVisitOneApply_nop ((void) 0)
#define _bsonVisitOneApply_parse(...)                                   \
   do {                                                                 \
      const uint8_t *data;                                              \
      uint32_t len;                                                     \
      bson_type_t typ = bson_iter_type (&bsonVisitContext.iter);        \
      if (typ == BSON_TYPE_ARRAY)                                       \
         bson_iter_array (&bsonVisitContext.iter, &len, &data);         \
      else if (typ == BSON_TYPE_DOCUMENT)                               \
         bson_iter_document (&bsonVisitContext.iter, &len, &data);      \
      else {                                                            \
         _bsonDSLDebug ("Ignoring doc() for non-document/array value"); \
         break;                                                         \
      }                                                                 \
      bson_t inner;                                                     \
      bson_init_static (&inner, data, len);                             \
      _bsonParse (&inner, __VA_ARGS__);                                 \
   } while (0);

#define _bsonVisitOneApply_setTrue(B)                       \
   _bsonDSL_begin ("Set [%s] to 'true'", _bsonDSL_str (B)); \
   (B) = true;                                              \
   _bsonDSL_end;

#define _bsonVisitOneApply_setFalse(B)                       \
   _bsonDSL_begin ("Set [%s] to 'false'", _bsonDSL_str (B)); \
   (B) = false;                                              \
   _bsonDSL_end;

#define _bsonVisitOneApply_continue _bvContinue = true
#define _bsonVisitOneApply_break _bvBreak = _bvContinue = true

#define _bsonVisitOneApply(P, _const, _count) \
   do {                                       \
      if (!_bvContinue && !_bwHalt) {         \
         _bsonVisitOneApply_##P;              \
      }                                       \
   } while (0);

/// Parse one entry referrenced by the context iterator
#define _bsonParseDo(P, _nil, Counter) _bsonParseDo_##P;

/// Match an entry by its key
#define _bsonParseDo_ifKey(Key, ...)                                    \
   _bsonDSL_begin (                                                     \
      "Searching for key \"%s\" [%s]", (Key), _bsonDSL_str (Key));      \
   if (bson_iter_init_find (&_bpCtx.iter, bsonVisitContext.doc, Key)) { \
      _bsonVisitOneApplyOps (__VA_ARGS__);                              \
   }                                                                    \
   _bsonDSL_end;

#define _bsonParseDo_append _bsonBuildAppend

/**
 * @brief Parse each entry in the document at the given pointer.
 */
#define _bsonParse(DocPointer, ...)                              \
   _bsonDSL_begin ("Walking: [%s]", _bsonDSL_str (__VA_ARGS__)); \
   /* Reset the context */                                       \
   struct _bsonVisitContext_t _bpCtx = {                         \
      .doc = (DocPointer),                                       \
      .parent = &bsonVisitContext,                               \
   };                                                            \
   *_bsonVisitContextPtr () = &_bpCtx;                           \
   _bsonDSL_mapMacro (_bsonParseDo, ~, __VA_ARGS__);             \
   /* Restore the dsl context */                                 \
   *_bsonVisitContextPtr () = bsonVisitContext.parent;           \
   _bsonDSL_end

// /**
//  * @brief Parse each entry in the document at the given pointer.
//  *
//  * Yes, this is a copy-paste of the above, but is required to defer macro
//  * expansion to allow for nested doc() walking. "mapMacroInner" must be used
//  in
//  * the below.
//  *
//  * Be sure to keep these copy-pasted macros in sync.
//  */
// #define _bsonParseInner(DocPointer, ...)                         \
//    _bsonDSL_begin ("Walking: [%s]", _bsonDSL_str (__VA_ARGS__)); \
//    /* Reset the context */                                       \
//    struct _bsonVisitContext_t _bpCtx = {                         \
//       .doc = (DocPointer),                                       \
//       .parent = &bsonVisitContext,                               \
//    };                                                            \
//    *_bsonVisitContextPtr () = &_bpCtx;                           \
//    bson_iter_t iter;                                             \
//    _bsonDSL_mapMacro (_bsonParseDo, ~, __VA_ARGS__);             \
//    /* Restore the dsl context */                                 \
//    *_bsonVisitContextPtr () = bsonVisitContext.parent;           \
//    _bsonDSL_end

#define _bsonVisitOneApplyOps _bsonVisitOneApplyOpsDeferred _bsonDSL_nothing ()
#define _bsonVisitOneApplyOpsDeferred(...)                   \
   _bsonDSL_begin ("Walk [%s]", _bsonDSL_str (__VA_ARGS__)); \
   _bsonDSL_mapMacro (_bsonVisitOneApply, ~, __VA_ARGS__);   \
   _bsonDSL_end;

/**
 * @brief Build a BSON document by appending to an existing bson_t document
 *
 * @param Pointer The document upon which to append
 * @param ... The Document elements to append to the document
 */
#define bsonBuildAppend(Pointer, ...) \
   _bsonDSL_eval (_bsonBuildAppend (Pointer, __VA_ARGS__))
#define _bsonBuildAppend(Pointer, ...)                                    \
   _bsonDSL_begin ("Appending to document '%s'", _bsonDSL_str (Pointer)); \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic push");)                    \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic ignored \"-Wshadow\"");)    \
   /* Save the dsl context */                                             \
   _bsonBuildContext_t _bbCtx = (_bsonBuildContext_t){                    \
      .doc = (Pointer),                                                   \
      .key = NULL,                                                        \
      .key_len = 0,                                                       \
      .index = 0,                                                         \
      .strbuf64 = {0},                                                    \
      .parent = *_bsonBuildContextPtr (),                                 \
   };                                                                     \
   *_bsonBuildContextPtr () = &_bbCtx;                                    \
   /* Reset the context */                                                \
   _bsonDSL_documentBuild (__VA_ARGS__);                                  \
   /* Restore the dsl context */                                          \
   *_bsonBuildContextPtr () = _bbCtx.parent;                              \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic pop");)                     \
   _bsonDSL_end

/**
 * @brief Build a new BSON document and assign the value into the given
 * pointer. The pointed-to document is destroyed before being assigned, but
 * after the document is built.
 */
#define bsonBuild(PointerDest, ...)                   \
   _bsonDSL_begin ("Build a new document for '%s'",   \
                   _bsonDSL_str (PointerDest));       \
   bson_t *_bson_dsl_new_doc_ = bson_new ();          \
   bsonBuildAppend (_bson_dsl_new_doc_, __VA_ARGS__); \
   bson_destroy ((PointerDest));                      \
   (PointerDest) = _bson_dsl_new_doc_;                \
   _bsonDSL_end

/**
 * @brief Declare a variable and build it with the BSON DSL @see bsonBuild
 */
#define bsonBuildDecl(Variable, ...) \
   bson_t *Variable = NULL;          \
   bsonBuild (Variable, __VA_ARGS__)


/**
 * @brief Walk the given BSON document.
 *
 * @param doc A bson_t object to walk. (Not a pointer)
 */
#define bsonParse(doc, ...)                                            \
   _bsonDSL_begin ("bsonParse(%s)", _bsonDSL_str (doc));               \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic push");)                 \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic ignored \"-Wshadow\"");) \
   bool _bwHalt = false;                                               \
   const bool _bvContinue = false;                                     \
   const bool _bvBreak = false;                                        \
   _bsonDSL_eval (_bsonParse (&(doc), __VA_ARGS__));                   \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic pop");)                  \
   _bsonDSL_end

/**
 * @brief Visit each element of a BSON document
 */
#define bsonVisitEach(Doc, ...)                                        \
   _bsonDSL_begin ("bsonVisitEach(%s)", _bsonDSL_str (Doc));           \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic push");)                 \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic ignored \"-Wshadow\"");) \
   bool _bwHalt = false;                                               \
   _bsonDSL_eval (_bsonVisitEach ((Doc), __VA_ARGS__));                \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic pop");)                  \
   _bsonDSL_end

#endif // BSON_BSON_DSL_H_INCLUDED
