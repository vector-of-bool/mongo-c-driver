#include "./view.h"

#include <string.h>

#include <bson/bson.h>

extern inline uint32_t
bson_read_uint32_le (const uint8_t *bytes);

extern inline uint32_t bson_view_len (bson_view);

extern inline bson_view
bson_view_from_data (const bson_byte *const data,
                     const size_t len,
                     enum bson_view_invalid_reason *const error);

extern inline bson_iterator
_bson_iterator_at (bson_byte const *const data, uint32_t bytes_remaining);

extern inline bson_byte const *
_bson_safe_addptr (const bson_byte *const from,
                   uint32_t dist,
                   const bson_byte *const end);

extern inline bson_iterator
bson_next (const bson_iterator it);

extern inline bson_view_utf8
bson_iterator_key (bson_iterator it);

extern inline bson_type
bson_iterator_type (bson_iterator it);

extern inline bson_byte const *
_bson_iterator_value_ptr (bson_iterator it);

extern inline bson_iterator
bson_begin (bson_view v);

extern inline bson_iterator
bson_end (bson_view v);

extern inline bson_view_utf8
bson_iterator_utf8 (bson_iterator it);

extern inline bool
bson_key_eq (const bson_iterator, const char *key);

extern inline bson_iterator
bson_find_key (bson_view v, const char *key);

extern inline bson_view
bson_iterator_document (bson_iterator it);

typedef struct validation_iterator {
   const bson_byte *dataptr;
   size_t bytes_remaining;
} validation_iterator;


enum bson_view_iterator_stop_reason
bson_validate_untrusted (bson_view view)
{
   // return _validate_document (view.data, view.data + bson_view_len (view));
   return 0;
}


bson_t *
bson_view_copy_as_bson_t (bson_view v)
{
   if (!v.data) {
      return NULL;
   }
   return bson_new_from_data ((const uint8_t *) v.data, bson_view_len (v));
}

bson_view
bson_view_from_bson_iter_t (bson_iter_t it)
{
   const bson_type_t type = (bson_type_t) (it.raw + it.type)[0];
   if (type == BSON_TYPE_DOCUMENT || type == BSON_TYPE_ARRAY) {
      return bson_view_from_data (
         (bson_byte const *) (it.raw + it.d1), it.len, NULL);
   }
   return BSON_VIEW_NULL;
}

bson_view
bson_view_from_bson_t (const bson_t *b)
{
   if (!b) {
      return BSON_VIEW_NULL;
   }
   return bson_view_from_data (
      (const bson_byte *) bson_get_data (b), b->len, NULL);
}

bson_t
bson_view_as_viewing_bson_t (bson_view v)
{
   bson_t r;
   bson_init_static (&r, (const uint8_t *) v.data, bson_view_len (v));
   return r;
}
