#ifndef BSON_BSON_DSL_H_INCLUDED
#define BSON_BSON_DSL_H_INCLUDED

#include "bson.h"

// clang-format off
#define pBsonDSL_nothing
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
                    _61, _62, _63, _64, _65, _66, _67, _68, _69, _70, \
                    _71, _72, _73, _74, _75, _76, _77, _78, _79, _80, \
                    _81, _82, _83, _84, _85, _86, _87, _88, _89, _90, \
                    _91, _92, _93, _94, _95, _96, _97, _98, _99, _100, \
                    _101, _102, _103, _104, _105, _106, _107, _108, _109, _110, \
                    _111, _112, _113, _114, _115, _116, _117, _118, _119, _120, \
                    _121, _122, _123, _124, _125, _126, _127, ...) \
    _127

#define pBsonDSL_hasComma(...) \
    pBSONDSL_PICK_128th(__VA_ARGS__, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                         0, ~)

#define pBsonDSL_commaIfRHSHasParens(...) ,

#define pBsonDSL_isEmpty_CASE_0001 ,
#define pBsonDSL_isEmpty_1(_1, _2, _3, _4) \
    pBsonDSL_hasComma(pBsonDSL_paste(pBsonDSL_isEmpty_CASE_, pBsonDSL_paste4(_1, _2, _3, _4)))

#define pBsonDSL_isEmpty(...) \
    pBsonDSL_isEmpty_1(\
        pBsonDSL_hasComma(__VA_ARGS__), \
        pBsonDSL_hasComma(pBsonDSL_commaIfRHSHasParens __VA_ARGS__), \
        pBsonDSL_hasComma(__VA_ARGS__ ()), \
        pBsonDSL_hasComma(pBsonDSL_commaIfRHSHasParens __VA_ARGS__ ()))

#define pBSONDSL_IF_ELSE_PICK_1(IfTrue, IfFalse) IfTrue
#define pBSONDSL_IF_ELSE_PICK_0(IfTrue, IfFalse) IfFalse

#define pBsonDSL_eval_1(...) __VA_ARGS__
#define pBsonDSL_eval_2(...) pBsonDSL_eval_1(pBsonDSL_eval_1(__VA_ARGS__))
#define pBsonDSL_eval_4(...) pBsonDSL_eval_2(pBsonDSL_eval_2(__VA_ARGS__))
#define pBsonDSL_eval_8(...) pBsonDSL_eval_4(pBsonDSL_eval_4(__VA_ARGS__))
#define pBsonDSL_eval_16(...) pBsonDSL_eval_8(pBsonDSL_eval_8(__VA_ARGS__))
#define pBsonDSL_eval(...) pBsonDSL_eval_16(pBsonDSL_eval_16(pBsonDSL_eval_16(pBsonDSL_eval_16(pBsonDSL_eval_16(__VA_ARGS__)))))

#define pBsonDSL_ifElse(Cond, IfTrue, IfFalse) \
    pBsonDSL_paste(pBSONDSL_IF_ELSE_PICK_, Cond)(IfTrue, IfFalse)

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

#define pBsonDSL_begin(Comment) do { ((void)(Comment))
#define pBsonDSL_end } while (0)

// clang-format on

#define pBsonDSL_append(Kind, ...) \
   bson_append_##Kind (            \
      _bson_dsl_doc_, _bson_dsl_key_, _bson_dsl_key_len_, __VA_ARGS__)

#define pBsonDSL_appendValue_doc \
   pBsonDSL_documentAppend_deferredDoc pBsonDSL_nothing

#define pBsonDSL_documentAppend_deferredDoc(...)                         \
   pBsonDSL_begin ("Append a new document");                             \
   bson_t _doc_ = BSON_INITIALIZER;                                      \
   pBsonDSL_append (document_begin, &_doc_);                             \
   bson_t *prev_doc = _bson_dsl_doc_;                                    \
   _bson_dsl_doc_ = &_doc_;                                              \
   pBsonDSL_mapMacroInner (pBsonDSL_documentAppend, Depth, __VA_ARGS__); \
   _bson_dsl_doc_ = prev_doc;                                            \
   bson_append_document_end (prev_doc, &_doc_);                          \
   pBsonDSL_end

#define pBsonDSL_appendValue_array \
   pBsonDSL_documentAppend_deferredArray pBsonDSL_nothing

#define pBsonDSL_documentAppend_deferredArray(...)                \
   pBsonDSL_begin ("Append an array() value");                    \
   bson_t _array_ = BSON_INITIALIZER;                             \
   pBsonDSL_append (array_begin, &_array_);                       \
   bson_t *prev_doc = _bson_dsl_doc_;                             \
   _bson_dsl_doc_ = &_array_;                                     \
   pBsonDSL_mapMacroInner (pBsonDSL_arrayAppend, ~, __VA_ARGS__); \
   _bson_dsl_doc_ = prev_doc;                                     \
   bson_append_array_end (_bson_dsl_doc_, &_array_);              \
   pBsonDSL_end

#define pBsonDSL_appendValue_cstr(String) \
   pBsonDSL_append (utf8, String, strlen (String));

#define pBsonDSL_appendValue_utf8_w_len(String, Len) \
   pBsonDSL_append (utf8, (String), (Len))

#define pBsonDSL_appendValue_i32(Integer) pBsonDSL_append (int32, (Integer))

#define pBsonDSL_appendValue_i64(Integer) pBsonDSL_append (int64, (Integer))

#define pBsonDSL_appendValue_iter(Iter) pBsonDSL_append (iter, (Iter))

#define pBsonDSL_documentAppend_kvl(String, Len, Element)    \
   pBsonDSL_begin ("Append a key-value pair to a document"); \
   _bson_dsl_key_ = (String);                                \
   _bson_dsl_key_len_ = (Len);                               \
   pBsonDSL_appendValue_##Element;                           \
   pBsonDSL_end

#define pBsonDSL_documentAppend_kv(String, Element) \
   pBsonDSL_documentAppend_kvl ((String), strlen ((String)), Element)

#define pBsonDSL_documentAppend_insert(OtherDoc)                \
   pBsonDSL_begin ("Insert a document");                        \
   bson_iter_t _iter_;                                          \
   bson_t *_merge_doc_ = (OtherDoc);                            \
   bson_iter_init (&_iter_, _merge_doc_);                       \
   while (bson_iter_next (&_iter_)) {                           \
      pBsonDSL_documentAppend_kvl (bson_iter_key (&_iter_),     \
                                   bson_iter_key_len (&_iter_), \
                                   iter (&_iter_));             \
   }                                                            \
   pBsonDSL_end

#define pBsonDSL_documentAppend(Pair, _nil, _count) \
   pBsonDSL_documentAppend_##Pair;

#define pBsonDSL_arrayAppend_i32 pBsonDSL_appendValue_i32
#define pBsonDSL_arrayAppend_i64 pBsonDSL_appendValue_i64
#define pBsonDSL_arrayAppend_cstr pBsonDSL_appendValue_cstr
#define pBsonDSL_arrayAppend_utf8_w_len pBsonDSL_appendValue_utf8_w_len
#define pBsonDSL_arrayAppend_doc pBsonDSL_appendValue_doc
#define pBsonDSL_arrayAppend_array pBsonDSL_appendValue_array
#define pBsonDSL_arrayAppend_iter pBsonDSL_appendValue_iter

#define pBsonDSL_setKeyToArrayIndex(Idx)                                   \
   _bson_dsl_key_len_ = sprintf (_bson_dsl_strbuf_, "%d", _dsl_array_idx); \
   _bson_dsl_key_ = _bson_dsl_strbuf_

#define pBsonDSL_arrayAppend_insert(Other)                                   \
   pBsonDSL_begin ("Insert from another array");                             \
   bson_iter_t iter;                                                         \
   bson_iter_init (&iter, (Other));                                          \
   while (bson_iter_next (&iter)) {                                          \
      pBsonDSL_setKeyToArrayIndex (_dsl_array_idx);                          \
      pBsonDSL_arrayAppend_iter (&iter);                                     \
      ++_dsl_array_idx;                                                      \
   }                                                                         \
   /* Decrement once to counter the outer increment that will be applied: */ \
   --_dsl_array_idx;                                                         \
   pBsonDSL_end

#define pBsonDSL_arrayAppend(Element, _nil, Counter) \
   pBsonDSL_begin ("Append an array element");       \
   pBsonDSL_setKeyToArrayIndex (_dsl_array_idx);     \
   pBsonDSL_arrayAppend_##Element;                   \
   ++_dsl_array_idx;                                 \
   pBsonDSL_end;

#define pBsonDSL_documentBuild(...) \
   pBsonDSL_mapMacro (pBsonDSL_documentAppend, ~, __VA_ARGS__)

/**
 * @brief Build a BSON document by appending to an existing bson_t document
 *
 * @param Pointer The document upon which to append
 * @param ... The Document elements to append to the document
 */
#define BSON_BUILD_APPEND(Pointer, ...)                                \
   pBsonDSL_begin ("Append to the given document");                    \
   const char *_bson_dsl_key_ = NULL;                                  \
   int _bson_dsl_key_len_ = 0;                                         \
   int _dsl_array_idx = 0;                                             \
   (void) _bson_dsl_key_;                                              \
   (void) _bson_dsl_key_len_;                                          \
   (void) _dsl_array_idx;                                              \
   char _bson_dsl_strbuf_[32];                                         \
   (void) _bson_dsl_strbuf_;                                           \
   bson_t *_bson_dsl_doc_ = (Pointer);                                 \
   (void) _bson_dsl_doc_;                                              \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic push");)                 \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic ignored \"-Wshadow\"");) \
   pBsonDSL_documentBuild (__VA_ARGS__);                               \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic pop");)                  \
   pBsonDSL_end

/**
 * @brief Build a new BSON document and assign the value into the given
 * pointer. The pointed-to document is destroyed before being assigned, but
 * after the document is built.
 */
#define BSON_BUILD(PointerDest, ...)                    \
   pBsonDSL_begin ("Build a new document");             \
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

#endif // BSON_BSON_DSL_H_INCLUDED
