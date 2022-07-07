#ifndef BSON_BSON_DSL_H_INCLUDED
#define BSON_BSON_DSL_H_INCLUDED

// clang-format off
/**
 * @file bson-dsl.h
 * @brief Define a C-preprocessor DSL for working with BSON objects
 *
 * This file defines an embedded DSL for working with BSON objects consisely and
 * correctly.

Each macro takes a set of arguments, often being invocations to other DSL
meta-macros. The DSL is split into *building* expressions and
_visiting_/_parsing_ expressions.

########  ##     ## #### ##       ########  #### ##    ##  ######
##     ## ##     ##  ##  ##       ##     ##  ##  ###   ## ##    ##
##     ## ##     ##  ##  ##       ##     ##  ##  ####  ## ##
########  ##     ##  ##  ##       ##     ##  ##  ## ## ## ##   ####
##     ## ##     ##  ##  ##       ##     ##  ##  ##  #### ##    ##
##     ## ##     ##  ##  ##       ##     ##  ##  ##   ### ##    ##
########   #######  #### ######## ########  #### ##    ##  ######

* @see bsonBuild(BSON, DocOperation...)

Construct a new document and assign the result into `BSON`. `BSON` must be a
modifiable lvalue-expression of type `bson_t`. bson_destroy(&BSON) will be
called before the assignment, but after the construction. The root operation
list is equivalent to the `doc()` build operation

* @see bsonBuildAppend(BSON, DocOperation...)

Append new items to `BSON`. `BSON` must be a modifiable lvalue of type`bson_t`.
This must be a valid `bson_t`. The root operation list is equivalent to the
`doc()` build operation

* @see bsonBuildDecl(Var, DocOperation...)

Declare a new bson_t `Var` and construct a new document into it. The root
operation list is equivalent to the `doc()` build operation

Equivalent to:

   bson_t Var = BSON_INITIALIZER;
   bsonBuildAppend(Var, ...)

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

DocOperation

   The operands of a doc() build operation. The following are supported:

   kv(CString, ValueOperation)

      Append an element with the given "CString" as a key. `CString` may be
      any expression that evaluates to a null-terminated string.

   kvl(CharPtr, Len, ValueOperation)

      Append an element with the given string as a key, given as a
      pointer-to-char and a length.

   insert(OtherBSON, Predicate...)

      `OtherBSON` must evaluate to an lvalue of type `bson_t`. Each element from
      the OtherBSON document will be copied into the document being built.

      `Predicate` is one or more visitation predicates. Refer to the "Predicate"
      parameter to find() in the Parsing section below.

   if(Condition, then(DocOperation...))
   if(Condition, then(DocOperation...), else(DocOperation...))

      Conditionally perform additional document building operations based on
      `Condition`, which must be a C expression that evaluates to a truthy or
      falsey value. If the `else()` operand is omitted and `Condition` evaluates
      to `false`, no action will be performed.

ValueOperation

   A building operator that specifies the value of the current document or array
   element. They element's key is set separately.

   null

      Create a BSON null value

   bool(CBoolExpr)

      Create a boolean value. Evaluates the given C expression.

   i32(Integer)
   i64(Integer)

      Create an integral value. Evaluates the given C expression.

   cstr(CString)

      Create a UTF-8 string from the given null-terminated C string. Evaluates
      the given C expression.

   utf8_w_len(CharPtr, Len)

      Create a UTF-8 string from the given character pointer and length.

   iter(bson_iter_t)

      Copy the BSON value referred to by the given bson_iter_t. The given
      argument must evaluate to a valid bson_iter_t lvalue.

   bson(bson_t)

      Duplicate the given bson_t as a document element.

   bsonArray(bson_t)

      Duplicate the given bson_t as an array element.

   doc(DocOperation...)

      Create a sub-document. with the given elements (may be empty).

   array(ArrayOperation...)

      Create an array sub-document. Each operand generates some number of array
      elements.

   if(Condition, then(ValueOperation), else(ValueOperation))

      If The given C expression `Condition` evaluates to `true`, use the
      `then()` operation, otherwise use the `else()` operation to compute the
      value.

ArrayOperation

   ArrayOperation includes most ValueOperation operators, to generate array
   values, but defines the following array-specific operations:

   insert(bson_t)

      Copy each value from the given bson_t into the current array position.

   if(Condition, then(ArrayOperation...))
   if(Condition, then(ArrayOperation...), else(ArrayOperation...))

      If The C expression `Condition` evaluates to `true`, append elements using
      the `then` clause, otherwise use the `else()` clause (which may be
      omitted)


########     ###    ########   ######  #### ##    ##  ######
##     ##   ## ##   ##     ## ##    ##  ##  ###   ## ##    ##
##     ##  ##   ##  ##     ## ##        ##  ####  ## ##
########  ##     ## ########   ######   ##  ## ## ## ##   ####
##        ######### ##   ##         ##  ##  ##  #### ##    ##
##        ##     ## ##    ##  ##    ##  ##  ##   ### ##    ##
##        ##     ## ##     ##  ######  #### ##    ##  ######

* @see bsonParse(BSON, ParseOperation...)

Parse the elements of the given bson_t document/array. Refer to `ParseOperation`
below.

* @see bsonVisitEach(BSON, VisitOperation...)

Visit each element of the given bson_t document/array. Refer to `VisitOperation`
below.

--------------------------------------------------------------------------------

The bsonParse()/parse() and bsonVisitEach()/visitEach() DSL functions may seem
similar, but have two importantly different behaviors:

The parse(...) DSL evaluates each of its operands one time in a definite order
for the given BSON document, and can be used to quickly find keys by their name.

The visit(...) DSL evaluates all of its operands in order for each element in
the document being visited.

During visiting and parsing, the special global name `bsonVisitIter` will refer
to a bson_iter_t that points to the element currently being visited/parsed.
Uttering this name outside of the evaluation of a bsonParse() or bsonVisitEach()
will result in undefined behavior.

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

ParseOperation

   An operation performed once for an entire document being visited. The
   following are defined:

   find(Predicate, VisitOperation...)

      Find the first element of the document that satisfies the given Predicate
      expression, which has an additional DSL below. When the element is found,
      each VisitOperation will be evaluated on the element that was found. If no
      element was found, nothing will happen.

      When the element is found, the given VisitOperations will be evaluated
      for the found element. The bsonVisitIter will point to the element that
      was found.

      The following operations are supported for Predicate:

      key(CString)

         Test an element's key string.

      type(Type)

         Test an element's value type.

      keyWithType(K, T)

         Equivalent to: allOf(key(K), type(T))

      allOf(Predicate...)

         Matches only if _all_ sub-predicates matches

      anyOf(Predicate...)

         Matches if _any_ sub-predicate matches.

      not(Predicate)

         Matches if `Predicate` does not match

      noneOf(Predicate...)

         Matches if none of the given predicates match

      true

         Always matches

      false

         Never matches

      truthy

         Matches if the current BSON value is truthy (A 'true' bool, non-zero
         number, or non-null)

      falsey

         Matches if the current BSON value is not truthy

      strEqual(S)

         Matches if the current BSON value is a text string and equivalent to
         the given C string.

   else(ParseOperation...) [Parse operation]

      If the previously executed ParseOperation did not match any key, evaluate
      the given ParseOperations.

   if(Condition, then(ParseOperation...)) [Parse operation]
   if(Condition, then(ParseOperation...), else(ParseOperation...))

      If the given C expression `Condition` evaluates to `true`, evaluate the
      `then()` clause, otherwise evaluate the `else()` clause (which may be
      omitted)

   append(bson_t, DocOperation...) [Parse operation]

      Executes bsonBuildAppend(bson_t, ...). The value of bsonVisitIter is
      unspecified.

   do(...) [Parse operation]

      Evaluate the given C code. The value of bsonVisitIter is unspecified.


VisitOperation

   The following operations are applied to a bson_iter_t given by the
   bsonVisitIter global name.

   halt

      Stop the entire bsonParse/bsonVisit call immediately.

   break

      Stop the current application of VisitOperations and do not visit any more
      elements. Only valid within a visitEach() scope.

   continue

      Stop the current application of VisitOperations and advance to the next
      element. Only valid within a visitEach() scope.

   storeBool(B)

      If the bsonVisitIter is a boolean value, assign that value in the C
      lvalue-expression given as B

   found(IterDest)

      Assign the bsonVisitIter into the bson_iter_t lvalue-expression given as
      IterDest.

   do(...) [Visit operation]

      Evaluate the given C code. The bsonVisitIter names the currently visited
      element.

   append(BSON, DocOperation...) [Visit operation]

      Evaluate bsonBuildAppend(BSON, ...). The bsonVisitIter names the currently
      visited element.

   setTrue(bool)
   setFalse(bool)

      Store 'true' or 'false' (respectively) into the bool lvalue-expression.

   visitEach(VisitOperation...)

      Evaluate bsonVisitEach() for the current BSON document/array being
      visited.

   parse(ParseOperation...)

      Evaluate bsonParse() for the current BSON document/array being visited.

   if(Predicate, then(VisitOperation...))
   if(Predicate, then(VisitOperation...), else(VisitOperation...))  [Visit operation]

      If `Predicate` matches the currently visited element, apply the then()
      clause, otherwise apply the else() clause. `Predicate` has the same
      meaning as in the `find()` ParseOperation.

 */
// clang-format on

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
   (void) _bvContinue;                                                 \
   (void) _bvBreak;                                                    \
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
bsonParse_strdupPathString ();

#define bsonBuildContext (**_bsonBuildContextPtr ())
#define bsonVisitContext (**_bsonVisitContextPtr ())
#define bsonBuildFailed (*_bsonBuildFailed ())

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
 * The _bsonDocOperation_XYZ macros handle the top-level bsonBuild()
 * items, and any nested doc() items, with XYZ being the doc-building
 * subcommand.
 */
#define _bsonDocOperation(Command, _ignore, _count) \
   if (_bbOkay) {                                   \
      _bsonDocOperation_##Command;                  \
   }

#define _bsonValueOperation(P) _bsonValueOperation_##P

/// key-value pair with explicit key length
#define _bsonDocOperation_kvl(String, Len, Element)                   \
   _bsonDSL_begin ("\"%s\" => [%s]", String, _bsonDSL_str (Element)); \
   _bbCtx.key = (String);                                             \
   _bbCtx.key_len = (Len);                                            \
   _bsonValueOperation (Element);                                     \
   _bsonDSL_end

/// Key-value pair with a C-string
#define _bsonDocOperation_kv(String, Element) \
   _bsonDocOperation_kvl ((String), strlen ((String)), Element)

/// We must defer expansion of the nested doc() to allow "recursive" evaluation
#define _bsonValueOperation_doc \
   _bsonValueOperationDeferred_doc _bsonDSL_nothing ()
#define _bsonArrayOperation_doc(...) _bsonArrayAppendValue (doc (__VA_ARGS__))

#define _bsonValueOperationDeferred_doc(...)                                   \
   _bsonDSL_begin ("doc(%s)", _bsonDSL_str (__VA_ARGS__));                     \
   /* Write to this variable as the child: */                                  \
   bson_t _bbChildDoc = BSON_INITIALIZER;                                      \
   _bbOkay &= bson_append_document_begin (_bsonBuildAppendArgs, &_bbChildDoc); \
   _bsonBuildAppend (_bbChildDoc, __VA_ARGS__);                                \
   _bbOkay &= bson_append_document_end (bsonBuildContext.doc, &_bbChildDoc);   \
   _bsonDSL_end

/// We must defer expansion of the nested array() to allow "recursive"
/// evaluation
#define _bsonValueOperation_array \
   _bsonValueOperationDeferred_array _bsonDSL_nothing ()
#define _bsonArrayOperation_array(...) \
   _bsonArrayAppendValue (array (__VA_ARGS__))

#define _bsonValueOperationDeferred_array(...)                           \
   _bsonDSL_begin ("array(%s)", _bsonDSL_str (__VA_ARGS__));             \
   /* Write to this variable as the child array: */                      \
   bson_t _bbArray = BSON_INITIALIZER;                                   \
   _bbOkay &= bson_append_array_begin (_bsonBuildAppendArgs, &_bbArray); \
   _bsonBuildArray (_bbArray, __VA_ARGS__);                              \
   _bbOkay &= bson_append_array_end (bsonBuildContext.doc, &_bbArray);   \
   _bsonDSL_end

/// Append a UTF-8 string with an explicit length
#define _bsonValueOperation_utf8_w_len(String, Len) \
   (_bbOkay &= bson_append_utf8 (_bsonBuildAppendArgs, (String), (Len)))
#define _bsonArrayOperation_utf8_w_len(X) _bsonArrayAppendValue (utf8_w_len (X))

/// Append a "cstr" as UTF-8
#define _bsonValueOperation_cstr(String) \
   _bsonValueOperation_utf8_w_len ((String), strlen (String))
#define _bsonArrayOperation_cstr(X) _bsonArrayAppendValue (cstr (X))

/// Append an int32
#define _bsonValueOperation_i32(Integer) \
   (_bbOkay &= bson_append_int32 (_bsonBuildAppendArgs, (Integer)))
#define _bsonArrayOperation_i32(X) _bsonArrayAppendValue (i32 (X))

/// Append an int64
#define _bsonValueOperation_i64(Integer) \
   (_bbOkay &= bson_append_int64 (_bsonBuildAppendArgs, (Integer)))
#define _bsonArrayOperation_i64(X) _bsonArrayAppendValue (i64 (X))

/// Append the value referenced by a given iterator
#define _bsonValueOperation_iterValue(Iter) \
   (_bbOkay &= bson_append_iter (_bsonBuildAppendArgs, &(Iter)))
#define _bsonArrayOperation_iterValue(X) _bsonArrayAppendValue (iterValue (X))

/// Append the BSON document referenced by the given pointer
#define _bsonValueOperation_bson(Doc) \
   (_bbOkay &= bson_append_document (_bsonBuildAppendArgs, &(Doc)))
#define _bsonArrayOperation_bson(X) _bsonArrayAppendValue (bson (X))

/// Append the BSON document referenced by the given pointer as an array
#define _bsonValueOperation_bsonArray(Arr) \
   (_bbOkay &= bson_append_array (_bsonBuildAppendArgs, &(Arr)))
#define _bsonArrayOperation_bsonArray(X) _bsonArrayAppendValue (bsonArray (X))

#define _bsonValueOperation_bool(b) \
   (_bbOkay &= bson_append_bool (_bsonBuildAppendArgs, (b)))
#define _bsonArrayOperation_bool(X) _bsonArrayAppendValue (bool (X))
#define _bsonValueOperation__Bool(b) \
   (_bbOkay &= bson_append_bool (_bsonBuildAppendArgs, (b)))
#define _bsonArrayOperation__Bool(X) _bsonArrayAppendValue (_Bool (X))

#define _bsonValueOperation_null \
   (_bbOkay &= bson_append_null (_bsonBuildAppendArgs))
#define _bsonArrayOperation_null _bsonArrayOperation (null)

/// Insert the given BSON document into the parent document in-place
#define _bsonDocOperation_insert(OtherBSON, ...)                              \
   _bsonDSL_begin ("Insert other document: [%s]", _bsonDSL_str (OtherBSON));  \
   const bool _bvHalt = false; /* Required for _bsonVisitEach() */            \
   _bsonVisitEach (                                                           \
      OtherBSON,                                                              \
      if (allOf (__VA_ARGS__),                                                \
          then (do(_bsonDocOperation_kvl (bson_iter_key (&bsonVisitIter),     \
                                          bson_iter_key_len (&bsonVisitIter), \
                                          iterValue (bsonVisitIter))))));     \
   _bsonDSL_end

#define _bsonDocOperation_insertFromIter(Iter, ...)                    \
   _bsonDSL_begin ("Insert document from iterator: [%s]",              \
                   _bsonDSL_str (Iter));                               \
   bson_iter_t _bbIt = Iter;                                           \
   uint32_t _bbLen = 0;                                                \
   const uint8_t *_bbData = NULL;                                      \
   if (BSON_ITER_HOLDS_ARRAY (&_bbIt)) {                               \
      bson_iter_array (&_bbIt, &_bbLen, &_bbData);                     \
   } else if (BSON_ITER_HOLDS_DOCUMENT (&_bbIt)) {                     \
      bson_iter_document (&_bbIt, &_bbLen, &_bbData);                  \
   } else {                                                            \
      _bsonDSLDebug (                                                  \
         "NOTE: Skipping insert of non-document value from iterator"); \
   }                                                                   \
   if (_bbData) {                                                      \
      bson_t _bbDocFromIter;                                           \
      _bbOkay &= bson_init_static (&_bbDocFromIter, _bbData, _bbLen);  \
      _bsonDocOperation_insert (_bbDocFromIter, __VA_ARGS__);          \
   }                                                                   \
   _bsonDSL_end

/// Insert the given BSON document into the parent array. Keys of the given
/// document are discarded and it is treated as an array of values.
#define _bsonArrayOperation_insert(OtherArr, ...)                        \
   _bsonDSL_begin ("Insert other array: [%s]", _bsonDSL_str (OtherArr)); \
   _bsonVisitEach (OtherArr,                /*                    */     \
                   if (allOf (__VA_ARGS__), /*                    */     \
                       then (do({                                        \
                          _bsonBuild_setKeyToArrayIndex (                \
                             bsonBuildContext.index);                    \
                          _bsonArrayOperation_iterValue (bsonVisitIter); \
                          ++_bbCtx.index;                                \
                       }))));                                            \
   _bsonDSL_end

#define _bsonArrayAppendValue(ValueOperation)                                 \
   _bsonDSL_begin (                                                           \
      "[%d] => [%s]", bsonBuildContext.index, _bsonDSL_str (ValueOperation)); \
   /* Set the doc key to the array index as a string: */                      \
   _bsonBuild_setKeyToArrayIndex (bsonBuildContext.index);                    \
   /* Append a value: */                                                      \
   _bsonValueOperation_##ValueOperation;                                      \
   /* Increment the array index: */                                           \
   ++_bbCtx.index;                                                            \
   _bsonDSL_end


#define _bsonDocOperationIfThen_then _bsonBuildAppendWithCurrentContext
#define _bsonDocOperationIfElse_else _bsonBuildAppendWithCurrentContext

#define _bsonDocOperationIfThenElse(Condition, Then, Else)              \
   if ((Condition)) {                                                   \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then));  \
      _bsonDocOperationIfThen_##Then;                                   \
   } else {                                                             \
      _bsonDSLDebug ("Taking FALSE branch: [%s]", _bsonDSL_str (Else)); \
      _bsonDocOperationIfElse_##Else;                                   \
   }

#define _bsonDocOperationIfThen(Condition, Then)                       \
   if ((Condition)) {                                                  \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then)); \
      _bsonDocOperationIfThen_##Then;                                  \
   }

#define _bsonDocOperation_if(Condition, ...)                                \
   _bsonDSL_begin ("Conditional append on [%s]", _bsonDSL_str (Condition)); \
   /* Pick a sub-macro depending on if there are one or two args */         \
   _bsonDSL_ifElse (_bsonDSL_hasComma (__VA_ARGS__),                        \
                    _bsonDocOperationIfThenElse,                            \
                    _bsonDocOperationIfThen) (Condition, __VA_ARGS__);      \
   _bsonDSL_end

#define _bsonArrayOperationIfThen_then _bsonBuildArrayWithCurrentContext
#define _bsonArrayOperationIfElse_else _bsonBuildArrayWithCurrentContext

#define _bsonArrayOperationIfThenElse(Condition, Then, Else)            \
   if ((Condition)) {                                                   \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then));  \
      _bsonArrayOperationIfThen_##Then;                                 \
   } else {                                                             \
      _bsonDSLDebug ("Taking FALSE branch: [%s]", _bsonDSL_str (Else)); \
      _bsonArrayOperationIfElse_##Else;                                 \
   }

#define _bsonArrayOperationIfThen(Condition, Then)                     \
   if ((Condition)) {                                                  \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then)); \
      _bsonArrayOperationIfThen_##Then;                                \
   }

#define _bsonArrayOperation_if(Condition, ...)                             \
   _bsonDSL_begin ("Conditional value on [%s]", _bsonDSL_str (Condition)); \
   /* Pick a sub-macro depending on if there are one or two args */        \
   _bsonDSL_ifElse (_bsonDSL_hasComma (__VA_ARGS__),                       \
                    _bsonArrayOperationIfThenElse,                         \
                    _bsonArrayOperationIfThen) (Condition, __VA_ARGS__);   \
   _bsonDSL_end

#define _bsonValueOperationIf_then(X) _bsonValueOperation_##X
#define _bsonValueOperationIf_else(X) _bsonValueOperation_##X

#define _bsonValueOperation_if(Condition, Then, Else)                   \
   if ((Condition)) {                                                   \
      _bsonDSLDebug ("Taking TRUE branch: [%s]", _bsonDSL_str (Then));  \
      _bsonValueOperationIf_##Then;                                     \
   } else {                                                             \
      _bsonDSLDebug ("Taking FALSE branch: [%s]", _bsonDSL_str (Else)); \
      _bsonValueOperationIf_##Else;                                     \
   }

#define _bsonBuild_setKeyToArrayIndex(Idx)                         \
   _bbCtx.key_len = sprintf (_bbCtx.strbuf64, "%d", _bbCtx.index); \
   _bbCtx.key = _bbCtx.strbuf64

/// Handle an element of array()
#define _bsonArrayOperation(Element, _nil, _count) \
   if (_bbOkay) {                                  \
      _bsonArrayOperation_##Element;               \
   }

#define _bsonBuildAppendWithCurrentContext(...) \
   _bsonDSL_mapMacro (_bsonDocOperation, ~, __VA_ARGS__)

#define _bsonBuildArrayWithCurrentContext(...) \
   _bsonDSL_mapMacro (_bsonArrayOperation, ~, __VA_ARGS__)

#define _bsonDSL_Type_double BSON_TYPE_DOUBLE
#define _bsonDSL_Type_utf8 BSON_TYPE_UTF8
#define _bsonDSL_Type_doc BSON_TYPE_DOCUMENT
#define _bsonDSL_Type_array BSON_TYPE_ARRAY
#define _bsonDSL_Type_binary BSON_TYPE_BINARY
#define _bsonDSL_Type_undefined BSON_TYPE_UNDEFINED
#define _bsonDSL_Type_oid BSON_TYPE_OID
#define _bsonDSL_Type_bool BSON_TYPE_BOOL
// ("bool" may be spelled _Bool due to macro expansion:)
#define _bsonDSL_Type__Bool BSON_TYPE_BOOL
#define _bsonDSL_Type_date_time BSON_TYPE_DATE_TIME
#define _bsonDSL_Type_null BSON_TYPE_NULL
#define _bsonDSL_Type_regex BSON_TYPE_REGEX
#define _bsonDSL_Type_dbPointer BSON_TYPE_DBPOINTER
#define _bsonDSL_Type_code BSON_TYPE_CODE
#define _bsonDSL_Type_code_w_scope BSON_TYPE_CODEWSCOPE
#define _bsonDSL_Type_int32 BSON_TYPE_INT32
#define _bsonDSL_Type_timestamp BSON_TYPE_TIMESTAMP
#define _bsonDSL_Type_int64 BSON_TYPE_INT64
#define _bsonDSL_Type_decimal128 BSON_TYPE_DECIMAL128

#define _bsonVisitOperation_halt _bvHalt = true

#define _bsonVisitOperation_if(Predicate, ...)                   \
   _bsonDSL_begin ("if(%s)", _bsonDSL_str (Predicate));          \
   _bsonDSL_ifElse (_bsonDSL_hasComma (__VA_ARGS__),             \
                    _bsonVisit_ifThenElse,                       \
                    _bsonVisit_ifThen) (Predicate, __VA_ARGS__); \
   _bsonDSL_end

#define _bsonVisit_ifThenElse(Predicate, Then, Else)                   \
   if (bsonPredicate (Predicate)) {                                    \
      _bsonDSLDebug ("Taking TRUE branch [%s]", _bsonDSL_str (Then));  \
      _bsonVisit_ifThen_##Then;                                        \
   } else {                                                            \
      _bsonDSLDebug ("Taking FALSE branch [%s]", _bsonDSL_str (Else)); \
      _bsonVisit_ifElse_##Else;                                        \
   }

#define _bsonVisit_ifThen(Predicate, Then)                            \
   if (bsonPredicate (Predicate)) {                                   \
      _bsonDSLDebug ("Taking TRUE branch [%s]", _bsonDSL_str (Then)); \
      _bsonVisit_ifThen_##Then;                                       \
   }

#define _bsonVisit_ifThen_then _bsonVisit_applyOps
#define _bsonVisit_ifElse_else _bsonVisit_applyOps

#define _bsonVisitOperation_storeBool(Dest)               \
   _bsonDSL_begin ("storeBool(%s)", _bsonDSL_str (Dest)); \
   (Dest) = bson_iter_as_bool (&bsonVisitIter);           \
   _bsonDSL_end

#define _bsonVisitOperation_found(IterDest)               \
   _bsonDSL_begin ("found(%s)", _bsonDSL_str (IterDest)); \
   (IterDest) = bsonVisitIter;                            \
   _bsonDSL_end

#define _bsonVisitOperation_do(...)                           \
   _bsonDSL_begin ("do: { %s }", _bsonDSL_str (__VA_ARGS__)); \
   do {                                                       \
      __VA_ARGS__;                                            \
   } while (0);                                               \
   _bsonDSL_end

#define _bsonVisitOperation_append \
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
      (void) _bvBreak;                                                      \
      bool _bvContinue = false;                                             \
      (void) _bvContinue;                                                   \
      while (bson_iter_next (&_bpCtx.iter) && !_bvHalt && !_bvBreak) {      \
         _bvContinue = false;                                               \
         _bsonVisit_applyOps (__VA_ARGS__);                                 \
      }                                                                     \
      /* Restore the dsl context */                                         \
      *_bsonVisitContextPtr () = bsonVisitContext.parent;                   \
   } while (0);                                                             \
   _bsonDSL_end

#define _bsonVisitOperation_visitEach \
   _bsonVisitOperation_visitEachDeferred _bsonDSL_nothing ()
#define _bsonVisitOperation_visitEachDeferred(...)                             \
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

#define _bsonVisitOperation_nop ((void) 0)
#define _bsonVisitOperation_parse(...)                                  \
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

#define _bsonVisitOperation_setTrue(B)                      \
   _bsonDSL_begin ("Set [%s] to 'true'", _bsonDSL_str (B)); \
   (B) = true;                                              \
   _bsonDSL_end;

#define _bsonVisitOperation_setFalse(B)                      \
   _bsonDSL_begin ("Set [%s] to 'false'", _bsonDSL_str (B)); \
   (B) = false;                                              \
   _bsonDSL_end;

#define _bsonVisitOperation_continue _bvContinue = true
#define _bsonVisitOperation_break _bvBreak = _bvContinue = true
#define _bsonVisitOperation_require(Cond) \
   if (!(Cond)) {                         \
      _bsonVisitOperation_halt;           \
   } else                                 \
      ((void) 0)

#define _bsonVisit_applyOp(P, _const, _count) \
   do {                                       \
      if (!_bvContinue && !_bvHalt) {         \
         _bsonVisitOperation_##P;             \
      }                                       \
   } while (0);

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
         _bsonParseOperation_##P;            \
      }                                      \
   } while (0);

#define _bsonParseOperation_find(Predicate, ...)          \
   _bsonDSL_begin ("find(%s)", _bsonDSL_str (Predicate)); \
   _bpFoundElement = false;                               \
   (void) _bpFoundElement;                                \
   bson_iter_init (&_bpCtx.iter, bsonVisitContext.doc);   \
   while (bson_iter_next (&_bpCtx.iter)) {                \
      if (bsonPredicate (Predicate)) {                    \
         _bsonVisit_applyOps (__VA_ARGS__);               \
         _bpFoundElement = true;                          \
         break;                                           \
      }                                                   \
   }                                                      \
   _bsonDSL_end

#define bsonPredicate(P) _bsonPredicate _bsonDSL_nothing () (P)
#define _bsonPredicate(P) _bsonPredicate_Condition_##P

#define _bsonPredicate_Condition_not(P) \
   (!(bsonPredicate _bsonDSL_nothing () (P)))
#define _bsonPredicate_Condition_allOf(...) \
   (1 _bsonDSL_mapMacro (_bsonPredicateAnd, ~, __VA_ARGS__))
#define _bsonPredicate_Condition_anyOf(...) \
   (0 _bsonDSL_mapMacro (_bsonPredicateOr, ~, __VA_ARGS__))
#define _bsonPredicate_Condition_noneOf(...) \
   (_bsonPredicate_Condition_not (anyOf (__VA_ARGS__)))
#define _bsonPredicateAnd(Pred, _ignore, _ignore1) \
   &&(_bsonPredicate_Condition_##Pred)
#define _bsonPredicateOr(Pred, _ignore, _ignore2) \
   || (_bsonPredicate_Condition_##Pred)


#define _bsonPredicate_Condition_key(Key) \
   (0 == strcmp (bson_iter_key (&bsonVisitIter), (Key)))

#define _bsonPredicate_Condition_type(Type) \
   (bson_iter_type (&bsonVisitIter) == _bsonDSL_Type_##Type)

#define _bsonPredicate_Condition_keyWithType(Key, Type) \
   (_bsonPredicate_Condition_allOf (key (Key), type (Type)))

#define _bsonPredicate_Condition_1 1
#define _bsonPredicate_Condition_0 0
#define _bsonPredicate_Condition_true true
#define _bsonPredicate_Condition_false false

#define _bsonPredicate_Condition_truthy (bson_iter_as_bool (&bsonVisitIter))
#define _bsonPredicate_Condition_falsey (!bson_iter_as_bool (&bsonVisitIter))
#define _bsonPredicate_Condition_empty \
   (_bson_dsl_is_empty_bson (&bsonVisitIter))

#define _bsonPredicate_Condition_strEqual(S) (_bson_dsl_test_strequal (S))

#define _bsonParseOperation_else _bsonParse_deferredElse _bsonDSL_nothing ()
#define _bsonParse_deferredElse(...)                           \
   if (!_bpFoundElement) {                                     \
      _bsonDSL_begin ("else(%s)", _bsonDSL_str (__VA_ARGS__)); \
      _bsonParse_applyOps (__VA_ARGS__);                       \
      _bsonDSL_end;                                            \
   } else                                                      \
      ((void) 0)

#define _bsonParseOperation_do(...)                       \
   _bsonDSL_begin ("do(%s)", _bsonDSL_str (__VA_ARGS__)); \
   do {                                                   \
      __VA_ARGS__;                                        \
   } while (0);                                           \
   _bsonDSL_end

/// Perform conditional parsing
#define _bsonParseOperation_if(Condition, ...)                      \
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

#define _bsonParseOperation_append _bsonBuildAppend

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
      struct _bsonBuildContext_t _bbCtx = {                               \
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
   struct _bsonBuildContext_t _bbCtx = {                               \
      .doc = &(BSON),                                                  \
      .parent = *_bsonBuildContextPtr (),                              \
   };                                                                  \
   *_bsonBuildContextPtr () = &_bbCtx;                                 \
   bool _bbOkay = true;                                                \
   /* Reset the context */                                             \
   _bsonBuildAppendWithCurrentContext (__VA_ARGS__);                   \
   /* Restore the dsl context */                                       \
   *_bsonBuildContextPtr () = _bbCtx.parent;                           \
   bsonBuildFailed = !_bbOkay;                                         \
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

struct _bsonBuildContext_t {
   bson_t *doc;
   const char *key;
   int key_len;
   int index;
   char strbuf64[64];
   struct _bsonBuildContext_t *parent;
};

struct _bsonVisitContext_t {
   const bson_t *doc;
   bson_iter_t iter;
   const struct _bsonVisitContext_t *parent;
};

extern struct _bsonBuildContext_t **
_bsonBuildContextPtr ();

extern struct _bsonVisitContext_t const **
_bsonVisitContextPtr ();

extern bool *
_bsonBuildFailed ();

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

static inline bool
_bson_dsl_test_strequal (const char *string)
{
   bson_iter_t it = bsonVisitIter;
   if (bson_iter_type (&it) == BSON_TYPE_UTF8) {
      uint32_t len;
      const char *s = bson_iter_utf8 (&it, &len);
      if (len == strlen (string) && (0 == memcmp (s, string, len))) {
         return true;
      }
   }
   return false;
}

static inline bool
_bson_dsl_is_empty_bson (const bson_iter_t *iter)
{
   uint32_t len = 0;
   const uint8_t *dataptr;
   if (BSON_ITER_HOLDS_ARRAY (iter)) {
      bson_iter_array (iter, &len, &dataptr);
   } else if (BSON_ITER_HOLDS_DOCUMENT (iter)) {
      bson_iter_document (iter, &len, &dataptr);
   }
   return len == 5; // Empty documents/arrays have byte-size of five
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
