#include "bson-dsl.h"

#ifdef _MSC_VER
__declspec(thread)
#else
__thread
#endif
   _bsonBuildContext_t *_tl_bsonBuildContextPtr = {0};

_bsonBuildContext_t **
_bsonBuildContextPtr ()
{
   return &_tl_bsonBuildContextPtr;
}

#ifdef _MSC_VER
__declspec(thread)
#else
__thread
#endif
   struct _bsonVisitContext_t const *_tl_bsonVisitContextPtr = NULL;


struct _bsonVisitContext_t const **
_bsonVisitContextPtr ()
{
   return &_tl_bsonVisitContextPtr;
}

char *
bsonParse_createPathString ()
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
