#include "./build.h"

extern inline bson_byte *
_bson_write_uint32le (bson_byte *, uint32_t);

extern inline bson_byte *
_bson_write_uint64le (bson_byte *, uint64_t);

extern inline bson_byte *
_bson_memcpy (bson_byte *, const void *src, uint32_t);

extern inline int32_t bson_capacity (bson_mut);

extern inline bson_byte *
bson_mut_default_reallocate (
   bson_byte *, uint32_t, uint32_t, uint32_t *, void *);

extern inline bool
_bson_mut_realloc (bson_mut *, uint32_t);

extern inline int32_t
bson_reserve (bson_mut *, uint32_t);

extern inline bson_mut
bson_mut_new_ex (const bson_mut_allocator *allocator, uint32_t reserve);


extern inline bson_mut
bson_mut_new (void);

extern inline void bson_mut_delete (bson_mut);

extern inline bson_byte *
_bson_mut_data_at (bson_mut doc, bson_iterator pos);

extern inline bson_byte *
_bson_splice_region (bson_mut *doc, bson_byte *, int32_t, int32_t);

extern inline bson_byte *
_bson_prep_element_region (
   bson_mut *, bson_iterator *, bson_type, const char *, int32_t);

extern inline bson_iterator
bson_insert_double (bson_mut *, bson_iterator, const char *, double);

extern inline bson_iterator
_bson_insert_stringlike (
   bson_mut *, bson_iterator, const char *, bson_type, const char *, int32_t);

extern inline bson_iterator
bson_insert_doc (bson_mut *, bson_iterator, const char *, bson_view);

extern inline bson_iterator
bson_insert_array (bson_mut *, bson_iterator, const char *);

extern inline bson_mut
bson_mut_subdocument (bson_mut *, bson_iterator);

extern inline bson_iterator bson_parent_iterator (bson_mut);

extern inline bson_iterator
bson_insert_binary (bson_mut *, bson_iterator, const char *, bson_binary);

extern inline bson_iterator
bson_insert_undefined (bson_mut *, bson_iterator, const char *);


extern inline bson_iterator
bson_insert_bool (bson_mut *, bson_iterator, const char *, bool);

extern inline bson_iterator
bson_insert_datetime (bson_mut *, bson_iterator, const char *, int64_t);

extern inline bson_iterator
bson_insert_null (bson_mut *, bson_iterator, const char *);

extern inline bson_iterator
bson_insert_regex (bson_mut *, bson_iterator, const char *, bson_regex);

extern inline bson_iterator
bson_insert_dbpointer (bson_mut *, bson_iterator, const char *, bson_dbpointer);

extern inline bson_iterator
bson_insert_code_with_scope (
   bson_mut *, bson_iterator, const char *, const char *, bson_view);

extern inline bson_iterator
bson_insert_int32 (bson_mut *, bson_iterator, const char *, int32_t);

extern inline bson_iterator
bson_insert_timestamp (bson_mut *,
                       bson_iterator,
                       const char *,
                       struct bson_timestamp);

extern inline bson_iterator
bson_insert_int64 (bson_mut *, bson_iterator, const char *, int64_t);

extern inline bson_iterator
bson_insert_decimal128 (bson_mut *,
                        bson_iterator,
                        const char *,
                        struct bson_decimal128);

extern inline bson_iterator
bson_insert_maxkey (bson_mut *, bson_iterator, const char *);

extern inline bson_iterator
bson_insert_minkey (bson_mut *, bson_iterator, const char *);

extern inline bson_iterator
bson_erase_range (bson_mut *doc, bson_iterator, bson_iterator);
