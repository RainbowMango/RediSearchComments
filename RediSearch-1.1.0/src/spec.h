#ifndef __SPEC_H__
#define __SPEC_H__
#include <stdlib.h>
#include <string.h>

#include "redismodule.h"
#include "doc_table.h"
#include "trie/trie_type.h"
#include "sortable.h"
#include "stopwords.h"
#include "gc.h"

typedef enum fieldType { FIELD_FULLTEXT, FIELD_NUMERIC, FIELD_GEO, FIELD_TAG } FieldType;

#define NUMERIC_STR "NUMERIC"
#define GEO_STR "GEO"

#define SPEC_NOOFFSETS_STR "NOOFFSETS"
#define SPEC_NOFIELDS_STR "NOFIELDS"
#define SPEC_NOFREQS_STR "NOFREQS"
#define SPEC_NOHL_STR "NOHL"
#define SPEC_SCHEMA_STR "SCHEMA"
#define SPEC_TEXT_STR "TEXT"
#define SPEC_WEIGHT_STR "WEIGHT"
#define SPEC_NOSTEM_STR "NOSTEM"
#define SPEC_TAG_STR "TAG"
#define SPEC_SORTABLE_STR "SORTABLE"
#define SPEC_STOPWORDS_STR "STOPWORDS" //停止词，例如"的"、"是"等，几乎所有文档都存在该词，搜索引擎会忽略掉这些词
#define SPEC_NOINDEX_STR "NOINDEX"
#define SPEC_SEPARATOR_STR "SEPARATOR"

static const char *SpecTypeNames[] = {[FIELD_FULLTEXT] = SPEC_TEXT_STR,
                                      [FIELD_NUMERIC] = NUMERIC_STR,
                                      [FIELD_GEO] = GEO_STR,
                                      [FIELD_TAG] = SPEC_TAG_STR};

#define INDEX_SPEC_KEY_PREFIX "idx:"
#define INDEX_SPEC_KEY_FMT INDEX_SPEC_KEY_PREFIX "%s"

#define SPEC_MAX_FIELDS 1024
#define SPEC_MAX_FIELD_ID (sizeof(t_fieldMask) * 8)
// The threshold after which we move to a special encoding for wide fields
#define SPEC_WIDEFIELD_THRESHOLD 32

typedef enum {
  FieldSpec_Sortable = 0x01,
  FieldSpec_NoStemming = 0x02,
  FieldSpec_NotIndexable = 0x04
} FieldSpecOptions;

// Specific options for text fields
typedef struct {
  // weight in frequency calculations
  double weight; //文本域的权重，默认为1.0
  // bitwise id for field masks
  t_fieldId id;
} TextFieldOptions;

// Flags for tag fields
typedef enum {
  TagField_CaseSensitive = 0x01,
  TagField_TrimSpace = 0x02,
  TagField_RemoveAccents = 0x04,
} TagFieldFlags;

#define TAG_FIELD_DEFAULT_FLAGS TagField_TrimSpace &TagField_RemoveAccents;

// Specific options for tag fields
typedef struct {
  char separator;
  TagFieldFlags flags;
} TagFieldOptions;

/* The fieldSpec represents a single field in the document's field spec.
Each field has a unique id that's a power of two, so we can filter fields
by a bit mask.
Each field has a type, allowing us to add non text fields in the future */
typedef struct fieldSpec { //文档域结构
  char *name; //域名
  FieldType type; //域的类型，比如文本、数字等
  FieldSpecOptions options;

  int sortIdx;

  /**
   * Unique field index. Each field has a unique index regardless of its type
   */
  uint16_t index; //域在当前索引用的位置，也即第几个域

  union {
    TextFieldOptions textOpts; //全文本域的选项
    TagFieldOptions tagOpts;
  };

  // TODO: More options here..
} FieldSpec;

#define FieldSpec_IsSortable(fs) ((fs)->options & FieldSpec_Sortable)
#define FieldSpec_IsNoStem(fs) ((fs)->options & FieldSpec_NoStemming)
#define FieldSpec_IsIndexable(fs) (0 == ((fs)->options & FieldSpec_NotIndexable))

typedef struct { //索引状态
  size_t numDocuments;
  size_t numTerms; //当前索引的索引词数量
  size_t numRecords;
  size_t invertedSize;
  size_t invertedCap;
  size_t skipIndexesSize;
  size_t scoreIndexesSize;
  size_t offsetVecsSize;
  size_t offsetVecRecords;
  size_t termsSize;
} IndexStats;

typedef enum {
  Index_StoreTermOffsets = 0x01,
  Index_StoreFieldFlags = 0x02,

  // Was StoreScoreIndexes, but these are always stored, so this option is unused
  Index__Reserved1 = 0x04,
  Index_HasCustomStopwords = 0x08,
  Index_StoreFreqs = 0x010,
  Index_StoreNumeric = 0x020,
  Index_StoreByteOffsets = 0x40,
  Index_WideSchema = 0x080,
  Index_DocIdsOnly = 0x00,
} IndexFlags;

/**
 * This "ID" type is independent of the field mask, and is used to distinguish
 * between one field and another field. For now, the ID is the position in
 * the array of fields - a detail we'll try to hide.
 */
typedef uint16_t FieldSpecDedupeArray[SPEC_MAX_FIELDS];

#define INDEX_DEFAULT_FLAGS \
  Index_StoreFreqs | Index_StoreTermOffsets | Index_StoreFieldFlags | Index_StoreByteOffsets

#define INDEX_STORAGE_MASK                                                                  \
  (Index_StoreFreqs | Index_StoreFieldFlags | Index_StoreTermOffsets | Index_StoreNumeric | \
   Index_WideSchema)

#define INDEX_CURRENT_VERSION 10
#define INDEX_MIN_COMPAT_VERSION 2
// Versions below this always store the frequency
#define INDEX_MIN_NOFREQ_VERSION 6
// Versions below this encode field ids as the actual value,
// above - field ides are encoded as their exponent (bit offset)
#define INDEX_MIN_WIDESCHEMA_VERSION 7

// Versions below this didn't know tag indexes
#define INDEX_MIN_TAGFIELD_VERSION 8

// Versions below this one don't save the document len when serializing the table
#define INDEX_MIN_DOCLEN_VERSION 9

#define INDEX_MIN_BINKEYS_VERSION 10

#define Index_SupportsHighlight(spec) \
  (((spec)->flags & Index_StoreTermOffsets) && ((spec)->flags & Index_StoreByteOffsets))

#define FIELD_BIT(fs) (((t_fieldMask)1) << (fs)->textOpts.id)

typedef struct { //索引数据结构定义
  char *name; //索引名字
  FieldSpec *fields; //域信息数组, 每个元素记录域基本信息, 创建索引时该数组长度初始化为1024
  int numFields; //当前索引中存放的域个数，也指明了fields中前多少个元素是有效的

  IndexStats stats;
  IndexFlags flags; //索引标志，比如是否支持offset等，多个标志可按位或

  Trie *terms;

  RSSortingTable *sortables;

  DocTable docs;

  StopWordList *stopwords; //停止词列表，停止词意味因为太常见而被搜索引擎忽略的词列表

  void *gc;

} IndexSpec;

extern RedisModuleType *IndexSpecType;

/*
 * Get a field spec by field name. Case insensitive!
 * Return the field spec if found, NULL if not
 */
FieldSpec *IndexSpec_GetField(IndexSpec *spec, const char *name, size_t len);

char *GetFieldNameByBit(IndexSpec *sp, t_fieldMask id);

/* Get the field bitmask id of a text field by name. Return 0 if the field is not found or is not a
 * text field */
t_fieldMask IndexSpec_GetFieldBit(IndexSpec *spec, const char *name, size_t len);

/* Get a sortable field's sort table index by its name. return -1 if the field was not found or is
 * not sortable */
int IndexSpec_GetFieldSortingIndex(IndexSpec *sp, const char *name, size_t len);

/* Initialize some index stats that might be useful for scoring functions */
void IndexSpec_GetStats(IndexSpec *sp, RSIndexStats *stats);
/*
 * Parse an index spec from redis command arguments.
 * Returns REDISMODULE_ERR if there's a parsing error.
 * The command only receives the relvant part of argv.
 *
 * The format currently is <field> <weight>, <field> <weight> ...
 */
IndexSpec *IndexSpec_ParseRedisArgs(RedisModuleCtx *ctx, RedisModuleString *name,
                                    RedisModuleString **argv, int argc, char **err);

/* Create a new index spec from redis arguments, set it in a redis key and start its GC.
 * If an error occurred - we set an error string in err and return NULL.
 */
IndexSpec *IndexSpec_CreateNew(RedisModuleCtx *ctx, RedisModuleString **argv, int argc, char **err);

/* Start the garbage collection loop on the index spec */
void IndexSpec_StartGC(RedisModuleCtx *ctx, IndexSpec *sp, float initialHZ);

/* Same as above but with ordinary strings, to allow unit testing */
IndexSpec *IndexSpec_Parse(const char *name, const char **argv, int argc, char **err);

IndexSpec *IndexSpec_Load(RedisModuleCtx *ctx, const char *name, int openWrite);

IndexSpec *IndexSpec_LoadEx(RedisModuleCtx *ctx, RedisModuleString *formattedKey, int openWrite,
                            RedisModuleKey **keyp);

int IndexSpec_AddTerm(IndexSpec *sp, const char *term, size_t len);

/* Get a random term from the index spec using weighted random. Weighted random is done by sampling
 * N terms from the index and then doing weighted random on them. A sample size of 10-20 should be
 * enough */
char *IndexSpec_GetRandomTerm(IndexSpec *sp, size_t sampleSize);
/*
 * Free an indexSpec. This doesn't free the spec itself as it's not allocated by the parser
 * and should be on the request's stack
 */
void IndexSpec_Free(void *spec);

/* Parse a new stopword list and set it. If the parsing fails we revert to the default stopword
 * list, and return 0 */
int IndexSpec_ParseStopWords(IndexSpec *sp, RedisModuleString **strs, size_t len);

/* Return 1 if a term is a stopword for the specific index */
int IndexSpec_IsStopWord(IndexSpec *sp, const char *term, size_t len);

IndexSpec *NewIndexSpec(const char *name, size_t numFields);
void *IndexSpec_RdbLoad(RedisModuleIO *rdb, int encver);
void IndexSpec_RdbSave(RedisModuleIO *rdb, void *value);
void IndexSpec_Digest(RedisModuleDigest *digest, void *value);
int IndexSpec_RegisterType(RedisModuleCtx *ctx);
// void IndexSpec_Free(void *value);

/*
 * Parse the field mask passed to a query, map field names to a bit mask passed down to the
 * execution engine, detailing which fields the query works on. See FT.SEARCH for API details
 */
t_fieldMask IndexSpec_ParseFieldMask(IndexSpec *sp, RedisModuleString **argv, int argc);

#endif
