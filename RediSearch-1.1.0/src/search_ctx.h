#ifndef __SEARCH_CTX_H
#define __SEARCH_CTX_H

#include <sched.h>

#include "redismodule.h"
#include "spec.h"
#include "trie/trie_type.h"
#include <time.h>

/** Context passed to all redis related search handling functions. */
typedef struct {
  RedisModuleCtx *redisCtx; //模块信息，也里RediSearch本身就是Redis的一个模块，也就是RediSearch模块的相关信息
  RedisModuleKey *key; //OpenKey 返回的东东
  RedisModuleString *keyName; //"idx:" + 索引名字
  IndexSpec *spec; //索引数据结构
} RedisSearchCtx;

#define SEARCH_CTX_STATIC(ctx, sp) \
  (RedisSearchCtx) {               \
    .redisCtx = ctx, .spec = sp    \
  }

#define SEARCH_CTX_SORTABLES(ctx) ((ctx && ctx->spec) ? ctx->spec->sortables : NULL)
// Create a string context on the heap
RedisSearchCtx *NewSearchCtx(RedisModuleCtx *ctx, RedisModuleString *indexName);

// Same as above, only from c string (null terminated)
RedisSearchCtx *NewSearchCtxC(RedisModuleCtx *ctx, const char *indexName);

void SearchCtx_Free(RedisSearchCtx *sctx);
#endif
