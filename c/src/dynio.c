#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(SUN4) || defined(LYNX) || defined(LINUX) || defined(FREEBSD)
#include <unistd.h>
#endif

#include "flipfuncs.h"
#include "df.h"
#include "dynio.h"

#ifdef WINDOWS
#define ZLIB_DLL
#define _WINDOWS
#endif
#include <zlib.h>

extern size_t compress_buffer_to_lz4_file(unsigned char *, int, FILE *);
extern int decompress_lz4_file_to_buffer(FILE *, int *, unsigned char **);


static int dgFlipEvents = 0;	/* to make up for byte ordering probs */
char dgMagicNumber[] = { 0x21, 0x12, 0x36, 0x63 };
float dgVersion = 1.0;

#define DG_DATA_BUFFER_SIZE 64000

static void dgDumpBuffer(unsigned char *buffer, int n, int type, FILE *fp);
static unsigned char *DgBuffer = NULL;
static int DgBufferIndex = 0;
static int DgBufferSize;
static int DgRecording = 0;
static int DgBufferIncrement = DG_DATA_BUFFER_SIZE;

/* Keep track of which structure we're in using a stack */
static int DgCurStruct = DG_TOP_LEVEL;
static char *DgCurStructName = "DG_TOP_LEVEL";
static TAG_INFO *DgStructStack = NULL;
static int DgStructStackIncrement = 10;
static int DgStructStackSize = 0;
static int DgStructStackIndex = -1;

static void send_event(unsigned char type, unsigned char *data);
static void send_bytes(int n, unsigned char *data);
static void push(unsigned char *data, int, int);

static int dguBufferToDynGroup(BUF_DATA *bdata, DYN_GROUP *dg);
static int dguBufferToDynList(BUF_DATA *bdata, DYN_LIST *dl);

int dguBufferToStruct(unsigned char *vbuf, int bufsize, DYN_GROUP *dg);

/***********************************************************************/
/*                        Structure Tag Tables                         */
/***********************************************************************/

/*                                                             */
/*   Tag_ID      Tag_Name     Tag_Type      Struct_Type        */
/* ------------  ----------  ------------  -------------       */

TAG_INFO DGTopLevelTags[] = {
  { DG_VERSION_TAG,  "VERSION",    DF_VERSION,    DG_TOP_LEVEL },
  { DG_BEGIN_TAG,    "DYN_GROUP",  DF_STRUCTURE,  DYN_GROUP_STRUCT }
};

TAG_INFO DGTags[] = {
  { DG_NAME_TAG,   "NAME",          DF_STRING,     DG_TOP_LEVEL },
  { DG_NLISTS_TAG, "NDYNLISTS",     DF_LONG,       DG_TOP_LEVEL },
  { DG_DYNLIST_TAG,"DYN_LIST",      DF_STRUCTURE,  DYN_LIST_STRUCT }
};

TAG_INFO DLTags[] = {
  { DL_NAME_TAG,        "NAME",        DF_STRING,       DG_TOP_LEVEL },
  { DL_INCREMENT_TAG,   "INCREMENT",   DF_LONG,         DG_TOP_LEVEL },
  { DL_DATA_TAG,        "DATA",        DF_VOID_ARRAY,   DG_TOP_LEVEL },
  { DL_STRING_DATA_TAG, "STRING_DATA", DF_STRING_ARRAY, DG_TOP_LEVEL },
  { DL_CHAR_DATA_TAG,   "CHAR_DATA",   DF_CHAR_ARRAY,   DG_TOP_LEVEL },
  { DL_SHORT_DATA_TAG,  "SHORT_DATA",  DF_SHORT_ARRAY,  DG_TOP_LEVEL },
  { DL_LONG_DATA_TAG,   "LONG_DATA",   DF_LONG_ARRAY,   DG_TOP_LEVEL },
  { DL_FLOAT_DATA_TAG,  "FLOAT_DATA",  DF_FLOAT_ARRAY,  DG_TOP_LEVEL },
  { DL_LIST_DATA_TAG,   "LIST_DATA",   DF_LIST_ARRAY,   DG_TOP_LEVEL },
  { DL_SUBLIST_TAG,     "SUBLIST",     DF_STRUCTURE,    DYN_LIST_STRUCT },
  { DL_FLAGS_TAG,       "FLAGS",       DF_LONG,         DG_TOP_LEVEL }
};

TAG_INFO *DGTagTable[] = { DGTopLevelTags, DGTags, DLTags };

/**************************************************************************/
/*                      Initialization Routines                           */
/**************************************************************************/

void dgInitBuffer(void)
{
  DgBufferSize = DG_DATA_BUFFER_SIZE;
  if (!(DgBuffer = (unsigned char *)
	calloc(DgBufferSize, sizeof(unsigned char)))) {
    fprintf(stderr,"Unable to allocate dg buffer\n");
    return;
  }
  
  dgResetBuffer();
}

void dgResetBuffer(void)
{
  DgRecording = 1;
  DgBufferIndex = 0;
  
  dgPushStruct(DG_TOP_LEVEL, "DG_TOP_LEVEL");
  
  dgRecordMagicNumber();
  dgRecordFloat(T_VERSION_TAG, dgVersion);
}

void dgCloseBuffer(void)
{
  if (DgBuffer) free(DgBuffer);
  dgFreeStructStack();
  DgRecording = 0;
}

unsigned char *dgGetBuffer(void)
{
  return DgBuffer;
}

int dgGetBufferSize(void)
{
  return DgBufferIndex;
}


/* get estimate of list size (in bytes) */
static int get_list_length(DYN_LIST *dl)
{
  int i, sum = 64;		/* overhead */
  DYN_LIST **vals;

  if (!dl) return sum;

  vals = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_LONG:
    case DF_FLOAT:
      return 4*DYN_LIST_N(dl);
      break;
    case DF_SHORT:
      return 2*DYN_LIST_N(dl);
      break;
    case DF_STRING:
      return 8*DYN_LIST_N(dl);	/* just a guess */
      break;
    case DF_CHAR:
      return DYN_LIST_N(dl);
      break;
    }
  }
  else {
    vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      sum += get_list_length(vals[i]);
    }
  }
  return sum;
}

int dgEstimateGroupSize(DYN_GROUP *dg)
{
  int i;
  int nelts = 0;
  
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    nelts += get_list_length(DYN_GROUP_LIST(dg,i));
  }
  return nelts;
}

int dgSetBufferIncrement(int increment)
{
  int old = DgBufferIncrement;
  if (increment >= 0) DgBufferIncrement = increment;
  return old;
}

int dgWriteBuffer(char *filename, char format)
{
   FILE *fp = stdout;
   char *filemode = "wb+";
   
   switch (format) {
   case DF_BINARY:
   case DF_LZ4:
     filemode = "wb+";
     break;
   default:
     filemode = "w+";
     break;
   }
   
   if (filename && filename[0]) {
     if (!(fp = fopen(filename, filemode))) {
       return 0;
     }
   }

   if (format == DF_LZ4) {
     int bytes_written;
     bytes_written = compress_buffer_to_lz4_file(DgBuffer, DgBufferIndex, fp);
     if (!bytes_written) {
       fclose(fp);
       return 0;
     }
   }
   else {
     dgDumpBuffer(DgBuffer, DgBufferIndex, format, fp);
   }

   if (filename && filename[0]) fclose(fp);
   return 1;
}

int dgWriteBufferCompressed(char *filename)
{
  gzFile file;
  
  if (filename && filename[0]) {
    if (!(file = gzopen(filename, "wb"))) {
      return 0;
    }
  }
  else {
    file = gzdopen(fileno(stdout), "wb");
  }
  
  if (gzwrite(file, DgBuffer, DgBufferIndex) != DgBufferIndex) {
    return 0;
  }
  
  if (filename && filename[0]) {
    if (gzclose(file) != Z_OK) {
      return 0;
    }
  }
  return 1;
}

int dgReadDynGroup(char *filename, DYN_GROUP *dg)
{
  FILE *fp = stdin;
  char *filemode = "rb";
  int status = 0;
  char *suffix;
  unsigned char *data;
  int size;
  
  if (filename && filename[0]) {
    if (!(fp = fopen(filename, filemode))) {
      return(0);
    }
  }
  
  if ((suffix = strrchr(filename, '.'))) {
    if (strlen(suffix) == 4) {
      if ((suffix[1] == 'l' && suffix[2] == 'z' && suffix[3] == '4') ||
	  (suffix[1] == 'L' && suffix[2] == 'Z' && suffix[3] == '4')) {
	if (decompress_lz4_file_to_buffer(fp, &size, &data)) {
	  fclose(fp);
	  status = dguBufferToStruct(data, size, dg);
	  free(data);
	  return status;
	}
	else {
	  fclose(fp);
	  return DF_ABORT;
	}
      }
    }
  }

  status = dguFileToStruct(fp, dg);
  
  if (filename && filename[0]) fclose(fp);
  return(status);
}

#ifdef COMPRESSION
int dgReadDynGroupCompressed(char *filename, DYN_GROUP *dg)
{
  char buf[BUFSIZ];
  int len, err;
  FILE *fp;
  gzFile file;
  int status = 0;
  char fname[L_tmpnam];

  tmpnam(fname);
  if (!(fp = fopen(fname,"wr"))) {
    fprintf(stderr,"dg: unable to open temp file \"%s\"\n",
	    fname);
    return 0;
  }

  if (filename && filename[0]) {
    if (!(file = gzopen(filename, "rb"))) {
      fprintf(stderr,"dg: unable to open file \"%s\" for input\n",
	      filename);
      return 0;
    }
  }
  else {
    file = gzdopen(fileno(stdout), "rb");
  }

  for (;;) {
    len = gzread(file, buf, sizeof(buf));
    if (len < 0) {
      fprintf(stderr, gzerror(file, &err));
      return 0;
    }
    if (len == 0) break;
    if (fwrite(buf, 1, (unsigned)len, fp) != len) {
      fprintf(stderr, "fwrite: error writing uncompressed file");
      if (filename && filename[0]) gzclose(file);
      fclose(fp);
      return 0;
    }
  }

  if (filename && filename[0]) {
    if (gzclose(file) != Z_OK) {
      return 0;
    }
  }

  rewind(fp);
  status = dguFileToStruct(fp, dg);
  
  fclose(fp);
  unlink(fname);
  
  return(status);
}
#endif



void dgLoadStructure(DYN_GROUP *dg)
{
  dguBufferToStruct(DgBuffer, DgBufferIndex, dg);
}

/*********************************************************************/
/*                   High Level Recording Funcs                      */
/*********************************************************************/


void dgRecordDynList(unsigned char tag, DYN_LIST *dl)
{
  dgBeginStruct(tag);
  dgRecordString(DL_NAME_TAG, DYN_LIST_NAME(dl));
  dgRecordLong(DL_INCREMENT_TAG, DYN_LIST_INCREMENT(dl));
  dgRecordLong(DL_FLAGS_TAG, DYN_LIST_FLAGS(dl));
  dgRecordVoidArray(DL_DATA_TAG, DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl),
		    DYN_LIST_VALS(dl));
  dgEndStruct();
}

void dgRecordDynGroup(DYN_GROUP *dg)
{
  int i = 0;
  dgBeginStruct(DG_BEGIN_TAG);
  dgRecordString(DG_NAME_TAG, DYN_GROUP_NAME(dg));
  dgRecordLong(DG_NLISTS_TAG, DYN_GROUP_NLISTS(dg));
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) 
    dgRecordDynList(DG_DYNLIST_TAG, DYN_GROUP_LIST(dg,i));
  dgEndStruct();
}

/*********************************************************************/
/*                   Array Event Recording Funcs                     */
/*********************************************************************/

void dgBeginStruct(unsigned char tag)
{
  dgRecordFlag(tag);
  dgPushStruct(dgGetStructureType(tag), dgGetTagName(tag));
}

void dgEndStruct(void)
{
  dgRecordFlag(END_STRUCT);
  dgPopStruct();
}

void dgRecordVoidArray(unsigned char type, int datatype, int n, void *data)
{
  int i;
  send_event(type, NULL);
  switch (datatype) {
  case DF_CHAR:
    dgRecordCharArray(DL_CHAR_DATA_TAG, n, (char *) data);
    break;
  case DF_SHORT:
    dgRecordShortArray(DL_SHORT_DATA_TAG, n, (short *) data);
    break;
  case DF_LONG:
    dgRecordLongArray(DL_LONG_DATA_TAG, n, (int *) data);
    break;
  case DF_FLOAT:
    dgRecordFloatArray(DL_FLOAT_DATA_TAG, n, (float *) data);
    break;
  case DF_STRING:
    dgRecordStringArray(DL_STRING_DATA_TAG, n, (char **) data);
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) data;
      dgRecordListArray(DL_LIST_DATA_TAG, n);
      for (i = 0; i < n; i++) {
	dgRecordDynList(DL_SUBLIST_TAG, vals[i]);
      }
    }
    break;
  }
}


void dgRecordString(unsigned char type, char *str)
{
  int length;
  if (!str) return;
  length = strlen(str) + 1;
  send_event(type, (unsigned char *) &length);
  send_bytes(length, (unsigned char *)str);
}

void dgRecordStringArray(unsigned char type, int n, char **s)
{
  int length, i;
  char *str;
  
  if (!s) return;
  send_event(type, (unsigned char *) &n);
  
  for (i = 0; i < n; i++) {
    str = s[i];
    length = strlen(str) + 1;
    send_bytes(sizeof(int), (unsigned char *) &length);
    send_bytes(length, (unsigned char *)str);
  }
}

void dgRecordLongArray(unsigned char type, int n, int *a)
{
  send_event(type, (unsigned char *) &n);
  send_bytes(n*sizeof(int), (unsigned char *) a);
}

void dgRecordCharArray(unsigned char type, int n, char *a)
{
  send_event(type, (unsigned char *) &n);
  send_bytes(n*sizeof(char), (unsigned char *) a);
}

void dgRecordShortArray(unsigned char type, int n, short *a)
{
  send_event(type, (unsigned char *) &n);
  send_bytes(n*sizeof(short), (unsigned char *) a);
}

void dgRecordFloatArray(unsigned char type, int n, float *a)
{
  send_event(type, (unsigned char *) &n);
  send_bytes(n*sizeof(float), (unsigned char *) a);
}

void dgRecordListArray(unsigned char type, int n)
{
  send_event(type, (unsigned char *) &n);
}

/*********************************************************************/
/*                  Low Level Event Recording Funcs                  */
/*********************************************************************/

void dgRecordMagicNumber(void)
{
  send_bytes(DG_MAGIC_NUMBER_SIZE, (unsigned char *) dgMagicNumber);
}

void dgRecordFlag(unsigned char type)
{
  send_event(type, (unsigned char *) NULL);
}

void dgRecordChar(unsigned char type, unsigned char val)
{
  send_event(type, (unsigned char *) &val);
}

void dgRecordLong(unsigned char type, int val)
{
  send_event(type, (unsigned char *) &val);
}

void dgRecordShort(unsigned char type, short val)
{
  send_event(type, (unsigned char *) &val);
}

void dgRecordFloat(unsigned char type, float val)
{
  send_event(type, (unsigned char *) &val);
}


/*********************************************************************/
/*                    Keep Track of Current Structure                */
/*********************************************************************/

void dgPushStruct(int newstruct, char *name)
{
  if (!DgStructStack) 
    DgStructStack = 
      (TAG_INFO *) calloc(DgStructStackIncrement, sizeof(TAG_INFO));
  
  else if (DgStructStackIndex == (DgStructStackSize-1)) {
    DgStructStackSize += DgStructStackIncrement;
    DgStructStack = 
      (TAG_INFO *) realloc(DgStructStack, DgStructStackSize*sizeof(TAG_INFO));
  }
  DgStructStackIndex++;
  DgStructStack[DgStructStackIndex].struct_type = newstruct;
  DgStructStack[DgStructStackIndex].tag_name = name;
  DgCurStruct = newstruct;
  DgCurStructName = name;
}
    
int dgPopStruct(void)
{
  if (!DgStructStackIndex) {
    fprintf(stderr, "dgPopStruct(): popped to an empty stack\n");
    return(-1);
  }

  DgStructStackIndex--;
  DgCurStruct = DgStructStack[DgStructStackIndex].struct_type;
  DgCurStructName = DgStructStack[DgStructStackIndex].tag_name;

  return(DgCurStruct);
}

void dgFreeStructStack(void)
{
  if (DgStructStack) free(DgStructStack);
  DgStructStack = NULL;
  DgStructStackSize = 0;
  DgStructStackIndex = -1;
}

int dgGetCurrentStruct(void)
{
  return(DgCurStruct);
}

char *dgGetCurrentStructName(void)
{
  return(DgCurStructName);
}


char *dgGetTagName(int type)
{
  return(DGTagTable[DgCurStruct][type].tag_name);
}

int dgGetDataType(int type)
{
  return(DGTagTable[DgCurStruct][type].data_type);
}

int dgGetStructureType(int type)
{
  return(DGTagTable[DgCurStruct][type].struct_type);
}
/*********************************************************************/
/*               Local Byte Stream Handling Functions                */
/*********************************************************************/

static void send_event(unsigned char type, unsigned char *data)
{
/* First push the tag into the buffer */
  push((unsigned char *)&type, 1, 1);
  
/* The only "special" tag is the END_STRUCT tag, which means pop up */
  if (type == END_STRUCT) return;

/* All other tags may have data; check the current struct tag table  */
  switch(DGTagTable[DgCurStruct][type].data_type) {
  case DF_STRUCTURE:            /* data follows via tags             */
  case DF_FLAG:		
  case DF_VOID_ARRAY:
    break;
  case DF_STRING:		/* all of these start w/a long int   */
  case DF_STRING_ARRAY:
  case DF_LONG_ARRAY:
  case DF_SHORT_ARRAY:
  case DF_FLOAT_ARRAY:
  case DF_CHAR_ARRAY:
  case DF_LIST_ARRAY:
  case DF_LONG:
    push(data, sizeof(int), 1);
    break;
  case DF_CHAR:
    push(data, sizeof(char), 1);
    break;
  case DF_SHORT:
    push(data, sizeof(short), 1);
    break;
  case DF_VERSION:
  case DF_FLOAT:
    push(data, sizeof(float), 1);
    break;
  default:
    fprintf(stderr,"Unrecognized event type: %d\n", type);
    break;
  }
}

static void send_bytes(int n, unsigned char *data)
{
  push(data, sizeof(unsigned char), n);
}

static void push(unsigned char *data, int size, int count)
{
   int nbytes, newsize;
   int buffer_increment = DgBufferIncrement;
   
   nbytes = count * size;
   
   if (DgBufferIndex + nbytes >= DgBufferSize) {
     if (nbytes > buffer_increment)
       buffer_increment = 2*nbytes;
     do {
       newsize = DgBufferSize + buffer_increment;
       DgBuffer = (unsigned char *) realloc(DgBuffer, newsize);
       /* Really need to check that buffer was reallocated properly */
       DgBufferSize = newsize;
     } while(DgBufferIndex + nbytes >= DgBufferSize);
   }
   
   memcpy(&DgBuffer[DgBufferIndex], data, nbytes);
   DgBufferIndex += nbytes;
}


/*************************************************************************/
/*                         Dump Helper Funcs                             */
/*************************************************************************/

static void dgDumpBuffer(unsigned char *buffer, int n, int type, FILE *fp)
{
  switch(type) {
  case DF_BINARY:
    fwrite(buffer, sizeof(unsigned char), n, fp);
    fflush(fp);
    break;
  case DF_ASCII:
    dguBufferToAscii(buffer, n, fp);
    break;
  default:
    break;
  }
}


/*--------------------------------------------------------------------
  -----               Magic Number Functions                     -----
  -------------------------------------------------------------------*/

static 
int confirm_magic_number(FILE *InFP)
{
  int i;
  for (i = 0; i < DG_MAGIC_NUMBER_SIZE; i++) {
    if (getc(InFP) != dgMagicNumber[i]) return(0);
  }
  return(1);
}

static 
int vconfirm_magic_number(char *s)
{
  int i;
  char number[DG_MAGIC_NUMBER_SIZE];
  memcpy(number, s, DG_MAGIC_NUMBER_SIZE);
  for (i = 0; i < DG_MAGIC_NUMBER_SIZE; i++) {
    if (number[i] != dgMagicNumber[i]) return(0);
  }
  return(1);
}


/*--------------------------------------------------------------------
  -----                   File Read Functions                    -----
  -------------------------------------------------------------------*/


static 
void read_version(FILE *InFP, FILE *OutFP)
{
  float val;
  if (fread(&val, sizeof(float), 1, InFP) != 1) {
     fprintf(stderr,"Error reading float info\n");
     exit(-1);
  }

  /* 
   * The VERSION should stay as a float, so that byte ordering can be 
   * checked dynamically.  If it doesn't match the first way, then the
   * dgFlipEvents flag is set and it's tried again.
   */

  if (val != dgVersion) {
    dgFlipEvents = 1;
    val = flipfloat(val);
    if (val != dgVersion) {
      fprintf(stderr,
	      "Unable to read this version of data file (V %5.1f/%5.1f)\n",
	      val, flipfloat(val));
      exit(-1);
    }
  }
  else dgFlipEvents = 0;
  fprintf(OutFP,"%-20s\t%3.1f\n", "DG_VERSION", val);
}

static 
void read_flag(char type, FILE *InFP, FILE *OutFP)
{
  fprintf(OutFP, "%-20s\n", dgGetTagName(type));
}

static 
void read_float(char type, FILE *InFP, FILE *OutFP)
{
  float val;
  if (fread(&val, sizeof(float), 1, InFP) != 1) {
     fprintf(stderr,"Error reading float info\n");
     exit(-1);
  }
  if (dgFlipEvents) val = flipfloat(val);

  fprintf(OutFP, "%-20s\t%6.3f\n", dgGetTagName(type), val);
}

static 
void read_char(char type, FILE *InFP, FILE *OutFP)
{
  char val;
  if (fread(&val, sizeof(char), 1, InFP) != 1) {
     fprintf(stderr,"Error reading char val\n");
     exit(-1);
  }
  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), val);
}


static
void read_long(char type, FILE *InFP, FILE *OutFP)
{
  int val;
  
  if (fread(&val, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading int val\n");
    exit(-1);
  }
  
  if (dgFlipEvents) val = fliplong(val);
  
  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), val);
}

static
void read_short(char type, FILE *InFP, FILE *OutFP)
{
  short val;
  
  if (fread(&val, sizeof(short), 1, InFP) != 1) {
    fprintf(stderr,"Error reading short val\n");
    exit(-1);
  }
  
  if (dgFlipEvents) val = flipshort(val);

  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), val);
}   


/*********************** ARRAY VERSIONS ************************/

static
void read_string(char type, FILE *InFP, FILE *OutFP)
{
  int length;
  char *str = "";

  if (fread(&length, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading string length\n");
    exit(-1);
  }
  
  if (dgFlipEvents) length = fliplong(length);
  if (length) {
    str = (char *) malloc(length);
    
    if (fread(str, length, 1, InFP) != 1) {
      fprintf(stderr,"Error reading\n");
      exit(-1);
    }
  }

  fprintf(OutFP, "%-20s\t%s\n", dgGetTagName(type), str);
  if (length) free(str);
}

static
void read_strings(char type, FILE *InFP, FILE *OutFP)
{
  int n, i;
  int length;
  char *str;

  if (fread(&n, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading string length\n");
    exit(-1);
  }
  if (dgFlipEvents) n = fliplong(n);
  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type),n);

  for (i = 0; i < n; i++) {
    if (fread(&length, sizeof(int), 1, InFP) != 1) {
      fprintf(stderr,"Error reading string length\n");
      exit(-1);
    }
    if (dgFlipEvents) length = fliplong(length);
    
    str = "";
    if (length) {
      str = (char *) malloc(length);
      
      if (fread(str, length, 1, InFP) != 1) {
	fprintf(stderr,"Error reading\n");
	exit(-1);
      }
    }
    
    fprintf(OutFP, "%d\t%s\n", i, str);
    if (length) free(str);
  }
}

static
void read_chars(char type, FILE *InFP, FILE *OutFP)
{
  int nchars, i;
  char *vals = NULL;
  
  if (fread(&nchars, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of chars\n");
    exit(-1);
  }

  if (dgFlipEvents) nchars = fliplong(nchars);
  
  if (nchars) {
    if (!(vals = (char *) calloc(nchars, sizeof(char)))) {
      fprintf(stderr,"Error allocating memory for char array\n");
      exit(-1);
    }
    
    if (fread(vals, sizeof(char), nchars, InFP) != nchars) {
      fprintf(stderr,"Error reading char array\n");
      exit(-1);
    }
  }
  
  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), nchars); 
  
  for (i = 0; i < nchars; i++) {
    fprintf(OutFP, "%d\t%c\n", i+1, vals[i]);
  }
  if (vals) free(vals);
}


static
void read_longs(char type, FILE *InFP, FILE *OutFP)
{
  int nlongs, i;
  int *vals = NULL;
  
  if (fread(&nlongs, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of longs\n");
    exit(-1);
  }
  
  if (dgFlipEvents) nlongs = fliplong(nlongs);
  
  if (nlongs) {
    if (!(vals = (int *) calloc(nlongs, sizeof(int)))) {
      fprintf(stderr,"Error allocating memory for long array\n");
      exit(-1);
    }
    
    if (fread(vals, sizeof(int), nlongs, InFP) != nlongs) {
      fprintf(stderr,"Error reading int array\n");
      exit(-1);
    }
    
    if (dgFlipEvents) fliplongs(nlongs, vals);
  }

  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), nlongs); 
  
  for (i = 0; i < nlongs; i++) {
    fprintf(OutFP, "%d\t%d\n", i+1, vals[i]);
  }
  if (vals) free(vals);
}


static
void read_shorts(char type, FILE *InFP, FILE *OutFP)
{
  int nshorts, i;
  short *vals = NULL;
  
  if (fread(&nshorts, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of shorts\n");
    exit(-1);
  }
  
  if (dgFlipEvents) nshorts = fliplong(nshorts);
  
  if (nshorts) {
    if (!(vals = (short *) calloc(nshorts, sizeof(short)))) {
      fprintf(stderr,"Error allocating memory for short array\n");
      exit(-1);
    }
    
    if (fread(vals, sizeof(short), nshorts, InFP) != nshorts) {
      fprintf(stderr,"Error reading short array\n");
      exit(-1);
    }
    
    if (dgFlipEvents) flipshorts(nshorts, vals);
  }
  
  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), nshorts); 
  
  for (i = 0; i < nshorts; i++) {
    fprintf(OutFP, "%d\t%d\n", i+1, vals[i]);
  }
  if (vals) free(vals);
}

static
void read_floats(char type, FILE *InFP, FILE *OutFP)
{
  int nfloats, i;
  float *vals = NULL;
  
  if (fread(&nfloats, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of floats\n");
    exit(-1);
  }
  
  if (dgFlipEvents) nfloats = fliplong(nfloats);
  
  if (nfloats) {
    if (!(vals = (float *) calloc(nfloats, sizeof(float)))) {
      fprintf(stderr,"Error allocating memory for float array\n");
      exit(-1);
    }
    
    if (fread(vals, sizeof(float), nfloats, InFP) != nfloats) {
      fprintf(stderr,"Error reading float array\n");
      exit(-1);
    }
    
    if (dgFlipEvents) flipfloats(nfloats, vals);
  }
  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), nfloats); 
  
  for (i = 0; i < nfloats; i++) {
    fprintf(OutFP, "%d\t%6.2f\n", i+1, vals[i]);
  }
  if (vals) free(vals);
}



/*--------------------------------------------------------------------
  -----                 Buffer Read Functions                    -----

      Routines for reading from a buffer & dumping events to stdout
      These functions (which start with a v...) all return the number
      of bytes which were accessed from the buffer.

  -----                                                          -----
  -------------------------------------------------------------------*/

static 
int vread_version(float *version, FILE *OutFP)
{
  float val;
  memcpy(&val, version, sizeof(float));
  
  /* 
   * The VERSION should stay as a float, so that byte ordering can be 
   * checked dynamically.  If it doesn't match the first way, then the
   * FlipEvents flag is set and it's tried again.
   */

  if (val != dgVersion) {
    dgFlipEvents = 1;
    val = flipfloat(val);
    if (val != dgVersion) {
      fprintf(stderr,
	      "Unable to read this version of data file (V %5.1f/%5.1f)\n",
	      val, flipfloat(val));
      exit(-1);
    }
  }
  else dgFlipEvents = 0;
  fprintf(OutFP,"%-20s\t%3.1f\n", "DG_VERSION", val);
  return(sizeof(float));
}

static int
vread_flag(char type, FILE *OutFP)
{
  fprintf(OutFP, "%-20s\n", dgGetTagName(type));
  return(0);
}

static 
int vread_float(char type, float *fval, FILE *OutFP)
{
  float val;
  memcpy(&val, fval, sizeof(float));

  if (dgFlipEvents) val = flipfloat(val);

  fprintf(OutFP, "%-20s\t%6.3f\n", dgGetTagName(type), val);
  return(sizeof(float));
}


static 
int vread_char(char type, char *cval, FILE *OutFP)
{
  char val;
  memcpy(&val, cval, sizeof(char));

  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), val);
  return(sizeof(char));
}


static
int vread_long(char type, int *ival, FILE *OutFP)
{
  int val;
  memcpy(&val, ival, sizeof(int));

  if (dgFlipEvents) val = fliplong(val);
  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), val);
  return(sizeof(int));
}   


static
int vread_short(char type, short *sval, FILE *OutFP)
{
  short val;
  memcpy(&val, sval, sizeof(short));

  if (dgFlipEvents) val = flipshort(val);
  
  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), val);
  return(sizeof(short));
}   

/*********************** ARRAY VERSIONS ************************/

static 
int vread_string(char type, int *iptr, FILE *OutFP)
{
  int length;
  int *next = iptr+1;
  char *str = (char *) next;

  memcpy(&length, iptr, sizeof(int));

  if (dgFlipEvents) length = fliplong(length);
  
  if (length) fprintf(OutFP, "%-20s\t%s\n", dgGetTagName(type), str);
  return(length+sizeof(int));
}

static 
int vread_strings(char type, int *iptr, FILE *OutFP)
{
  int n, i;
  int length, sum = 0;
  char *next = (char *) iptr + sizeof(int);
  char *str = "";
  
  memcpy(&n, iptr++, sizeof(int));
  if (dgFlipEvents) n = fliplong(n);
  
  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), n);

  for (i = 0; i < n; i++) {
    memcpy(&length, next, sizeof(int));
    if (dgFlipEvents) length = fliplong(length);

    if (length) str = (char *) next+sizeof(int);
    
    fprintf(OutFP, "%d\t%s\n", i, str);
    sum += length;
    next += sizeof(int)+length;
  }
  return(sum+(n*sizeof(int))+sizeof(int));
}

static
int vread_longs(char type, int *n, FILE *OutFP)
{
  int i;
  int nvals;
  int *next = n+1;
  int *vl = (int *) next;
  int *vals = NULL;

  memcpy(&nvals, n, sizeof(int));
  if (dgFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (int *) calloc(nvals, sizeof(int)))) {
      fprintf(stderr,"dgutils: error allocating space for int array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(int)*nvals);

    if (dgFlipEvents) fliplongs(nvals, vals);
  }
  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), nvals);
  
  for (i = 0; i < nvals; i++) {
    fprintf(OutFP, "%d\t%d\n", i+1, vals[i]);
  }
  
  if (vals) free(vals);
  return(sizeof(int)+nvals*sizeof(int));
}


static
int vread_shorts(char type, int *n, FILE *OutFP)
{
  int i;
  int nvals;
  int *next = n+1;
  short *vl = (short *) next;
  short *vals = NULL;

  memcpy(&nvals, n, sizeof(int));
  if (dgFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (short *) calloc(nvals, sizeof(short)))) {
      fprintf(stderr,"dgutils: error allocating space for short array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(short)*nvals);
    
    if (dgFlipEvents) flipshorts(nvals, vals);
  }
  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), nvals);
  
  for (i = 0; i < nvals; i++) {
    fprintf(OutFP, "%d\t%d\n", i+1, vals[i]);
  }
  
  if (vals) free(vals);
  return(sizeof(int)+nvals*sizeof(short));
}


static
int vread_chars(char type, int *n, FILE *OutFP)
{
  int i;
  int nvals;
  int *next = n+1;
  char *vl = (char *) next;
  char *vals = NULL;

  memcpy(&nvals, n, sizeof(int));
  if (dgFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (char *) calloc(nvals, sizeof(char)))) {
      fprintf(stderr,"dgutils: error allocating space for char array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(char)*nvals);
  }

  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), nvals);
  
  for (i = 0; i < nvals; i++) {
    fprintf(OutFP, "%d\t%c\n", i+1, vals[i]);
  }
  
  if (vals) free(vals);
  return(sizeof(int)+nvals*sizeof(char));
}

static
int vread_floats(char type, int *n, FILE *OutFP)
{
  int i;
  int nvals;
  int *next = n+1;
  float *vl = (float *) next;
  float *vals = NULL;

  memcpy(&nvals, n, sizeof(int));
  if (dgFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (float *) calloc(nvals, sizeof(float)))) {
      fprintf(stderr,"dgutils: error allocating space for float array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(float)*nvals);
    
    if (dgFlipEvents) flipfloats(nvals, vals);
  }
  fprintf(OutFP, "%-20s\t%d\n", dgGetTagName(type), nvals);
  
  for (i = 0; i < nvals; i++) {
    fprintf(OutFP, "%d\t%6.2f\n", i+1, vals[i]);
  }
  
  if (vals) free(vals);
  return(sizeof(int)+nvals*sizeof(float));
}

/*--------------------------------------------------------------------
  -----                   File Skip Functions                    -----
  -------------------------------------------------------------------*/

static
int skip_bytes(FILE *InFP, int n)
{
  if (fseek(InFP, (int) sizeof(float), SEEK_CUR)) {
    fprintf(stderr,"Error skipping float\n");
    return(0);
  }
  return(1);
}

static
void skip_version(FILE *InFP) 
{
  float val;
  if (fread(&val, sizeof(float), 1, InFP) != 1) {
     fprintf(stderr,"Error reading float info\n");
     exit(-1);
  }

  /* 
   * The VERSION should stay as a float, so that byte ordering can be 
   * checked dynamically.  If it doesn't match the first way, then the
   * dgFlipEvents flag is set and it's tried again.
   */

  if (val != dgVersion) {
    dgFlipEvents = 1;
    val = flipfloat(val);
    if (val != dgVersion) {
      fprintf(stderr,
	      "Unable to read this version of data file (V %5.1f/%5.1f)\n",
	      val, flipfloat(val));
      exit(-1);
    }
  }
  else dgFlipEvents = 0;
}

static int skip_float(FILE *InFP) 
{
  return(skip_bytes(InFP, sizeof(float)));
}

static int skip_char(FILE *InFP)
{
  return(skip_bytes(InFP, sizeof(char)));
}

static int skip_short(FILE *InFP)
{
  return(skip_bytes(InFP, sizeof(short)));
}

static int skip_long(FILE *InFP)
{
  return(skip_bytes(InFP, sizeof(int)));
}

static int skip_string(FILE *InFP)
{
  int length;
  
  if (fread(&length, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading string length\n");
    return(0);
  }
  if (dgFlipEvents) length = fliplong(length);
  return(skip_bytes(InFP, length));
}

static int skip_strings(FILE *InFP)
{
  int i, n, sum = 0, size; 
  
  if (fread(&n, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of strings\n");
    return(0);
  }
  if (dgFlipEvents) n = fliplong(n);
  for (i = 0; i < n; i++) {
    size = skip_string(InFP);
    if (!size) return(0);
    sum += size;
  }
  return(sizeof(int)+sum);
}

static int skip_longs(FILE *InFP)
{
  int nvals;
  if (fread(&nvals, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of ints\n");
    exit(-1);
  }
  if (dgFlipEvents) nvals = fliplong(nvals);
  return(skip_bytes(InFP, nvals*sizeof(int)));
}

static int skip_shorts(FILE *InFP)
{
  int nvals;
  if (fread(&nvals, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of shorts\n");
    exit(-1);
  }
  if (dgFlipEvents) nvals = fliplong(nvals);
  return(skip_bytes(InFP, nvals*sizeof(short)));
}

static int skip_floats(FILE *InFP)
{
  int nvals;
  if (fread(&nvals, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of floats\n");
    exit(-1);
  }
  if (dgFlipEvents) nvals = fliplong(nvals);
  return(skip_bytes(InFP, nvals*sizeof(float)));
}

/*--------------------------------------------------------------------
  -----                   Buffer Skip Functions                  -----
  -------------------------------------------------------------------*/

static 
int vskip_version(float *version)
{
  float val;
  memcpy(&val, version, sizeof(float));
  
  /* 
   * The VERSION should stay as a float, so that byte ordering can be 
   * checked dynamically.  If it doesn't match the first way, then the
   * FlipEvents flag is set and it's tried again.
   */

  if (val != dgVersion) {
    dgFlipEvents = 1;
    val = flipfloat(val);
    if (val != dgVersion) {
      fprintf(stderr,
	      "Unable to read this version of data file (V %5.1f/%5.1f)\n",
	      val, flipfloat(val));
      exit(-1);
    }
  }
  else dgFlipEvents = 0;
  return(sizeof(float));
}

static int vskip_float(void)
{ 
  return(sizeof(float)); 
}

static int vskip_char(void) 
{ 
  return(sizeof(char));  
}

static int vskip_short(void) 
{ 
  return(sizeof(short)); 
}

static int vskip_long(void) 
{ 
  return(sizeof(int)); 
}

static int vskip_string(int *l)
{
  int length;
  memcpy(&length, l, sizeof(int));
  
  if (dgFlipEvents) length = fliplong(length);
  return(sizeof(int)+length);
}

static int vskip_strings(int *l)
{
  int n, size, sum = 0, i;
  char *next = (char *) (l) + sizeof(int);

  memcpy(&n, l, sizeof(int));
  if (dgFlipEvents) n = fliplong(n);
  
  for (i = 0; i < n; i++) {
    size = vskip_string((int *)next);
    sum += size;
    next += size;
  }
  return(sizeof(int)+sum);
}

static int vskip_floats(int *n)
{
  int nvals;
  memcpy(&nvals, n, sizeof(int));
  if (dgFlipEvents) nvals = fliplong(nvals);
  return(sizeof(int)+(nvals*sizeof(float)));
}

static int vskip_shorts(int *n)
{
  int nvals;
  memcpy(&nvals, n, sizeof(int));
  if (dgFlipEvents) nvals = fliplong(nvals);
  return(sizeof(int)+(nvals*sizeof(short)));
}

static int vskip_longs(int *n)
{
  int nvals;
  memcpy(&nvals, n, sizeof(int));
  if (dgFlipEvents) nvals = fliplong(nvals);
  return(sizeof(int)+(nvals*sizeof(int)));
}

/*--------------------------------------------------------------------
  -----                    File Get Functions                    -----
  -------------------------------------------------------------------*/

static 
void get_version(FILE *InFP, float *version)
{
  float val;
  if (fread(&val, sizeof(float), 1, InFP) != 1) {
     fprintf(stderr,"Error reading float info\n");
     exit(-1);
  }

  /* 
   * The VERSION should stay as a float, so that byte ordering can be 
   * checked dynamically.  If it doesn't match the first way, then the
   * dgFlipEvents flag is set and it's tried again.
   */

  if (val != dgVersion) {
    dgFlipEvents = 1;
    val = flipfloat(val);
    if (val != dgVersion) {
      fprintf(stderr,
	      "Unable to read this version of data file (V %5.1f/%5.1f)\n",
	      val, flipfloat(val));
      exit(-1);
    }
  }
  else dgFlipEvents = 0;
  *version = val;
}


static 
void get_float(FILE *InFP, float *fval)
{
  float val;
  if (fread(&val, sizeof(float), 1, InFP) != 1) {
     fprintf(stderr,"Error reading float info\n");
     exit(-1);
  }
  if (dgFlipEvents) val = flipfloat(val);
  *fval = val;
}

static
void get_char(FILE *InFP, char *cval)
{
  char val;
  if (fread(&val, sizeof(char), 1, InFP) != 1) {
     fprintf(stderr,"Error reading char val\n");
     exit(-1);
  }
  *cval = val;
}

static 
void get_long(FILE *InFP,  int *ival)
{
  int val;
  
  if (fread(&val, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading int val\n");
    exit(-1);
  }
  
  if (dgFlipEvents) val = fliplong(val);

  *ival = val;
}

static
void get_short(FILE *InFP,  short *sval)
{
  short val;
  
  if (fread(&val, sizeof(short), 1, InFP) != 1) {
    fprintf(stderr,"Error reading short val\n");
    exit(-1);
  }
  
  if (dgFlipEvents) val = flipshort(val);
  *sval = val;
}

static
void get_string(FILE *InFP, int *n, char **s)
{
  int length;
  char *str;
  
  if (fread(&length, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading string length\n");
    exit(-1);
  }
  
  if (dgFlipEvents) length = fliplong(length);
  
  if (length) {
    str = (char *) malloc(length);
    
    if (fread(str, length, 1, InFP) != 1) {
      fprintf(stderr,"Error reading\n");
      exit(-1);
    }
  }
  else str = strdup("");	/* malloc'd empty string */
  
  *n = length;
  *s = str;
}   

static
void get_strings(FILE *InFP, int *num, char ***s)
{
  int i, n, length;
  char **strings = NULL;
  
  if (fread(&n, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of strings\n");
    exit(-1);
  }
  if (dgFlipEvents) n = fliplong(n);

  if (n) {
    strings = (char **) calloc(n, sizeof(char *));
    for (i = 0; i < n; i++) {
      get_string(InFP, &length, &strings[i]);
    }
  }
  
  *num = n;
  *s = strings;
}   

static
void get_chars(FILE *InFP, int *n, char **v)
{
  int nvals;
  char *vals = NULL;

  if (fread(&nvals, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of chars\n");
    exit(-1);
  }
  
  if (dgFlipEvents) nvals = fliplong(nvals);
  
  if (nvals) {
    if (!(vals = (char *) calloc(nvals, sizeof(char)))) {
      fprintf(stderr,"Error allocating memory for char elements\n");
      exit(-1);
    }
    
    if (fread(vals, sizeof(char), nvals, InFP) != nvals) {
      fprintf(stderr,"Error reading char elements\n");
      exit(-1);
    }
  }

  *n = nvals;
  *v = vals;
}

static
void get_shorts(FILE *InFP, int *n, short **v)
{
  int nvals;
  short *vals = NULL;

  if (fread(&nvals, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of shorts\n");
    exit(-1);
  }
  
  if (dgFlipEvents) nvals = fliplong(nvals);
  
  if (nvals) {
    if (!(vals = (short *) calloc(nvals, sizeof(short)))) {
      fprintf(stderr,"Error allocating memory for short elements\n");
      exit(-1);
    }
    
    if (fread(vals, sizeof(short), nvals, InFP) != nvals) {
      fprintf(stderr,"Error reading short elements\n");
      exit(-1);
    }
  if (dgFlipEvents) flipshorts(nvals, vals);
  }

  *n = nvals;
  *v = vals;
}

static
void get_longs(FILE *InFP, int *n, int **v)
{
  int nvals;
  int *vals = NULL;

  if (fread(&nvals, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of ints\n");
    exit(-1);
  }
  
  if (dgFlipEvents) nvals = fliplong(nvals);
  
  if (nvals) {
    if (!(vals = (int *) calloc(nvals, sizeof(int)))) {
      fprintf(stderr,"Error allocating memory for long elements\n");
      exit(-1);
    }
    
    if (fread(vals, sizeof(int), nvals, InFP) != nvals) {
      fprintf(stderr,"Error reading long elements\n");
      exit(-1);
    }
    
    if (dgFlipEvents) fliplongs(nvals, vals);
  }

  *n = nvals;
  *v = vals;
}

static
void get_floats(FILE *InFP, int *n, float **v)
{
  int nvals;
  float *vals = NULL;

  if (fread(&nvals, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of floats\n");
    exit(-1);
  }
  
  if (dgFlipEvents) nvals = fliplong(nvals);
  
  if (nvals) {
    if (!(vals = (float *) calloc(nvals, sizeof(float)))) {
      fprintf(stderr,"Error allocating memory for float elements\n");
      exit(-1);
    }
    
    if (fread(vals, sizeof(float), nvals, InFP) != nvals) {
      fprintf(stderr,"Error reading float elements\n");
      exit(-1);
    }
    
    if (dgFlipEvents) flipfloats(nvals, vals);
  }

  *n = nvals;
  *v = vals;
}

/*--------------------------------------------------------------------
  -----                  Buffer Get Functions                    -----
  -------------------------------------------------------------------*/

static
int vget_version(float *v, float *version)
{
  float val;
  memcpy(&val, v, sizeof(float));
  if (val != dgVersion) {
    dgFlipEvents = 1;
    val = flipfloat(val);
    if (val != dgVersion) {
      fprintf(stderr,
	      "Unable to read this version of data file (V %5.1f/%5.1f)\n",
	      val, flipfloat(val));
      exit(-1);
    }
  }
  else dgFlipEvents = 0;
  *version = val;
  return(sizeof(float));
}
   

static 
int vget_float(float *fval, float *v)
{
  float val;
  memcpy(&val, fval, sizeof(float));

  if (dgFlipEvents) val = flipfloat(val);
  *v = val;
  return(sizeof(float));
}

static 
int vget_char(char *cval, char *c)
{
  *c = *cval;
  return(sizeof(char));
}

static
int vget_long(int *ival, int *l)
{
  int val;
  memcpy(&val, ival, sizeof(int));

  if (dgFlipEvents) val = fliplong(val);
  *l = val;
  return(sizeof(int));
}

static 
int vget_short(short *sval, short *s)
{
  short val;
  memcpy(&val, sval, sizeof(short));

  if (dgFlipEvents) val = flipshort(val);
  *s = val;
  return(sizeof(short));
}

static 
int vget_string(int *iptr, int *l, char **s)
{
  int length;
  int *next = iptr+1;
  static char *str;
  
  memcpy(&length, iptr, sizeof(int));
  
  if (dgFlipEvents) length = fliplong(length);
  
  if (length) {
    str = (char *) malloc(length);
    memcpy(str, (char *) next, length);
  }
  else str = strdup("");

  *l = length;
  *s = str;
  
  return(sizeof(int)+length);
}


static 
int vget_strings(int *iptr, int *num, char ***s)
{
  int n, i, size, sum, length;
  char *next = (char *) iptr + sizeof(int);
  char **strings = NULL;
  
  memcpy(&n, iptr, sizeof(int));
  if (dgFlipEvents) n = fliplong(n);

  if (n) strings = (char **) calloc(n, sizeof(char *));
  for (i = 0, sum = 0; i < n; i++) {
    size = vget_string((int *) next, &length, &strings[i]);
    sum += size;
    next += size;
  }
  *num = n;
  *s = strings;

  return(sizeof(int)+sum);
}

static
int vget_shorts(int *n, int *nv, short **v)
{
  int nvals;
  int *next = n+1;
  short *vl = (short *) next;
  short *vals = NULL;

  memcpy(&nvals, n, sizeof(int));
  if (dgFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (short *) calloc(nvals, sizeof(short)))) {
      fprintf(stderr,"dgutils: error allocating space for short array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(short)*nvals);
    
    if (dgFlipEvents) flipshorts(nvals, vals);
  }

  *nv = nvals;
  *v  = vals;

  return(sizeof(int)+nvals*sizeof(short));
}

static
int vget_chars(int *n, int *nv, char **v)
{
  int nvals;
  int *next = n+1;
  char *vl = (char *) next;
  char *vals = NULL;

  memcpy(&nvals, n, sizeof(int));
  if (dgFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (char *) calloc(nvals, sizeof(char)))) {
      fprintf(stderr,"dgutils: error allocating space for char array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(char)*nvals);
  }

  *nv = nvals;
  *v  = vals;

  return(sizeof(int)+nvals*sizeof(char));
}

static
int vget_longs(int *n, int *nv, int **v)
{
  int nvals;
  int *next = n+1;
  int *vl = (int *) next;
  int *vals = NULL;

  memcpy(&nvals, n, sizeof(int));
  if (dgFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (int *) calloc(nvals, sizeof(int)))) {
      fprintf(stderr,"dgutils: error allocating space for int array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(int)*nvals);
    
    if (dgFlipEvents) fliplongs(nvals, vals);
  }

  *nv = nvals;
  *v  = vals;

  return(sizeof(int)+nvals*sizeof(int));
}

static
int vget_floats(int *n, int *nv, float **v)
{
  int nvals;
  int *next = n+1;
  float *vl = (float *) next;
  float *vals = NULL;

  memcpy(&nvals, n, sizeof(int));
  if (dgFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (float *) calloc(nvals, sizeof(float)))) {
      fprintf(stderr,"dgutils: error allocating space for float array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(float)*nvals);
    
    if (dgFlipEvents) flipfloats(nvals, vals);
  }

  *nv = nvals;
  *v  = vals;

  return(sizeof(int)+nvals*sizeof(float));
}



/*--------------------------------------------------------------------
  -----           File to Structure Transfer Functions           -----
  -------------------------------------------------------------------*/

int dguFileToStruct(FILE *InFP, DYN_GROUP *dg)
{
  int c, status = DF_OK;
  float version;
  
  if (!confirm_magic_number(InFP)) {
    fprintf(stderr,"dgutils: file not recognized as dg format\n");
    return(0);
  }
  
  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:
      status = DF_FINISHED;
      break;
    case DG_VERSION_TAG:
      get_version(InFP, &version);
      break;
    case DG_BEGIN_TAG:
      status = dguFileToDynGroup(InFP, dg);
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      status = DF_ABORT;
      break;
    }
  }
  if (status != DF_ABORT) return(DF_OK);
  else return(status);
}

int dguFileToDynGroup(FILE *InFP, DYN_GROUP *dg)
{
  int n = 0, nlists, c, status = DF_OK;

  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:
      status = DF_FINISHED;
      break;
    case DG_NAME_TAG:
      {
	char *string;
	int n;
	get_string(InFP, &n, &string);
	strncpy(DYN_GROUP_NAME(dg), string, DYN_GROUP_NAME_SIZE-1);
	free((void *) string);
      }
      break;
    case DG_NLISTS_TAG:
      get_long(InFP, (int *) &nlists);
      break;
    case DG_DYNLIST_TAG:
      {
	DYN_LIST *dl = (DYN_LIST *) calloc(1, sizeof(DYN_LIST));
	DYN_LIST_INCREMENT(dl) = 10;
	status = dguFileToDynList(InFP, dl);
	dfuAddDynGroupExistingList(dg, DYN_LIST_NAME(dl), dl);
	n++;
      }
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      status = DF_ABORT;
      break;
    }
  }
  if (status != DF_ABORT) return(DF_OK);
  else return(status);
}

int dguFileToDynList(FILE *InFP, DYN_LIST *dl)
{
  int c, status = DF_OK;

  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:
      status = DF_FINISHED;
      break;
    case DL_INCREMENT_TAG:
      get_long(InFP, (int *) &DYN_LIST_INCREMENT(dl));
      break;
    case DL_FLAGS_TAG:
      get_long(InFP, (int *) &DYN_LIST_FLAGS(dl));
      break;
    case DL_DATA_TAG:
      break;
    case DL_NAME_TAG:
      {
	char *string;
	int n;
	get_string(InFP, &n, &string);
	strncpy(DYN_LIST_NAME(dl), string, DYN_LIST_NAME_SIZE-1);
	free((void *) string);
      }
      break;
    case DL_STRING_DATA_TAG:
      {
	char **data;
	int n;
	get_strings(InFP, &n, &data);
	DYN_LIST_DATATYPE(dl) = DF_STRING;
	DYN_LIST_MAX(dl) = n;
	DYN_LIST_N(dl) = n;
	if (n) DYN_LIST_VALS(dl) = data;
	else {
	  DYN_LIST_VALS(dl) = (char **) calloc(1, sizeof(char *));
	  DYN_LIST_MAX(dl) = 1;
	}
      }
      break;
    case DL_FLOAT_DATA_TAG:
      {
	float *data;
	int n;
	get_floats(InFP, &n, &data);
	DYN_LIST_DATATYPE(dl) = DF_FLOAT;
	DYN_LIST_MAX(dl) = n;
	DYN_LIST_N(dl) = n;
	if (n) DYN_LIST_VALS(dl) = data;
	else DYN_LIST_VALS(dl) = NULL;
      }
      break;
    case DL_LONG_DATA_TAG:
      {
	int *data;
	int n;
	get_longs(InFP, &n, &data);
	DYN_LIST_DATATYPE(dl) = DF_LONG;
	DYN_LIST_MAX(dl) = n;
	DYN_LIST_N(dl) = n;
	if (n) DYN_LIST_VALS(dl) = data;
	else DYN_LIST_VALS(dl) = NULL;
      }
      break;
    case DL_SHORT_DATA_TAG:
      {
	short *data;
	int n;
	get_shorts(InFP, &n, &data);
	DYN_LIST_DATATYPE(dl) = DF_SHORT;
	DYN_LIST_MAX(dl) = n;
	DYN_LIST_N(dl) = n;
	DYN_LIST_VALS(dl) = data;
      }
      break;
    case DL_CHAR_DATA_TAG:
      {
	char *data;
	int n;
	get_chars(InFP, &n, &data);
	DYN_LIST_DATATYPE(dl) = DF_CHAR;
	DYN_LIST_MAX(dl) = n;
	DYN_LIST_N(dl) = n;
	if (n) DYN_LIST_VALS(dl) = data;
	else DYN_LIST_VALS(dl) = NULL;
      }
      break;
    case DL_LIST_DATA_TAG:
      {
	DYN_LIST *newlist, **vals;
	int n, i;

	/* Figure out how many there are */
	get_long(InFP, (int *) &n);

	/* Set the datatype */
	DYN_LIST_DATATYPE(dl) = DF_LIST;

	/* Setup and allocate the appropriate amount of space */
	DYN_LIST_INCREMENT(dl) = 10;
	DYN_LIST_MAX(dl) = n ? n : 1;
	DYN_LIST_N(dl) = n;
	DYN_LIST_VALS(dl) = 
	  (DYN_LIST **) calloc(DYN_LIST_MAX(dl), sizeof(DYN_LIST *));
	vals = (DYN_LIST **) DYN_LIST_VALS(dl);

	/* Now fill up the list of lists by recursively calling this func */
	for (i = 0; i < n; i++) {
	  newlist = (DYN_LIST *) calloc(1, sizeof(DYN_LIST));
	  DYN_LIST_INCREMENT(newlist) = 10;
	  if ((c = getc(InFP)) != DL_SUBLIST_TAG) return(DF_ABORT);
	  status = dguFileToDynList(InFP, newlist);
	  vals[i] = newlist;
	}
      }
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      status = DF_ABORT;
      break;
    }
  }
  if (status != DF_ABORT) return(DF_OK);
  else return(status);
}


/*--------------------------------------------------------------------
  -----         Buffer to Structure Transfer Functions           -----
  -------------------------------------------------------------------*/

int dguBufferToStruct(unsigned char *vbuf, int bufsize, DYN_GROUP *dg)
{
  int c, status = DF_OK;
  int advance_bytes = 0;
  float version;
  BUF_DATA *bdata = (BUF_DATA *) calloc(1, sizeof(BUF_DATA));

  if (!vconfirm_magic_number((char *)vbuf)) {
    return(0);
  }

  BD_BUFFER(bdata) = vbuf;
  BD_INDEX(bdata) = DF_MAGIC_NUMBER_SIZE;
  BD_SIZE(bdata) = bufsize;

  while (status == DF_OK && BD_INDEX(bdata) < BD_SIZE(bdata)) {
    BD_INCINDEX(bdata, advance_bytes);
    advance_bytes = 0;
    c = BD_GETC(bdata);
    switch (c) {
    case END_STRUCT:
      status = DF_FINISHED;
      break;
    case DG_VERSION_TAG:
      advance_bytes += vget_version((float *) BD_DATA(bdata), &version);
      break;
    case DG_BEGIN_TAG:
      status = dguBufferToDynGroup(bdata, dg);
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      status = DF_ABORT;
      break;
    }
  }
  free(bdata);

  if (status != DF_ABORT) return(DF_OK);
  else return(status);
}

static int dguBufferToDynGroup(BUF_DATA *bdata, DYN_GROUP *dg)
{
  int n = 0, c, status = DF_OK, advance_bytes = 0;
  int nlists;

  while (status == DF_OK && !BD_EOF(bdata)) {
    BD_INCINDEX(bdata, advance_bytes);
    advance_bytes = 0;
    c = BD_GETC(bdata);
    switch (c) {
    case END_STRUCT:
      status = DF_FINISHED;
      break;
    case DG_NAME_TAG:
      {
	char *string;
	int n;
	advance_bytes += vget_string((int *) BD_DATA(bdata), 
				     &n, &string);
	strncpy(DYN_GROUP_NAME(dg), string, DYN_GROUP_NAME_SIZE-1);
	free((void *) string);
      }
      break;
    case DG_NLISTS_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), &nlists);
      break;
    case DG_DYNLIST_TAG:
      {
	DYN_LIST *dl = (DYN_LIST *) calloc(1, sizeof(DYN_LIST));
	DYN_LIST_INCREMENT(dl) = 10;
	status = dguBufferToDynList(bdata, dl);
	dfuAddDynGroupExistingList(dg, DYN_LIST_NAME(dl), dl);
	n++;
      }
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      status = DF_ABORT;
      break;
    }
  }
  if (status != DF_ABORT) return(DF_OK);
  else return(status);
}

static int dguBufferToDynList(BUF_DATA *bdata, DYN_LIST *dl)
{
  int c, status = DF_OK;
  int advance_bytes = 0;

  while (status == DF_OK && !BD_EOF(bdata)) {
    BD_INCINDEX(bdata, advance_bytes);
    advance_bytes = 0;
    c = BD_GETC(bdata);
    switch (c) {
    case END_STRUCT:
      status = DF_FINISHED;
      break;
    case DL_INCREMENT_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &DYN_LIST_INCREMENT(dl));
      break;
    case DL_FLAGS_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &DYN_LIST_FLAGS(dl));
      break;
    case DL_DATA_TAG:
      break;
    case DL_NAME_TAG:
      {
	char *string;
	int n;
	advance_bytes += vget_string((int *) BD_DATA(bdata), 
				     &n, &string);
	strncpy(DYN_LIST_NAME(dl), string, DYN_LIST_NAME_SIZE-1);
	free((void *) string);
      }
      break;
    case DL_STRING_DATA_TAG:
      {
	char **data;
	int n;
	advance_bytes += vget_strings((int *) BD_DATA(bdata), &n, &data);
	DYN_LIST_DATATYPE(dl) = DF_STRING;
	DYN_LIST_MAX(dl) = n;
	DYN_LIST_N(dl) = n;
	if (n) DYN_LIST_VALS(dl) = data;
	else {
	  DYN_LIST_VALS(dl) = (char **) calloc(1, sizeof(char *));
	  DYN_LIST_MAX(dl) = 1;
	}

      }
      break;
    case DL_FLOAT_DATA_TAG:
      {
	float *data;
	int n;
	advance_bytes += vget_floats((int *) BD_DATA(bdata), &n, &data);
	DYN_LIST_DATATYPE(dl) = DF_FLOAT;
	DYN_LIST_MAX(dl) = n;
	DYN_LIST_N(dl) = n;
	if (n) DYN_LIST_VALS(dl) = data;
	else DYN_LIST_VALS(dl) = NULL;
      }
      break;
    case DL_LONG_DATA_TAG:
      {
	int *data;
	int n;
	advance_bytes += vget_longs((int *) BD_DATA(bdata), &n, &data);
	DYN_LIST_DATATYPE(dl) = DF_LONG;
	DYN_LIST_MAX(dl) = n;
	DYN_LIST_N(dl) = n;
	if (n) DYN_LIST_VALS(dl) = data;
	else DYN_LIST_VALS(dl) = NULL;
      }
      break;
    case DL_SHORT_DATA_TAG:
      {
	short *data;
	int n;
	advance_bytes += vget_shorts((int *) BD_DATA(bdata), &n, &data);
	DYN_LIST_DATATYPE(dl) = DF_SHORT;
	DYN_LIST_MAX(dl) = n;
	DYN_LIST_N(dl) = n;
	DYN_LIST_VALS(dl) = data;
      }
      break;
    case DL_CHAR_DATA_TAG:
      {
	char *data;
	int n;
	advance_bytes += vget_chars((int *) BD_DATA(bdata), &n, &data);
	DYN_LIST_DATATYPE(dl) = DF_CHAR;
	DYN_LIST_MAX(dl) = n;
	DYN_LIST_N(dl) = n;
	if (n) DYN_LIST_VALS(dl) = data;
	else DYN_LIST_VALS(dl) = NULL;
      }
      break;
    case DL_LIST_DATA_TAG:
      {
	DYN_LIST *newlist, **vals;
	int n, i;

	/* Figure out how many there are */
	advance_bytes = vget_long((int *) BD_DATA(bdata), (int *) &n);
	BD_INCINDEX(bdata, advance_bytes);
	advance_bytes = 0;
	
	/* Set the datatype */
	DYN_LIST_DATATYPE(dl) = DF_LIST;

	/* Setup and allocate the appropriate amount of space */
	DYN_LIST_INCREMENT(dl) = 10;
	DYN_LIST_MAX(dl) = n ? n : 1;
	DYN_LIST_N(dl) = n;
	DYN_LIST_VALS(dl) = 
	  (DYN_LIST **) calloc(DYN_LIST_MAX(dl), sizeof(DYN_LIST *));
	vals = (DYN_LIST **) DYN_LIST_VALS(dl);

	/* Now fill up the list of lists by recursively calling this func */
	for (i = 0; i < n; i++) {
	  newlist = (DYN_LIST *) calloc(1, sizeof(DYN_LIST));
	  DYN_LIST_INCREMENT(newlist) = 10;
	  c = BD_GETC(bdata);
	  if (c != DL_SUBLIST_TAG) return(DF_ABORT);
	  status = dguBufferToDynList(bdata, newlist);
	  vals[i] = newlist;
	}
      }
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      status = DF_ABORT;
      break;
    }
  }
  if (status != DF_ABORT) return(DF_OK);
  else return(status);
}



/*--------------------------------------------------------------------
  -----                    Output Functions                      -----
  -------------------------------------------------------------------*/

void dguBufferToAscii(unsigned char *vbuf, int bufsize, FILE *OutFP)
{
  int c, dtype;
  int i, advance_bytes = 0;
  
  dgPushStruct(DG_TOP_LEVEL, "DG_TOP_LEVEL");

  if (!vconfirm_magic_number((char *)vbuf)) {
    fprintf(stderr,"dgutils: file not recognized as dg format\n");
    exit(-1);
  }
  
  for (i = DG_MAGIC_NUMBER_SIZE; i < bufsize; i+=advance_bytes) {
    c = vbuf[i++];
    if (c == END_STRUCT) {
      fprintf(OutFP, "END:   %s\n", dgGetCurrentStructName());
      dgPopStruct();
      advance_bytes = 0;
      continue;
    }
    switch (dtype = dgGetDataType(c)) {
    case DF_STRUCTURE:
      fprintf(OutFP, "BEGIN: %s\n", dgGetTagName(c));
      dgPushStruct(dgGetStructureType(c), dgGetTagName(c));
      advance_bytes = 0;
      break;
    case DF_VERSION:
      advance_bytes = vread_version((float *) &vbuf[i], OutFP);
      break;
    case DF_VOID_ARRAY:
      advance_bytes = 0;
      break;
    case DF_FLAG:
      advance_bytes = vread_flag(c, OutFP);
      break;
    case DF_CHAR:
      advance_bytes = vread_char(c, (char *) &vbuf[i], OutFP);
      break;
    case DF_LONG:
      advance_bytes = vread_long(c, (int *) &vbuf[i], OutFP);
      break;
    case DF_SHORT:
      advance_bytes = vread_short(c, (short *) &vbuf[i], OutFP);
      break;
    case DF_FLOAT:
      advance_bytes = vread_float(c, (float *) &vbuf[i], OutFP);
      break;
    case DF_STRING:
      advance_bytes = vread_string(c, (int *) &vbuf[i], OutFP);
      break;
    case DF_STRING_ARRAY:
      advance_bytes = vread_strings(c, (int *) &vbuf[i], OutFP);
      break;
    case DF_FLOAT_ARRAY:
      advance_bytes = vread_floats(c, (int *) &vbuf[i], OutFP);
      break;
    case DF_LONG_ARRAY:
      advance_bytes = vread_longs(c, (int *) &vbuf[i], OutFP);
      break;
    case DF_SHORT_ARRAY:
      advance_bytes = vread_shorts(c, (int *) &vbuf[i], OutFP);
      break;
    case DF_LIST_ARRAY:
      advance_bytes = vread_long(c, (int *) &vbuf[i], OutFP);
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      break;
    }
  }
}

void dguFileToAscii(FILE *InFP, FILE *OutFP)
{
  int c, dtype;
  
  dgPushStruct(DG_TOP_LEVEL, "DG_TOP_LEVEL");

  if (!confirm_magic_number(InFP)) {
    fprintf(stderr,"dgutils: file not recognized as dg format\n");
    return;
  }
  
  while((c = getc(InFP)) != EOF) {
    if (c == END_STRUCT) {
      fprintf(OutFP, "END:   %s\n", dgGetCurrentStructName());
      dgPopStruct();
      continue;
    }
    switch (dtype = dgGetDataType(c)) {
    case DF_STRUCTURE:
      fprintf(OutFP, "BEGIN: %s\n", dgGetTagName(c));
      dgPushStruct(dgGetStructureType(c), dgGetTagName(c));
      break;
    case DF_VERSION:
      read_version(InFP, OutFP);
      break;
    case DF_VOID_ARRAY:
      break;
    case DF_FLAG:
      read_flag(c, InFP, OutFP);
      break;
    case DF_CHAR:
      read_char(c, InFP, OutFP);
      break;
    case DF_LONG:
      read_long(c, InFP, OutFP);
      break;
    case DF_SHORT:
      read_short(c, InFP, OutFP);
      break;
    case DF_FLOAT:
      read_float(c, InFP, OutFP);
      break;
    case DF_STRING:
      read_string(c, InFP, OutFP);
      break;
    case DF_STRING_ARRAY:
      read_strings(c, InFP, OutFP);
      break;
    case DF_FLOAT_ARRAY:
      read_floats(c, InFP, OutFP);
      break;
    case DF_LONG_ARRAY:
      read_longs(c, InFP, OutFP);
      break;
    case DF_CHAR_ARRAY:
      read_chars(c, InFP, OutFP);
      break;
    case DF_SHORT_ARRAY:
      read_shorts(c, InFP, OutFP);
      break;
    case DF_LIST_ARRAY:
      read_long(c, InFP, OutFP);
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      break;
    }
  }
}


