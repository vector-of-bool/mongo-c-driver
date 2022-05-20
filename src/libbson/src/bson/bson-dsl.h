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

#define pBsonDSL_defer(...) __VA_ARGS__

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
#define pBsonDSL_nothing /* Expands to nothing */
#define pBsonDSL_ignore(...) /* Ignore your arguments */

#define pBsonDSL_paste_IMPL(a, ...) a##__VA_ARGS__
#define pBsonDSL_paste(a, ...) pBsonDSL_paste_IMPL(a, __VA_ARGS__)

#define pBsonDSL_paste3(a, b, c) pBsonDSL_paste(a, pBsonDSL_paste(b, c))
#define pBsonDSL_paste4(a, b, c, d) pBsonDSL_paste(a, pBsonDSL_paste3(b, c, d))

#define pBSONDSL_PICK_64th(\
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
 */
#define pBsonDSL_hasComma(...) \
    pBSONDSL_PICK_64th \
    pBsonDSL_nothing (__VA_ARGS__, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, ~)

/** Expands to a comma if the token to the right-hand side is an opening parenthesis */
#define pBsonDSL_commaIfRHSHasParens(...) ,

#define pBsonDSL_isEmpty_CASE_0001 ,
#define pBsonDSL_isEmpty_1(_1, _2, _3, _4) \
    pBsonDSL_hasComma(pBsonDSL_paste(pBsonDSL_isEmpty_CASE_, pBsonDSL_paste4(_1, _2, _3, _4)))

#define pBsonDSL_str(...) pBsonDSL_str_1(__VA_ARGS__)
#define pBsonDSL_str_1(...) #__VA_ARGS__

/**
 * @brief Expand to 1 if given no arguments, otherwise 0
 */
#define pBsonDSL_isEmpty(...) \
    pBsonDSL_isEmpty_1(\
        pBsonDSL_hasComma(__VA_ARGS__), \
        pBsonDSL_hasComma(pBsonDSL_commaIfRHSHasParens __VA_ARGS__), \
        pBsonDSL_hasComma(__VA_ARGS__ ()), \
        pBsonDSL_hasComma(pBsonDSL_commaIfRHSHasParens __VA_ARGS__ ()))

#define pBsonDSL_ifElse_PICK_1(IfTrue, IfFalse) IfTrue pBsonDSL_ignore(#IfFalse)
#define pBsonDSL_ifElse_PICK_0(IfTrue, IfFalse) IfFalse pBsonDSL_ignore(#IfTrue)

/**
 * @brief Expand to the first argument if `Cond` is 1, the second argument if `Cond` is 0
 */
#define pBsonDSL_ifElse(Cond, IfTrue, IfFalse) \
    pBsonDSL_ignore(#IfTrue, #IfFalse) /* <- Suppress expansion of the two conditionals */ \
    pBsonDSL_paste(pBsonDSL_ifElse_PICK_, Cond)(IfTrue, IfFalse)

#ifdef _MSVC_TRADITIONAL
// MSVC's "traditional" preprocessor requires many more expansion passes,
// but GNU and Clang are very slow when evaluating hugely nested expansions.
#define pBsonDSL_eval_1(...) __VA_ARGS__
#define pBsonDSL_eval_2(...) pBsonDSL_eval_1(pBsonDSL_eval_1(pBsonDSL_eval_1(pBsonDSL_eval_1(pBsonDSL_eval_1(__VA_ARGS__)))))
#define pBsonDSL_eval_4(...) pBsonDSL_eval_2(pBsonDSL_eval_2(pBsonDSL_eval_2(pBsonDSL_eval_2(pBsonDSL_eval_2(__VA_ARGS__)))))
#define pBsonDSL_eval_8(...) pBsonDSL_eval_4(pBsonDSL_eval_4(pBsonDSL_eval_4(pBsonDSL_eval_4(pBsonDSL_eval_4(__VA_ARGS__)))))
#define pBsonDSL_eval_16(...) pBsonDSL_eval_8(pBsonDSL_eval_8(pBsonDSL_eval_8(pBsonDSL_eval_8(pBsonDSL_eval_8(__VA_ARGS__)))))
#define pBsonDSL_eval(...) pBsonDSL_eval_16(pBsonDSL_eval_16(pBsonDSL_eval_16(pBsonDSL_eval_16(pBsonDSL_eval_16(__VA_ARGS__)))))
#else
#define pBsonDSL_eval_1(...) __VA_ARGS__
#define pBsonDSL_eval_2(...) pBsonDSL_eval_1(pBsonDSL_eval_1(__VA_ARGS__))
#define pBsonDSL_eval_4(...) pBsonDSL_eval_2(pBsonDSL_eval_2(__VA_ARGS__))
#define pBsonDSL_eval_8(...) pBsonDSL_eval_4(pBsonDSL_eval_4(__VA_ARGS__))
#define pBsonDSL_eval_16(...) pBsonDSL_eval_8(pBsonDSL_eval_8(__VA_ARGS__))
#define pBsonDSL_eval_32(...) pBsonDSL_eval_16(pBsonDSL_eval_16(__VA_ARGS__))
#define pBsonDSL_eval(...) pBsonDSL_eval_32(__VA_ARGS__)
#endif

#define pBsonDSL_mapMacro_final(Action, Constant, Counter, Fin) \
    Action(Fin, Constant, Counter)

#define pBsonDSL_mapMacro_A(Action, Constant, Counter, Head, ...) \
    Action(Head, Constant, Counter) \
    pBsonDSL_ifElse(pBsonDSL_hasComma(__VA_ARGS__), pBsonDSL_mapMacro_B, pBsonDSL_mapMacro_final) \
    pBsonDSL_nothing (Action, Constant, Counter + 1, __VA_ARGS__)

#define pBsonDSL_mapMacro_B(Action, Constant, Counter, Head, ...) \
    Action(Head, Constant, Counter) \
    pBsonDSL_ifElse(pBsonDSL_hasComma(__VA_ARGS__), pBsonDSL_mapMacro_A, pBsonDSL_mapMacro_final) \
    pBsonDSL_nothing (Action, Constant, Counter + 1, __VA_ARGS__)

#define pBsonDSL_mapMacro_first(Action, Constant, Counter, ...) \
    pBsonDSL_ifElse(pBsonDSL_hasComma(__VA_ARGS__), pBsonDSL_mapMacro_A, pBsonDSL_mapMacro_final) \
    pBsonDSL_nothing (Action, Constant, Counter, __VA_ARGS__)

#define pBsonDSL_mapMacro(Action, Constant, ...) \
    pBsonDSL_ifElse(pBsonDSL_isEmpty(__VA_ARGS__), pBsonDSL_ignore, pBsonDSL_mapMacro_first) \
    pBsonDSL_nothing (Action, Constant, 0, __VA_ARGS__)

// clang-format on

#define _bsonDSL_begin(Str, ...)        \
   do {                                 \
      _bsonDSLDebug (Str, __VA_ARGS__); \
   ++_bson_dsl_indent

#define _bsonDSL_end   \
   --_bson_dsl_indent; \
   }                   \
   while (0)

/**
 * @brief Expands to a call to bson_append_{Kind}, with the three first
 * arguments filled in by the DSL context variables.
 */
#define _bsonBuild_append(Kind, ...)             \
   bson_append_##Kind (bsonBuildContext.doc,     \
                       bsonBuildContext.key,     \
                       bsonBuildContext.key_len, \
                       __VA_ARGS__)

/// We must defer expansion of the nested doc() to allow "recursive" evaluation
#define _bsonBuild_docAppendValue_doc \
   _bsonBuild_docAppendValueDeferred_doc pBsonDSL_nothing
#define _bsonBuild_arrAppendValue_doc _bsonBuild_docAppendValue_doc

#define _bsonBuild_docAppendValueDeferred_doc(...)                   \
   _bsonDSL_begin ("Append a new document [%s]",                     \
                   "" pBsonDSL_str (__VA_ARGS__));                   \
   /* Write to this variable as the child: */                        \
   bson_t _doc_ = BSON_INITIALIZER;                                  \
   _bsonBuild_append (document_begin, &_doc_);                       \
   /* Remember the parent: */                                        \
   /* Set the child as the new context doc to modify: */             \
   /* Remember the parent: */                                        \
   struct _bsonBuildContext_t _bbCtx = (struct _bsonBuildContext_t){ \
      .doc = &_doc_,                                                 \
      .parent = *_bsonBuildContextPtr (),                            \
   };                                                                \
   *_bsonBuildContextPtr () = &_bbCtx;                               \
   /* Apply the nested dsl for documents: */                         \
   pBsonDSL_mapMacro (_bsonBuild_docElement, ~, __VA_ARGS__);        \
   /* Restore the prior context document and finish the append: */   \
   *_bsonBuildContextPtr () = _bbCtx.parent;                         \
   bson_append_document_end (bsonBuildContext.doc, &_doc_);          \
   _bsonDSL_end

/// We must defer expansion of the nested array() to allow "recursive"
/// evaluation
#define _bsonBuild_docAppendValue_array \
   _bsonBuild_docAppendValueDeferred_array pBsonDSL_nothing
#define _bsonBuild_arrAppendValue_array _bsonBuild_docAppendValue_array

#define _bsonBuild_docAppendValueDeferred_array(...)                 \
   _bsonDSL_begin ("Append an array() value: [%s]",                  \
                   "" pBsonDSL_str (__VA_ARGS__));                   \
   /* Write to this variable as the child array: */                  \
   bson_t _array_ = BSON_INITIALIZER;                                \
   _bsonBuild_append (array_begin, &_array_);                        \
   /* Set the child as the new context doc to modify: */             \
   /* Remember the parent: */                                        \
   struct _bsonBuildContext_t _bbCtx = (struct _bsonBuildContext_t){ \
      .doc = &_array_,                                               \
      .parent = *_bsonBuildContextPtr (),                            \
      .index = 0,                                                    \
   };                                                                \
   *_bsonBuildContextPtr () = &_bbCtx;                               \
   pBsonDSL_mapMacro (pBsonDSL_arrayAppend, ~, __VA_ARGS__);         \
   /* Restore the prior context document and finish the append: */   \
   *_bsonBuildContextPtr () = _bbCtx.parent;                         \
   bson_append_array_end (bsonBuildContext.doc, &_array_);           \
   _bsonDSL_end

/// Append a "cstr" as UTF-8
#define _bsonBuild_docAppendValue_cstr(String) \
   _bsonBuild_append (utf8, String, strlen (String))
#define _bsonBuild_arrAppendValue_cstr _bsonBuild_docAppendValue_cstr

/// Append a UTF-8 string with an explicit length
#define _bsonBuild_docAppendValue_utf8_w_len(String, Len) \
   _bsonBuild_append (utf8, (String), (Len))
#define _bsonBuild_arrAppendValue_utf8_w_len \
   _bsonBuild_docAppendValue_utf8_w_len

/// Append an int32
#define _bsonBuild_docAppendValue_i32(Integer) \
   _bsonBuild_append (int32, (Integer))
#define _bsonBuild_arrAppendValue_i32 _bsonBuild_docAppendValue_i32

/// Append an int64
#define _bsonBuild_docAppendValue_i64(Integer) \
   _bsonBuild_append (int64, (Integer))
#define _bsonBuild_arrAppendValue_i64 _bsonBuild_docAppendValue_i64

/// Append the value referenced by a given iterator
#define _bsonBuild_docAppendValue_iter(Iter) _bsonBuild_append (iter, &(Iter))
#define _bsonBuild_arrAppendValue_iter _bsonBuild_docAppendValue_iter

/// Append the BSON document referenced by the given pointer
#define _bsonBuild_docAppendValue_bson(Ptr) _bsonBuild_append (document, (Ptr))
#define _bsonBuild_arrAppendValue_bson _bsonBuild_docAppendValue_bson

/// Append the BSON document referenced by the given pointer as an array
#define _bsonBuild_docAppendValue_bsonArray(Ptr) \
   _bsonBuild_append (array, (Ptr))
#define _bsonBuild_arrAppendValue_bsonArray _bsonBuild_docAppendValue_bsonArray

#define _bsonBuild_docAppendValue_bool(b) _bsonBuild_append (bool, (b))
#define _bsonBuild_arrAppendValue_bool _bsonBuild_docAppendValue_bool
#define _bsonBuild_docAppendValue__Bool(b) _bsonBuild_append (bool, (b))
#define _bsonBuild_arrAppendValue__Bool _bsonBuild_docAppendValue_bool

#define _bsonBuild_docAppendValue_null \
   bson_append_null (                  \
      bsonBuildContext.doc, bsonBuildContext.key, bsonBuildContext.key_len)
#define _bsonBuild_arrAppendValue_null _bsonBuild_docAppendValue_null

// DSL items for doc() appending (and the top-level bsonBuild):

/// key-value pair with explicit key length
#define _bsonBuild_docElement_kvl(String, Len, Element)                     \
   _bsonDSL_begin ("Append: '%s' => [%s]", String, pBsonDSL_str (Element)); \
   _bbCtx.key = (String);                                                   \
   _bbCtx.key_len = (Len);                                                  \
   _bsonBuild_docAppendValue_##Element;                                     \
   _bsonDSL_end

/// Key-value pair with a C-string
#define _bsonBuild_docElement_kv(String, Element) \
   _bsonBuild_docElement_kvl ((String), strlen ((String)), Element)

#define pBsonDSL_insertFilter_all _filter_drop_ = false
#define pBsonDSL_insertFilter_excluding(...)                         \
   _filter_drop_ =                                                   \
      _bson_dsl_key_is_anyof (bson_iter_key (&_bbDocInsertIter),     \
                              bson_iter_key_len (&_bbDocInsertIter), \
                              __VA_ARGS__,                           \
                              NULL)

#define pBsonDSL_insertFilter(F, _nil, _count) \
   if (!_filter_drop_) {                       \
      pBsonDSL_insertFilter_##F;               \
   }

/// Insert the given BSON document into the parent document in-place
#define _bsonBuild_docElement_insert(OtherDoc, ...)                         \
   _bsonDSL_begin ("Insert other document: [%s]", pBsonDSL_str (OtherDoc)); \
   bson_iter_t _bbDocInsertIter;                                            \
   bson_t *_bbInsertDoc = (bson_t *) (OtherDoc);                            \
   bson_iter_init (&_bbDocInsertIter, _bbInsertDoc);                        \
   while (bson_iter_next (&_bbDocInsertIter)) {                             \
      bool _filter_drop_ = false;                                           \
      pBsonDSL_mapMacro (pBsonDSL_insertFilter, ~, __VA_ARGS__);            \
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
   _bsonDSL_begin ("Insert other array: [%s]", pBsonDSL_str (OtherArr));     \
   bson_iter_t iter;                                                         \
   bson_iter_init (&iter, (bson_t *) (OtherArr));                            \
   while (bson_iter_next (&iter)) {                                          \
      pBsonDSL_setKeyToArrayIndex (bsonBuildContext.index);                  \
      _bsonBuild_arrAppendValue_iter (iter);                                 \
      ++_bbCtx.index;                                                        \
   }                                                                         \
   /* Decrement once to counter the outer increment that will be applied: */ \
   --_bbCtx.index;                                                           \
   _bsonDSL_end

#define _bsonBuild_docElementIfThen_then _bsonBuild_docAppendValue_doc
#define _bsonBuild_docElementIfElse_else _bsonBuild_docAppendValue_doc

#define _bsonBuild_docElement_if(Condition, Then, Else)                     \
   _bsonDSL_begin ("Conditional append on [%s]", pBsonDSL_str (Condition)); \
   if ((Condition)) {                                                       \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", pBsonDSL_str (Then));      \
      _bsonBuild_docElementIfThen_##Then;                                   \
   } else {                                                                 \
      _bsonDSLDebug ("Taking FALSE branch: [%s]", pBsonDSL_str (Else));     \
      _bsonBuild_docElementIfElse_##Else;                                   \
   }                                                                        \
   _bsonDSL_end

#define _bsonBuild_arrAppendValueIfThen_then _bsonBuild_docAppendValue_array
#define _bsonBuild_arrAppendValueIfElse_else _bsonBuild_docAppendValue_array

#define _bsonBuild_arrAppendValue_if(Condition, Then, Else)                \
   _bsonDSL_begin ("Conditional value on [%s]", pBsonDSL_str (Condition)); \
   if ((Condition)) {                                                      \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", pBsonDSL_str (Then));     \
      _bsonBuild_arrAppendValueIfThen_##Then;                              \
   } else {                                                                \
      _bsonDSLDebug ("Taking FALSE branch: [%s]", pBsonDSL_str (Else));    \
      _bsonBuild_arrAppendValueIfElse_##Else;                              \
   }                                                                       \
   _bsonDSL_end
#define _bsonBuild_docAppendValue_if _bsonBuild_arrAppendValue_if

#define pBsonDSL_setKeyToArrayIndex(Idx)                           \
   _bbCtx.key_len = sprintf (_bbCtx.strbuf64, "%d", _bbCtx.index); \
   _bbCtx.key = _bbCtx.strbuf64

// Handle an element of doc()
#define _bsonBuild_docElement(Elem, _nil, _count) _bsonBuild_docElement_##Elem;

/// Handle an element of array()
#define pBsonDSL_arrayAppend(Element, _nil, _count)      \
   _bsonDSL_begin ("Array append [%d]: [%s]",            \
                   bsonBuildContext.index,               \
                   pBsonDSL_str (Element));              \
   /* Set the doc key to the array index as a string: */ \
   pBsonDSL_setKeyToArrayIndex (bsonBuildContext.index); \
   /* Append a value: */                                 \
   _bsonBuild_arrAppendValue_##Element;                  \
   /* Increment the array index: */                      \
   ++_bbCtx.index;                                       \
   _bsonDSL_end;

#define pBsonDSL_documentBuild(...) \
   pBsonDSL_mapMacro (_bsonBuild_docElement, ~, __VA_ARGS__)

#define pBsonDSL_iterType_array BSON_TYPE_ARRAY
#define pBsonDSL_iterType_bool BSON_TYPE_BOOL
#define pBsonDSL_iterType__Bool BSON_TYPE_BOOL
#define pBsonDSL_iterType_doc BSON_TYPE_DOCUMENT

#define _bsonVisitOneApply_ifType(T, ...)                \
   if (bson_iter_type_unsafe (&bsonVisitContext.iter) == \
       pBsonDSL_iterType_##T) {                          \
      _bsonVisitOneApplyOps (__VA_ARGS__);               \
   }

#define _bsonVisitOneApply_halt _bwHalt = true

#define _bsonVisitOneApply_ifTruthy(...)                        \
   _bsonDSL_begin ("ifTruthy(%s)", pBsonDSL_str (__VA_ARGS__)); \
   if (bson_iter_as_bool (&bsonVisitContext.iter)) {            \
      _bsonVisitOneApplyOps (__VA_ARGS__);                      \
   }                                                            \
   _bsonDSL_end;

#define _bsonVisitOneApply_ifStrEqual(S, ...)                              \
   _bsonDSL_begin ("ifStrEqual(%s)", pBsonDSL_str (S));                    \
   if (bson_iter_type_unsafe (&bsonVisitContext.iter) == BSON_TYPE_UTF8) { \
      uint32_t len;                                                        \
      const char *str = bson_iter_utf8 (&bsonVisitContext.iter, &len);     \
      if (len == strlen (S) && (0 == memcmp (str, S, len))) {              \
         _bsonVisitOneApplyOps (__VA_ARGS__);                              \
      }                                                                    \
   }                                                                       \
   _bsonDSL_end

#define _bsonVisitOneApply_storeBool(Dest)                 \
   _bsonDSL_begin ("storeBool(%s)", pBsonDSL_str (Dest));  \
   if (BSON_ITER_HOLDS_BOOL (&bsonVisitContext.iter)) {    \
      (Dest) = bson_iter_as_bool (&bsonVisitContext.iter); \
   }                                                       \
   _bsonDSL_end

#define _bsonVisitOneApply_found(IterDest)                \
   _bsonDSL_begin ("found(%s)", pBsonDSL_str (IterDest)); \
   (IterDest) = bsonVisitContext.iter;                    \
   _bsonDSL_end

#define _bsonVisitOneApply_do(...)                            \
   _bsonDSL_begin ("do: { %s }", pBsonDSL_str (__VA_ARGS__)); \
   do {                                                       \
      __VA_ARGS__;                                            \
   } while (0);                                               \
   _bsonDSL_end

#define _bsonVisitOneApply_append \
   _bsonVisitOneApplyDeferred_append pBsonDSL_nothing
#define _bsonVisitOneApplyDeferred_append(Doc, ...)                           \
   _bsonDSL_begin (                                                           \
      "append to [%s] : %s", pBsonDSL_str (Doc), pBsonDSL_str (__VA_ARGS__)); \
   _bsonBuildAppend (Doc, __VA_ARGS__);                                       \
   _bsonDSL_end

#define _bsonVisitEach(Doc, ...)                                            \
   _bsonDSL_begin (                                                         \
      "visitEach(%s, %s)", pBsonDSL_str (Doc), pBsonDSL_str (__VA_ARGS__)); \
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
   _bsonVisitOneApply_visitEachDeferred pBsonDSL_nothing
#define _bsonVisitOneApply_visitEachDeferred(...)                              \
   _bsonDSL_begin ("visitEach(%s)", pBsonDSL_str (__VA_ARGS__));               \
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

#define _bsonParseDeferred _bsonParse pBsonDSL_nothing

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
   _bsonDSL_begin ("Set [%s] to 'true'", pBsonDSL_str (B)); \
   (B) = true;                                              \
   _bsonDSL_end;

#define _bsonVisitOneApply_setFalse(B)                       \
   _bsonDSL_begin ("Set [%s] to 'false'", pBsonDSL_str (B)); \
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
      "Searching for key \"%s\" [%s]", (Key), pBsonDSL_str (Key));      \
   if (bson_iter_init_find (&_bpCtx.iter, bsonVisitContext.doc, Key)) { \
      _bsonVisitOneApplyOps (__VA_ARGS__);                              \
   }                                                                    \
   _bsonDSL_end;

#define _bsonParseDo_append(...) _bsonBuildAppend (__VA_ARGS__)

/**
 * @brief Parse each entry in the document at the given pointer.
 */
#define _bsonParse(DocPointer, ...)                              \
   _bsonDSL_begin ("Walking: [%s]", pBsonDSL_str (__VA_ARGS__)); \
   /* Reset the context */                                       \
   struct _bsonVisitContext_t _bpCtx = {                         \
      .doc = (DocPointer),                                       \
      .parent = &bsonVisitContext,                               \
   };                                                            \
   *_bsonVisitContextPtr () = &_bpCtx;                           \
   pBsonDSL_mapMacro (_bsonParseDo, ~, __VA_ARGS__);             \
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
//    _bsonDSL_begin ("Walking: [%s]", pBsonDSL_str (__VA_ARGS__)); \
//    /* Reset the context */                                       \
//    struct _bsonVisitContext_t _bpCtx = {                         \
//       .doc = (DocPointer),                                       \
//       .parent = &bsonVisitContext,                               \
//    };                                                            \
//    *_bsonVisitContextPtr () = &_bpCtx;                           \
//    bson_iter_t iter;                                             \
//    pBsonDSL_mapMacro (_bsonParseDo, ~, __VA_ARGS__);             \
//    /* Restore the dsl context */                                 \
//    *_bsonVisitContextPtr () = bsonVisitContext.parent;           \
//    _bsonDSL_end

#define _bsonVisitOneApplyOps _bsonVisitOneApplyOpsDeferred pBsonDSL_nothing
#define _bsonVisitOneApplyOpsDeferred(...)                   \
   _bsonDSL_begin ("Walk [%s]", pBsonDSL_str (__VA_ARGS__)); \
   pBsonDSL_mapMacro (_bsonVisitOneApply, ~, __VA_ARGS__);   \
   _bsonDSL_end;

/**
 * @brief Build a BSON document by appending to an existing bson_t document
 *
 * @param Pointer The document upon which to append
 * @param ... The Document elements to append to the document
 */
#define bsonBuildAppend(Pointer, ...) \
   pBsonDSL_eval (_bsonBuildAppend (Pointer, __VA_ARGS__))
#define _bsonBuildAppend(Pointer, ...)                                    \
   _bsonDSL_begin ("Appending to document '%s'", pBsonDSL_str (Pointer)); \
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
   pBsonDSL_documentBuild (__VA_ARGS__);                                  \
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
                   pBsonDSL_str (PointerDest));       \
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
   _bsonDSL_begin ("bsonParse(%s)", pBsonDSL_str (doc));               \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic push");)                 \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic ignored \"-Wshadow\"");) \
   bool _bwHalt = false;                                               \
   const bool _bvContinue = false;                                     \
   const bool _bvBreak = false;                                        \
   pBsonDSL_eval (_bsonParse (&(doc), __VA_ARGS__));                   \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic pop");)                  \
   _bsonDSL_end

/**
 * @brief Visit each element of a BSON document
 */
#define bsonVisitEach(Doc, ...)                                        \
   _bsonDSL_begin ("bsonVisitEach(%s)", pBsonDSL_str (Doc));           \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic push");)                 \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic ignored \"-Wshadow\"");) \
   bool _bwHalt = false;                                               \
   pBsonDSL_eval (_bsonVisitEach ((Doc), __VA_ARGS__));                \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic pop");)                  \
   _bsonDSL_end

#endif // BSON_BSON_DSL_H_INCLUDED
