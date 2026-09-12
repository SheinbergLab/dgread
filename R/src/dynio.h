#ifndef DYNIO_H
#define DYNIO_H
/*************************************************************************
 *
 *  NAME
 *    dynio.h
 *
 *  DESCRIPTION
 *    Structure definitions for reading and writing dynamic groups
 *
 *  AUTHOR
 *    DLS
 *
 ************************************************************************/

extern float dynVersion;	/* to keep track of different versions */

#define DG_MAGIC_NUMBER_SIZE 4 
extern char dynMagicNumber[];	/* to uniquely identify this file type */


/***********************************************************************
 *
 *  Structures which are described here and are dumpable/readable:
 *      DG_FILE
 *         DG_INFO
 *         DYN_LIST
 *
 ***********************************************************************/

enum DYN_STRUCT_TYPE {
  DG_TOP_LEVEL,			/* regular data type (e.g. int, float)*/
  DYN_GROUP_STRUCT, 
  DYN_LIST_STRUCT, 
  N_DG_STRUCT_TYPES		/* leave as last struct                */
};

/*
 * The only TAGS which can appear before one of the defined structures
 * are the following.  These contain info about the datafile version and
 * then enter the highest level structure.
 */

enum DG_TOP_LEVEL_TAGS { DG_VERSION_TAG, DG_BEGIN_TAG };
enum DG_TAG { DG_NAME_TAG, DG_NLISTS_TAG, DG_DYNLIST_TAG };
enum DL_TAG { DL_NAME_TAG, DL_INCREMENT_TAG, DL_DATA_TAG,
	    DL_STRING_DATA_TAG, DL_CHAR_DATA_TAG, DL_SHORT_DATA_TAG,
	    DL_LONG_DATA_TAG, DL_FLOAT_DATA_TAG, DL_LIST_DATA_TAG,
	    DL_SUBLIST_TAG, DL_FLAGS_TAG,
	    /* 8-byte element arrays, tags 11 and 12 (added 2026).  Readers
	       older than that abort on them: see DL_INT64_DATA_TAG in the
	       docs before writing one into a file others will open. */
	    DL_INT64_DATA_TAG, DL_DOUBLE_DATA_TAG };

/*
 * Skippable extension envelope (added 2026, after the int64/double tags).
 *
 * Every tag above is a bare opcode: a reader that doesn't know one has no
 * way to find the next record, so it aborts the whole file.  That is why
 * adding tags 11 and 12 needed every reader in the field updated first.
 * DG_EXT_TAG is one tag value, valid at EVERY level (top, group, list),
 * whose record carries its own length:
 *
 *     byte   DG_EXT_TAG
 *     int32  ext_id      which extension (0 is reserved, never written)
 *     int32  length      payload bytes
 *     bytes  payload
 *
 * Both ints are in the file's byte order, like every other int.  A reader
 * that doesn't recognise ext_id skips the payload and carries on, so
 * anything new that is carried inside an envelope degrades to "ignored"
 * on readers from this version on, instead of "file unreadable".  Readers
 * older than this still abort on it, exactly as they do on 11 and 12.
 *
 * The value sits far above the per-level tag tables and below END_STRUCT
 * (255); it is never used as a table index.  Writers emit one with
 * dgRecordExtension() between records; readers see them through the
 * optional handler set by dgSetExtensionHandler().
 */
#define DG_EXT_TAG 250

enum DG_EXT_SCOPE { DG_EXT_SCOPE_TOP, DG_EXT_SCOPE_GROUP, DG_EXT_SCOPE_LIST };

/* scope says where the envelope sat; owner is the DYN_GROUP * being read
   for TOP and GROUP scope, the DYN_LIST * for LIST scope.  The payload
   pointer is only valid during the call. */
typedef int (*DG_EXT_HANDLER)(int scope, void *owner, int ext_id,
			      const unsigned char *payload, int length);

/***********************************************************************
 *
 *                      DG_FILE_IO Function Prototypes
 *
 ***********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

void dgInitBuffer(void);	              /* init a datafile buf   */
void dgResetBuffer(void);                     /* reset and initialize  */
void dgCloseBuffer(void);	              /* free mem assoc. w/buf */
int  dgWriteBuffer(char *filename, char format);
int  dgWriteBufferCompressed(char *filename);
unsigned char *dgGetBuffer(void);
int dgGetBufferSize(void);
int dgSetBufferIncrement(int);
int dgEstimateGroupSize(DYN_GROUP *dg);

void dgRecordDynGroup(DYN_GROUP *dg);

void dgRecordMagicNumber(void);

void dgRecordFlag(unsigned char);
void dgRecordChar(unsigned char, unsigned char);
void dgRecordLong(unsigned char, int);
void dgRecordShort(unsigned char, short);
void dgRecordFloat(unsigned char, float);

void dgRecordString(unsigned char, char *);
void dgRecordStringArray(unsigned char, int, char **);
void dgRecordVoidArray(unsigned char, int, int, void *);
void dgRecordLongArray(unsigned char, int, int *);
void dgRecordShortArray(unsigned char, int, short *);
void dgRecordFloatArray(unsigned char, int, float *);
void dgRecordInt64Array(unsigned char, int, int64_t *);
void dgRecordDoubleArray(unsigned char, int, double *);
void dgRecordCharArray(unsigned char, int, char *);
void dgRecordListArray(unsigned char type, int n);

/* Extension envelope: write one into the current buffer position (legal
   anywhere a tag is), and install/replace the reader-side handler (returns
   the previous one; NULL means skip silently, the default). */
void dgRecordExtension(int ext_id, int length, const void *payload);
DG_EXT_HANDLER dgSetExtensionHandler(DG_EXT_HANDLER handler);

void dgBeginStruct(unsigned char tag);
void dgEndStruct(void);

void dgPushStruct(int newstruct, char *);
int  dgPopStruct(void);
void dgFreeStructStack(void);
int  dgGetCurrentStruct(void);
char *dgGetCurrentStructName(void);
char *dgGetTagName(int type);
int  dgGetDataType(int type);
int  dgGetStructureType(int type);

int dgReadDynGroup(char *, DYN_GROUP *dg);
int dgReadDynGroupCompressed(char *, DYN_GROUP *dg);
int dguGzipFileToStruct(char *filename, DYN_GROUP *dg);
int dguFileToStruct(FILE *InFP, DYN_GROUP *dg);
int dguBufferToStruct(unsigned char *vbuf, int n, DYN_GROUP *dg);

void dguFileToAscii(FILE *InFP, FILE *OutFP);

int dguFileToDynGroup(FILE *InFP, DYN_GROUP *dg);
int dguFileToDynList(FILE *InFP, DYN_LIST *dl);
void dguBufferToAscii(unsigned char *vbuf, int bufsize, FILE *OutFP);


#ifdef __cplusplus
}
#endif
#endif
