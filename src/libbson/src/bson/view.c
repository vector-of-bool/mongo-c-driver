#include "./view.h"

#include <string.h>

#include <bson/bson.h>

extern inline uint32_t
_bson_read_uint32_le (const bson_byte *bytes);

extern inline uint32_t bson_view_len (bson_view);

extern inline bson_view
bson_view_from_data (const bson_byte *const data,
                     const size_t len,
                     enum bson_view_invalid_reason *const error);

extern inline bson_iterator
_bson_iterator_at (bson_byte const *const data, int32_t bytes_remaining);

extern inline bson_byte const *
_bson_safe_addptr (const bson_byte *const from,
                   uint32_t dist,
                   const bson_byte *const end);

extern inline bson_iterator
bson_next (const bson_iterator it);

extern inline bson_utf8_view
bson_iterator_key (bson_iterator it);

extern inline bson_type
bson_iterator_type (bson_iterator it);

extern inline bson_byte const *
_bson_iterator_value_ptr (bson_iterator iter);

extern inline bson_iterator
bson_begin (bson_view v);

extern inline bson_iterator
bson_end (bson_view v);

extern inline bool
bson_iterator_eq (bson_iterator left, bson_iterator right);

extern inline bool
bson_iterator_done (bson_iterator it);

extern inline double
bson_iterator_double (bson_iterator it);

extern inline bson_utf8_view
bson_iterator_utf8 (bson_iterator it);

extern inline bool
bson_key_eq (const bson_iterator, const char *key);

extern inline bson_iterator
bson_view_find (bson_view v, const char *key);

inline bson_view
bson_iterator_document (bson_iterator it, enum bson_view_invalid_reason *error);

extern inline void
_bson_view_assert (bool b, const char *expr, const char *file, int line);

extern inline int32_t
_bson_value_re_len (const char *valptr, int32_t maxlen);

extern inline int32_t
_bson_valsize (bson_type tag,
               bson_byte const *const valptr,
               int32_t val_maxlen);

extern inline bson_iterator
_bson_iterator_error (enum bson_iterator_error_cond err);

void
_bson_assert_fail (const char *expr, const char *file, int line)
{
   fprintf (stderr,
            "bson/view ASSERTION FAILED at %s:%d: Expression [%s] evaluated to "
            "false\n",
            file,
            line,
            expr);
   abort ();
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
