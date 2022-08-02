#ifndef BSON_BSON_VIEW_H_INCLUDED
#define BSON_BSON_VIEW_H_INCLUDED

#ifdef __cplusplus
#define BSON2_EXTERN_C_BEGIN extern "C" {
#define BSON2_EXTERN_C_END }
#define BSON2_CONSTEXPR constexpr
#define BSON2_NOEXCEPT noexcept
#define BSON2_INIT(X) X
#else
#define BSON2_EXTERN_C_BEGIN
#define BSON2_EXTERN_C_END
#define BSON2_CONSTEXPR inline
#define BSON2_NOEXCEPT
#define BSON2_INIT(X) (X)
#endif

#include <bson/types.h>

#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

BSON2_EXTERN_C_BEGIN

struct _bson_t;
struct _bson_iter_t;

enum { BSON_VIEW_CHECKED = 1 };

inline uint32_t
bson_read_uint32_le (const uint8_t *bytes) BSON2_NOEXCEPT
{
   uint32_t ret = 0;
   ret |= bytes[3];
   ret <<= 8;
   ret |= bytes[2];
   ret <<= 8;
   ret |= bytes[1];
   ret <<= 8;
   ret |= bytes[0];
   return ret;
}

extern void
_bson_assert_fail (const char *, const char *file, int line);

#define BV_ASSERT(Cond)                              \
   if (BSON_VIEW_CHECKED && !(Cond)) {               \
      _bson_assert_fail (#Cond, __FILE__, __LINE__); \
      abort ();                                      \
   } else                                            \
      ((void) 0)


/**
 * @brief A type that is the size of a byte, but does not alias with other types
 * (except char)
 */
typedef struct bson_byte {
   /// The 8-bit value of the byte
   uint8_t v;
} bson_byte;

/**
 * @brief A type specifically representing a nullable read-only view of a BSON
 * document.
 *
 * @note This structure SHOULD NOT be created manually. Prefer instead to use
 * @ref bson_view_from data or @ref bson_view_from_bson_t or
 * @ref bson_view_from_iter, which will validate the header and trailer of the
 * pointed-to data. Use @ref BSON_VIEW_NULL to initialize a view to a "null"
 * value.
 */
typedef struct bson_view {
   /**
    * @brief Pointer to the beginning of the referenced BSON document, or NULL.
    *
    * If NULL, the view refers to nothing.
    */
   const bson_byte *data;
} bson_view;

/// A "null" constant expression for bson_view objects
#define BSON_VIEW_NULL (BSON2_INIT (bson_view){NULL})

/**
 * @brief Obtain the byte-size of the BSON document referred to by the given
 * bson_view.
 *
 * @param v A bson_view to inspect
 * @return uint32_t The size (in bytes) of the viewed document, or zero if `v`
 * is BSON_VIEW_NULL
 */
inline uint32_t
bson_view_len (bson_view v) BSON2_NOEXCEPT
{
   // A null document has length=0
   if (!v.data) {
      return 0;
   }
   // The length of a document is contained in a four-bytes little-endian
   // encoded integer. This size includes the header, the document body, and the
   // null terminator byte on the document.
   return bson_read_uint32_le ((const uint8_t *) v.data);
}

/**
 * @brief The reason that we may have failed to create a bson_view object in
 * @see bson_view_from_trusted_data() or @see bson_view_from_untrusted_data
 */
enum bson_view_invalid_reason {
   /**
    * @brief There is no error creating the view, and the view is ready.
    * Equivalent to zero.
    */
   BSON_VIEW_OKAY,
   /**
    * @brief The given data buffer is too short to possibly hold the document.
    *
    * If the buffer is less than five bytes, it is impossible to be a valid
    * document. If the buffer is more than five bytes and this error occurs, the
    * document header declares a length that is longer than the buffer.
    */
   BSON_VIEW_SHORT_READ,
   /**
    * @brief The document header declares an invalid length.
    */
   BSON_VIEW_INVALID_HEADER,
   /**
    * @brief The document does not have a null terminator
    */
   BSON_VIEW_INVALID_TERMINATOR,
};

/**
 * @brief View the given pointed-to data as a BSON document.
 *
 * @param data A pointer to the beginning of a BSON document.
 * @param len The length of the pointed-to data buffer.
 * @param[out] error An out-param to determine why view creation may have
 * failed.
 * @return bson_view A view, or BSON_VIEW_NULL if the data is invalid.
 *
 * @throws BSON_VIEW_SHORT_READ if data_len is shorter than the length of the
 * document as declared by the header, or data_len is less than five (the
 * minimum size of a BSON document).
 * @throws BSON_VIEW_INVALID_HEADER if the BSON header in the data is of an
 * invalid length (either too large or too small).
 * @throws BSON_VIEW_INVALID_TERMINATOR if the BSON null terminator is missing
 * from the input data.
 *
 * @note IMPORTANT: This function does not error if 'data_len' is longer than
 * the document. This allows support of buffered reads of unknown sizes. The
 * actual length of the resulting document can be obtained with
 * @ref bson_view_len.
 *
 * @note IMPORTANT: This function does not validate the content of the document:
 * Those are validated lazily as they are iterated over.
 */
inline bson_view
bson_view_from_data (const bson_byte *const data,
                     const size_t data_len,
                     enum bson_view_invalid_reason *const error) BSON2_NOEXCEPT
{
   bson_view ret = BSON_VIEW_NULL;
   uint32_t len = 0;
   enum bson_view_invalid_reason err = BSON_VIEW_OKAY;
   // All BSON data must be at least five bytes long
   if (data_len < 5) {
      err = BSON_VIEW_SHORT_READ;
      goto done;
   }
   // Read the length header. This includes the header's four bytes, the
   // document's element data, and the null terminator byte.
   len = bson_read_uint32_le ((const uint8_t *) data);
   // Check that the size is in-bounds
   if (len > (1ull << 31) || len < 5) {
      err = BSON_VIEW_INVALID_HEADER;
      goto done;
   }
   // Check that the buffer is large enough to hold expected the document
   if (len > data_len) {
      // Not enough data to do the read
      err = BSON_VIEW_SHORT_READ;
      goto done;
   }
   // The document must have a zero-byte at the end.
   if (data[len - 1].v != 0) {
      err = BSON_VIEW_INVALID_TERMINATOR;
      goto done;
   }
   // Okay!
   ret.data = data;
done:
   if (error) {
      *error = err;
   }
   return ret;
}

/**
 * @brief Obtain a bson_t from a given bson_view.
 *
 * @param view The view to convert to a bson_t value.
 * @return bson_t A non-owning bson_t value. Should not be destroyed.
 */
extern struct _bson_t
bson_view_as_viewing_bson_t (bson_view view) BSON2_NOEXCEPT;

/**
 * @brief Create a bson_view that refers to a bson_t
 *
 * @param b A pointer to a bson_t, or NULL
 * @return bson_view A view of `b`. If `b` is NULL, returns BSON_VIEW_NULL
 */
extern bson_view
bson_view_from_bson_t (const struct _bson_t *b) BSON2_NOEXCEPT;

/**
 * @brief Create a new bson_view that views the document referred to by the
 * given bson_iter_t
 *
 * @param it A bson_iter_t.
 * @return bson_view A view of the pointed-to document, or NULL if bson_iter_t
 * does not point to a document/array value.
 */
static inline bson_view
bson_view_from_iter (struct _bson_iter_t it) BSON2_NOEXCEPT;

/**
 * @brief Create a bson_t that copies the given bson_view's data
 *
 * @param v A bson_view. If null, returns a NULL bson_t*
 * @return bson_t* A new pointer-to-bson_t. Must be destroyed with
 * bson_destroy()
 */
extern struct _bson_t *
bson_view_copy_as_bson_t (bson_view v) BSON2_NOEXCEPT;

/**
 * @brief The stop-state of a bson_iterator
 */
enum bson_view_iterator_stop_reason {
   BSONV_ITER_STOP_NOT_DONE,
   BSONV_ITER_STOP_DONE,
   BSONV_ITER_STOP_INVALID,
   BSONV_ITER_STOP_INVALID_TYPE,
   BSONV_ITER_STOP_SHORT_READ,
};


/**
 * @brief A reference-like type that points to an element within a bson_view
 * document.
 */
typedef struct bson_iterator {
   /// A pointer to the beginning of the element.
   bson_byte const *ptr;
   /// @private The number of bytes remaining in the document, or an error code.
   int32_t _rlen;
   /// @private The length of the key string, in bytes, not including the nul
   int32_t _keylen;
} bson_iterator;

typedef struct bson_view_utf8 {
   const char *data;
   int32_t len;
} bson_view_utf8;

/**
 * @brief Obtain a pointer to the beginning of the null-terminated string
 * denoting the BSON element's key
 *
 * @param it The iterator under inspection
 * @return const char* A pointer to the beginning of a null-terminated string
 */
inline bson_view_utf8
bson_iterator_key (bson_iterator it) BSON2_NOEXCEPT
{
   BV_ASSERT (it._rlen >= it._keylen + 1);
   return (bson_view_utf8){.data = (const char *) it.ptr + 1,
                           .len = it._keylen};
}

inline bson_type
bson_iterator_type (bson_iterator it) BSON2_NOEXCEPT
{
   BV_ASSERT (it._rlen >= 1);
   return (bson_type) it.ptr[0].v;
}

enum bson_iterator_error_cond {
   BSON_ITER_NO_ERROR = 0,
   BSON_ITER_SHORT_READ = 1,
   BSON_ITER_INVALID_TYPE,
   BSON_ITER_INVALID,
   BSON_ITER_INVALID_LENGTH,
};

inline bson_iterator
_bson_iterator_error (enum bson_iterator_error_cond err)
{
   return BSON2_INIT (bson_iterator){._rlen = -((int32_t) err)};
}

static inline enum bson_iterator_error_cond
bson_get_error (bson_iterator it)
{
   return it._rlen < 0 ? (enum bson_iterator_error_cond) - it._rlen
                       : BSON_ITER_NO_ERROR;
}

inline bson_byte const *
_bson_iterator_value_ptr (bson_iterator it) BSON2_NOEXCEPT
{
   BV_ASSERT (it._rlen > 2);
   return it.ptr + 1 + it._keylen + 1;
}

/**
 * @brief Compute the byte-length of a regular expression BSON element.
 *
 * @param valptr The beginning of the value of the regular expression element
 * (the byte following the element key's NUL character)
 * @param maxlen The number of bytes available following `valptr`
 * @return int32_t The number of bytes occupied by the regex value.
 */
inline int32_t
_bson_value_re_len (const char *valptr, int32_t maxlen)
{
   BV_ASSERT (maxlen > 0);
   // A regex is encoded as <cstring><cstring>
   int re_len = strnlen ((const char *) valptr, maxlen);
   // Because the entire document is guaranteed to have null terminator
   // (this was checked before the iterator was created) and `maxlen >= 0`, we
   // can assume that re_len is less than `maxlen`
   re_len += 1; // Add the null terminator. Now re_len *might* be equal to
                // maxlen.
   // The pointer to the beginning of the regex option cstring. If the
   // regex is missing the null terminator, then 're_len' will be equal to
   // value_bytes_remaining, which will cause this pointer to point
   // past-the-end of the entire document.
   const char *opt_begin_ptr = (const char *) valptr + re_len;
   // The number of bytes available for the regex option string. If the
   // regex cstring was missing a null terminator, this will end up as
   // zero.
   const size_t opt_bytes_avail = maxlen - re_len;
   // The length of the option string. If the regex string was missing a
   // null terminator, then strnlen()'s maxlen argument will be zero, and
   // opt_len will therefore also be zero.
   size_t opt_len = strnlen (opt_begin_ptr, opt_bytes_avail);
   /// The number of bytes remaining in the doc following the option. This
   /// includes the null terminator for the option string, which we
   /// haven't passed yet.
   const size_t trailing_bytes = opt_bytes_avail - opt_len;
   // There MUST be two more null terminators (the one after the opt
   // string, and the one at the end of the doc itself), so
   // 'trailing_bytes' must be at least two.
   if (trailing_bytes < 2) {
      return -(int) BSON_ITER_SHORT_READ;
   }
   // Advance past the option string's nul
   opt_len += 1;
   // This is the value's length
   return re_len + opt_len;
}

/**
 * @brief Compute the size of the value data in a BSON element stored in
 * contiguous memory.
 *
 * @param tag The type tag
 * @param valptr The pointer where the value would begin
 * @param val_maxlen The number of bytes available following `valptr`
 * @return int32_t The size of the value, in bytes, or a negative encoded @ref
 * bson_iter_error_cond
 *
 * @pre The `val_maxlen` MUST be greater than zero.
 */
inline int32_t
_bson_valsize (bson_type tag, bson_byte const *const valptr, int32_t val_maxlen)
{
   // Assume: ValMaxLen > 0
   BV_ASSERT (val_maxlen > 0);
   /// "const_sizes" are the minimum distance we need to consume for each
   /// datatype. Length-prefixed strings have a 32-bit integer which needs to be
   /// skipped. The length prefix on strings already includes the null
   /// terminator after the string itself.
   ///
   /// docs/arrays are also length-prefixed, but this length prefix accounts for
   /// itself as part of its value, so it will be taken care of automatically
   /// when we jump based on that value.
   static const int32_t const_sizes[20] = {
      0,         // 0x0 BSON_TYPE_EOD
      8,         // 0x1 double
      4,         // 0x2 utf8
      0,         // 0x3 document,
      0,         // 0x4 array
      4 + 1,     // 0x5 binary (+1 for subtype)
      0,         // 0x6 undefined
      12,        // 0x7 OID (twelve bytes)
      1,         // 0x8 bool
      8,         // 0x9 datetime
      0,         // 0xa null
      INT32_MAX, // 0xb regex. This value is special and triggers the overflow
                 // guard, which then inspects the regular expression value
                 // to find the length.
      12,        // 0xc dbpointer
      4,         // 0xd JS code
      4,         // 0xe Symbol
      4 + 4,     // 0xf Code with scope
      4,         // 0x10 int32
      8,         // 0x11 MongoDB timestamp
      8,         // 0x12 int64
      16,        // 0x13 decimal128
   };

   if (tag < 0 || tag > 0x13) {
      if (tag == 0xff || tag == 0x7f) {
         // Minkey / Maxkey
         return 0;
      }
      // The tag itself is out-of-range.
      return -(int) BSON_ITER_INVALID_TYPE;
   }

   const int32_t const_size = const_sizes[tag];

   /// "varsize_pick" encodes whether a type tag contains a length prefix
   /// before its value.
   static const bool varsize_pick[] = {
      false, // 0x0 BSON_TYPE_EOD
      false, // 0x1 double
      true,  // 0x2 utf8
      true,  // 0x3 document,
      true,  // 0x4 array
      true,  // 0x5 binary,
      false, // 0x6 undefined
      false, // 0x7 OID (twelve bytes)
      false, // 0x8 bool
      false, // 0x9 datetime
      false, // 0xa null
      false, // 0xb regex
      true,  // 0xc dbpointer
      true,  // 0xd JS code
      true,  // 0xe Symbol
      true,  // 0xf Code with scope
      false, // 0x10 int32
      false, // 0x11 MongoDB timestamp
      false, // 0x12 int64
      false, // 0x13 decimal128
   };

   /// Whether we have enough data in the buffer to read four bytes.
   const bool have_enough = val_maxlen >= 4;
   const bool has_varsize_prefix = varsize_pick[tag];
   const int buf_picker = (have_enough << 1) | (int) has_varsize_prefix;
   static const uint32_t zero = 0;
   static const uint32_t n1 = INT32_MAX;
   const void *bufs[] = {
      &zero,  // 0b00 -> !have_enough & !has_varsize_prefix
      &n1,    // 0b01 -> !have_enough &  has_varsize_prefix
      &zero,  // 0b10 ->  have_enough & !has_varsize_prefix
      valptr, // 0b11 ->  have_enough &  has_varsize_prefix
   };
   // If the type is length-prefixed, and we have enough to read that prefix,
   // this points to the beginning of that prefix, otherwise points to a
   // constant: If we expect a length prefix but do not have enough data, this
   // points to an INT32_MAX. Otherwise, this points to a zero.
   const uint8_t *const varsize_source = (const uint8_t *) bufs[buf_picker];
   // The length of the value given by the length prefix, if applicable and we
   // have enough data. If we expect a length prefix and do not have enough
   // data, INT32_MAX. Otherwise zero.
   const int32_t varlen = (int32_t) bson_read_uint32_le (varsize_source);
   // The number of integers we have beyond `const_size` before overflowing
   // int32_t.
   const int32_t headroom = INT32_MAX - const_size;
   // Whether adding `varlen` and `const_size` will overflow int32_t
   const bool add_would_overload = headroom < varlen;
   // Either the `const_size`, or `headroom`
   const int32_t add_pick[] = {const_size, headroom};
   // An integer that is safe to add to `varlen`. If addition would overflow,
   // this addend will sum to INT32_MAX and indicate a buffer overflow.
   // Otherwise, this addend will sum to the actual value length successfully.
   const int32_t safe_addend = add_pick[(int) add_would_overload];
   // Calculate the actual size of the value. If varlen is too large to add to
   // `const_size` or type is a regex, will add to INT32_MAX and definitively
   // trigger the overrun guard below. Otherwise, calculates the length of value
   // that we expect.
   const int32_t full_len = varlen + safe_addend;
   // Check whether there is enough room to hold the value's required size.
   if (full_len >= val_maxlen) {
      if (tag == BSON_TYPE_REGEX) {
         // Oops! We're actually reading a regular expression, which is designed
         // above to trigger the overrun check so that we can do something
         // different to calculate its size:
         return _bson_value_re_len ((const char *) valptr, val_maxlen);
      }
      // Indicate an overrun by returning -1
      return -(int) BSON_ITER_INVALID_LENGTH;
   } else {
      // We have a good size:
      return full_len;
   }
}

/**
 * @brief Obtain an iterator pointing to the given 'data', which must be the
 * beginning of a document element.
 *
 * This is guaranteed to return a valid element iterator, a stopped iterator, or
 * to a sentinel iterator that indicates an error. This function validates that
 * the pointed-to element is complete and valid.
 *
 * @param data A pointer to the type tag that starts a document element, or to
 * the null-terminator at the end of a document.
 * @param bytes_remaining The number of bytes that are available following
 * `data` before overrunning the document. `data + bytes_remaining` will be the
 * past-the-end byte of the document.
 *
 * @return bson_iterator A new iterator, which may be stopped or errant.
 */
inline bson_iterator
_bson_iterator_at (bson_byte const *const data, int32_t maxlen) BSON2_NOEXCEPT
{
   // Assert: maxlen > 0
   BV_ASSERT (maxlen > 0);
   // Define:
   const int last_index = maxlen - 1;
   // Given : (N > M) → (N - 1 >= M)
   // Derive: (last_index = maxlen - 1)
   //       ⋀ (maxlen > 0)
   //       ⊢ last_index >= 0
   BV_ASSERT (last_index >= 0);
   // Assume: data[last_index] = 0
   BV_ASSERT (data[last_index].v == 0);

   /// The type of the next data element
   const bson_type type = (bson_type) data[0].v;

   if (maxlen == 1) {
      // There's only the last byte remaining. Creation of the original
      // bson_view validated that the data ended with a nullbyte, so we may
      // assume that the only remaining byte is a nul.
      BV_ASSERT (data[last_index].v == 0);
      // This "zero" type is actually the null terminator at the end of the
      // document.
      return (bson_iterator){.ptr = data, ._rlen = 1, ._keylen = 0};
   }

   //? Prove: maxlen > 1
   // Assume: maxlen != 1
   // Derive: maxlen > 0
   //       ⋀ maxlen != 1
   //*      ⊢ maxlen > 1
   BV_ASSERT (maxlen > 1);

   //? Prove: maxlen - 1 >= 1
   // Derive: maxlen > 1
   //       ⋀ (N > M) → (N - 1 >= M)
   //*      ⊢ maxlen - 1 >= 1

   // Define:
   const int key_maxlen = maxlen - 1;

   //? Prove: key_maxlen >= 1
   // Given : key_maxlen = maxlen - 1
   //       ⋀ maxlen - 1 >= 1
   //*      ⊢ key_maxlen >= 1
   BV_ASSERT (key_maxlen >= 1);

   //? Prove: key_maxlen = last_index
   // Subst : key_maxlen = maxlen - 1
   //       ⋀ last_index = maxlen - 1
   //*      ⊢ key_maxlen = last_index
   BV_ASSERT (key_maxlen == last_index);

   // Define: ∀ (n : n >= 0 ⋀ n < key_maxlen) . keyptr[n] = data[n+1];
   const char *const keyptr = (const char *) data + 1;

   // Define: KeyLastIndex = last_index - 1
   const int key_lastindex = last_index - 1;

   //? Prove: keyptr[KeyLastIndx] = data[last_index]
   // Derive: keyptr[n] = data[n+1]
   //       ⊢ keyptr[last_index-1] = data[(last_index-1)+1]
   // Simplf: keyptr[last_index-1] = data[last_index]
   // Subst : KeyLastIndex = last_index - 1
   //*      ⊢ keyptr[KeyLastIndex] = data[last_index]

   //? Prove: keyptr[KeyLastIndex] = 0
   // Derive: data[last_index] = 0
   //       ⋀ keyptr[KeyLastIndex] = data[last_index]
   //*      ⊢ keyptr[KeyLastIndex] = 0
   BV_ASSERT (keyptr[key_lastindex] == 0);

   /*
   Assume: The guarantees of R = strnlen(S, M):

      HasNul(S, M) ↔ (∃ (n : n >= 0 ⋀ n < M) . S[n] = 0)

        HasNull(S, M) → R >= 0
                      ⋀ R < M
                      ⋀ S[R] = 0

      ¬ HasNull(S, M) → R = M
   */

   //? Prove: KeyLastIndex < key_maxlen
   // Given : N - 1 < N
   // Derive: KeyLastIndex = key_maxlen - 1
   //       ⋀ (key_maxlen - 1) < key_maxlen
   //*      ⊢ (KeyLastIndex < key_maxlen)

   //? Prove: HasNul(keyptr, key_maxlen)
   // Derive: (KeyLastIndex < key_maxlen)
   //       ⋀ (keyptr[KeyLastIndex] = 0)
   //       ⋀ key_maxlen >= 1
   //*      ⊢ HasNul(keyptr, key_maxlen)

   // Define:
   const int keylen = strnlen (keyptr, key_maxlen);


   // Derive: HasNul(keyptr, key_maxlen)
   //       ⋀ R.keylen = strnlen(S.keyptr, M.key_maxlen)
   //       ⋀ HasNul(S, M) -> (R >= 0) ⋀ (R < M) ⋀ (S[R] = 0)
   //*      ⊢ (keylen >= 0)
   //*      ⋀ (keylen < key_maxlen)
   //*      ⋀ (keyptr[keylen] = 0)
   BV_ASSERT (keylen >= 0);
   BV_ASSERT (keylen < key_maxlen);
   BV_ASSERT (keyptr[keylen] == 0);

   // Define:
   const int p1 = key_maxlen - keylen;
   // Given : (M < N) → (N - M) > 0
   // Derive: (keylen < key_maxlen)
   //       ⋀ ((M < N) → (N - M) > 0)
   //       ⊢ (key_maxlen - keylen) > 0
   // Derive: (p1 = key_maxlen - keylen)
   //       ⋀ ((key_maxlen - keylen) > 0)
   //       ⊢ p1 > 0
   BV_ASSERT (p1 > 0);
   // Define: ValMaxLen = p1 - 1
   const int val_maxlen = p1 - 1;
   // Given : (N > M) → (N-1 >= M)
   // Derive: (p1 > 0)
   //       ⋀ (N > M) → (N-1 >= M)
   //       ⊢ (p1-1 >= 0)
   // Derive: ValMaxLen = p1 - 1
   //       ⋀ (p1-1 >= 0)
   //       ⊢ ValMaxLen >= 0
   BV_ASSERT (val_maxlen >= 0);

   if (val_maxlen < 1) {
      // We require at least one byte for the document's own nul terminator
      return _bson_iterator_error (BSON_ITER_SHORT_READ);
   }

   // Assume: ValMaxLen > 0
   BV_ASSERT (val_maxlen > 0);

   const char *const valptr = keyptr + (keylen + 1);

   const int vallen =
      _bson_valsize (type, (const bson_byte *) valptr, val_maxlen);
   if (vallen < 0) {
      return _bson_iterator_error ((enum bson_iterator_error_cond) (-vallen));
   }
   return BSON2_INIT (bson_iterator){
      .ptr = data, ._rlen = maxlen, ._keylen = keylen};
}

/**
 * @brief Obtain an iterator pointing to the next element of the BSON document
 * after the given iterator's position
 *
 * @param it A valid iterator to a bson document element.
 * @return bson_iterator A new iterator. Check the `stop` member to see if
 * it is done.
 */
inline bson_iterator
bson_next (const bson_iterator it) BSON2_NOEXCEPT
{
   const int val_offset = 1            // The type
                          + it._keylen // The key
                          + 1;         // The nul after the key
   const int skip_size =
      val_offset +
      _bson_valsize (bson_iterator_type (it), it.ptr + val_offset, INT32_MAX);
   return _bson_iterator_at (it.ptr + skip_size, it._rlen - skip_size);
}

inline bson_iterator
bson_begin (bson_view v) BSON2_NOEXCEPT
{
   return _bson_iterator_at (v.data + sizeof (int32_t),
                             bson_view_len (v) - sizeof (int32_t));
}

inline bson_iterator
bson_end (bson_view v) BSON2_NOEXCEPT
{
   return _bson_iterator_at (v.data + bson_view_len (v) - 1, 1);
}

inline bool
bson_iterator_eq (bson_iterator left, bson_iterator right) BSON2_NOEXCEPT
{
   return left.ptr == right.ptr;
}

inline bool
bson_iterator_done (bson_iterator it) BSON2_NOEXCEPT
{
   return it._rlen <= 1;
}

inline bson_view_utf8
bson_iterator_utf8 (bson_iterator it) BSON2_NOEXCEPT
{
   if (bson_iterator_type (it) != BSON_TYPE_UTF8) {
      return BSON2_INIT (bson_view_utf8){NULL};
   }
   bson_byte const *after_key = _bson_iterator_value_ptr (it);
   const uint32_t len = bson_read_uint32_le ((const uint8_t *) after_key);
   if (len > it._rlen || len < 1) {
      return (bson_view_utf8){NULL};
   }
   return (bson_view_utf8){(const char *) (after_key + sizeof len),
                           (int32_t) len - 1};
}

inline bson_view
bson_iterator_document (bson_iterator it) BSON2_NOEXCEPT
{
   if (bson_iterator_type (it) != BSON_TYPE_DOCUMENT &&
       bson_iterator_type (it) != BSON_TYPE_ARRAY) {
      return BSON_VIEW_NULL;
   }
   const bson_byte *const valptr = _bson_iterator_value_ptr (it);
   const int valoffset = valptr - it.ptr;
   enum bson_view_invalid_reason err;
   bson_view r = bson_view_from_data (valptr, it._rlen - valoffset, &err);
   if (err) {
      return BSON_VIEW_NULL;
   }
   return r;
}

inline bool
bson_key_eq (const bson_iterator it, const char *key) BSON2_NOEXCEPT
{
   const bson_view_utf8 k = bson_iterator_key (it);
   return k.len == strlen (key) && memcmp (k.data, key, it._keylen) == 0;
}

inline bson_iterator
bson_find_key (bson_view v, const char *key) BSON2_NOEXCEPT
{
   bson_iterator it = bson_begin (v);
   for (; it._rlen > 1; it = bson_next (it)) {
      if (bson_key_eq (it, key)) {
         break;
      }
   }
   return it;
}

#undef BV_ASSERT

BSON2_EXTERN_C_END

#endif // BSON_BSON_VIEW_H_INCLUDED
