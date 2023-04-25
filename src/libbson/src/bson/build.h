#ifndef BSON_BUILD_H_INCLUDED
#define BSON_BUILD_H_INCLUDED

#include "./view.h"

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stddef.h>

/**
 * @brief Assert the truth of the given expression. In checked mode, this is a
 * runtime assertion. In unchecked mode, this becomes an optimization hint only.
 */
#define BV_ASSERT(Cond)                              \
   if (BSON_VIEW_CHECKED && !(Cond)) {               \
      _bson_assert_fail (#Cond, __FILE__, __LINE__); \
      abort ();                                      \
   } else if (!(Cond)) {                             \
      __builtin_unreachable ();                      \
   } else                                            \
      ((void) 0)

BSON2_EXTERN_C_BEGIN

/**
 * @internal
 * @brief Write a little-endian 32-bit unsigned integer into the given memory
 * location.
 *
 * @param bytes The destination to write into
 * @param v The value to write
 */
inline bson_byte *
_bson_write_uint32le (bson_byte *bytes, uint32_t v) BSON2_NOEXCEPT
{
   bytes[0].v = (v >> 0) & 0xff;
   bytes[1].v = (v >> 8) & 0xff;
   bytes[2].v = (v >> 16) & 0xff;
   bytes[3].v = (v >> 24) & 0xff;
   return bytes + 4;
}

/**
 * @internal
 * @brief Write a little-endian 64-bit unsigned integer into the given memory
 * location.
 *
 * @param bytes The destination of the write
 * @param v  The value to write
 */
inline bson_byte *
_bson_write_uint64le (bson_byte *out, uint64_t v) BSON2_NOEXCEPT
{
   out = _bson_write_uint32le (out, (uint32_t) v);
   out = _bson_write_uint32le (out, v >> 32);
   return out;
}

inline bson_byte *
_bson_memcpy (bson_byte *out, const void *src, uint32_t len) BSON2_NOEXCEPT
{
   if (src && len) {
      memcpy (out, src, len);
   }
   return out + len;
}

/**
 * @brief Type of the function used to manage memory
 *
 * @param ptr Pointer to the memory that was previously allocated, or NULL if
 * no data has been allocated previously.
 * @param requested_size The requested allocation size, or zero to free the
 * memory.
 * @param previous_size The previous size of the allocated region, or zero.
 * @param out_new_size Upon success, the allocator MUST write the actual size
 * of memory that was allocated to this address.
 * @param userdata Arbitrary pointer that was given when the allocator was
 * created.
 *
 * @returns A pointer to the allocated memory, or NULL on allocation failure.
 *
 * @note The allocation function is responsible for copying data from the
 * prior memory region into the new memory region.
 */
typedef bson_byte *(*bson_mut_allocator_fn) (bson_byte *ptr,
                                             uint32_t requested_size,
                                             uint32_t previous_size,
                                             uint32_t *out_new_size,
                                             void *userdata);

/**
 * @brief Type used to control memory allocation in a @ref bson_mut, given to
 * @ref bson_mut_new_ex
 */
typedef struct bson_mut_allocator {
   /**
    * @brief The function used to allocate memory for a bson_mut. Refer to
    * @ref bson_mut_allocator_fn for signature information
    */
   bson_mut_allocator_fn reallocate;
   /**
    * @brief An arbitrary pointer. Will be passed to `reallocate()` when it is
    * called.
    */
   void *userdata;
} bson_mut_allocator;

/**
 * @brief A mutable BSON document.
 *
 * This type is trivially relocatable.
 *
 * @internal
 *
 * The sign bit of `_capacity_or_negative_offset_within_parent_data` is used as
 * a binary flag to control the interpretation of the data members, since the
 * sign bit would otherwise be unused.
 *
 * If _capacity_or_negative_offset_within_parent_data is less than zero, the
 * whole object is in CHILD MODE, otherwise it is in ROOT MODE
 */
typedef struct bson_mut {
   /**
    * @brief Points to the beginning of the document data.
    *
    * @internal
    *
    * In CHILD MODE, this is a non-owning pointer. It should not be freed or
    * reallocated.
    *
    * In ROOT MODE, this is an owning pointer and can be freed or reallocated.
    */
   bson_byte *_bson_document_data;
   /**
    * @brief The parent mutator, or the allocator context
    *
    * @internal
    *
    * In CHILD MODE, this points to a `bson_mut` that manages the document that
    * contains this document.
    *
    * In ROOT_MODE, this is a pointer to a `bson_mut_allocator`, to be used to
    * manage the `_bson_document_data` pointer.
    */
   void *_parent_mut_or_allocator_context;
   /**
    * @brief The capacity of the data buffer, or the offset of the element
    * within its parent.
    *
    * @internal
    *
    * If zero or positive, we are in ROOT MODE. If negative, we are in
    * CHILD MODE.
    *
    * In ROOT MODE, this refers to the number of bytes available in
    * `_bosn_document_data` that are available for writing before we must
    * reallocate the region.
    *
    * In CHILD MODE, this is the negative value of the byte-offset of the
    * document /element/ within the parent document. This is used to quickly
    * recover a bson_iterator that refers to the document element within the
    * parent, since iterators point to the element data. This is also necessary
    * to quickly compute the element key length without a call to strlen(),
    * since we can compute the key length based on the different between the
    * element's address and the address of the document data within the element.
    */
   int32_t _capacity_or_negative_offset_within_parent_data;
} bson_mut;

/**
 * @brief Compute the number of bytes available before we will require
 * reallocating
 *
 * @param d The document to inspect
 * @return int32_t The capacity of the document `d`
 *
 * @note If `d` is a child document, the return value reflects the number of
 * bytes for the maximum size of `d` until which we will require reallocating
 * the parent, transitively.
 */
inline int32_t
bson_capacity (bson_mut d)
{
   if (d._capacity_or_negative_offset_within_parent_data < 0) {
      // We are a subdocument, so we need to calculate the capacity of our
      // document in the context of the parent's capacity.
      bson_mut parent = *(bson_mut *) d._parent_mut_or_allocator_context;
      // Number of bytes between the parent start and this element start:
      int32_t bytes_before = -d._capacity_or_negative_offset_within_parent_data;
      // Number of bytes from the element start until the end of the parent:
      int32_t bytes_until_parent_end = bson_ssize (parent) - bytes_before;
      // Num bytes in the parent after this document element:
      int32_t bytes_after = bytes_until_parent_end - bson_ssize (d);
      // Number of bytes in the parent that are not our own:
      int32_t bytes_other = bytes_before + bytes_after;
      // The number of bytes we can grow to until we will break the capacity of
      // the parent:
      int32_t remaining = bson_capacity (parent) - bytes_other;
      return remaining;
   }
   return d._capacity_or_negative_offset_within_parent_data;
}

/**
 * @brief The default reallocation function for a bson_mut
 *
 * This function is used to manage the data buffers of a bson_mut as it is
 * manipulated. This is implemented in terms of realloc() and free() from
 * stdlib.h
 */
inline bson_byte *
bson_mut_default_reallocate (bson_byte *previous,
                             uint32_t request_size,
                             uint32_t prev_size,
                             uint32_t *actual_size,
                             void *userdata)
{
   // An allocation of zero is a request to free:
   if (request_size == 0) {
      free (previous);
      *actual_size = 0;
      return NULL;
   }
   // Reallocate:
   bson_byte *const p = (bson_byte *) realloc (previous, request_size);
   if (!p) {
      // Failure:
      return NULL;
   }
   // Tell the caller how much we allocated
   *actual_size = request_size;
   return p;
}

/**
 * @internal
 * @brief Reallocate the data buffer of a bson_mut
 *
 * @param m The mut object that is being resized
 * @param new_size The desired size of the data buffer
 * @retval true Upon success
 * @retval false Otherwise
 *
 * All iterators and pointers to the underlying data are invalidated
 */
inline bool
_bson_mut_realloc (bson_mut *m, uint32_t new_size)
{
   // We are only called on ROOT MODE bson_muts
   BV_ASSERT (m->_capacity_or_negative_offset_within_parent_data >= 0);
   // Cap allocation size:
   if (new_size > INT32_MAX) {
      return false;
   }
   // Get the allocator:
   bson_mut_allocator *alloc =
      (bson_mut_allocator *) m->_parent_mut_or_allocator_context;
   // Perform the reallocation:
   uint32_t got_size = 0;
   bson_byte *newptr =
      alloc->reallocate (m->_bson_document_data,
                         new_size,
                         m->_capacity_or_negative_offset_within_parent_data,
                         &got_size,
                         alloc->userdata);
   if (!newptr) {
      // The allocatore reports failure
      return false;
   }
   // Assert that the allocator actually gave us the space we requested:
   BV_ASSERT (got_size >= new_size);
   // Save the new pointer
   m->_bson_document_data = newptr;
   // Remember the buffer size that the allocator gave us:
   m->_capacity_or_negative_offset_within_parent_data = got_size;
   return true;
}

/**
 * @brief Adjust the capacity of the given bson_mut
 *
 * @param d The document to update
 * @param size The requested capacity
 * @return int32_t Returns the new capacity upon success, otherwise a negative
 * value
 *
 * @note If `size` is less than our current capacity, the capacity will remain
 * unchanged.
 *
 * All outstanding iterators and pointers are invalidated if the requested size
 * is greater than our current capacity.
 */
inline int32_t
bson_reserve (bson_mut *d, uint32_t size)
{
   BV_ASSERT (d->_capacity_or_negative_offset_within_parent_data >= 0 &&
              "Called bson_reserve() on child bson_mut");
   if (bson_capacity (*d) >= size) {
      return bson_capacity (*d);
   }
   if (!_bson_mut_realloc (d, size)) {
      return -1;
   }
   return d->_capacity_or_negative_offset_within_parent_data;
}

/**
 * @brief Create a new bson_mut with an allocator and reserved size
 *
 * @param allocator A custom allocator, or NULL for the default allocator
 * @param reserve The size to reserve within the new document
 * @return bson_mut A new mutator. Must be deleted with bson_mut_delete()
 */
inline bson_mut
bson_mut_new_ex (const bson_mut_allocator *allocator, uint32_t reserve)
{
   // The default allocator, stored as a static object:
   static const bson_mut_allocator default_vtab = {
      .reallocate = bson_mut_default_reallocate,
   };
   if (allocator == NULL) {
      // They didn't provide an allocator: Use the default one
      allocator = &default_vtab;
   }
   // Create the object:
   bson_mut r = {0};
   r._capacity_or_negative_offset_within_parent_data = 0;
   r._parent_mut_or_allocator_context = (void *) allocator;
   if (reserve < 5) {
      // An empty document requires *at least* five bytes:
      reserve = 5;
   }
   // Create the initial storage:
   if (!bson_reserve (&r, reserve)) {
      return r;
   }
   // Set an initial empty document:
   memset (r._bson_document_data,
           0,
           r._capacity_or_negative_offset_within_parent_data);
   r._bson_document_data[0].v = 5;
   return r;
}

/**
 * @brief Create a new empty document for later manipulation
 *
 * @note The return value must later be deleted using bson_mut_delete
 */
inline bson_mut
bson_mut_new (void)
{
   return bson_mut_new_ex (NULL, 512);
}

/**
 * @brief Free the resources of the given BSON document
 */
inline void
bson_mut_delete (bson_mut d)
{
   _bson_mut_realloc (&d, 0);
}

/**
 * @internal
 * @brief Obtain a pointer to the element data at the given iterator position
 *
 * @param doc The document that owns the element
 * @param pos An element iterator
 */
inline bson_byte *
_bson_mut_data_at (bson_mut doc, bson_iterator pos)
{
   const ssize_t off = bson_iterator_data (pos) - bson_data (doc);
   return bson_mut_data (doc) + off;
}

/**
 * @internal
 * @brief Delete and/or insert a region of bytes within document data.
 *
 * This function will resize a region of memory within a document, and update
 * the document's size header to reflect the changed size. If this is a child
 * document, all parent documents headers will also be updated.
 *
 * @param doc The document to update
 * @param position The position at which we will delete/add bytes
 * @param n_delete The number of bytes to "delete" at `position`
 * @param n_insert The number of bytes to "insert" at `position`
 * @return bson_byte* A pointer within the document that corresponds to the
 * beginning of the modified area.
 *
 * @note Any pointers or iterators into the document will be invalidated if
 * the splice results in growing the document beyond its capacity. Care must
 * be taken to restore iterators and pointers following this operation.
 */
inline bson_byte *
_bson_splice_region (bson_mut *const doc,
                     bson_byte *position,
                     int32_t n_delete,
                     int32_t n_insert)
{
   // The offset of the position. We use this to later recover a pointer
   const ssize_t pos_offset = position - bson_data (*doc);
   BV_ASSERT (pos_offset >= 0);
   // The amount that we will grow/shrink the data
   const ssize_t size_diff = n_insert - n_delete;
   // Calculate the new document size:
   const ssize_t new_doc_size64 = bson_ssize (*doc) + size_diff;
   _bson_safe_int32 new_doc_size =
      _bson_i64_to_i32 (_bson_i64 (new_doc_size64));
   if (new_doc_size.flags) {
      return NULL;
   }

   if (doc->_capacity_or_negative_offset_within_parent_data < 0) {
      // We are in CHILD MODE, so we need to tell the parent to do the work:
      bson_mut *parent = (bson_mut *) doc->_parent_mut_or_allocator_context;
      // Our document offset within the parent, to adjust after reallocation:
      const ptrdiff_t my_doc_offset = bson_data (*doc) - bson_data (*parent);
      // Resize, and get the new position:
      position = _bson_splice_region (parent, position, n_delete, n_insert);
      if (!position) {
         // Allocation failed
         return NULL;
      }
      // Adjust our document begin point, since we may have reallocated:
      doc->_bson_document_data = bson_mut_data (*parent) + my_doc_offset;
   } else {
      // We are the root of the document, so we do the actual work:
      if (new_doc_size.value > bson_capacity (*doc)) {
         // We need to grow larger. Add some extra to prevent repeated
         // allocations:
         if (!_bson_i32_iadd (&new_doc_size, _bson_i32 (1024))) {
            // We would overflow
            return NULL;
         }
         // Resize:
         if (bson_reserve (doc, new_doc_size.value) < 0) {
            // Allocation failed...
            return NULL;
         }
         // Recalc the position, since we reallocated
         position = bson_mut_data (*doc) + pos_offset;
      }
      // Data beginning:
      bson_byte *const doc_begin = bson_mut_data (*doc);
      // Data past-then-end:
      bson_byte *const doc_end = doc_begin + bson_size (*doc);
      // The beginning of the tail that needs to be shifted
      bson_byte *const move_dest = position + n_insert;
      // The destination of the shifted buffer tail:
      bson_byte *const move_from = position + n_delete;
      // The size of the tail that is being moved:
      const ssize_t data_remain = doc_end - move_from;
      BV_ASSERT (data_remain >= 0);
      // Move the data, and insert a marker in the empty space:
      memmove (move_dest, move_from, data_remain);
      memset (position, 'X', n_insert);
   }
   // Update our document header to match the new size:
   _bson_write_uint32le (bson_mut_data (*doc), (uint32_t) new_doc_size.value);
   return position;
}

/**
 * @internal
 * @brief Prepare a region within the document for a new element
 *
 * @param d The document to update
 * @param pos The iterator position at which the element will be inserted. This
 * will be updated to point to the inserted element data, or the end on failure
 * @param type The element type tag
 * @param key The element key string (null terminated)
 * @param datasize The amount of data we need to prepare for the element value
 * @return bson_byte* A pointer to the beginning of the element's value region
 * (after the key), or NULL in case of failure.
 *
 * @note In case of failure, returns NULL, and `pos` will be updated to refer to
 * the end position within the document, since that is never otherwise a valid
 * result of an insert.
 *
 * @note In case of success, `pos` will be updated to point to the newly
 * inserted element.
 */
inline bson_byte *
_bson_prep_element_region (bson_mut *const d,
                           bson_iterator *const pos,
                           const bson_type type,
                           const char *const key,
                           const int32_t datasize)
{
   // Compute the string length:
   const _bson_safe_int32 keylen = _bson_safe_strlen32 (key);
   // The total size of the element (add two: for the tag and the key's null):
   _bson_safe_int32 elem_size = _bson_i32_add (
      _bson_i32 (datasize), _bson_i32_add (_bson_i32 (2), keylen));
   if (elem_size.flags) {
      // Overflow.
      *pos = bson_end (*d);
      return NULL;
   }
   // The offset of the element within the doc:
   const ssize_t pos_offset = bson_iterator_data (*pos) - bson_mut_data (*d);
   // Insert a new set of bytes for the data:
   bson_byte *outptr =
      _bson_splice_region (d, _bson_mut_data_at (*d, *pos), 0, elem_size.value);
   if (!outptr) {
      // Allocation failed:
      *pos = bson_end (*d);
      return NULL;
   }
   // Wriet the type tag:
   (*outptr++).v = type;
   // Write the key
   outptr = _bson_memcpy (outptr, key, keylen.value);
   // Write the null terminator:
   (*outptr++).v = 0; // null terminator

   // Create a new iterator at the inserted data position
   pos->_ptr = bson_mut_data (*d) + pos_offset;
   pos->_keylen = keylen.value;
   pos->_rlen = bson_ssize (*d) - pos_offset;
   return outptr;
}


inline bson_iterator
bson_insert_double (bson_mut *doc, bson_iterator pos, const char *key, double d)
{
   bson_byte *out = _bson_prep_element_region (
      doc, &pos, BSON_TYPE_DOUBLE, key, sizeof (double));
   if (out) {
      uint64_t tmp;
      memcpy (&tmp, &d, sizeof d);
      _bson_write_uint64le (out, tmp);
   }
   return pos;
}

inline bson_iterator
_bson_insert_stringlike (bson_mut *doc,
                         bson_iterator pos,
                         const char *key,
                         bson_type realtype,
                         const char *string,
                         int32_t string_length)
{
   const int32_t string_len = _bson_safe_strnlen32 (string, string_length);
   const _bson_safe_int32 string_size =
      _bson_i32_add (_bson_i32 (string_length), _bson_i32 (1));
   if (string_size.flags) {
      return bson_end (*doc);
   }
   const _bson_safe_int32 el_size = _bson_i32_add (string_size, _bson_i32 (4));
   if (el_size.flags) {
      return bson_end (*doc);
   }
   bson_byte *out =
      _bson_prep_element_region (doc, &pos, realtype, key, el_size.value);
   if (out) {
      out = _bson_write_uint32le (out, (uint32_t) string_size.value);
      out = _bson_memcpy (out, string, string_size.value - 1);
      out->v = 0;
   }
   return pos;
}


static inline bson_iterator
bson_insert_utf8 (bson_mut *doc,
                  bson_iterator pos,
                  const char *key,
                  const char *utf8,
                  int32_t utf8_length)
{
   return _bson_insert_stringlike (
      doc, pos, key, BSON_TYPE_UTF8, utf8, utf8_length);
}


inline bson_iterator
bson_insert_doc (bson_mut *doc,
                 bson_iterator pos,
                 const char *key,
                 bson_view insert_doc)
{
   int32_t insert_size = 5;
   if (bson_data (insert_doc)) {
      insert_size = bson_ssize (insert_doc);
   }
   bson_byte *out = _bson_prep_element_region (
      doc, &pos, BSON_TYPE_DOCUMENT, key, insert_size);
   if (out) {
      if (!bson_data (insert_doc)) {
         // Just insert an empty document:
         memset (out, 0, 5);
         out[0].v = 5;
      } else {
         // Copy the document into place:
         _bson_memcpy (out, bson_data (insert_doc), bson_size (insert_doc));
      }
   }
   return pos;
}


inline bson_iterator
bson_insert_array (bson_mut *doc, bson_iterator pos, const char *key)
{
   bson_byte *out =
      _bson_prep_element_region (doc, &pos, BSON_TYPE_ARRAY, key, 5);
   if (out) {
      memset (out, 0, 5);
      out[0].v = 5;
   }
   return pos;
}


/**
 * @brief Obtain a mutator for the subdocument at the given position within
 * another document mutator
 *
 * @param parent A document being mutated.
 * @param subdoc_iter An iterator referring to a document/array element witihn
 * `parent`
 * @return bson_mut A new child mutator.
 *
 * @note The returned mutator MUST NOT be deleted.
 * @note The `parent` is passed by-pointer, because the child mutator may need
 * to update the parent in case of element insertion/deletion.
 */
inline bson_mut
bson_mut_subdocument (bson_mut *parent, bson_iterator subdoc_iter)
{
   bson_mut ret = {0};
   if (bson_iterator_type (subdoc_iter) != BSON_TYPE_DOCUMENT &&
       bson_iterator_type (subdoc_iter) != BSON_TYPE_ARRAY) {
      // The element is not a document, so return a null mutator.
      return ret;
   }
   // Remember the parent:
   ret._parent_mut_or_allocator_context = parent;
   // The offset of the element referred-to by the iterator
   const int32_t elem_offset =
      bson_iterator_data (subdoc_iter) - bson_data (*parent);
   // Point to the value data:
   ret._bson_document_data =
      bson_mut_data (*parent) + elem_offset + subdoc_iter._keylen + 2;
   // Save the element offset as a negative value. This triggers the mutator to
   // use CHILD MODE
   ret._capacity_or_negative_offset_within_parent_data = -elem_offset;
   return ret;
}

/**
 * @brief Given a bson_mut that is a child of another bson_mut, obtain an
 * iterator within the parent that refers to the child document.
 *
 * @param doc A child document mutator
 * @return bson_iterator An iterator within the parent that refers to the child
 */
inline bson_iterator
bson_parent_iterator (bson_mut doc)
{
   BV_ASSERT (doc._capacity_or_negative_offset_within_parent_data < 0);
   bson_mut par = *(bson_mut *) doc._parent_mut_or_allocator_context;
   bson_iterator ret = {0};
   // Recover the address of the element:
   ret._ptr = bson_mut_data (par) +
              -doc._capacity_or_negative_offset_within_parent_data;
   int32_t offset = ret._ptr - bson_mut_data (par);
   ret._keylen = doc._bson_document_data - ret._ptr - 2;
   ret._rlen = bson_size (par) - offset;
   return ret;
}


inline bson_iterator
bson_insert_binary (bson_mut *doc,
                    bson_iterator pos,
                    const char *key,
                    bson_binary bin)
{
   _bson_safe_int32 size = _bson_i32_add (
      _bson_i64_to_i32 (_bson_i64 (bin.data_len)), _bson_i32 (5));
   if (size.flags) {
      return bson_end (*doc);
   }
   bson_byte *out =
      _bson_prep_element_region (doc, &pos, BSON_TYPE_BINARY, key, size.value);
   if (out) {
      out = _bson_write_uint32le (out, size.value);
      out[0].v = bin.subtype;
      ++out;
      out = _bson_memcpy (out, bin.data, size.value);
   }
   return pos;
}


inline bson_iterator
bson_insert_undefined (bson_mut *doc, bson_iterator pos, const char *key)
{
   _bson_prep_element_region (doc, &pos, BSON_TYPE_UNDEFINED, key, 0);
   return pos;
}


inline bson_iterator
bson_insert_oid (bson_mut *doc,
                 bson_iterator pos,
                 const char *key,
                 bson_oid oid)
{
   bson_byte *out =
      _bson_prep_element_region (doc, &pos, BSON_TYPE_OID, key, sizeof oid);
   if (out) {
      memcpy (out, &oid, sizeof oid);
   }
   return pos;
}

inline bson_iterator
bson_insert_bool (bson_mut *doc, bson_iterator pos, const char *key, bool b)
{
   bson_byte *out =
      _bson_prep_element_region (doc, &pos, BSON_TYPE_BOOL, key, 1);
   if (out) {
      out[0].v = b;
   }
   return pos;
}

inline bson_iterator
bson_insert_datetime (bson_mut *doc,
                      bson_iterator pos,
                      const char *key,
                      int64_t dt)
{
   bson_byte *out = _bson_prep_element_region (
      doc, &pos, BSON_TYPE_DATE_TIME, key, sizeof dt);
   if (out) {
      _bson_write_uint64le (out, (uint64_t) dt);
   }
   return pos;
}

inline bson_iterator
bson_insert_null (bson_mut *doc, bson_iterator pos, const char *key)
{
   _bson_prep_element_region (doc, &pos, BSON_TYPE_NULL, key, 0);
   return pos;
}

inline bson_iterator
bson_insert_regex (bson_mut *doc,
                   bson_iterator pos,
                   const char *key,
                   bson_regex rx)
{
   // Neither regex nor options may contain embedded nulls, so we cannot trust
   // the caller giving us the correct length:
   // Compute regex length:
   const int32_t rx_len = _bson_safe_strnlen32 (rx.regex, rx.regex_len);
   // Compute options length:
   const int32_t opts_len = _bson_safe_strnlen32 (
      rx.options ? rx.options : "", rx.options ? rx.options_len : 0);
   // The total value size:
   const _bson_safe_int32 size = _bson_i32_add (
      _bson_i32 (rx_len), _bson_i32_add (_bson_i32 (rx_len), _bson_i32 (2)));
   // Overflow guard:
   if (size.flags || size.value < 2) {
      return bson_end (*doc);
   }
   bson_byte *out =
      _bson_prep_element_region (doc, &pos, BSON_TYPE_REGEX, key, size.value);
   if (out) {
      out = _bson_memcpy (out, rx.regex, (uint32_t) rx_len);
      (out++)->v = 0;
      if (rx.options) {
         out = _bson_memcpy (out, rx.options, (uint32_t) opts_len);
      }
      (out++)->v = 0;
   }
   return pos;
}

inline bson_iterator
bson_insert_dbpointer (bson_mut *doc,
                       bson_iterator pos,
                       const char *key,
                       bson_dbpointer dbp)
{
   const int32_t coll_string_len =
      _bson_safe_strnlen32 (dbp.collection, dbp.collection_len);
   const _bson_safe_int32 coll_string_size =
      _bson_i32_add (_bson_i32 (coll_string_len), _bson_i32 (1));
   const _bson_safe_int32 el_size =
      _bson_i32_add (_bson_i32 (12 + 4), coll_string_size);
   if (el_size.flags) {
      return bson_end (*doc);
   }
   bson_byte *out = _bson_prep_element_region (
      doc, &pos, BSON_TYPE_DBPOINTER, key, el_size.value);
   if (out) {
      out = _bson_write_uint32le (out, coll_string_size.value);
      out = _bson_memcpy (out, dbp.collection, coll_string_size.value - 1);
      (out++)->v = 0;
      out = _bson_memcpy (out, &dbp.object_id, sizeof dbp.object_id);
   }
   return pos;
}

static inline bson_iterator
bson_insert_code (bson_mut *doc,
                  bson_iterator pos,
                  const char *key,
                  const char *code)
{
   return _bson_insert_stringlike (doc, pos, key, BSON_TYPE_CODE, code, -1);
}

static inline bson_iterator
bson_insert_symbol (bson_mut *doc,
                    bson_iterator pos,
                    const char *key,
                    const char *sym)
{
   return _bson_insert_stringlike (doc, pos, key, BSON_TYPE_SYMBOL, sym, -1);
}

inline bson_iterator
bson_insert_code_with_scope (bson_mut *doc,
                             bson_iterator pos,
                             const char *key,
                             const char *code,
                             bson_view scope)
{
   const _bson_safe_int32 code_size =
      _bson_i32_add (_bson_safe_strlen32 (code), _bson_i32 (1));
   const _bson_safe_int32 el_size = _bson_i32_add (
      code_size, _bson_i32_add (_bson_i32 (bson_ssize (scope)), _bson_i32 (4)));
   if (el_size.flags) {
      return bson_end (*doc);
   }
   bson_byte *out = _bson_prep_element_region (
      doc, &pos, BSON_TYPE_CODEWSCOPE, key, el_size.value);
   if (out) {
      out = _bson_write_uint32le (out, el_size.value);
      out = _bson_write_uint32le (out, code_size.value);
      out = _bson_memcpy (out, code, code_size.value - 1);
      out = _bson_memcpy (out, bson_data (scope), bson_size (scope));
   }
   return pos;
}

inline bson_iterator
bson_insert_int32 (bson_mut *doc,
                   bson_iterator pos,
                   const char *key,
                   int32_t value)
{
   bson_byte *out = _bson_prep_element_region (
      doc, &pos, BSON_TYPE_INT32, key, sizeof (int32_t));
   if (out) {
      out = _bson_write_uint32le (out, (uint32_t) value);
   }
   return pos;
}

struct bson_timestamp {
   int32_t increment;
   int32_t timestamp;
};

inline bson_iterator
bson_insert_timestamp (bson_mut *doc,
                       bson_iterator pos,
                       const char *key,
                       struct bson_timestamp ts)
{
   bson_byte *out = _bson_prep_element_region (
      doc, &pos, BSON_TYPE_TIMESTAMP, key, sizeof ts);
   if (out) {
      out = _bson_write_uint32le (out, (uint32_t) ts.increment);
      out = _bson_write_uint32le (out, (uint32_t) ts.timestamp);
   }
   return pos;
}

inline bson_iterator
bson_insert_int64 (bson_mut *doc,
                   bson_iterator pos,
                   const char *key,
                   int64_t value)
{
   bson_byte *out =
      _bson_prep_element_region (doc, &pos, BSON_TYPE_INT64, key, sizeof value);
   if (out) {
      out = _bson_write_uint64le (out, (uint64_t) value);
   }
   return pos;
}

struct bson_decimal128 {
   uint8_t bytes[16];
};

inline bson_iterator
bson_insert_decimal128 (bson_mut *doc,
                        bson_iterator pos,
                        const char *key,
                        struct bson_decimal128 value)
{
   bson_byte *out = _bson_prep_element_region (
      doc, &pos, BSON_TYPE_DECIMAL128, key, sizeof value);
   if (out) {
      out = _bson_memcpy (out, &value, sizeof value);
   }
   return pos;
}

inline bson_iterator
bson_insert_maxkey (bson_mut *doc, bson_iterator pos, const char *key)
{
   _bson_prep_element_region (doc, &pos, BSON_TYPE_MAXKEY, key, 0);
   return pos;
}

inline bson_iterator
bson_insert_minkey (bson_mut *doc, bson_iterator pos, const char *key)
{
   _bson_prep_element_region (doc, &pos, BSON_TYPE_MINKEY, key, 0);
   return pos;
}

/**
 * @brief Remove a range of elements from a document
 *
 * Delete elements from the document, starting with `first`, and continuing
 * until (but not including) `last`.
 *
 * @param doc The document to update
 * @param first The first element to remove from `doc`
 * @param last The element at which to stop removing elements.
 * @return bson_iterator An iterator that refers to the new location of `last`
 *
 * @note If `first` and `last` are equal, no elements are removed.
 * @note If `last` is not reachable from `first`, the behavior is undefined.
 */
inline bson_iterator
bson_erase_range (bson_mut *const doc,
                  const bson_iterator first,
                  const bson_iterator last)
{
   int32_t del_size = 0;
   for (bson_iterator it = first; !bson_iterator_eq (it, last);
        it = bson_next (it)) {
      del_size += bson_iterator_data_size (it);
   }
   bson_iterator ret = last;
   bson_byte *newptr =
      _bson_splice_region (doc, _bson_mut_data_at (*doc, first), del_size, 0);
   BV_ASSERT (newptr);
   ret._ptr = newptr;
   return ret;
}

/**
 * @brief Remove a single element from the given bson document
 *
 * @param doc The document to modify
 * @param pos An iterator pointing to the single element to remove. Must not be
 * the end position
 * @return bson_iterator An iterator referring to the element after the removed
 * element.
 */
static inline bson_iterator
bson_erase (bson_mut *doc, bson_iterator pos)
{
   return bson_erase_range (doc, pos, bson_next (pos));
}

BSON2_EXTERN_C_END

#undef BV_ASSERT

#endif // BSON_BUILD_H_INCLUDED
