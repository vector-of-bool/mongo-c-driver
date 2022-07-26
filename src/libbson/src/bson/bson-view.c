#include "./bson-view.h"

typedef struct validation_iterator {
   const bson_byte *dataptr;
   size_t bytes_remaining;
} validation_iterator;

static bson_byte const *
_find_after_cstring (bson_byte const *cstring,
                     bson_byte const *const end,
                     bool *okay)
{
   *okay = true;
   while (cstring != end && cstring->v) {
      ++cstring;
   }
   if (cstring == end) {
      *okay = false;
      return NULL;
   }
   return cstring + 1;
}

enum bson_view_iterator_stop_reason
_validate_document (bson_byte const *iter, bson_byte const *const end)
{
   const int8_t jumpsize_table[] = {
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

   bool cstring_okay = true;

   while (iter != end) {
      const uint8_t type = iter->v;
      if (type == 0) {
         // We should have found the end of the document here
         if (iter + 1 != end) {
            return BSONV_ITER_STOP_INVALID_TYPE;
         }
         return 0;
      }
      // Check that the type is in-bounds
      if (type >= sizeof jumpsize_table) {
         return BSONV_ITER_STOP_INVALID_TYPE;
      }

      // Scan for the null-terminator after the key string
      iter = _find_after_cstring (iter, end, &cstring_okay);
      if (!cstring_okay) {
         return BSONV_ITER_STOP_INVALID;
      }

      // 'iter' now points to the beginning of the element data after the
      // cstring key

      // Prepare to jump
      int32_t jump = jumpsize_table[type];
      if (jump >= 0) {
         // A known size of jump
      } else if (jump == -1) {
         // The next object is a length-prefixed object, and we can jump
         // by reading an int32
         uint32_t len;
         if ((end - iter) < sizeof len) {
            return BSONV_ITER_STOP_SHORT_READ;
         }
         memcpy (&len, iter, sizeof len);
         len = BSON_UINT32_FROM_LE (len);
         jump = len;
         if (type != BSON_TYPE_DOCUMENT && type != BSON_TYPE_ARRAY) {
            if (len > (end - iter)) {
               // There's not enough data remaining for the document of the
               // given size
               return BSONV_ITER_STOP_SHORT_READ;
            }
            // We need to jump over the length header too
            jump += sizeof len;
            // Before jumping, validate the sub-document
            enum bson_view_iterator_stop_reason v =
               _validate_document (iter, iter + len);
            if (v) {
               return v;
            }
            // Sub-document validated okay. We can jump over it
         }
      } else if (jump == -2) {
         // The object is special and needs special rules to validate it
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
            // Find the end of the first string
            const bson_byte *scan =
               _find_after_cstring (iter, end, &cstring_okay);
            if (cstring_okay) {
               // Find the end of the second string
               scan = _find_after_cstring (scan, end, &cstring_okay);
            }
            // We okay?
            if (!cstring_okay) {
               return BSONV_ITER_STOP_INVALID;
            }
            // Jump however much we read
            jump = scan - iter;
            break;
         }
         case BSON_TYPE_DBPOINTER: {
            // Read a int32 length prefix
            uint32_t len;
            if ((end - iter) < sizeof len) {
               return BSONV_ITER_STOP_SHORT_READ;
            }
            memcpy (&len, iter, sizeof len);
            len = BSON_UINT32_FROM_LE (len);
            // Avoid overflowing on the addition:
            if (UINT32_MAX - len < 12) {
               return BSONV_ITER_STOP_SHORT_READ;
            }
            // +Twelve bytes for the OID
            jump = len + 12u;
            break;
         }
         }
      }

      // We're ready to jump
      if ((end - iter) < jump) {
         // There's not enough data!
         return BSONV_ITER_STOP_SHORT_READ;
      }
      iter += jump;
   }

   return BSONV_ITER_STOP_SHORT_READ;
}

bson_validation_result
bson_validate_untrusted (bson_view_untrusted view)
{
   enum bson_view_iterator_stop_reason error =
      _validate_document (view.data, view.data + bson_view_ut_len (view));
   if (error) {
      return (bson_validation_result){.error = error};
   }
   return (bson_validation_result){
      .view = bson_view_from_trusted_data (view.data, bson_view_ut_len (view))};
}
