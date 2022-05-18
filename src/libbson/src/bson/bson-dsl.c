#include "bson-dsl.h"

#ifdef _MSC_VER
__declspec(thread)
#else
__thread
#endif
   bson_dsl_context_t _tl_context = {0};

bson_dsl_context_t *
pBsonDSLctx ()
{
   return &_tl_context;
}
