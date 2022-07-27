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
 * @note This structure should not be created manually. Prefer instead to use
 * @ref bson_view_from data or @ref bson_view_from_bson_t or
 * @ref bson_view_from_iter, which will validate content of the pointed-to data.
 * Use @ref BSON_VIEW_NULL to initialize a view to a "null" value.
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

inline bson_byte const *
_bson_find_after_cstring (bson_byte const *cstring,
                          bson_byte const *const end,
                          bool *okay) BSON2_NOEXCEPT
{
   *okay = true;
   long long dist = end - cstring;
   cstring += strnlen ((const char *) cstring, dist);
   if (cstring == end) {
      *okay = false;
      return NULL;
   }
   return cstring + 1;
}

/**
 * @brief A reference-like type that points to an element within a bson_view
 * document.
 */
typedef struct bson_iterator {
   /// A pointer to the beginning of the element.
   bson_byte const *ptr;
   /// The number of bytes remaining in the document
   uint32_t bytes_remaining;
   int16_t keylen;
   /**
    * @brief The stop-state of the iterator.
    *
    * If non-zero, the iterator is stopped, and attempting to advance it is
    * illegal. If non-zero, the value is one of the
    * @ref bson_view_iterator_stop_reason values to indicate the stopping
    * reason.
    */
   uint8_t stop;
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
   return (bson_view_utf8){.data = (const char *) it.ptr + 1, .len = it.keylen};
}

inline bson_type
bson_iterator_type (bson_iterator it) BSON2_NOEXCEPT
{
   return (bson_type) it.ptr[0].v;
}

inline bson_byte const *
_bson_iterator_value_ptr (bson_iterator it) BSON2_NOEXCEPT
{
   return it.ptr + 1 + it.keylen + 1;
}

/**
 * @brief Obtain an iterator pointing to the given 'data', which must be the
 * beginning of a document element.
 *
 * @param data A pointer to the type tag that starts a document element, or to
 * the null-terminator at the end of a document.
 * @param bytes_remaining The number of bytes that are available following
 * `data` before overrunning the document.
 * @return bson_iterator A new iterator, which may be stopped.
 */
inline bson_iterator
_bson_iterator_at (bson_byte const *const data,
                   uint32_t bytes_remaining) BSON2_NOEXCEPT
{
   if (bytes_remaining < 1) {
      return (bson_iterator){.stop = BSONV_ITER_STOP_INVALID};
   }
   uint8_t next_type = (bson_type) (data->v);
   if (bytes_remaining == 1 && next_type == 0) {
      return (bson_iterator){.stop = BSONV_ITER_STOP_DONE};
   }
   // 'keyptr' points to the first byte after the type tag, which begins the key
   // string
   bson_byte const *const keyptr = data + 1;
   long long keylen = strnlen ((const char *) keyptr, bytes_remaining - 1);
   if (keylen > INT16_MAX || keylen == bytes_remaining - 1) {
      // Missing a null terminator for the key, or the key is too long
      return (bson_iterator){.stop = BSONV_ITER_STOP_INVALID};
   }
   // We've found the element
   return (bson_iterator){.ptr = data,
                          .bytes_remaining = bytes_remaining,
                          .keylen = (int16_t) keylen};
}

/// Advance the given pointer by N bytes, but do not run past 'end'
inline bson_byte const *
_bson_safe_addptr (const bson_byte *const from,
                   uint32_t dist,
                   const bson_byte *const end)
{
   const int remain = end - from;
   if (remain < dist) {
      return end;
   }
   return from + dist;
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
   /// The first byte denotes the type of the element
   const uint8_t type = it.ptr[0].v;
   // For some types, we know their size in the document, and we will be able to
   // just jump over them without any switching.
   int8_t jumpsize_table[] = {
      0,  // 0x0 BSON_TYPE_EOD
      8,  // 0x1 double
      -1, // 0x2 utf8
      -1, // 0x3 document,
      -1, // 0x4 array
      -1, // 0x5 binary,
      0,  // 0x6 undefined
      12, // 0x7 OID (twelve bytes)
      1,  // 0x8 bool
      8,  // 0x9 datetime
      0,  // 0xa null
      -2, // 0xb regex
      -2, // 0xc dbpointer
      -1, // 0xd JS code
      -1, // 0xe Symbol
      -1, // 0xf Code with scope
      4,  // 0x10 int32
      8,  // 0x11 MongoDB timestamp
      8,  // 0x12 int64
      16, // 0x13 decimal128
   };
   if (type >= sizeof jumpsize_table) {
      return (bson_iterator){.stop = BSONV_ITER_STOP_INVALID_TYPE};
   }
   // 'valptr' points to the first bytes following the key (which is a null
   // terminated string)
   const bson_byte *const valptr = _bson_iterator_value_ptr (it);
   const bson_byte *const doc_end_ptr = it.ptr + it.bytes_remaining;
   const bson_byte *next_el_ptr = valptr;
   // For positive 'jump', it tells how many bytes we must jump after 'valptr'
   // to find the next element. For negative jump, we do something special
   const int32_t jump = jumpsize_table[type];
   if (jump >= 0) {
      // We have a known jump size
      next_el_ptr = _bson_safe_addptr (valptr, jump, doc_end_ptr);
   } else if (jump == -1) {
      // The next object is a simple length-prefixed object, and we can jump by
      // reading an int32
      if (it.bytes_remaining < sizeof (uint32_t)) {
         return (bson_iterator){.stop = BSONV_ITER_STOP_SHORT_READ};
      }
      const uint32_t len = bson_read_uint32_le ((const uint8_t *) valptr);
      next_el_ptr = _bson_safe_addptr (valptr, len, doc_end_ptr);
      if (type != BSON_TYPE_DOCUMENT && type != BSON_TYPE_ARRAY) {
         // The 'len' includes the length and a null terminator
         next_el_ptr =
            _bson_safe_addptr (next_el_ptr, sizeof (uint32_t), doc_end_ptr);
      }
      if (type == BSON_TYPE_BINARY) {
         // Binaries have an extra byte denoting the subtype
         next_el_ptr = _bson_safe_addptr (next_el_ptr, 1, doc_end_ptr);
      }
   } else if (jump == -2) {
      switch (type) {
      case BSON_TYPE_EOD:
      case BSON_TYPE_DOUBLE:
      case BSON_TYPE_UTF8:
      case BSON_TYPE_DOCUMENT:
      case BSON_TYPE_ARRAY:
      case BSON_TYPE_BINARY:
      case BSON_TYPE_UNDEFINED:
      case BSON_TYPE_OID:
      case BSON_TYPE_BOOL:
      case BSON_TYPE_DATE_TIME:
      case BSON_TYPE_NULL:
      case BSON_TYPE_CODE:
      case BSON_TYPE_SYMBOL:
      case BSON_TYPE_CODEWSCOPE:
      case BSON_TYPE_INT32:
      case BSON_TYPE_INT64:
      case BSON_TYPE_TIMESTAMP:
      case BSON_TYPE_DECIMAL128:
      case BSON_TYPE_MINKEY:
      case BSON_TYPE_MAXKEY:
         abort ();
      case BSON_TYPE_REGEX: {
         const char *strptr = (const char *) valptr;
         int32_t maxlen = doc_end_ptr - valptr;
         const int re_len = strnlen (strptr, maxlen);
         if (re_len != maxlen) {
            strptr += re_len + 1;
            maxlen -= re_len + 1;
            const int opt_len = strnlen (strptr, maxlen);
            if (opt_len != maxlen) {
               strptr += opt_len + 1;
               next_el_ptr = (const bson_byte *) strptr;
            }
         }
         if (valptr == next_el_ptr) {
            // This condition occurs iff we encountered an invalid regex value,
            // as a valid regex+opt skip will always advance at least two bytes
            // forward.
            return (bson_iterator){.stop = BSONV_ITER_STOP_INVALID};
         }
         break;
      }
      case BSON_TYPE_DBPOINTER: {
         if (it.bytes_remaining < sizeof (uint32_t)) {
            return (bson_iterator){.stop = BSONV_ITER_STOP_SHORT_READ};
         }
         const uint32_t len = bson_read_uint32_le ((const uint8_t *) valptr);
         next_el_ptr = _bson_safe_addptr (valptr, len, doc_end_ptr);
         next_el_ptr = _bson_safe_addptr (next_el_ptr, 12, doc_end_ptr);
         break;
      }
      }
   }

   return _bson_iterator_at (next_el_ptr, doc_end_ptr - next_el_ptr);
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

inline bson_view_utf8
bson_iterator_utf8 (bson_iterator it) BSON2_NOEXCEPT
{
   if (it.stop || bson_iterator_type (it) != BSON_TYPE_UTF8) {
      return BSON2_INIT (bson_view_utf8){NULL};
   }
   bson_byte const *after_key = _bson_iterator_value_ptr (it);
   const uint32_t len = bson_read_uint32_le ((const uint8_t *) after_key);
   if (len > it.bytes_remaining || len < 1) {
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
   bson_view r =
      bson_view_from_data (valptr, it.bytes_remaining - valoffset, &err);
   if (err) {
      return BSON_VIEW_NULL;
   }
   return r;
}

inline bool
bson_key_eq (const bson_iterator it, const char *key) BSON2_NOEXCEPT
{
   const bson_view_utf8 k = bson_iterator_key (it);
   return k.len == strlen (key) && memcmp (k.data, key, it.keylen) == 0;
}

inline bson_iterator
bson_find_key (bson_view v, const char *key) BSON2_NOEXCEPT
{
   bson_iterator it = bson_begin (v);
   for (; !it.stop; it = bson_next (it)) {
      if (bson_key_eq (it, key)) {
         break;
      }
   }
   return it;
}

enum bson_view_iterator_stop_reason
   bson_validate_untrusted (bson_view) BSON2_NOEXCEPT;

BSON2_EXTERN_C_END

#endif // BSON_BSON_VIEW_H_INCLUDED
