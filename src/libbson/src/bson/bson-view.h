#ifndef BSON_BSON_VIEW_H_INCLUDED
#define BSON_BSON_VIEW_H_INCLUDED

#include <bson/bson.h>

/**
 * @brief A type specifically representing a nullable read-only view of a BSON
 * document.
 */
typedef struct bson_view {
   /**
    * @brief Pointer to the beginning of the referenced BSON document, or NULL.
    *
    * If NULL, the view refers to nothing.
    */
   const char *data;
} bson_view;

/// A "null" constant expression for bson_view objectss
#define BSON_VIEW_NULL ((bson_view){.data = NULL})

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
   if (!v.data) {
      return 0;
   }
   uint32_t len;
   memcpy (&len, v.data, sizeof len);
   return BSON_UINT32_FROM_LE (len);
}

enum bson_view_invalid_reason {
   BSON_VIEW_OKAY,
   BSON_VIEW_SHORT_READ,
   BSON_VIEW_OVERFLOW,
   BSON_VIEW_INVALID_HEADER,
   BSON_VIEW_INVALID_TERMINATOR,
};

/**
 * @brief View the given pointed-to data as a BSON document.
 *
 * @param data A pointer to the beginning of a BSON document.
 * @param len The length of the pointed-to data buffer.
 * @return bson_view A view, or BSON_VIEW_NULL if the length is invalid.
 */
static inline bson_view
bson_view_from_data (const void *data,
                     size_t data_len,
                     enum bson_view_invalid_reason *error)
{
   bson_view ret = BSON_VIEW_NULL;
   enum bson_view_invalid_reason err = BSON_VIEW_OKAY;
   // All BSON data must be at least five bytes long
   if (data_len < 5) {
      err = BSON_VIEW_SHORT_READ;
      goto done;
   }
   // Read the length header
   uint32_t len = 0;
   memcpy (&len, data, sizeof len);
   len = BSON_UINT32_FROM_LE (len);
   if (len > BSON_MAX_SIZE || len < 5) {
      err = BSON_VIEW_INVALID_HEADER;
      goto done;
   }
   if (len > data_len) {
      // Not enough data to do the read
      err = BSON_VIEW_SHORT_READ;
      goto done;
   }
   // Get the view
   bson_view r = {.data = data};
   if (r.data[len - 1] != 0) {
      err = BSON_VIEW_INVALID_TERMINATOR;
      goto done;
   }
   ret = r;
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
 * @return bson_t A new non-owning bson_t. Should not be destroyed.
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
static inline bson_view
bson_view_from_bson_t (const bson_t *b)
{
   if (b == NULL) {
      return BSON_VIEW_NULL;
   } else {
      return bson_view_from_data (bson_get_data (b), b->len, NULL);
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
   const bson_type_t type = bson_iter_type (&it);
   if (type == BSON_TYPE_DOCUMENT || type == BSON_TYPE_ARRAY) {
      return bson_view_from_data (it.raw + it.d1, it.len, NULL);
   }
   return BSON_VIEW_NULL;
}

static inline bson_t *
bson_view_copy (bson_view v)
{
   if (v.data == NULL) {
      return NULL;
   } else {
      return bson_new_from_data ((const uint8_t *) v.data, bson_view_len (v));
   }
}

/// The reason that a bson_view_iterator might stop
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
   const char *keyptr;
   /// A pointer to the beginning of the element value.
   const char *dataptr;
   /// The remaining bytes in the document
   uint32_t bytes_remaining;
   /// The type of the element value
   uint8_t type;
   /// Whether this iterator has reached the end, and a reason why it reached
   /// the end.
   uint8_t stop;
} bson_view_iterator;


extern inline bson_view_iterator
_bson_view_iterator_at (const char *const data, uint32_t bytes_remaining)
{
   if (bytes_remaining == 0) {
      return (bson_view_iterator){.stop = BSONV_ITER_STOP_INVALID};
   }
   bson_type_t next_type = (bson_type_t) (*data);
   --bytes_remaining;
   if (bytes_remaining == 0 && next_type == BSON_TYPE_EOD) {
      return (bson_view_iterator){.stop = BSONV_ITER_STOP_DONE};
   }
   // Point at the byte after the type tag
   const char *ptr = data + 1;
   // If the type tag is zero, we've hit the end of the document
   // 'keyptr' points to the first byte after the type tag
   const char *const keyptr = ptr;
   // Advance until we run out of data or we find a null terminator
   while (bytes_remaining && *ptr != 0) {
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
      .keyptr = keyptr,
      .dataptr = ptr,
      .bytes_remaining = bytes_remaining,
      .type = next_type,
      .stop = 0,
   };
}

extern inline bson_view_iterator
bson_view_next (const bson_view_iterator it)
{
   bson_type_t type = (bson_type_t) it.type;
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
      if (jump > it.bytes_remaining) {
         return (bson_view_iterator){.stop = BSONV_ITER_STOP_SHORT_READ};
      }
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
         while (remain && it.dataptr[jump] != 0) {
            --remain;
            ++jump;
         }
         // Consume the option string
         if (remain) {
            ++jump;
            --remain;
         }
         while (remain && it.dataptr[jump] != 0) {
            --remain;
            ++jump;
         }
         // "jump" now jumps over the two strings
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
      }
      }
   }

   return _bson_view_iterator_at (it.dataptr + jump, it.bytes_remaining - jump);
}

static inline bson_view_iterator
bson_view_begin (bson_view v)
{
   bson_view_iterator it = {
      .keyptr = v.data + sizeof (int32_t),
      .dataptr = v.data + sizeof (int32_t),
      .bytes_remaining = bson_view_len (v) - sizeof (int32_t),
      .type = 0x0a,
   };
   return bson_view_next (it);
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
   return (bson_view_utf8){.data = it.dataptr + sizeof len, .len = len};
}

#endif // BSON_BSON_VIEW_H_INCLUDED
