#include "bson-dsl.h"

static struct _bsonBuildContext_t _null_build_context;
static struct _bsonVisitContext_t _null_visit_context;

_bsonDSL_thread_local struct _bsonBuildContext_t
   *_bsonBuildContextThreadLocalPtr = &_null_build_context;

_bsonDSL_thread_local struct _bsonVisitContext_t const
   *_bsonVisitContextThreadLocalPtr = &_null_visit_context;


_bsonDSL_thread_local bool _tl_bsonBuildFailed = false;

bool *
_bsonBuildFailed ()
{
   return &_tl_bsonBuildFailed;
}

char *
bsonParse_strdupPathString ()
{
   char *path = NULL;
   struct _bsonVisitContext_t const *node = &bsonVisitContext;
   for (; node != &_null_visit_context; node = node->parent) {
      const char *key = bson_iter_key (&node->iter);
      char *newpath = NULL;
      if (node->parent != &_null_visit_context &&
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
