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

/**
 * @brief Parse the given BSON document.
 *
 * @param doc A bson_t object to walk. (Not a pointer)
 */
#define bsonParse(Document, ...)                                       \
   _bsonDSL_begin ("bsonParse(%s)", _bsonDSL_str (Document));          \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic push");)                 \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic ignored \"-Wshadow\"");) \
   bool _bvHalt = false;                                               \
   const bool _bvContinue = false;                                     \
   const bool _bvBreak = false;                                        \
   _bsonDSL_eval (_bsonParse ((Document), __VA_ARGS__));               \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic pop");)                  \
   _bsonDSL_end

/**
 * @brief Visit each element of a BSON document
 */
#define bsonVisitEach(Document, ...)                                   \
   _bsonDSL_begin ("bsonVisitEach(%s)", _bsonDSL_str (Document));      \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic push");)                 \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic ignored \"-Wshadow\"");) \
   bool _bvHalt = false;                                               \
   _bsonDSL_eval (_bsonVisitEach ((Document), __VA_ARGS__));           \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic pop");)                  \
   _bsonDSL_end

BSON_EXPORT (char *)
bsonParse_createPathString ();

#define bsonBuildContext (**_bsonBuildContextPtr ())
#define bsonVisitContext (**_bsonVisitContextPtr ())

/// An iterator pointing to the current BSON value being visited.
#define bsonVisitIter (bsonVisitContext.iter)

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

/**
 * The _bsonBuild_doc_XYZ macros handle the top-level bsonBuild() items, and
 * any nested doc() items, with XYZ being the doc-building subcommand.
 */
#define _bsonBuild_doc(Command, _ignore, _count) _bsonBuild_doc_##Command;

/// key-value pair with explicit key length
#define _bsonBuild_doc_kvl(String, Len, Element)                      \
   _bsonDSL_begin ("\"%s\" => [%s]", String, _bsonDSL_str (Element)); \
   _bbCtx.key = (String);                                             \
   _bbCtx.key_len = (Len);                                            \
   _bsonBuild_appendValue (Element);                                  \
   _bsonDSL_end

#define _bsonBuild_appendValue(P) _bsonBuild_appendValue_##P

/// Key-value pair with a C-string
#define _bsonBuild_doc_kv(String, Element) \
   _bsonBuild_doc_kvl ((String), strlen ((String)), Element)

/// We must defer expansion of the nested doc() to allow "recursive" evaluation
#define _bsonBuild_appendValue_doc \
   _bsonBuild_appendValueDeferred_doc _bsonDSL_nothing ()
#define _bsonBuild_array_doc _bsonBuild_appendValue_doc

#define _bsonBuild_appendValueDeferred_doc(...)                     \
   _bsonDSL_begin ("doc(%s)", _bsonDSL_str (__VA_ARGS__));          \
   /* Write to this variable as the child: */                       \
   bson_t _bbChildDoc = BSON_INITIALIZER;                           \
   bson_append_document_begin (_bsonBuildAppendArgs, &_bbChildDoc); \
   _bsonBuildAppend (_bbChildDoc, __VA_ARGS__);                     \
   bson_append_document_end (bsonBuildContext.doc, &_bbChildDoc);   \
   _bsonDSL_end

/// We must defer expansion of the nested array() to allow "recursive"
/// evaluation
#define _bsonBuild_appendValue_array \
   _bsonBuild_appendValueDeferred_array _bsonDSL_nothing ()
#define _bsonBuild_array_array _bsonBuild_appendValue_array

#define _bsonBuild_appendValueDeferred_array(...)             \
   _bsonDSL_begin ("array(%s)", _bsonDSL_str (__VA_ARGS__));  \
   /* Write to this variable as the child array: */           \
   bson_t _bbArray = BSON_INITIALIZER;                        \
   bson_append_array_begin (_bsonBuildAppendArgs, &_bbArray); \
   _bsonBuildArray (_bbArray, __VA_ARGS__);                   \
   bson_append_array_end (bsonBuildContext.doc, &_bbArray);   \
   _bsonDSL_end

/// Append a UTF-8 string with an explicit length
#define _bsonBuild_appendValue_utf8_w_len(String, Len) \
   bson_append_utf8 (_bsonBuildAppendArgs, (String), (Len))
#define _bsonBuild_array_utf8_w_len _bsonBuild_appendValue_utf8_w_len

/// Append a "cstr" as UTF-8
#define _bsonBuild_appendValue_cstr(String) \
   _bsonBuild_appendValue_utf8_w_len ((String), strlen (String))
#define _bsonBuild_array_cstr _bsonBuild_appendValue_cstr

/// Append an int32
#define _bsonBuild_appendValue_i32(Integer) \
   bson_append_int32 (_bsonBuildAppendArgs, (Integer))
#define _bsonBuild_array_i32 _bsonBuild_appendValue_i32

/// Append an int64
#define _bsonBuild_appendValue_i64(Integer) \
   bson_append_int64 (_bsonBuildAppendArgs, (Integer))
#define _bsonBuild_array_i64 _bsonBuild_appendValue_i64

/// Append the value referenced by a given iterator
#define _bsonBuild_appendValue_iter(Iter) \
   bson_append_iter (_bsonBuildAppendArgs, &(Iter))
#define _bsonBuild_array_iter _bsonBuild_appendValue_iter

/// Append the BSON document referenced by the given pointer
#define _bsonBuild_appendValue_bson(Doc) \
   bson_append_document (_bsonBuildAppendArgs, &(Doc))
#define _bsonBuild_array_bson _bsonBuild_appendValue_bson

/// Append the BSON document referenced by the given pointer as an array
#define _bsonBuild_appendValue_bsonArray(Arr) \
   bson_append_array (_bsonBuildAppendArgs, &(Arr))
#define _bsonBuild_array_bsonArray _bsonBuild_appendValue_bsonArray

#define _bsonBuild_appendValue_bool(b) \
   bson_append_bool (_bsonBuildAppendArgs, (b))
#define _bsonBuild_array_bool _bsonBuild_appendValue_bool
#define _bsonBuild_appendValue__Bool(b) \
   bson_append_bool (_bsonBuildAppendArgs, (b))
#define _bsonBuild_array__Bool _bsonBuild_appendValue_bool

#define _bsonBuild_appendValue_null bson_append_null (_bsonBuildAppendArgs)
#define _bsonBuild_array_null _bsonBuild_appendValue_null

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
#define _bsonBuild_doc_insert(OtherBSON, ...)                                \
   _bsonDSL_begin ("Insert other document: [%s]", _bsonDSL_str (OtherBSON)); \
   bson_iter_t _bbDocInsertIter;                                             \
   bson_iter_init (&_bbDocInsertIter, &(OtherBSON));                         \
   while (bson_iter_next (&_bbDocInsertIter)) {                              \
      bool _filter_drop_ = false;                                            \
      _bsonDSL_mapMacro (_bsonDSL_insertFilter, ~, __VA_ARGS__);             \
      if (_filter_drop_) {                                                   \
         continue;                                                           \
      }                                                                      \
      _bsonBuild_doc_kvl (bson_iter_key (&_bbDocInsertIter),                 \
                          bson_iter_key_len (&_bbDocInsertIter),             \
                          iter (_bbDocInsertIter));                          \
   }                                                                         \
   _bsonDSL_end

/// Insert the given BSON document into the parent array. Keys of the given
/// document are discarded and it is treated as an array of values.
#define _bsonBuild_array_insert(OtherArr)                                    \
   _bsonDSL_begin ("Insert other array: [%s]", _bsonDSL_str (OtherArr));     \
   bson_iter_t iter;                                                         \
   bson_iter_init (&iter, (bson_t *) (OtherArr));                            \
   while (bson_iter_next (&iter)) {                                          \
      _bsonBuild_setKeyToArrayIndex (bsonBuildContext.index);                \
      _bsonBuild_array_iter (iter);                                          \
      ++_bbCtx.index;                                                        \
   }                                                                         \
   /* Decrement once to counter the outer increment that will be applied: */ \
   --_bbCtx.index;                                                           \
   _bsonDSL_end

#define _bsonBuild_docIfThen_then \
   _bsonBuildAppendWithCurrentContext _bsonDSL_nothing ()
#define _bsonBuild_docIfElse_else \
   _bsonBuildAppendWithCurrentContext _bsonDSL_nothing ()

#define _bsonBuild_docIfThenElse(Condition, Then, Else)                 \
   if ((Condition)) {                                                   \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then));  \
      _bsonBuild_docIfThen_##Then;                                      \
   } else {                                                             \
      _bsonDSLDebug ("Taking FALSE branch: [%s]", _bsonDSL_str (Else)); \
      _bsonBuild_docIfElse_##Else;                                      \
   }

#define _bsonBuild_docIfThen(Condition, Then)                          \
   if ((Condition)) {                                                  \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then)); \
      _bsonBuild_docIfThen_##Then;                                     \
   }

#define _bsonBuild_doc_if(Condition, ...)                                   \
   _bsonDSL_begin ("Conditional append on [%s]", _bsonDSL_str (Condition)); \
   /* Pick a sub-macro depending on if there are one or two args */         \
   _bsonDSL_ifElse (_bsonDSL_hasComma (__VA_ARGS__),                        \
                    _bsonBuild_docIfThenElse,                               \
                    _bsonBuild_docIfThen) (Condition, __VA_ARGS__);         \
   _bsonDSL_end

#define _bsonBuild_arrayIfThen_then _bsonBuildArrayWithCurrentContext
#define _bsonBuild_arrayIfElse_else _bsonBuildArrayWithCurrentContext

#define _bsonBuild_arrayIfThenElse(Condition, Then, Else)               \
   if ((Condition)) {                                                   \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then));  \
      _bsonBuild_arrayIfThen_##Then;                                    \
   } else {                                                             \
      _bsonDSLDebug ("Taking FALSE branch: [%s]", _bsonDSL_str (Else)); \
      _bsonBuild_arrayIfElse_##Else;                                    \
   }

#define _bsonBuild_arrayIfThen(Condition, Then)                        \
   if ((Condition)) {                                                  \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then)); \
      _bsonBuild_arrayIfThen_##Then;                                   \
   }

#define _bsonBuild_array_if(Condition, ...)                                \
   _bsonDSL_begin ("Conditional value on [%s]", _bsonDSL_str (Condition)); \
   /* Pick a sub-macro depending on if there are one or two args */        \
   _bsonDSL_ifElse (_bsonDSL_hasComma (__VA_ARGS__),                       \
                    _bsonBuild_arrayIfThenElse,                            \
                    _bsonBuild_arrayIfThen) (Condition, __VA_ARGS__);      \
   _bsonDSL_end

#define _bsonBuild_appendValueIf_then(X) _bsonBuild_appendValue_##X
#define _bsonBuild_appendValueIf_else(X) _bsonBuild_appendValue_##X

#define _bsonBuild_appendValue_if(Condition, Then, Else)                \
   if ((Condition)) {                                                   \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then));  \
      _bsonBuild_appendValueIf_##Then;                                  \
   } else {                                                             \
      _bsonDSLDebug ("Taking FALSE branch: [%s]", _bsonDSL_str (Else)); \
      _bsonBuild_appendValueIf_##Else;                                  \
   }

#define _bsonBuild_setKeyToArrayIndex(Idx)                         \
   _bbCtx.key_len = sprintf (_bbCtx.strbuf64, "%d", _bbCtx.index); \
   _bbCtx.key = _bbCtx.strbuf64

/// Handle an element of array()
#define _bsonDSL_array(Element, _nil, _count)                          \
   _bsonDSL_begin (                                                    \
      "[%d] => [%s]", bsonBuildContext.index, _bsonDSL_str (Element)); \
   /* Set the doc key to the array index as a string: */               \
   _bsonBuild_setKeyToArrayIndex (bsonBuildContext.index);             \
   /* Append a value: */                                               \
   _bsonBuild_array_##Element;                                         \
   /* Increment the array index: */                                    \
   ++_bbCtx.index;                                                     \
   _bsonDSL_end;

#define _bsonBuildAppendWithCurrentContext(...) \
   _bsonDSL_mapMacro (_bsonBuild_doc, ~, __VA_ARGS__)

#define _bsonBuildArrayWithCurrentContext(...) \
   _bsonDSL_mapMacro (_bsonDSL_array, ~, __VA_ARGS__)

#define _bsonDSL_iterType_array BSON_TYPE_ARRAY
#define _bsonDSL_iterType_bool BSON_TYPE_BOOL
#define _bsonDSL_iterType__Bool BSON_TYPE_BOOL
#define _bsonDSL_iterType_doc BSON_TYPE_DOCUMENT

#define _bsonVisitOp_ifType(T, ...)                      \
   if (bson_iter_type_unsafe (&bsonVisitContext.iter) == \
       _bsonDSL_iterType_##T) {                          \
      _bsonVisit_applyOps (__VA_ARGS__);                 \
   }

#define _bsonVisitOp_halt _bvHalt = true

#define _bsonVisitOp_ifTruthy(...)                              \
   _bsonDSL_begin ("ifTruthy(%s)", _bsonDSL_str (__VA_ARGS__)); \
   if (bson_iter_as_bool (&bsonVisitContext.iter)) {            \
      _bsonVisit_applyOps (__VA_ARGS__);                        \
   }                                                            \
   _bsonDSL_end;

#define _bsonVisitOp_ifStrEqual(S, ...)                                    \
   _bsonDSL_begin ("ifStrEqual(%s)", _bsonDSL_str (S));                    \
   if (bson_iter_type_unsafe (&bsonVisitContext.iter) == BSON_TYPE_UTF8) { \
      uint32_t len;                                                        \
      const char *str = bson_iter_utf8 (&bsonVisitContext.iter, &len);     \
      if (len == strlen (S) && (0 == memcmp (str, S, len))) {              \
         _bsonVisit_applyOps (__VA_ARGS__);                                \
      }                                                                    \
   }                                                                       \
   _bsonDSL_end

#define _bsonVisitOp_storeBool(Dest)                       \
   _bsonDSL_begin ("storeBool(%s)", _bsonDSL_str (Dest));  \
   if (BSON_ITER_HOLDS_BOOL (&bsonVisitContext.iter)) {    \
      (Dest) = bson_iter_as_bool (&bsonVisitContext.iter); \
   }                                                       \
   _bsonDSL_end

#define _bsonVisitOp_found(IterDest)                      \
   _bsonDSL_begin ("found(%s)", _bsonDSL_str (IterDest)); \
   (IterDest) = bsonVisitContext.iter;                    \
   _bsonDSL_end

#define _bsonVisitOp_do(...)                                  \
   _bsonDSL_begin ("do: { %s }", _bsonDSL_str (__VA_ARGS__)); \
   do {                                                       \
      __VA_ARGS__;                                            \
   } while (0);                                               \
   _bsonDSL_end

#define _bsonVisitOp_append \
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
      while (bson_iter_next (&_bpCtx.iter) && !_bvHalt && !_bvBreak) {      \
         _bvContinue = false;                                               \
         _bsonVisit_applyOps (__VA_ARGS__);                                 \
      }                                                                     \
      /* Restore the dsl context */                                         \
      *_bsonVisitContextPtr () = bsonVisitContext.parent;                   \
   } while (0);                                                             \
   _bsonDSL_end

#define _bsonVisitOp_visitEach \
   _bsonVisitOp_visitEachDeferred _bsonDSL_nothing ()
#define _bsonVisitOp_visitEachDeferred(...)                                    \
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

#define _bsonVisitOp_nop ((void) 0)
#define _bsonVisitOp_parse(...)                                         \
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
      _bsonParse (inner, __VA_ARGS__);                                  \
   } while (0);

#define _bsonVisitOp_setTrue(B)                             \
   _bsonDSL_begin ("Set [%s] to 'true'", _bsonDSL_str (B)); \
   (B) = true;                                              \
   _bsonDSL_end;

#define _bsonVisitOp_setFalse(B)                             \
   _bsonDSL_begin ("Set [%s] to 'false'", _bsonDSL_str (B)); \
   (B) = false;                                              \
   _bsonDSL_end;

#define _bsonVisitOp_continue _bvContinue = true
#define _bsonVisitOp_break _bvBreak = _bvContinue = true
#define _bsonVisitOp_require(Cond) \
   if (!(Cond)) {                  \
      _bsonVisitOp_halt;           \
   } else                          \
      ((void) 0)

#define _bsonVisit_applyOp(P, _const, _count) \
   do {                                       \
      if (!_bvContinue && !_bvHalt) {         \
         _bsonVisitOp_##P;                    \
      }                                       \
   } while (0);

/*
########     ###    ########   ######  ########
##     ##   ## ##   ##     ## ##    ## ##
##     ##  ##   ##  ##     ## ##       ##
########  ##     ## ########   ######  ######
##        ######### ##   ##         ## ##
##        ##     ## ##    ##  ##    ## ##
##        ##     ## ##     ##  ######  ########
*/

#define _bsonParse(Doc, ...)                              \
   do {                                                   \
      /* Reset the context */                             \
      struct _bsonVisitContext_t _bpCtx = {               \
         .doc = &(Doc),                                   \
         .parent = &bsonVisitContext,                     \
      };                                                  \
      *_bsonVisitContextPtr () = &_bpCtx;                 \
      bool _bpFoundElement = false;                       \
      _bsonParse_applyOps (__VA_ARGS__);                  \
      /* Restore the dsl context */                       \
      *_bsonVisitContextPtr () = bsonVisitContext.parent; \
   } while (0)

#define _bsonParse_applyOps(...) \
   _bsonDSL_mapMacro (_bsonParse_applyOp, ~, __VA_ARGS__)

/// Parse one entry referrenced by the context iterator
#define _bsonParse_applyOp(P, _nil, Counter) \
   do {                                      \
      if (!_bvHalt) {                        \
         _bsonParseOp_##P;                   \
      }                                      \
   } while (0);


/*
 .d888 d8b               888
d88P"  Y8P               888
888                      888
888888 888 88888b.   .d88888
888    888 888 "88b d88" 888
888    888 888  888 888  888
888    888 888  888 Y88b 888
888    888 888  888  "Y88888
*/

#define _bsonParseOp_find(Predicate, ...)               \
   _bsonDSL_begin ("find(%s)", _bsonDSL_str (Key));     \
   _bpFoundElement = false;                             \
   bson_iter_init (&_bpCtx.iter, bsonVisitContext.doc); \
   while (bson_iter_next (&_bpCtx.iter)) {              \
      if (_bsonParse_findPredicate_##Predicate) {       \
         _bsonVisit_applyOps (__VA_ARGS__);             \
         _bpFoundElement = true;                        \
         break;                                         \
      }                                                 \
   }                                                    \
   _bsonDSL_end

#define _bsonParse_findPredicate_key(Key) \
   (0 == strcmp (bson_iter_key (&bsonVisitIter), (Key)))

#define _bsonParse_findPredicate_type(Type) \
   (bson_iter_type (&bsonVisitIter) == _bsonDSL_iterType_##Type)

#define _bsonParse_findPredicate_keyWithType(Key, Type) \
   (_bsonParse_findPredicate_allOf (key (Key), type (Type)))

#define _bsonParse_findPredicate_allOf(...) \
   (1 _bsonDSL_mapMacro (_bsonParse_findPredicateAnd, ~, __VA_ARGS__))

#define _bsonParse_findPredicateAnd(Pred, _ignore, _ignore1) \
   &&(_bsonParse_findPredicate_##Pred)

#define _bsonParse_findPredicate_anyOf(...) \
   (0 _bsonDSL_mapMacro (_bsonParse_findPredicateOr, ~, __VA_ARGS__))

#define _bsonParse_findPredicateOr(Pred, _ignore, _ignore2) \
   || (_bsonParse_findPredicate_##Pred)

/// Util find()s:

#define _bsonParseOp_findKey(Key, ...) \
   _bsonParseOp_find (key (Key), __VA_ARGS__)

#define _bsonParseOp_findKeyWithType(Key, Type, ...) \
   _bsonParseOp_find (keyWithType ((Key), Type))

/*
         888
         888
         888
 .d88b.  888 .d8888b   .d88b.
d8P  Y8b 888 88K      d8P  Y8b
88888888 888 "Y8888b. 88888888
Y8b.     888      X88 Y8b.
 "Y8888  888  88888P'  "Y8888
*/

#define _bsonParseOp_else _bsonParse_deferredElse _bsonDSL_nothing ()
#define _bsonParse_deferredElse(...)                           \
   if (!_bpFoundElement) {                                     \
      _bsonDSL_begin ("else(%s)", _bsonDSL_str (__VA_ARGS__)); \
      _bsonParse_applyOps (__VA_ARGS__);                       \
      _bsonDSL_end;                                            \
   } else                                                      \
      ((void) 0)

/*
     888
     888
     888
 .d88888  .d88b.
d88" 888 d88""88b
888  888 888  888
Y88b 888 Y88..88P
 "Y88888  "Y88P"
*/

#define _bsonParseOp_do(...)                              \
   _bsonDSL_begin ("do(%s)", _bsonDSL_str (__VA_ARGS__)); \
   __VA_ARGS__;                                           \
   _bsonDSL_end

/*
d8b  .d888
Y8P d88P"
    888
888 888888
888 888
888 888
888 888
888 888
*/
/// Perform conditional parsing
#define _bsonParseOp_if(Condition, ...)                             \
   _bsonDSL_begin ("parse(if(%s))", _bsonDSL_str (Condition));      \
   /* Pick a sub-macro depending on if there are one or two args */ \
   _bsonDSL_ifElse (_bsonDSL_hasComma (__VA_ARGS__),                \
                    _bsonParse_ifThenElse,                          \
                    _bsonParse_ifThen) (Condition, __VA_ARGS__);    \
   _bsonDSL_end

#define _bsonParse_ifThen_then _bsonParse_applyOps
#define _bsonParse_ifElse_else _bsonParse_applyOps

#define _bsonParse_ifThenElse(Condition, Then, Else)                    \
   if ((Condition)) {                                                   \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then));  \
      _bsonParse_ifThen_##Then;                                         \
   } else {                                                             \
      _bsonDSLDebug ("Taking FALSE branch: [%s]", _bsonDSL_str (Else)); \
      _bsonParse_ifElse_##Else;                                         \
   }

#define _bsonParse_ifThen(Condition, Then)                             \
   if ((Condition)) {                                                  \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then)); \
      _bsonParse_ifThen_##Then;                                        \
   }

#define _bsonParseOp_append _bsonBuildAppend

#define _bsonVisit_applyOps _bsonVisit_applyOpsDeferred _bsonDSL_nothing ()
#define _bsonVisit_applyOpsDeferred(...)                     \
   _bsonDSL_begin ("Walk [%s]", _bsonDSL_str (__VA_ARGS__)); \
   _bsonDSL_mapMacro (_bsonVisit_applyOp, ~, __VA_ARGS__);   \
   _bsonDSL_end;

#define bsonBuildArray(BSON, ...)                       \
   _bsonDSL_begin ("bsonBuildArray(%s, %s)",            \
                   _bsonDSL_str (BSON),                 \
                   _bsonDSL_str (__VA_ARGS__));         \
   _bsonDSL_eval (_bsonBuildArray (BSON, __VA_ARGS__)); \
   _bsonDSL_end

#define _bsonBuildArray(BSON, ...)                                        \
   do {                                                                   \
      BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic push");)                 \
      BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic ignored \"-Wshadow\"");) \
      _bsonBuildContext_t _bbCtx = {                                      \
         .doc = &(BSON),                                                  \
         .parent = *_bsonBuildContextPtr (),                              \
      };                                                                  \
      *_bsonBuildContextPtr () = &_bbCtx;                                 \
      _bsonBuildArrayWithCurrentContext (__VA_ARGS__);                    \
      *_bsonBuildContextPtr () = _bbCtx.parent;                           \
   } while (0)

/**
 * @brief Build a BSON document by appending to an existing bson_t document
 *
 * @param Pointer The document upon which to append
 * @param ... The Document elements to append to the document
 */
#define bsonBuildAppend(BSON, ...) \
   _bsonDSL_eval (_bsonBuildAppend (BSON, __VA_ARGS__))
#define _bsonBuildAppend(BSON, ...)                                    \
   _bsonDSL_begin ("Appending to document '%s'", _bsonDSL_str (BSON)); \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic push");)                 \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic ignored \"-Wshadow\"");) \
   /* Save the dsl context */                                          \
   _bsonBuildContext_t _bbCtx = (_bsonBuildContext_t){                 \
      .doc = &(BSON),                                                  \
      .parent = *_bsonBuildContextPtr (),                              \
   };                                                                  \
   *_bsonBuildContextPtr () = &_bbCtx;                                 \
   /* Reset the context */                                             \
   _bsonBuildAppendWithCurrentContext (__VA_ARGS__);                   \
   /* Restore the dsl context */                                       \
   *_bsonBuildContextPtr () = _bbCtx.parent;                           \
   BSON_IF_GNU_LIKE (_Pragma ("GCC diagnostic pop");)                  \
   _bsonDSL_end

/**
 * @brief Build a new BSON document and assign the value into the given
 * pointer. The pointed-to document is destroyed before being assigned, but
 * after the document is built.
 */
#define bsonBuild(BSON, ...)                                              \
   _bsonDSL_begin ("Build a new document for '%s'", _bsonDSL_str (BSON)); \
   bson_t _bson_dsl_new_doc_ = BSON_INITIALIZER;                          \
   bsonBuildAppend (_bson_dsl_new_doc_, __VA_ARGS__);                     \
   bson_destroy (&(BSON));                                                \
   bson_steal (&(BSON), &_bson_dsl_new_doc_);                             \
   _bsonDSL_end

/**
 * @brief Declare a variable and build it with the BSON DSL @see bsonBuild
 */
#define bsonBuildDecl(Variable, ...)   \
   bson_t Variable = BSON_INITIALIZER; \
   bsonBuild (Variable, __VA_ARGS__)


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

/// Convert the given argument into a string without inhibitting macro expansion
#define _bsonDSL_str(...) _bsonDSL_str_1 (__VA_ARGS__)
// Empty quotes "" are to ensure a string appears. Old MSVC has a bug
// where empty #__VA_ARGS__ just vanishes.
#define _bsonDSL_str_1(...) "" #__VA_ARGS__

/// Paste two tokens:
#define _bsonDSL_paste(a, ...) _bsonDSL_paste_impl (a, __VA_ARGS__)
#define _bsonDSL_paste_impl(a, ...) a##__VA_ARGS__

/// Paste three tokens:
#define _bsonDSL_paste3(a, b, c) _bsonDSL_paste (a, _bsonDSL_paste (b, c))
/// Paste four tokens:
#define _bsonDSL_paste4(a, b, c, d) \
   _bsonDSL_paste (a, _bsonDSL_paste3 (b, c, d))

// clang-format off

/// Now we need a MAP() macro. This idiom is common, but fairly opaque. Below is
/// some crazy preprocessor trickery to implement it. Fortunately, once we have
/// MAP(), the remainder of this file is straightforward. This implementation
/// isn't the simplest one possible, but is one that supports the old
/// non-compliant MSVC preprocessor.

/* Expands to nothing. Used to defer a function-like macro and to ignore arguments */
#define _bsonDSL_nothing(...)

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
 * @brief Expands to 1 if the given arguments contain any top-level commas, zero otherwise.
 *
 * There is an expansion of __VA_ARGS__, followed by 62 '1' arguments, followed
 * by single '0'. If __VA_ARGS__ contains no commas, pick64th() will return the
 * single zero. If __VA_ARGS__ contains any top-level commas, the series of ones
 * will shift to the right and pick64th will return one of those ones. (This only
 * works __VA_ARGS__ contains fewer than 62 commas, which is a somewhat reasonable
 * limit.) The _bsonDSL_nothing() is a workaround for MSVC's bad preprocessor that
 * expands __VA_ARGS__ incorrectly.
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
 * Expands to a single comma if "invoked" as a function-like macro.
 * (This will make sense, I promise.)
 */
#define _bsonDSL_commaIfRHSHasParens(...) ,

/**
 * @brief Expand to 1 if given no arguments, otherwise 0.
 *
 * This could be done much more simply using __VA_OPT__, but we need to work on
 * older compilers.
 */
#define _bsonDSL_isEmpty(...) \
    _bsonDSL_isEmpty_1(\
        /* Expands to '1' if __VA_ARGS__ contains any top-level commas */ \
        _bsonDSL_hasComma(__VA_ARGS__), \
        /* Expands to '1' if __VA_ARGS__ begins with a parenthesis, because \
         * that will cause an "invocation" of _bsonDSL_commaIfRHSHasParens, \
         * which immediately expands to a single comma. */ \
        _bsonDSL_hasComma(_bsonDSL_commaIfRHSHasParens __VA_ARGS__), \
        /* Expands to '1' if __VA_ARGS__ expands to a function-like macro name \
         * that then expands to anything containing a top-level comma */ \
        _bsonDSL_hasComma(__VA_ARGS__ ()), \
        /* Expands to '1' if __VA_ARGS__ expands to nothing. */ \
        _bsonDSL_hasComma(_bsonDSL_commaIfRHSHasParens __VA_ARGS__ ()))

/**
 * A helper for isEmpty(): If given (0, 0, 0, 1), expands as:
 *    - first: _bsonDSL_hasComma(_bsonDSL_isEmpty_CASE_0001)
 *    -  then: _bsonDSL_hasComma(,)
 *    -  then: 1
 * Given any other aruments:
 *    - first: _bsonDSL_hasComma(_bsonDSL_isEmpty_CASE_<somethingelse>)
 *    -  then: 0
 */
#define _bsonDSL_isEmpty_1(_1, _2, _3, _4) \
    _bsonDSL_hasComma(_bsonDSL_paste(_bsonDSL_isEmpty_CASE_, _bsonDSL_paste4(_1, _2, _3, _4)))
#define _bsonDSL_isEmpty_CASE_0001 ,

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
// Each level of "eval" applies double the expansions of the previous level.
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
 * __VA_OPT__ this would be simpler.
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


#endif // BSON_BSON_DSL_H_INCLUDED
