#ifndef PROJECT_H__
#define PROJECT_H__

#include <result_processor.h>
#include <aggregate/expr/expression.h>

ResultProcessor *NewProjector(RedisSearchCtx *sctx, ResultProcessor *upstream, const char *alias,
                              const char *expr, size_t len, char **err);
#endif
