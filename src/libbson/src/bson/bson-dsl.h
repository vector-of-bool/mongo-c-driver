#ifndef BSON_BSON_DSL_H_INCLUDED
#define BSON_BSON_DSL_H_INCLUDED

/**
 * @file bson-dsl.h
 * @brief Define a C-preprocessor DSL for working with BSON objects
 *
 * This file defines an embedded DSL for working with BSON objects consisely and
 * correctly. The main macros are:
 *
 * - @see BSON_BUILD(Ptr, ...)
 *       Construct a new document and assign the result to `Ptr`.
 *       bson_destroy(Ptr) will be called before the assignment, but after the
 *       construction.
 * - @see BSON_BUILD_APPEND(Ptr, ...)
 *       Append new items to the bson_t at `Ptr`. `Ptr` must be an initialized
 *       bson_t object.
 * - @see BSON_BUILD_DECL(Var, ...)
 *       Declare a new bson_t `Var` and construct a new document into it.
 *
 * The remaining arguments are the same for all three macros, and the same as
 * the `doc()` element. See below.
 */

#include "bson.h"

enum { BSON_DSL_DEBUG = 1 };

struct _bson_dsl_parseDoc_keyInfo {
   const char *key;
   bool isRequired;
   int seenCount;
};

typedef struct bson_dsl_context_t {
   bson_t *doc;
   const char *key;
   int key_len;
   int index;
   char strbuf64[64];
   bson_iter_t iter;
   bson_type_t type;
   struct bson_dsl_context_t const *parent;
} bson_dsl_context_t;

BSON_EXPORT (bson_dsl_context_t *)
pBsonDSLctx ();

#define bsonDSLContext (*pBsonDSLctx ())

static const char *_bson_dsl_file = NULL;
static int _bson_dsl_line = 0;
static int _bson_dsl_indent = 0;

static inline void BSON_GNUC_PRINTF (1, 2)
   _bson_dsl_debug (const char *string, ...)
{
   if (BSON_DSL_DEBUG) {
      fprintf (stderr, "%s:%d: bson_dsl: ", _bson_dsl_file, _bson_dsl_line);
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

#define pBSONDSL_PICK_128th(\
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
    pBSONDSL_PICK_128th(__VA_ARGS__, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, ~)

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

#define pBsonDSL_eval_1(...) __VA_ARGS__
#define pBsonDSL_eval_2(...) pBsonDSL_eval_1(pBsonDSL_eval_1(__VA_ARGS__))
#define pBsonDSL_eval_4(...) pBsonDSL_eval_2(pBsonDSL_eval_2(__VA_ARGS__))
#define pBsonDSL_eval_8(...) pBsonDSL_eval_4(pBsonDSL_eval_4(__VA_ARGS__))
#define pBsonDSL_eval_16(...) pBsonDSL_eval_8(pBsonDSL_eval_8(__VA_ARGS__))
#define pBsonDSL_eval(...) pBsonDSL_eval_16(pBsonDSL_eval_16(__VA_ARGS__))

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
    pBsonDSL_eval(\
        pBsonDSL_ifElse(pBsonDSL_isEmpty(__VA_ARGS__), pBsonDSL_ignore, pBsonDSL_mapMacro_first) \
        pBsonDSL_nothing (Action, Constant, 0, __VA_ARGS__))

#define pBsonDSL_mapMacroInner(Action, Constant, ...) \
    pBsonDSL_ifElse(pBsonDSL_isEmpty(__VA_ARGS__), pBsonDSL_ignore, pBsonDSL_mapMacro_first) \
    pBsonDSL_nothing (Action, Constant, 0, __VA_ARGS__)

// clang-format on

#define pBsonDSL_begin(Str, ...)          \
   do {                                   \
      _bson_dsl_file = __FILE__;          \
      _bson_dsl_line = __LINE__;          \
      _bson_dsl_debug (Str, __VA_ARGS__); \
   ++_bson_dsl_indent

#define pBsonDSL_end   \
   --_bson_dsl_indent; \
   }                   \
   while (0)

/**
 * @brief Expands to a call to bson_append_{Kind}, with the three first
 * arguments filled in by the DSL context variables.
 */
#define pBsonDSL_append(Kind, ...)             \
   bson_append_##Kind (bsonDSLContext.doc,     \
                       bsonDSLContext.key,     \
                       bsonDSLContext.key_len, \
                       __VA_ARGS__)

/// We must defer expansion of the nested doc() to allow "recursive" evaluation
#define pBsonDSL_documentValue_doc \
   pBsonDSL_docElementDeferred_doc pBsonDSL_nothing
#define pBsonDSL_arrayElement_doc pBsonDSL_documentValue_doc

#define pBsonDSL_docElementDeferred_doc(...)                                  \
   pBsonDSL_begin ("Append a new document [%s]", pBsonDSL_str (__VA_ARGS__)); \
   /* Write to this variable as the child: */                                 \
   bson_t _doc_ = BSON_INITIALIZER;                                           \
   pBsonDSL_append (document_begin, &_doc_);                                  \
   /* Remember the parent: */                                                 \
   bson_t *prev_doc = bsonDSLContext.doc;                                     \
   /* Set the child as the new context doc to modify: */                      \
   bsonDSLContext.doc = &_doc_;                                               \
   /* Apply the nested dsl for documents: */                                  \
   pBsonDSL_mapMacroInner (pBsonDSL_docElement, ~, __VA_ARGS__);              \
   /* Restore the prior context document and finish the append: */            \
   bsonDSLContext.doc = prev_doc;                                             \
   bson_append_document_end (prev_doc, &_doc_);                               \
   pBsonDSL_end

/// We must defer expansion of the nested array() to allow "recursive"
/// evaluation
#define pBsonDSL_documentValue_array \
   pBsonDSL_docElementDeferred_array pBsonDSL_nothing
#define pBsonDSL_arrayElement_array pBsonDSL_documentValue_array

#define pBsonDSL_docElementDeferred_array(...)                        \
   pBsonDSL_begin ("Append an array() value: [%s]",                   \
                   pBsonDSL_str (__VA_ARGS__));                       \
   /* Write to this variable as the child array: */                   \
   bson_t _array_ = BSON_INITIALIZER;                                 \
   pBsonDSL_append (array_begin, &_array_);                           \
   /* Remember the parent: */                                         \
   bson_t *prev_doc = bsonDSLContext.doc;                             \
   /* Set the child as the new context doc to modify: */              \
   bsonDSLContext.doc = &_array_;                                     \
   /* Remember the array index, as we may be within another array: */ \
   int prev_idx = bsonDSLContext.index;                               \
   /* Apply the nested dsl for arrays: */                             \
   bsonDSLContext.index = 0;                                          \
   pBsonDSL_mapMacroInner (pBsonDSL_arrayAppend, ~, __VA_ARGS__);     \
   /* Restore the prior context document and finish the append: */    \
   bsonDSLContext.doc = prev_doc;                                     \
   bsonDSLContext.index = prev_idx;                                   \
   bson_append_array_end (bsonDSLContext.doc, &_array_);              \
   pBsonDSL_end

/// Append a "cstr" as UTF-8
#define pBsonDSL_documentValue_cstr(String) \
   pBsonDSL_append (utf8, String, strlen (String))
#define pBsonDSL_arrayElement_cstr pBsonDSL_documentValue_cstr

/// Append a UTF-8 string with an explicit length
#define pBsonDSL_documentValue_utf8_w_len(String, Len) \
   pBsonDSL_append (utf8, (String), (Len))
#define pBsonDSL_arrayElement_utf8_w_len pBsonDSL_documentValue_utf8_w_len

/// Append an int32
#define pBsonDSL_documentValue_i32(Integer) pBsonDSL_append (int32, (Integer))
#define pBsonDSL_arrayElement_i32 pBsonDSL_documentValue_i32

/// Append an int64
#define pBsonDSL_documentValue_i64(Integer) pBsonDSL_append (int64, (Integer))
#define pBsonDSL_arrayElement_i64 pBsonDSL_documentValue_i64

/// Append the value referenced by a given iterator
#define pBsonDSL_documentValue_iter(Iter) pBsonDSL_append (iter, (Iter))
#define pBsonDSL_arrayElement_iter pBsonDSL_documentValue_iter

/// Append the BSON document referenced by the given pointer
#define pBsonDSL_documentValue_bson(Ptr) pBsonDSL_append (document, (Ptr))
#define pBsonDSL_arrayElement_bson pBsonDSL_documentValue_bson

/// Append the BSON document referenced by the given pointer as an array
#define pBsonDSL_documentValue_bsonArray(Ptr) pBsonDSL_append (array, (Ptr))
#define pBsonDSL_arrayElement_bsonArray pBsonDSL_documentValue_bsonArray

// DSL items for doc() appending (and the top-level BSON_BUILD):

/// key-value pair with explicit key length
#define pBsonDSL_docElement_kvl(String, Len, Element)                       \
   pBsonDSL_begin ("Append: '%s' => [%s]", String, pBsonDSL_str (Element)); \
   bsonDSLContext.key = (String);                                           \
   bsonDSLContext.key_len = (Len);                                          \
   pBsonDSL_documentValue_##Element;                                        \
   pBsonDSL_end

/// Key-value pair with a C-string
#define pBsonDSL_docElement_kv(String, Element) \
   pBsonDSL_docElement_kvl ((String), strlen ((String)), Element)

#define pBsonDSL_insertFilter_all _filter_drop_ = false
#define pBsonDSL_insertFilter_excluding(...) \
   _filter_drop_ = _bson_dsl_key_is_anyof (  \
      bson_iter_key (&_iter_), bson_iter_key_len (&_iter_), __VA_ARGS__, NULL)

#define pBsonDSL_insertFilter(F, _nil, _count) \
   if (!_filter_drop_) {                       \
      pBsonDSL_insertFilter_##F;               \
   }

/// Insert the given BSON document into the parent document in-place
#define pBsonDSL_docElement_insert(OtherDoc, ...)                           \
   pBsonDSL_begin ("Insert other document: [%s]", pBsonDSL_str (OtherDoc)); \
   bson_iter_t _iter_;                                                      \
   bson_t *_merge_doc_ = (bson_t *) (OtherDoc);                             \
   bson_iter_init (&_iter_, _merge_doc_);                                   \
   while (bson_iter_next (&_iter_)) {                                       \
      bool _filter_drop_ = false;                                           \
      pBsonDSL_mapMacroInner (pBsonDSL_insertFilter, ~, __VA_ARGS__);       \
      if (_filter_drop_) {                                                  \
         continue;                                                          \
      }                                                                     \
      pBsonDSL_docElement_kvl (bson_iter_key (&_iter_),                     \
                               bson_iter_key_len (&_iter_),                 \
                               iter (&_iter_));                             \
   }                                                                        \
   pBsonDSL_end

/// Insert the given BSON document into the parent array. Keys of the given
/// document are discarded and it is treated as an array of values.
#define pBsonDSL_arrayElement_insert(OtherArr)                               \
   pBsonDSL_begin ("Insert other array: [%s]", pBsonDSL_str (OtherArr));     \
   bson_iter_t iter;                                                         \
   bson_iter_init (&iter, (bson_t *) (OtherArr));                            \
   while (bson_iter_next (&iter)) {                                          \
      pBsonDSL_setKeyToArrayIndex (bsonDSLContext.index);                    \
      pBsonDSL_arrayElement_iter (&iter);                                    \
      ++bsonDSLContext.index;                                                \
   }                                                                         \
   /* Decrement once to counter the outer increment that will be applied: */ \
   --bsonDSLContext.index;                                                   \
   pBsonDSL_end

#define pBsonDSL_docElementIfThen_then pBsonDSL_documentValue_doc
#define pBsonDSL_docElementIfElse_else pBsonDSL_documentValue_doc

#define pBsonDSL_docElement_if(Condition, Then, Else)                       \
   pBsonDSL_begin ("Conditional append on [%s]", pBsonDSL_str (Condition)); \
   if ((Condition)) {                                                       \
      _bson_dsl_debug ("Taking TRUE branch: [%s]", pBsonDSL_str (Then));    \
      pBsonDSL_docElementIfThen_##Then;                                     \
   } else {                                                                 \
      _bson_dsl_debug ("Taking FALSE branch: [%s]", pBsonDSL_str (Else));   \
      pBsonDSL_docElementIfElse_##Else;                                     \
   }                                                                        \
   pBsonDSL_end

#define pBsonDSL_arrayElementIfThen_then pBsonDSL_documentValue_array
#define pBsonDSL_arrayElementIfElse_else pBsonDSL_documentValue_array

#define pBsonDSL_arrayElement_if(Condition, Then, Else)                    \
   pBsonDSL_begin ("Conditional array on [%s]", pBsonDSL_str (Condition)); \
   if ((Condition)) {                                                      \
      _bson_dsl_debug ("Taking TRUE branch: [%s]", pBsonDSL_str (Then));   \
      pBsonDSL_arrayElementIfThen_##Then;                                  \
   } else {                                                                \
      _bson_dsl_debug ("Taking FALSE branch: [%s]", pBsonDSL_str (Else));  \
      pBsonDSL_arrayElementIfElse_##Else;                                  \
   }

#define pBsonDSL_setKeyToArrayIndex(Idx)                             \
   bsonDSLContext.key_len =                                          \
      sprintf (bsonDSLContext.strbuf64, "%d", bsonDSLContext.index); \
   bsonDSLContext.key = bsonDSLContext.strbuf64

// Handle an element of doc()
#define pBsonDSL_docElement(Elem, _nil, _count) pBsonDSL_docElement_##Elem;

/// Handle an element of array()
#define pBsonDSL_arrayAppend(Element, _nil, _count)      \
   pBsonDSL_begin ("Array append [%d]: [%s]",            \
                   bsonDSLContext.index,                 \
                   pBsonDSL_str (Element));              \
   /* Set the doc key to the array index as a string: */ \
   pBsonDSL_setKeyToArrayIndex (bsonDSLContext.index);   \
   /* Append a value: */                                 \
   pBsonDSL_arrayElement_##Element;                      \
   /* Increment the array index: */                      \
   ++bsonDSLContext.index;                               \
   pBsonDSL_end;

#define pBsonDSL_documentBuild(...) \
   pBsonDSL_mapMacro (pBsonDSL_docElement, ~, __VA_ARGS__)

/**
 * @brief Build a BSON document by appending to an existing bson_t document
 *
 * @param Pointer The document upon which to append
 * @param ... The Document elements to append to the document
 */
#define BSON_BUILD_APPEND(Pointer, ...)                                   \
   pBsonDSL_begin ("Appending to document '%s'", pBsonDSL_str (Pointer)); \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic push");)                    \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic ignored \"-Wshadow\"");)    \
   /* Save the dsl context */                                             \
   bson_dsl_context_t prev_ctx = bsonDSLContext;                          \
   /* Reset the context */                                                \
   bsonDSLContext = (bson_dsl_context_t){                                 \
      .doc = (Pointer),                                                   \
      .key = NULL,                                                        \
      .key_len = 0,                                                       \
      .index = 0,                                                         \
      .strbuf64 = {0},                                                    \
   };                                                                     \
   pBsonDSL_documentBuild (__VA_ARGS__);                                  \
   /* Restore the dsl context */                                          \
   bsonDSLContext = prev_ctx;                                             \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic pop");)                     \
   pBsonDSL_end

/**
 * @brief Build a new BSON document and assign the value into the given
 * pointer. The pointed-to document is destroyed before being assigned, but
 * after the document is built.
 */
#define BSON_BUILD(PointerDest, ...)                    \
   pBsonDSL_begin ("Build a new document for '%s'",     \
                   pBsonDSL_str (PointerDest));         \
   bson_t *_bson_dsl_new_doc_ = bson_new ();            \
   BSON_BUILD_APPEND (_bson_dsl_new_doc_, __VA_ARGS__); \
   bson_destroy ((PointerDest));                        \
   (PointerDest) = _bson_dsl_new_doc_;                  \
   pBsonDSL_end

/**
 * @brief Declare a variable and build it with the BSON DSL @see BSON_BUILD
 */
#define BSON_BUILD_DECL(Variable, ...) \
   bson_t *Variable = NULL;            \
   BSON_BUILD (Variable, __VA_ARGS__)

#define pBsonDSL_iterType_array BSON_TYPE_ARRAY

#define pBsonDSL_parseEntry_ifType(T, ...)             \
   if (bsonDSLContext.type == pBsonDSL_iterType_##T) { \
      pBsonDSL_parseCurrent (__VA_ARGS__);             \
   }

#define pBsonDSL_parseEntry_ifStrEqual(S, ...)                       \
   pBsonDSL_begin ("ifStrEqual(%s)", pBsonDSL_str (S));              \
   if (bsonDSLContext.type == BSON_TYPE_UTF8) {                      \
      uint32_t len;                                                  \
      const char *str = bson_iter_utf8 (&bsonDSLContext.iter, &len); \
      if (len == strlen (S) && (0 == memcmp (str, S, len))) {        \
         pBsonDSL_parseCurrent (__VA_ARGS__);                        \
      }                                                              \
   }                                                                 \
   pBsonDSL_end

#define pBsonDSL_parseEntry_do(...)                           \
   pBsonDSL_begin ("do: { %s }", pBsonDSL_str (__VA_ARGS__)); \
   do {                                                       \
      __VA_ARGS__;                                            \
   } while (0);                                               \
   pBsonDSL_end

#define pBsonDSL_parseEntry_forEach \
   pBsonDSL_parseEntryDeferred_forEach pBsonDSL_nothing
#define pBsonDSL_parseEntryDeferred_forEach(...)                             \
   pBsonDSL_begin ("forEach(%s)", pBsonDSL_str (__VA_ARGS__));               \
   do {                                                                      \
      const uint8_t *data;                                                   \
      uint32_t len;                                                          \
      if (bsonDSLContext.type == BSON_TYPE_ARRAY)                            \
         bson_iter_array (&bsonDSLContext.iter, &len, &data);                \
      else if (bsonDSLContext.type == BSON_TYPE_DOCUMENT)                    \
         bson_iter_document (&bsonDSLContext.iter, &len, &data);             \
      else                                                                   \
         break;                                                              \
      bson_t inner;                                                          \
      bson_init_static (&inner, data, len);                                  \
      /* Save the dsl context */                                             \
      bson_dsl_context_t prev_ctx = bsonDSLContext;                          \
      /* Reset the context */                                                \
      bsonDSLContext = (bson_dsl_context_t){                                 \
         .doc = (bson_t *) (&inner),                                         \
         .parent = &prev_ctx,                                                \
      };                                                                     \
      /* Iterate over each element of the document */                        \
      bson_iter_init (&bsonDSLContext.iter, bsonDSLContext.doc);             \
      while (bson_iter_next (&bsonDSLContext.iter)) {                        \
         bsonDSLContext.key = bson_iter_key (&bsonDSLContext.iter);          \
         bsonDSLContext.key_len =                                            \
            (int) bson_iter_key_len (&bsonDSLContext.iter);                  \
         bsonDSLContext.type = bson_iter_type_unsafe (&bsonDSLContext.iter); \
         pBsonDSL_mapMacroInner (pBsonDSL_parseEntry, ~, __VA_ARGS__)        \
      }                                                                      \
      /* Restore the dsl context */                                          \
      bsonDSLContext = prev_ctx;                                             \
   } while (0);                                                              \
   pBsonDSL_end

#define pBsonDSL_parseDocDeferred pBsonDSL_parseDocB pBsonDSL_nothing

#define pBsonDSL_parseEntry_nop ((void) 0)
#define pBsonDSL_parseEntry_doc(...)                          \
   const uint8_t *data;                                       \
   uint32_t len;                                              \
   if (bsonDSLContext.type == BSON_TYPE_ARRAY)                \
      bson_iter_array (&bsonDSLContext.iter, &len, &data);    \
   else if (bsonDSLContext.type == BSON_TYPE_DOCUMENT)        \
      bson_iter_document (&bsonDSLContext.iter, &len, &data); \
   else                                                       \
      break;                                                  \
   bson_t inner;                                              \
   bson_init_static (&inner, data, len);                      \
   pBsonDSL_parseDocDeferred (&inner, __VA_ARGS__);

#define pBsonDSL_parseEntry(P, _const, _count) pBsonDSL_parseEntry_##P;

/// Match an entry by its key
#define pBsonDSL_parseDocEntry_ifKey(Key, ...)                            \
   if (strcmp (currentKey, (thisKey->key)) == 0) {                        \
      handledKey = true;                                                  \
      _bson_dsl_debug ("Found key %s [[%s]]", pBsonDSL_str (Key), (Key)); \
      pBsonDSL_parseCurrent (__VA_ARGS__)                                 \
   }


/// Parse one entry referrenced by the context iterator
#define pBsonDSL_parseDocEntry(P, _nil, Counter) \
   thisKey = _parseKeyInfos + Counter;           \
   pBsonDSL_parseDocEntry_##P;                   \
   if (handledKey) {                             \
      thisKey->seenCount++;                      \
      continue;                                  \
   }

#define pBsonDSL_parseDocBefore_ifKey(K, ...)      \
   *thisKey = (struct _bson_dsl_parseDoc_keyInfo){ \
      .key = K,                                    \
      .isRequired = false,                         \
   };
#define pBsonDSL_parseDocAfter_ifKey(K, ...) /* Nothing */
#define pBsonDSL_parseDocBefore_requireKey(K, ...) \
   *thisKey = (struct _bson_dsl_parseDoc_keyInfo){ \
      .key = K,                                    \
      .isRequired = true,                          \
   };
#define pBsonDSL_parseDocBefore(P, _nil, Counter) \
   thisKey = _parseKeyInfos + Counter;            \
   pBsonDSL_parseDocBefore_##P

#define pBsonDSL_parseDocAfter(P, _nil, Counter) \
   thisKey = _parseKeyInfos + Counter;           \
   pBsonDSL_parseDocAfter_##P

#define pBsonDSL_plusOne(...) +1

/**
 * @brief Parse each entry in the document at the given pointer.
 */
#define pBsonDSL_parseDoc(docPointer, ...)                                    \
   pBsonDSL_begin ("Parsing a document: [%s]", pBsonDSL_str (__VA_ARGS__));   \
   /* Save the dsl context */                                                 \
   bson_dsl_context_t prev_ctx = bsonDSLContext;                              \
   /* Reset the context */                                                    \
   bsonDSLContext = (bson_dsl_context_t){                                     \
      .doc = (bson_t *) (docPointer),                                         \
      .parent = &prev_ctx,                                                    \
   };                                                                         \
   struct _bson_dsl_parseDoc_keyInfo                                          \
      _parseKeyInfos[1 pBsonDSL_mapMacro (pBsonDSL_plusOne, ~, __VA_ARGS__)]; \
   struct _bson_dsl_parseDoc_keyInfo *thisKey = NULL;                         \
   pBsonDSL_mapMacro (pBsonDSL_parseDocBefore, ~, __VA_ARGS__);               \
   /* Iterate over each element of the document */                            \
   bson_iter_init (&bsonDSLContext.iter, bsonDSLContext.doc);                 \
   while (bson_iter_next (&bsonDSLContext.iter)) {                            \
      bool handledKey = false;                                                \
      const char *const currentKey = bsonDSLContext.key =                     \
         bson_iter_key (&bsonDSLContext.iter);                                \
      const uint32_t currentKeyLen = bsonDSLContext.key_len =                 \
         (int) bson_iter_key_len (&bsonDSLContext.iter);                      \
      (void) currentKeyLen;                                                   \
      bsonDSLContext.type = bson_iter_type_unsafe (&bsonDSLContext.iter);     \
      _bson_dsl_debug ("Handle key: %s", currentKey);                         \
      pBsonDSL_mapMacro (pBsonDSL_parseDocEntry, ~, __VA_ARGS__)              \
   }                                                                          \
   pBsonDSL_mapMacro (pBsonDSL_parseDocAfter, ~, __VA_ARGS__);                \
   /* Restore the dsl context */                                              \
   bsonDSLContext = prev_ctx;                                                 \
   pBsonDSL_end

/**
 * @brief Parse each entry in the document at the given pointer.
 */
#define pBsonDSL_parseDocB(docPointer, ...)                                    \
   pBsonDSL_begin ("Parsing a document: [%s]", pBsonDSL_str (__VA_ARGS__));    \
   /* Save the dsl context */                                                  \
   bson_dsl_context_t prev_ctx = bsonDSLContext;                               \
   /* Reset the context */                                                     \
   bsonDSLContext = (bson_dsl_context_t){                                      \
      .doc = (bson_t *) (docPointer),                                          \
      .parent = &prev_ctx,                                                     \
   };                                                                          \
   struct _bson_dsl_parseDoc_keyInfo _parseKeyInfos[1 pBsonDSL_mapMacroInner ( \
      pBsonDSL_plusOne, ~, __VA_ARGS__)];                                      \
   struct _bson_dsl_parseDoc_keyInfo *thisKey = NULL;                          \
   pBsonDSL_mapMacroInner (pBsonDSL_parseDocBefore, ~, __VA_ARGS__);           \
   /* Iterate over each element of the document */                             \
   bson_iter_init (&bsonDSLContext.iter, bsonDSLContext.doc);                  \
   while (bson_iter_next (&bsonDSLContext.iter)) {                             \
      const char *const currentKey = bsonDSLContext.key =                      \
         bson_iter_key (&bsonDSLContext.iter);                                 \
      const uint32_t currentKeyLen = bsonDSLContext.key_len =                  \
         (int) bson_iter_key_len (&bsonDSLContext.iter);                       \
      (void) currentKeyLen;                                                    \
      bsonDSLContext.type = bson_iter_type_unsafe (&bsonDSLContext.iter);      \
      pBsonDSL_mapMacroInner (pBsonDSL_parseDocEntry, ~, __VA_ARGS__)          \
   }                                                                           \
   pBsonDSL_mapMacroInner (pBsonDSL_parseDocAfter, ~, __VA_ARGS__);            \
   /* Restore the dsl context */                                               \
   bsonDSLContext = prev_ctx;                                                  \
   pBsonDSL_end

#define pBsonDSL_parseCurrent pBsonDSL_parseCurrentDeferred pBsonDSL_nothing
#define pBsonDSL_parseCurrentDeferred(...) \
   pBsonDSL_mapMacroInner (pBsonDSL_parseEntry, ~, __VA_ARGS__)

#define BSON_PARSE(docPointer, ...)                                    \
   pBsonDSL_begin ("Parse document [%s]", pBsonDSL_str (docPointer));  \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic push");)                 \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic ignored \"-Wshadow\"");) \
   pBsonDSL_parseDoc ((docPointer), __VA_ARGS__);                      \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic pop");)                  \
   pBsonDSL_end

#endif // BSON_BSON_DSL_H_INCLUDED
