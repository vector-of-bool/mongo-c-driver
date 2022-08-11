#include "bson-dsl.h"

char *
bsonParse_strdupPathString ()
{
   char *path = NULL;
   struct _bsonVisitContext_t const *node = &bsonVisitContext;
   for (; node; node = node->parent) {
      const char *key = bson_iter_key (&node->iter);
      char *newpath = NULL;
      if (node->parent &&
          bson_iter_type_unsafe (&node->parent->iter) == BSON_TYPE_ARRAY) {
         newpath = bson_strdup_printf ("[%s]%s", key, path ? path : "");
      } else {
         newpath = bson_strdup_printf (".%s%s", key, path ? path : "");
      }
      bson_free (path);
      path = newpath;
   }
   char *dollar = bson_strdup_printf ("$%s", path ? path : "");
   bson_free (path);
   return dollar;
}
