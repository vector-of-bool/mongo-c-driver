#include <bson/bson.h>
#include <bson/view.h>

#include <stdint.h>

int
LLVMFuzzerTestOneInput (const uint8_t *data, size_t len)
{
   bson_t *b = bson_new_from_data (data, len);
   if (!b) {
      return 0;
   }
   bson_validate (b, 0xffffff, NULL);
   bson_destroy (b);

   bson_view utv = bson_view_from_data ((const bson_byte *) data, len, NULL);
   if (!utv._bson_document_data) {
      return 0;
   }

   return 0;
}
