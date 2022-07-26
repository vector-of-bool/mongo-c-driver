#ifndef BSON_BSON_VIEW_H_INCLUDED
#define BSON_BSON_VIEW_H_INCLUDED

#include <bson/bson.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A type that is the size of a byte, but does not alias with other types
 * (except char)
 */
typedef struct bson_byte {
   /// The 8-bit value of the byte
   uint8_t v;
} bson_byte;

typedef struct bson_view_untrusted {
   /// The pointed-to data of the document
   bson_byte const *data;
} bson_view_untrusted;

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
#define BSON_VIEW_NULL ((bson_view){NULL})
#define BSON_VIEW_UNTRUSTED_NULL ((bson_view_untrusted){NULL})

/**
 * @brief Obtain the byte-size of the BSON document referred to by the given
 * bson_view.
 *
 * @param v A bson_view to inspect
 * @return uint32_t The size (in bytes) of the viewed document, or zero if `v`
 * is BSON_VIEW_NULL
 */
static inline uint32_t
bson_view_len (bson_view v)
{
   // A null document has length=0
   if (!v.data) {
      return 0;
   }
   // The length of a document is contained in a four-bytes little-endian
   // encoded integer. This size includes the header, the document body, and the
   // null terminator byte on the document.
   uint32_t len;
   memcpy (&len, v.data, sizeof len);
   return BSON_UINT32_FROM_LE (len);
}

/**
 * @brief Obtain the byte-size of the untrusted BSON document referred to by the
 * given bson_view_untrusted.
 *
 * @see bson_view_len
 *
 * @see bson_view_from_untrusted_data() will validate that the root document at
 * least has the correct size and null terminator. The contents of the document
 * are not validated.
 */
static inline uint32_t
bson_view_ut_len (bson_view_untrusted v)
{
   if (!v.data) {
      return 0;
   }
   uint32_t len;
   memcpy (&len, v.data, sizeof len);
   return BSON_UINT32_FROM_LE (len);
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
 * @return bson_view_untrusted A view, or BSON_VIEW_UNTRUSTED_NULL if the length
 * is invalid.
 *
 * @note IMPORTANT: This function does not error if 'data_len' is longer than
 * the document. This allows support of buffered reads of unknown sizes. The
 * actual length of the resulting document can be obtained with
 * @ref bson_view_ut_len.
 */
static inline bson_view_untrusted
bson_view_from_untrusted_data (const bson_byte *const data,
                               const size_t data_len,
                               enum bson_view_invalid_reason *const error)
{
   bson_view_untrusted ret = BSON_VIEW_UNTRUSTED_NULL;
   uint32_t len = 0;
   enum bson_view_invalid_reason err = BSON_VIEW_OKAY;
   // All BSON data must be at least five bytes long
   if (data_len < 5) {
      err = BSON_VIEW_SHORT_READ;
      goto done;
   }
   // Read the length header. This includes the header's four bytes, the
   // document's element data, and the null terminator byte.
   memcpy (&len, data, sizeof len);
   len = BSON_UINT32_FROM_LE (len);
   // Check that the size is in-bounds
   if (len > BSON_MAX_SIZE || len < 5) {
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

static inline bson_view
bson_view_from_trusted_data (const bson_byte *data, size_t data_len)
{
   bson_view_untrusted ut =
      bson_view_from_untrusted_data (data, data_len, NULL);
   bson_view r;
   r.data = ut.data;
   return r;
}

/**
 * @brief Obtain a bson_t from a given bson_view.
 *
 * @param view The view to convert to a bson_t value.
 * @return bson_t A non-owning bson_t value. Should not be destroyed.
 */
static inline bson_t
bson_view_as_viewing_bson_t (bson_view view)
{
   bson_t r;
   bson_init_static (&r, (uint8_t const *) view.data, bson_view_len (view));
   return r;
}

/**
 * @brief Create a bson_view that refers to a bson_t
 *
 * @param b A pointer to a bson_t, or NULL
 * @return bson_view A view of `b`. If `b` is NULL, returns BSON_VIEW_NULL
 */
static inline bson_view_untrusted
bson_view_from_bson_t (const bson_t *b)
{
   if (b == NULL) {
      return BSON_VIEW_UNTRUSTED_NULL;
   } else {
      return bson_view_from_untrusted_data (
         (bson_byte const *) bson_get_data (b), b->len, NULL);
   }
}


/**
 * @brief Create a new bson_view that views the document referred to by the
 * given bson_iter_t
 *
 * @param it A bson_iter_t.
 * @return bson_view A view of the pointed-to document, or NULL if bson_iter_t
 * does not point to a document/array value.
 */
static inline bson_view
bson_view_from_iter (bson_iter_t it)
{
   const bson_type_t type = (bson_type_t) (it.raw + it.type)[0];
   if (type == BSON_TYPE_DOCUMENT || type == BSON_TYPE_ARRAY) {
      return bson_view_from_trusted_data ((bson_byte const *) (it.raw + it.d1),
                                          it.len);
   }
   return BSON_VIEW_NULL;
}

/**
 * @brief Create a bson_t that copies the given bson_view's data
 *
 * @param v A bson_view. If null, returns a NULL bson_t*
 * @return bson_t* A new pointer-to-bson_t. Must be destroyed with
 * bson_destroy()
 */
static inline bson_t *
bson_view_copy (bson_view v)
{
   if (v.data == NULL) {
      return NULL;
   } else {
      return bson_new_from_data ((const uint8_t *) v.data, bson_view_len (v));
   }
}

/**
 * @brief The stop-state of a bson_view_iterator
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
 * document
 */
typedef struct bson_view_iterator {
   /// A pointer to the beginning of the element key.
   bson_byte const *keyptr;
   /// A pointer to the beginning of the element value.
   bson_byte const *dataptr;
   /// The number of bytes remaining in the document
   uint32_t bytes_remaining;
   /// The type of the current element value. One of @ref bson_type_t
   uint8_t type;
   /**
    * @brief The stop-state of the iterator.
    *
    * If non-zero, the iterator is stopped, and attempting to advance it is
    * illegal. If non-zero, the value is one of the
    * @ref bson_view_iterator_stop_reason values to indicate the stopping
    * reason.
    */
   uint8_t stop;
} bson_view_iterator;

/**
 * @brief Obtain an iterator pointing to the given 'data', which must be the
 * beginning of a document element.
 *
 * @param data A pointer to the type tag that starts a document element
 * @param bytes_remaining The number of bytes that are available following
 * `data`
 * @return bson_view_iterator A new iterator, which may be stopped.
 */
static inline bson_view_iterator
_bson_view_iterator_at (bson_byte const *const data, uint32_t bytes_remaining)
{
   if (bytes_remaining == 0) {
      return (bson_view_iterator){.stop = BSONV_ITER_STOP_INVALID};
   }
   bson_type_t next_type = (bson_type_t) (data->v);
   --bytes_remaining;
   if (bytes_remaining == 0 && next_type == BSON_TYPE_EOD) {
      return (bson_view_iterator){.stop = BSONV_ITER_STOP_DONE};
   }
   // Point at the byte after the type tag
   bson_byte const *ptr = data + 1;
   // If the type tag is zero, we've hit the end of the document
   // 'keyptr' points to the first byte after the type tag
   bson_byte const *const keyptr = ptr;
   // Advance until we run out of data or we find a null terminator
   while (bytes_remaining && ptr->v != 0) {
      ++ptr;
      --bytes_remaining;
   }
   if (!bytes_remaining) {
      return (bson_view_iterator){.stop = BSONV_ITER_STOP_SHORT_READ};
   }
   --bytes_remaining;
   ++ptr;
   // We've found the element
   return (bson_view_iterator){
      keyptr, ptr, bytes_remaining, (uint8_t) next_type, 0};
}

static inline bson_byte const *
_bson_find_after_cstring (bson_byte const *cstring,
                          bson_byte const *const end,
                          bool *okay)
{
   *okay = true;
   ssize_t dist = end - cstring;
   cstring += strnlen ((const char *) cstring, dist);
   if (cstring == end) {
      *okay = false;
      return NULL;
   }
   return cstring + 1;
}

/**
 * @brief Obtain an iterator pointing to the next element of the BSON document
 * after the given iterator's position
 *
 * @param it A valid iterator to a bson document element.
 * @return bson_view_iterator A new iterator. Check the `stop` member to see if
 * it is done.
 */
static inline bson_view_iterator
bson_view_next (const bson_view_iterator it)
{
   bson_type_t type = (bson_type_t) it.type;
   // For some types, we know their size in the document, and we will be able to
   // just jump over them without any switching.
   int8_t jumpsize_table[] = {
      0,  // BSON_TYPE_EOD
      8,  // double
      -1, // utf8
      -1, // document,
      -1, // array
      -1, // binary,
      0,  // undefined
      12, // OID (twelve bytes)
      1,  // bool
      8,  // datetime
      0,  // null
      -2, // regex
      -2, // dbpointer
      -1, // JS code
      -1, // Symbol
      -1, // Code with scope
      4,  // int32
      8,  // int64
      16, // decimal128
      0,  // minkey
      0,  // maxkey
   };
   if (it.type >= sizeof jumpsize_table) {
      return (bson_view_iterator){.stop = BSONV_ITER_STOP_INVALID_TYPE};
   }
   int32_t jump = jumpsize_table[it.type];
   if (jump >= 0) {
      // We have a known jump size
   } else if (jump == -1) {
      // The next object is a simple length-prefixed object, and we can jump by
      // reading and int32
      int32_t len;
      if (it.bytes_remaining < sizeof len) {
         return (bson_view_iterator){.stop = BSONV_ITER_STOP_SHORT_READ};
      }
      memcpy (&len, it.dataptr, sizeof len);
      len = BSON_UINT32_FROM_LE (len);
      jump = len;
      if (type != BSON_TYPE_DOCUMENT && type != BSON_TYPE_ARRAY) {
         // We need to jump over the length header too
         jump += sizeof len;
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
         BSON_UNREACHABLE ("Invalid type jump");
      case BSON_TYPE_REGEX: {
         uint32_t remain = it.bytes_remaining;
         // Consume the first regex
         int l1 = strnlen ((const char *) it.dataptr, remain);
         remain -= l1;
         jump += l1;
         // Consume the option string
         if (remain) {
            ++jump;
            --remain;
         }
         int l2 = strnlen ((const char *) (it.dataptr + jump), remain);
         jump += l2;
         remain -= l2;
         // "jump" now jumps over the two strings
         break;
      }
      case BSON_TYPE_DBPOINTER: {
         uint32_t len;
         if (it.bytes_remaining < sizeof len) {
            return (bson_view_iterator){.stop = BSONV_ITER_STOP_SHORT_READ};
         }
         memcpy (&len, it.dataptr, sizeof len);
         len = BSON_UINT32_FROM_LE (len);
         if (UINT32_MAX - len < 12) {
            return (bson_view_iterator){.stop = BSONV_ITER_STOP_SHORT_READ};
         }
         uint32_t ujump = len + 12u;
         if (ujump > it.bytes_remaining) {
            return (bson_view_iterator){.stop = BSONV_ITER_STOP_SHORT_READ};
         }
         jump = ujump;
         break;
      }
      }
   }

   if (jump > it.bytes_remaining) {
      return (bson_view_iterator){.stop = BSONV_ITER_STOP_SHORT_READ};
   }

   return _bson_view_iterator_at (it.dataptr + jump, it.bytes_remaining - jump);
}

static inline bson_view_iterator
bson_view_begin (bson_view v)
{
   bson_view_iterator it = {
      v.data + sizeof (int32_t),                       // keyptr
      v.data + sizeof (int32_t),                       // dataptr
      bson_view_len (v) - (uint32_t) sizeof (int32_t), // bytes_remaining
      0x0a,                                            // type
   };
   return bson_view_next (it);
}

static inline const char *
bson_view_iter_key (bson_view_iterator it)
{
   return (const char *) it.keyptr;
}

typedef struct bson_view_utf8 {
   const char *data;
   int32_t len;
} bson_view_utf8;

static inline bson_view_utf8
bson_view_iter_as_utf8 (bson_view_iterator it)
{
   uint32_t len;
   memcpy (&len, it.dataptr, sizeof len);
   len = BSON_UINT32_FROM_LE (len);
   if (len > it.bytes_remaining) {
      return (bson_view_utf8){.data = NULL};
   }
   return (bson_view_utf8){(const char *) (it.dataptr + sizeof len),
                           (int32_t) len};
}

typedef struct bson_validation_result {
   enum bson_view_iterator_stop_reason error;
   bson_view view;
} bson_validation_result;

bson_validation_result bson_validate_untrusted (bson_view_untrusted);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // BSON_BSON_VIEW_H_INCLUDED
