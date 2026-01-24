/*************************************************************************
 *
 *  NAME
 *    dfutils.c
 *
 *  DESCRIPTION
 *    Functions for reading/writing/manipulating datafiles in the
 *  common tagged format.
 *
 *  AUTHOR
 *    DLS
 *
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(SUN4) || defined(LYNX)
#include <unistd.h>
#endif

#include "utilc.h"
#include "df.h"

static int dfFlipEvents = 0;	/* to make up for byte ordering probs */

/*--------------------------------------------------------------------
  -----               Magic Number Functions                     -----
  -------------------------------------------------------------------*/

static 
int confirm_magic_number(FILE *InFP)
{
  int i;
  for (i = 0; i < DF_MAGIC_NUMBER_SIZE; i++) {
    if (getc(InFP) != dfMagicNumber[i]) return(0);
  }
  return(1);
}

static 
int vconfirm_magic_number(char *s)
{
  int i;
  char number[DF_MAGIC_NUMBER_SIZE];
  memcpy(number, s, DF_MAGIC_NUMBER_SIZE);
  for (i = 0; i < DF_MAGIC_NUMBER_SIZE; i++) {
    if (number[i] != dfMagicNumber[i]) return(0);
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
   * dfFlipEvents flag is set and it's tried again.
   */

  if (val != dfVersion) {
    dfFlipEvents = 1;
    val = flipfloat(val);
    if (val != dfVersion) {
      fprintf(stderr,
	      "Unable to read this version of data file (V %5.1f/%5.1f)\n",
	      val, flipfloat(val));
      exit(-1);
    }
  }
  else dfFlipEvents = 0;
  fprintf(OutFP,"%-20s\t%3.1f\n", "DF_VERSION", val);
}

static 
void read_flag(char type, FILE *InFP, FILE *OutFP)
{
  fprintf(OutFP, "%-20s\n", dfGetTagName(type));
}

static 
void read_float(char type, FILE *InFP, FILE *OutFP)
{
  float val;
  if (fread(&val, sizeof(float), 1, InFP) != 1) {
     fprintf(stderr,"Error reading float info\n");
     exit(-1);
  }
  if (dfFlipEvents) val = flipfloat(val);

  fprintf(OutFP, "%-20s\t%6.3f\n", dfGetTagName(type), val);
}

static 
void read_char(char type, FILE *InFP, FILE *OutFP)
{
  char val;
  if (fread(&val, sizeof(char), 1, InFP) != 1) {
     fprintf(stderr,"Error reading char val\n");
     exit(-1);
  }
  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type), val);
}


static
void read_long(char type, FILE *InFP, FILE *OutFP)
{
  int val;
  
  if (fread(&val, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading int val\n");
    exit(-1);
  }
  
  if (dfFlipEvents) val = fliplong(val);
  
  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type), val);
}

static
void read_short(char type, FILE *InFP, FILE *OutFP)
{
  short val;
  
  if (fread(&val, sizeof(short), 1, InFP) != 1) {
    fprintf(stderr,"Error reading short val\n");
    exit(-1);
  }
  
  if (dfFlipEvents) val = flipshort(val);

  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type), val);
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
  
  if (dfFlipEvents) length = fliplong(length);
  if (length) {
    str = (char *) malloc(length);
    
    if (fread(str, length, 1, InFP) != 1) {
      fprintf(stderr,"Error reading\n");
      exit(-1);
    }
  }

  fprintf(OutFP, "%-20s\t%s\n", dfGetTagName(type), str);
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
  if (dfFlipEvents) n = fliplong(n);
  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type),n);

  for (i = 0; i < n; i++) {
    if (fread(&length, sizeof(int), 1, InFP) != 1) {
      fprintf(stderr,"Error reading string length\n");
      exit(-1);
    }
    if (dfFlipEvents) length = fliplong(length);
    
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
void read_longs(char type, FILE *InFP, FILE *OutFP)
{
  int nlongs, i;
  int *vals = NULL;
  
  if (fread(&nlongs, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of longs\n");
    exit(-1);
  }
  
  if (dfFlipEvents) nlongs = fliplong(nlongs);
  
  if (nlongs) {
    if (!(vals = (int *) calloc(nlongs, sizeof(int)))) {
      fprintf(stderr,"Error allocating memory for long array\n");
      exit(-1);
    }
  
    if (fread(vals, sizeof(int), nlongs, InFP) != nlongs) {
      fprintf(stderr,"Error reading long array\n");
      exit(-1);
    }
  
    if (dfFlipEvents) fliplongs(nlongs, vals);
  }

  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type), nlongs); 
  
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
  
  if (dfFlipEvents) nshorts = fliplong(nshorts);
  
  if (nshorts) {
    if (!(vals = (short *) calloc(nshorts, sizeof(short)))) {
      fprintf(stderr,"Error allocating memory for short array\n");
      exit(-1);
    }
    
    if (fread(vals, sizeof(short), nshorts, InFP) != nshorts) {
      fprintf(stderr,"Error reading short array\n");
      exit(-1);
    }
    
    if (dfFlipEvents) flipshorts(nshorts, vals);
  }
  
  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type), nshorts); 
  
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
  
  if (dfFlipEvents) nfloats = fliplong(nfloats);
  
  if (nfloats) {
    if (!(vals = (float *) calloc(nfloats, sizeof(float)))) {
      fprintf(stderr,"Error allocating memory for float array\n");
      exit(-1);
    }
    
    if (fread(vals, sizeof(float), nfloats, InFP) != nfloats) {
      fprintf(stderr,"Error reading float array\n");
      exit(-1);
    }
    
    if (dfFlipEvents) flipfloats(nfloats, vals);
  }

  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type), nfloats); 
  
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

  if (val != dfVersion) {
    dfFlipEvents = 1;
    val = flipfloat(val);
    if (val != dfVersion) {
      fprintf(stderr,
	      "Unable to read this version of data file (V %5.1f/%5.1f)\n",
	      val, flipfloat(val));
      exit(-1);
    }
  }
  else dfFlipEvents = 0;
  fprintf(OutFP,"%-20s\t%3.1f\n", "DF_VERSION", val);
  return(sizeof(float));
}

static int
vread_flag(char type, FILE *OutFP)
{
  fprintf(OutFP, "%-20s\n", dfGetTagName(type));
  return(0);
}

static 
int vread_float(char type, float *fval, FILE *OutFP)
{
  float val;
  memcpy(&val, fval, sizeof(float));

  if (dfFlipEvents) val = flipfloat(val);

  fprintf(OutFP, "%-20s\t%6.3f\n", dfGetTagName(type), val);
  return(sizeof(float));
}


static 
int vread_char(char type, char *cval, FILE *OutFP)
{
  char val;
  memcpy(&val, cval, sizeof(char));

  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type), val);
  return(sizeof(char));
}


static
int vread_long(char type, int *ival, FILE *OutFP)
{
  int val;
  memcpy(&val, ival, sizeof(int));

  if (dfFlipEvents) val = fliplong(val);
  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type), val);
  return(sizeof(int));
}   


static
int vread_short(char type, short *sval, FILE *OutFP)
{
  short val;
  memcpy(&val, sval, sizeof(short));

  if (dfFlipEvents) val = flipshort(val);
  
  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type), val);
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

  if (dfFlipEvents) length = fliplong(length);
  
  if (length) fprintf(OutFP, "%-20s\t%s\n", dfGetTagName(type), str);
  return(length+sizeof(int));
}

static 
int vread_strings(char type, int *iptr, FILE *OutFP)
{
  int n, i;
  int length, sum = 0;
  char *next = (char *) iptr + sizeof(int);
  char *str;
  
  memcpy(&n, iptr++, sizeof(int));
  if (dfFlipEvents) n = fliplong(n);
  
  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type), n);

  for (i = 0; i < n; i++) {
    memcpy(&length, next, sizeof(int));
    if (dfFlipEvents) length = fliplong(length);

    str = "";
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
  if (dfFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (int *) calloc(nvals, sizeof(int)))) {
      fprintf(stderr,"dfutils: error allocating space for int array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(int)*nvals);

    if (dfFlipEvents) fliplongs(nvals, vals);
  }
  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type), nvals);
  
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
  if (dfFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (short *) calloc(nvals, sizeof(short)))) {
      fprintf(stderr,"dfutils: error allocating space for short array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(short)*nvals);
    
    if (dfFlipEvents) flipshorts(nvals, vals);
  }
  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type), nvals);
  
  for (i = 0; i < nvals; i++) {
    fprintf(OutFP, "%d\t%d\n", i+1, vals[i]);
  }
  
  if (vals) free(vals);
  return(sizeof(int)+nvals*sizeof(short));
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
  if (dfFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (float *) calloc(nvals, sizeof(float)))) {
      fprintf(stderr,"dfutils: error allocating space for float array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(float)*nvals);

    if (dfFlipEvents) flipfloats(nvals, vals);
  }
  fprintf(OutFP, "%-20s\t%d\n", dfGetTagName(type), nvals);
  
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
   * dfFlipEvents flag is set and it's tried again.
   */

  if (val != dfVersion) {
    dfFlipEvents = 1;
    val = flipfloat(val);
    if (val != dfVersion) {
      fprintf(stderr,
	      "Unable to read this version of data file (V %5.1f/%5.1f)\n",
	      val, flipfloat(val));
      exit(-1);
    }
  }
  else dfFlipEvents = 0;
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
  if (dfFlipEvents) length = fliplong(length);
  return(skip_bytes(InFP, length));
}

static int skip_strings(FILE *InFP)
{
  int i, n, sum = 0, size; 
  
  if (fread(&n, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of strings\n");
    return(0);
  }
  if (dfFlipEvents) n = fliplong(n);
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
  if (dfFlipEvents) nvals = fliplong(nvals);
  return(skip_bytes(InFP, nvals*sizeof(int)));
}

static int skip_shorts(FILE *InFP)
{
  int nvals;
  if (fread(&nvals, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of shorts\n");
    exit(-1);
  }
  if (dfFlipEvents) nvals = fliplong(nvals);
  return(skip_bytes(InFP, nvals*sizeof(short)));
}

static int skip_floats(FILE *InFP)
{
  int nvals;
  if (fread(&nvals, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of floats\n");
    exit(-1);
  }
  if (dfFlipEvents) nvals = fliplong(nvals);
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

  if (val != dfVersion) {
    dfFlipEvents = 1;
    val = flipfloat(val);
    if (val != dfVersion) {
      fprintf(stderr,
	      "Unable to read this version of data file (V %5.1f/%5.1f)\n",
	      val, flipfloat(val));
      exit(-1);
    }
  }
  else dfFlipEvents = 0;
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
  
  if (dfFlipEvents) length = fliplong(length);
  return(sizeof(int)+length);
}

static int vskip_strings(int *l)
{
  int n, size, sum = 0, i;
  char *next = (char *) (l) + sizeof(int);

  memcpy(&n, l, sizeof(int));
  if (dfFlipEvents) n = fliplong(n);
  
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
  if (dfFlipEvents) nvals = fliplong(nvals);
  return(sizeof(int)+(nvals*sizeof(float)));
}

static int vskip_shorts(int *n)
{
  int nvals;
  memcpy(&nvals, n, sizeof(int));
  if (dfFlipEvents) nvals = fliplong(nvals);
  return(sizeof(int)+(nvals*sizeof(short)));
}

static int vskip_longs(int *n)
{
  int nvals;
  memcpy(&nvals, n, sizeof(int));
  if (dfFlipEvents) nvals = fliplong(nvals);
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
   * dfFlipEvents flag is set and it's tried again.
   */

  if (val != dfVersion) {
    dfFlipEvents = 1;
    val = flipfloat(val);
    if (val != dfVersion) {
      fprintf(stderr,
	      "Unable to read this version of data file (V %5.1f/%5.1f)\n",
	      val, flipfloat(val));
      exit(-1);
    }
  }
  else dfFlipEvents = 0;
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
  if (dfFlipEvents) val = flipfloat(val);
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
    fprintf(stderr,"Error reading long val\n");
    exit(-1);
  }
  
  if (dfFlipEvents) val = fliplong(val);

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
  
  if (dfFlipEvents) val = flipshort(val);
  *sval = val;
}

static
void get_string(FILE *InFP, int *n, char **s)
{
  int length;
  static char *str = "";
  
  if (fread(&length, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading string length\n");
    exit(-1);
  }
  
  if (dfFlipEvents) length = fliplong(length);
  
  if (length) {
    str = (char *) malloc(length);
    
    if (fread(str, length, 1, InFP) != 1) {
      fprintf(stderr,"Error reading\n");
      exit(-1);
    }
  }
  
  *n = length;
  *s = str;
}   

static
void get_strings(FILE *InFP, int *num, char ***s)
{
  int n, i, length;
  char **strings = NULL;
  
  if (fread(&n, sizeof(int), 1, InFP) != 1) {
    fprintf(stderr,"Error reading number of strings\n");
    exit(-1);
  }
  if (dfFlipEvents) n = fliplong(n);

  if (n) strings = (char **) calloc(n, sizeof(char *));

  for (i = 0; i < n; i++) {
    get_string(InFP, &length, &strings[i]);
  }
  
  *num = n;
  *s = strings;
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
  
  if (dfFlipEvents) nvals = fliplong(nvals);
  
  if (nvals) {
    if (!(vals = (short *) calloc(nvals, sizeof(short)))) {
      fprintf(stderr,"Error allocating memory for short elements\n");
      exit(-1);
    }
    
    if (fread(vals, sizeof(short), nvals, InFP) != nvals) {
      fprintf(stderr,"Error reading short elements\n");
      exit(-1);
    }
  
    if (dfFlipEvents) flipshorts(nvals, vals);
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
  
  if (dfFlipEvents) nvals = fliplong(nvals);
  
  if (nvals) {
    if (!(vals = (int *) calloc(nvals, sizeof(int)))) {
      fprintf(stderr,"Error allocating memory for long elements\n");
      exit(-1);
    }
    
    if (fread(vals, sizeof(int), nvals, InFP) != nvals) {
      fprintf(stderr,"Error reading int elements\n");
      exit(-1);
    }
    
    if (dfFlipEvents) fliplongs(nvals, vals);
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
  
  if (dfFlipEvents) nvals = fliplong(nvals);
  
  if (nvals) {
    if (!(vals = (float *) calloc(nvals, sizeof(float)))) {
      fprintf(stderr,"Error allocating memory for float elements\n");
      exit(-1);
    }
    
    if (fread(vals, sizeof(float), nvals, InFP) != nvals) {
      fprintf(stderr,"Error reading float elements\n");
      exit(-1);
    }
  
  if (dfFlipEvents) flipfloats(nvals, vals);
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
  if (val != dfVersion) {
    dfFlipEvents = 1;
    val = flipfloat(val);
    if (val != dfVersion) {
      fprintf(stderr,
	      "Unable to read this version of data file (V %5.1f/%5.1f)\n",
	      val, flipfloat(val));
      exit(-1);
    }
  }
  else dfFlipEvents = 0;
  *version = val;
  return(sizeof(float));
}
   

static 
int vget_float(float *fval, float *v)
{
  float val;
  memcpy(&val, fval, sizeof(float));

  if (dfFlipEvents) val = flipfloat(val);
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

  if (dfFlipEvents) val = fliplong(val);
  *l = val;
  return(sizeof(int));
}

static 
int vget_short(short *sval, short *s)
{
  short val;
  memcpy(&val, sval, sizeof(short));

  if (dfFlipEvents) val = flipshort(val);
  *s = val;
  return(sizeof(short));
}

static 
int vget_string(int *iptr, int *l, char **s)
{
  int length;
  int *next = iptr+1;
  static char *str = "";
  
  memcpy(&length, iptr, sizeof(int));
  
  if (dfFlipEvents) length = fliplong(length);
  
  if (length) {
    str = (char *) malloc(length);
    memcpy(str, (char *) next, length);
  }

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
  if (dfFlipEvents) n = fliplong(n);

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
  if (dfFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (short *) calloc(nvals, sizeof(short)))) {
      fprintf(stderr,"dfutils: error allocating space for short array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(short)*nvals);

    if (dfFlipEvents) flipshorts(nvals, vals);
  }

  *nv = nvals;
  *v  = vals;

  return(sizeof(int)+nvals*sizeof(short));
}

static
int vget_longs(int *n, int *nv, int **v)
{
  int nvals;
  int *next = n+1;
  int *vl = (int *) next;
  int *vals = NULL;

  memcpy(&nvals, n, sizeof(int));
  if (dfFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (int *) calloc(nvals, sizeof(int)))) {
      fprintf(stderr,"dfutils: error allocating space for int array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(int)*nvals);
  }

  if (dfFlipEvents) fliplongs(nvals, vals);

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
  if (dfFlipEvents) nvals = fliplong(nvals);

  if (nvals) {
    if (!(vals = (float *) calloc(nvals, sizeof(float)))) {
      fprintf(stderr,"dfutils: error allocating space for float array\n");
      exit(-1);
    }
    memcpy(vals, vl, sizeof(float)*nvals);
    
    if (dfFlipEvents) flipfloats(nvals, vals);
  }
  
  *nv = nvals;
  *v  = vals;

  return(sizeof(int)+nvals*sizeof(float));
}


/*--------------------------------------------------------------------
  -----           File to Structure Transfer Functions           -----
  -------------------------------------------------------------------*/

int dfuFileToStruct(FILE *InFP, DATA_FILE *df)
{
  int c, status = DF_OK;
  float version;
  
  if (!confirm_magic_number(InFP)) {
    fprintf(stderr,"dfutils: file not recognized as df format\n");
    return(0);
  }
  
  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:
      status = DF_FINISHED;
      break;
    case T_VERSION_TAG:
      get_version(InFP, &version);
      break;
    case T_BEGIN_DF_TAG:
      status = dfuFileToDataFile(InFP, df);
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

int dfuFileToDataFile(FILE *InFP, DATA_FILE *df)
{
  int c, status = DF_OK, current_obs = 0, current_cinfo = 0;

  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:
      status = DF_FINISHED;
      break;
    case D_DFINFO_TAG:
      status = dfuFileToDFInfo(InFP, DF_DFINFO(df));
      break;
    case D_NOBSP_TAG:
      get_long(InFP, (int *) &DF_NOBSP(df));
      if (!DF_NOBSP(df)) DF_OBSPS(df) = NULL;
      else {
	DF_OBSPS(df) = (OBS_P *) calloc(DF_NOBSP(df), sizeof(OBS_P));
	if (!DF_OBSPS(df)) status = DF_ABORT;
      }
      break;
    case D_OBSP_TAG:
      if (current_obs == DF_NOBSP(df)) {
	fprintf(stderr, "dfuFileToStruct(): too many obs periods\n");
	status = DF_ABORT;
      }
      status = dfuFileToObsPeriod(InFP, DF_OBSP(df, current_obs));
      current_obs++;
      break;
    case D_NCINFO_TAG:
      get_long(InFP, (int *) &DF_NCINFO(df));
      if (DF_NCINFO(df)) {
	DF_CINFOS(df) = (CELL_INFO *) calloc(DF_NCINFO(df), sizeof(CELL_INFO));
	if (!DF_CINFOS(df)) status = DF_ABORT;
      }
      break;
    case D_CINFO_TAG:
      if (current_cinfo == DF_NCINFO(df)) {
	fprintf(stderr, "dfuFileToStruct(): too many cell infos\n");
	status = DF_ABORT;
      }
      status = dfuFileToCellInfo(InFP, DF_CINFO(df, current_cinfo));
      current_cinfo++;
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

int dfuFileToDFInfo(FILE *InFP, DF_INFO *dfinfo)
{
  int c, status = DF_OK;
  int n;

  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:
      status = DF_FINISHED;
      break;
    case D_FILENAME_TAG:
      get_string(InFP, &n, &DF_FILENAME(dfinfo));
      break;
    case D_TIME_TAG:
      get_long(InFP, &DF_TIME(dfinfo));
      break;
    case D_FILENUM_TAG:
      get_long(InFP, (int *) &DF_FILENUM(dfinfo));
      break;
    case D_AUXFILES_TAG:
      get_strings(InFP, &DF_NAUXFILES(dfinfo), &DF_AUXFILES(dfinfo));
      break;
    case D_COMMENT_TAG:
      get_string(InFP, &n, &DF_COMMENT(dfinfo));
      break;
    case D_EXP_TAG:
      get_long(InFP, (int *) &DF_EXPERIMENT(dfinfo));
      break;
    case D_TMODE_TAG:
      get_long(InFP, (int *) &DF_TESTMODE(dfinfo));
      break;
    case D_NSTIMTYPES_TAG:
      get_long(InFP, (int *) &DF_NSTIMTYPES(dfinfo));
      break;
    case D_EMCOLLECT_TAG:
      get_char(InFP, &DF_EMCOLLECT(dfinfo));
      break;
    case D_SPCOLLECT_TAG:
      get_char(InFP, &DF_SPCOLLECT(dfinfo));
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

int dfuFileToObsPeriod(FILE *InFP, OBS_P *obsp)
{
  int c, status = DF_OK;

  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:
      status = DF_FINISHED;
      break;
    case O_INFO_TAG:
      status = dfuFileToObsInfo(InFP, OBSP_INFO(obsp));
      break;
    case O_EVDATA_TAG:
      status = dfuFileToEvData(InFP, OBSP_EVDATA(obsp));
      break;
    case O_EMDATA_TAG:
      status = dfuFileToEmData(InFP, OBSP_EMDATA(obsp));
      break;
    case O_SPDATA_TAG:
      status = dfuFileToSpData(InFP, OBSP_SPDATA(obsp));
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

int dfuFileToObsInfo(FILE *InFP, OBS_INFO *oinfo)
{
  int c, status = DF_OK;

  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:
      status = DF_FINISHED;
      break;
    case O_FILENUM_TAG:
      get_long(InFP, (int *) &OBS_FILENUM(oinfo));
      break;
    case O_INDEX_TAG:
      get_long(InFP, (int *) &OBS_INDEX(oinfo));
      break;
    case O_BLOCK_TAG:
      get_long(InFP, (int *) &OBS_BLOCK(oinfo));
      break;
    case O_OBSP_TAG:
      get_long(InFP, (int *) &OBS_OBSP(oinfo));
      break;
    case O_STATUS_TAG:
      get_long(InFP, (int *) &OBS_STATUS(oinfo));
      break;
    case O_DURATION_TAG:
      get_long(InFP, (int *) &OBS_DURATION(oinfo));
      break;
    case O_NTRIALS_TAG:
      get_long(InFP, (int *) &OBS_NTRIALS(oinfo));
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

int dfuFileToEvData(FILE *InFP, EV_DATA *evdata)
{
  int c, status = DF_OK;

  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:    
      status = DF_FINISHED;
      break;
    case E_FIXON_TAG:    dfuFileToEvList(InFP, EV_FIXON(evdata));    break;
    case E_FIXOFF_TAG:   dfuFileToEvList(InFP, EV_FIXOFF(evdata));   break;
    case E_STIMON_TAG:   dfuFileToEvList(InFP, EV_STIMON(evdata));   break;
    case E_STIMOFF_TAG:  dfuFileToEvList(InFP, EV_STIMOFF(evdata));  break;
    case E_RESP_TAG:     dfuFileToEvList(InFP, EV_RESP(evdata));     break;
    case E_PATON_TAG:    dfuFileToEvList(InFP, EV_PATON(evdata));    break;
    case E_PATOFF_TAG:   dfuFileToEvList(InFP, EV_PATOFF(evdata));   break;
    case E_STIMTYPE_TAG: dfuFileToEvList(InFP, EV_STIMTYPE(evdata)); break;
    case E_PATTERN_TAG:  dfuFileToEvList(InFP, EV_PATTERN(evdata));  break;
    case E_REWARD_TAG:   dfuFileToEvList(InFP, EV_REWARD(evdata));   break;
    case E_PROBEON_TAG:  dfuFileToEvList(InFP, EV_PROBEON(evdata));  break;
    case E_PROBEOFF_TAG: dfuFileToEvList(InFP, EV_PROBEOFF(evdata)); break;
    case E_SAMPON_TAG:   dfuFileToEvList(InFP, EV_SAMPON(evdata));   break;
    case E_SAMPOFF_TAG:  dfuFileToEvList(InFP, EV_SAMPOFF(evdata));  break;
    case E_FIXATE_TAG:   dfuFileToEvList(InFP, EV_FIXATE(evdata));   break;
    case E_DECIDE_TAG:   dfuFileToEvList(InFP, EV_DECIDE(evdata));   break;
    case E_STIMULUS_TAG: dfuFileToEvList(InFP, EV_STIMULUS(evdata)); break;
    case E_DELAY_TAG:    dfuFileToEvList(InFP, EV_DELAY(evdata));    break;
    case E_ISI_TAG:      dfuFileToEvList(InFP, EV_ISI(evdata));      break;
    case E_CUE_TAG:      dfuFileToEvList(InFP, EV_CUE(evdata));      break;
    case E_TARGET_TAG:   dfuFileToEvList(InFP, EV_TARGET(evdata));   break;
    case E_DISTRACTOR_TAG: dfuFileToEvList(InFP, EV_DISTRACTOR(evdata)); break;
    case E_CORRECT_TAG:  dfuFileToEvList(InFP, EV_CORRECT(evdata));   break;
    case E_UNIT_TAG:     dfuFileToEvList(InFP, EV_UNIT(evdata));     break;
    case E_INFO_TAG:     dfuFileToEvList(InFP, EV_INFO(evdata));     break;
    case E_TRIALTYPE_TAG:  dfuFileToEvList(InFP, EV_TRIALTYPE(evdata));
      break;
    case E_ABORT_TAG:    dfuFileToEvList(InFP, EV_ABORT(evdata));      break;
    case E_PUNISH_TAG:   dfuFileToEvList(InFP, EV_PUNISH(evdata));     break;
    case E_SACCADE_TAG:   dfuFileToEvList(InFP, EV_SACCADE(evdata));   break;
    case E_WRONG_TAG:   dfuFileToEvList(InFP, EV_WRONG(evdata));       break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      status = DF_ABORT;
      break;
    }
  }
  if (status != DF_ABORT) return(DF_OK);
  else return(status);
}

int dfuFileToEvList(FILE *InFP, EV_LIST *evlist)
{
  int c, status = DF_OK;

  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:    
      status = DF_FINISHED;
      break;
    case E_VAL_LIST_TAG: 
      get_longs(InFP, &EV_LIST_N(evlist), &EV_LIST_VALS(evlist));
      break;
    case E_TIME_LIST_TAG: 
      get_longs(InFP, &EV_LIST_NTIMES(evlist), &EV_LIST_TIMES(evlist));
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

int dfuFileToEmData(FILE *InFP, EM_DATA *emdata)
{
  int c, status = DF_OK;
  int n;
  short *data;

  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:    
      status = DF_FINISHED;
      break;
    case E_ONTIME_TAG:
      get_long(InFP, (int *) &EM_ONTIME(emdata));
      break;
    case E_RATE_TAG:
      get_float(InFP, &EM_RATE(emdata));
      break;
    case E_FIXPOS_TAG:
      get_shorts(InFP, &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToEmData: invalid fixpos data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(EM_FIXPOS(emdata), data, sizeof(short)*2);
      free(data);
      break;
    case E_WINDOW_TAG:
      get_shorts(InFP, &n, &data);
      if (n != 4) {
	fprintf(stderr,"dfuFileToEmData: invalid window data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(EM_WINDOW(emdata), data, sizeof(short)*4);
      free(data);
      break;
    case E_WINDOW2_TAG:
      get_shorts(InFP, &n, &data);
      if (n != 4) {
	fprintf(stderr,"dfuFileToEmData: invalid window data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(EM_WINDOW2(emdata), data, sizeof(short)*4);
      free(data);
      break;
    case E_PNT_DEG_TAG:
      get_long(InFP, (int *) &EM_PNT_DEG(emdata));
      break;
    case E_H_EM_LIST_TAG:
      get_shorts(InFP, &EM_NSAMPS(emdata), &EM_SAMPS_H(emdata));
      break;
    case E_V_EM_LIST_TAG:
      get_shorts(InFP, &EM_NSAMPS(emdata), &EM_SAMPS_V(emdata));
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

int dfuFileToSpData(FILE *InFP, SP_DATA *spdata)
{
  int c, status = DF_OK;
  int current_ch = 0;

  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:    
      status = DF_FINISHED;
      break;
    case S_NCHANNELS_TAG:
      get_long(InFP, (int *) &SP_NCHANNELS(spdata));
      if (!SP_NCHANNELS(spdata)) SP_CHANNELS(spdata) = NULL;
      else {
	SP_CHANNELS(spdata) = 
	  (SP_CH_DATA *) calloc(SP_NCHANNELS(spdata), sizeof(SP_CH_DATA));
	if (!SP_CHANNELS(spdata)) status = DF_ABORT;
      }
      break;
    case S_CHANNEL_TAG:
      if (current_ch == SP_NCHANNELS(spdata)) {
	fprintf(stderr, "dfuFileToSpData(): too many spike channels\n");
	status = DF_ABORT;
      }
      status = dfuFileToSpChData(InFP, SP_CHANNEL(spdata, current_ch));
      current_ch++;
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

int dfuFileToSpChData(FILE *InFP, SP_CH_DATA *spchdata)
{
  int c, status = DF_OK;

  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:    
      status = DF_FINISHED;
      break;
    case S_CH_DATA_TAG: 
      get_floats(InFP, 
		 &SP_CH_NSPTIMES(spchdata), 
		 &SP_CH_SPTIMES(spchdata));
      break;
    case S_CH_CELLNUM_TAG:
      get_long(InFP, (int *) &SP_CH_CELLNUM(spchdata));
      break;
    case S_CH_SOURCE_TAG:
      get_char(InFP, &SP_CH_SOURCE(spchdata));
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

int dfuFileToCellInfo(FILE *InFP, CELL_INFO *cinfo)
{
  int c, status = DF_OK;
  int n;
  float *data;

  while(status == DF_OK && (c = getc(InFP)) != EOF) {
    switch (c) {
    case END_STRUCT:    
      status = DF_FINISHED;
      break;
    case C_NUM_TAG:
      get_long(InFP, (int *) &CI_NUMBER(cinfo));
      break;
    case C_DISCRIM_TAG:
      get_float(InFP, &CI_DISCRIM(cinfo));
      break;
    case C_DEPTH_TAG:
      get_float(InFP, &CI_DEPTH(cinfo));
      break;
    case C_EV_TAG:
      get_floats(InFP, &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid ev data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_EVCOORDS(cinfo), data, sizeof(float)*2);
      free(data);
      break;
    case C_XY_TAG:
      get_floats(InFP, &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid xy data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_XYCOORDS(cinfo), data, sizeof(float)*2);
      free(data);
      break;
    case C_RFCENTER_TAG:
      get_floats(InFP, &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid rfcenter data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_RF_CENTER(cinfo), data, sizeof(float)*2);
      free(data);
      break;
    case C_TL_TAG:
      get_floats(InFP, &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid tl rfbox data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_RF_QUAD_UL(cinfo), data, sizeof(float)*2);
      free(data);
      break;
    case C_BL_TAG:
      get_floats(InFP, &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid bl rfbox data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_RF_QUAD_LL(cinfo), data, sizeof(float)*2);
      free(data);
      break;
    case C_BR_TAG:
      get_floats(InFP, &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid br rfbox data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_RF_QUAD_LR(cinfo), data, sizeof(float)*2);
      free(data);
      break;
    case C_TR_TAG:
      get_floats(InFP, &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid tr rfbox data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_RF_QUAD_UR(cinfo), data, sizeof(float)*2);
      free(data);
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

int dfuBufferToStruct(unsigned char *vbuf, int bufsize, DATA_FILE *df)
{
  int c, status = DF_OK;
  int advance_bytes = 0;
  float version;
  BUF_DATA *bdata = (BUF_DATA *) calloc(1, sizeof(BUF_DATA));

  if (!vconfirm_magic_number((char *)vbuf)) {
    fprintf(stderr,"dfutils: file not recognized as df format\n");
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
    case T_VERSION_TAG:
      advance_bytes += vget_version((float *) BD_DATA(bdata), &version);
      break;
    case T_BEGIN_DF_TAG:
      status = dfuBufferToDataFile(bdata, df);
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      status = DF_ABORT;
      break;
    }
  }
  if (status != DF_ABORT) return(DF_OK);
  else return(status);
  
  free(bdata);
}

int dfuBufferToDataFile(BUF_DATA *bdata, DATA_FILE *df)
{
  int c, status = DF_OK, current_obs = 0, current_cinfo = 0, advance_bytes = 0;
  
  while (status == DF_OK && !BD_EOF(bdata)) {
    BD_INCINDEX(bdata, advance_bytes);
    advance_bytes = 0;
    c = BD_GETC(bdata);
    switch (c) {
    case END_STRUCT:
      status = DF_FINISHED;
      break;
    case D_DFINFO_TAG:
      status = dfuBufferToDFInfo(bdata, DF_DFINFO(df));
      break;
    case D_NOBSP_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &DF_NOBSP(df));
      if (!DF_NOBSP(df)) DF_OBSPS(df) = NULL;
      else {
	DF_OBSPS(df) = (OBS_P *) calloc(DF_NOBSP(df), sizeof(OBS_P));
	if (!DF_OBSPS(df)) status = DF_ABORT;
      }
      break;
    case D_OBSP_TAG:
      if (current_obs == DF_NOBSP(df)) {
	fprintf(stderr, "dfuBufferToStruct(): too many obs periods\n");
	status = DF_ABORT;
      }
      status = dfuBufferToObsPeriod(bdata, DF_OBSP(df, current_obs));
      current_obs++;
      break;
    case D_NCINFO_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &DF_NCINFO(df));
      if (!DF_NCINFO(df)) DF_CINFOS(df) = NULL;
      else {
	DF_CINFOS(df) = (CELL_INFO *) calloc(DF_NCINFO(df), sizeof(CELL_INFO));
	if (!DF_CINFOS(df)) status = DF_ABORT;
      }
      break;
    case D_CINFO_TAG:
      if (current_cinfo == DF_NCINFO(df)) {
	fprintf(stderr, "dfuFileToStruct(): too many cell infos\n");
	status = DF_ABORT;
      }
      status = dfuBufferToCellInfo(bdata, DF_CINFO(df, current_cinfo));
      current_cinfo++;
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

int dfuBufferToDFInfo(BUF_DATA *bdata, DF_INFO *dfinfo)
{
  int c, status = DF_OK;
  int advance_bytes = 0, n;
  
  while (status == DF_OK && !BD_EOF(bdata)) {
    BD_INCINDEX(bdata, advance_bytes);
    advance_bytes = 0;
    c = BD_GETC(bdata);
    switch (c) {
    case END_STRUCT:
      status = DF_FINISHED;
      break;
    case D_FILENAME_TAG:
      advance_bytes += vget_string((int *) BD_DATA(bdata), 
				   &n, &DF_FILENAME(dfinfo));
      break;
    case D_AUXFILES_TAG:
      advance_bytes += vget_strings((int *) BD_DATA(bdata),
				    &DF_NAUXFILES(dfinfo), &DF_AUXFILES(dfinfo));
      break;
    case D_TIME_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata),
				 &DF_TIME(dfinfo));
      break;
    case D_FILENUM_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &DF_FILENUM(dfinfo));
      break;
    case D_COMMENT_TAG:
      advance_bytes += vget_string((int *) BD_DATA(bdata), 
				   &n, &DF_COMMENT(dfinfo));
      break;
    case D_EXP_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &DF_EXPERIMENT(dfinfo));
      break;
    case D_TMODE_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &DF_TESTMODE(dfinfo));
      break;
    case D_NSTIMTYPES_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &DF_NSTIMTYPES(dfinfo));
      break;
    case D_EMCOLLECT_TAG:
      advance_bytes += vget_char((char *) BD_DATA(bdata),
				 &DF_EMCOLLECT(dfinfo));
      break;
    case D_SPCOLLECT_TAG:
      advance_bytes += vget_char((char *) BD_DATA(bdata),
				 &DF_SPCOLLECT(dfinfo));
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

int dfuBufferToObsPeriod(BUF_DATA *bdata, OBS_P *obsp)
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
    case O_INFO_TAG:
      status = dfuBufferToObsInfo(bdata, OBSP_INFO(obsp));
      break;
    case O_EVDATA_TAG:
      status = dfuBufferToEvData(bdata, OBSP_EVDATA(obsp));
      break;
    case O_EMDATA_TAG:
      status = dfuBufferToEmData(bdata, OBSP_EMDATA(obsp));
      break;
    case O_SPDATA_TAG:
      status = dfuBufferToSpData(bdata, OBSP_SPDATA(obsp));
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

int dfuBufferToObsInfo(BUF_DATA *bdata, OBS_INFO *oinfo)
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
    case O_FILENUM_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &OBS_FILENUM(oinfo));
      break;
    case O_INDEX_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &OBS_INDEX(oinfo));
      break;
    case O_BLOCK_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &OBS_BLOCK(oinfo));
      break;
    case O_OBSP_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &OBS_OBSP(oinfo));
      break;
    case O_STATUS_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata),
				 (int *) &OBS_STATUS(oinfo));
      break;
    case O_DURATION_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &OBS_DURATION(oinfo));
      break;
    case O_NTRIALS_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &OBS_NTRIALS(oinfo));
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

int dfuBufferToEvData(BUF_DATA *bdata, EV_DATA *evdata)
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
    case E_FIXON_TAG:    dfuBufferToEvList(bdata, EV_FIXON(evdata));    break;
    case E_FIXOFF_TAG:   dfuBufferToEvList(bdata, EV_FIXOFF(evdata));   break;
    case E_STIMON_TAG:   dfuBufferToEvList(bdata, EV_STIMON(evdata));   break;
    case E_STIMOFF_TAG:  dfuBufferToEvList(bdata, EV_STIMOFF(evdata));  break;
    case E_RESP_TAG:     dfuBufferToEvList(bdata, EV_RESP(evdata));     break;
    case E_PATON_TAG:    dfuBufferToEvList(bdata, EV_PATON(evdata));    break;
    case E_PATOFF_TAG:   dfuBufferToEvList(bdata, EV_PATOFF(evdata));   break;
    case E_STIMTYPE_TAG: dfuBufferToEvList(bdata, EV_STIMTYPE(evdata)); break;
    case E_PATTERN_TAG:  dfuBufferToEvList(bdata, EV_PATTERN(evdata));  break;
    case E_REWARD_TAG:   dfuBufferToEvList(bdata, EV_REWARD(evdata));   break;
    case E_PROBEON_TAG:  dfuBufferToEvList(bdata, EV_PROBEON(evdata));  break;
    case E_PROBEOFF_TAG: dfuBufferToEvList(bdata, EV_PROBEOFF(evdata)); break;
    case E_SAMPON_TAG:   dfuBufferToEvList(bdata, EV_SAMPON(evdata));   break;
    case E_SAMPOFF_TAG:  dfuBufferToEvList(bdata, EV_SAMPOFF(evdata));  break;
    case E_FIXATE_TAG:   dfuBufferToEvList(bdata, EV_FIXATE(evdata));   break;
    case E_DECIDE_TAG:   dfuBufferToEvList(bdata, EV_DECIDE(evdata));   break;
    case E_STIMULUS_TAG: dfuBufferToEvList(bdata, EV_STIMULUS(evdata)); break;
    case E_DELAY_TAG:    dfuBufferToEvList(bdata, EV_DELAY(evdata));    break;
    case E_ISI_TAG:      dfuBufferToEvList(bdata, EV_ISI(evdata));      break;
    case E_CUE_TAG:      dfuBufferToEvList(bdata, EV_CUE(evdata));      break;
    case E_TARGET_TAG:   dfuBufferToEvList(bdata, EV_TARGET(evdata));   break;
    case E_DISTRACTOR_TAG: 
                         dfuBufferToEvList(bdata,EV_DISTRACTOR(evdata));break;
    case E_CORRECT_TAG:  dfuBufferToEvList(bdata, EV_CORRECT(evdata));   break;
    case E_UNIT_TAG:     dfuBufferToEvList(bdata, EV_UNIT(evdata));     break;
    case E_INFO_TAG:     dfuBufferToEvList(bdata, EV_INFO(evdata));     break;
    case E_TRIALTYPE_TAG:  dfuBufferToEvList(bdata, EV_TRIALTYPE(evdata));
      break;
    case E_ABORT_TAG:    dfuBufferToEvList(bdata, EV_ABORT(evdata));    break;
    case E_PUNISH_TAG:   dfuBufferToEvList(bdata, EV_PUNISH(evdata));   break;
    case E_SACCADE_TAG:   dfuBufferToEvList(bdata, EV_SACCADE(evdata)); break;
    case E_WRONG_TAG:   dfuBufferToEvList(bdata, EV_WRONG(evdata));     break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      status = DF_ABORT;
      break;
    }
  }
  if (status != DF_ABORT) return(DF_OK);
  else return(status);
}

int dfuBufferToEvList(BUF_DATA *bdata, EV_LIST *evlist)
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
    case E_VAL_LIST_TAG: 
      advance_bytes += vget_longs((int *) BD_DATA(bdata), 
				  &EV_LIST_N(evlist), &EV_LIST_VALS(evlist));
      break;
    case E_TIME_LIST_TAG: 
      advance_bytes += vget_longs((int *) BD_DATA(bdata), 
				  &EV_LIST_N(evlist), &EV_LIST_TIMES(evlist));
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

int dfuBufferToEmData(BUF_DATA *bdata, EM_DATA *emdata)
{
  int c, status = DF_OK;
  int n, advance_bytes = 0;
  short *data;

  while (status == DF_OK && !BD_EOF(bdata)) {
    BD_INCINDEX(bdata, advance_bytes);
    advance_bytes = 0;
    c = BD_GETC(bdata);
    switch (c) {
    case END_STRUCT:    
      status = DF_FINISHED;
      break;
    case E_ONTIME_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &EM_ONTIME(emdata));
      break;
    case E_RATE_TAG:
      advance_bytes += vget_float((float *) BD_DATA(bdata), 
				  &EM_RATE(emdata));
      break;
    case E_FIXPOS_TAG:
      advance_bytes += vget_shorts((int *) BD_DATA(bdata), &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToEmData: invalid fixpos data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(EM_FIXPOS(emdata), data, sizeof(short)*2);
      free(data);
      break;
    case E_WINDOW_TAG:
      advance_bytes += vget_shorts((int *) BD_DATA(bdata), &n, &data);
      if (n != 4) {
	fprintf(stderr,"dfuFileToEmData: invalid window data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(EM_WINDOW(emdata), data, sizeof(short)*4);
      free(data);
      break;
    case E_WINDOW2_TAG:
      advance_bytes += vget_shorts((int *) BD_DATA(bdata), &n, &data);
      if (n != 4) {
	fprintf(stderr,"dfuFileToEmData: invalid window data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(EM_WINDOW2(emdata), data, sizeof(short)*4);
      free(data);
      break;
    case E_PNT_DEG_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &EM_PNT_DEG(emdata));
      break;
    case E_H_EM_LIST_TAG:
      advance_bytes += vget_shorts((int *) BD_DATA(bdata),
				   &EM_NSAMPS(emdata), &EM_SAMPS_H(emdata));
      break;
    case E_V_EM_LIST_TAG:
      advance_bytes += vget_shorts((int *) BD_DATA(bdata),
				   &EM_NSAMPS(emdata), &EM_SAMPS_V(emdata));
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

int dfuBufferToSpData(BUF_DATA *bdata, SP_DATA *spdata)
{
  int c, status = DF_OK;
  int current_ch = 0, advance_bytes = 0;

  while (status == DF_OK && !BD_EOF(bdata)) {
    BD_INCINDEX(bdata, advance_bytes);
    advance_bytes = 0;
    c = BD_GETC(bdata);
    switch (c) {
    case END_STRUCT:    
      status = DF_FINISHED;
      break;
    case S_NCHANNELS_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &SP_NCHANNELS(spdata));
      if (!SP_NCHANNELS(spdata)) SP_CHANNELS(spdata) = NULL;
      else {
	SP_CHANNELS(spdata) = 
	  (SP_CH_DATA *) calloc(SP_NCHANNELS(spdata), sizeof(SP_CH_DATA));
	if (!SP_CHANNELS(spdata)) status = DF_ABORT;
      }
      break;
    case S_CHANNEL_TAG:
      if (current_ch == SP_NCHANNELS(spdata)) {
	fprintf(stderr, "dfuBufferToSpData(): too many spike channels\n");
	status = DF_ABORT;
      }
      status = dfuBufferToSpChData(bdata, SP_CHANNEL(spdata, current_ch));
      current_ch++;
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

int dfuBufferToSpChData(BUF_DATA *bdata, SP_CH_DATA *spchdata)
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
    case S_CH_DATA_TAG: 
      advance_bytes += vget_floats((int *) BD_DATA(bdata),
				   &SP_CH_NSPTIMES(spchdata), 
				   &SP_CH_SPTIMES(spchdata));
      break;
    case S_CH_CELLNUM_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &SP_CH_CELLNUM(spchdata));
      break;
    case S_CH_SOURCE_TAG:
      advance_bytes += vget_char((char *) BD_DATA(bdata), 
				 &SP_CH_SOURCE(spchdata));
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

int dfuBufferToCellInfo(BUF_DATA *bdata, CELL_INFO *cinfo)
{
  int c, status = DF_OK;
  int n, advance_bytes = 0;
  float *data;

  while (status == DF_OK && !BD_EOF(bdata)) {
    BD_INCINDEX(bdata, advance_bytes);
    advance_bytes = 0;
    c = BD_GETC(bdata);
    switch (c) {
    case END_STRUCT:    
      status = DF_FINISHED;
      break;
    case C_NUM_TAG:
      advance_bytes += vget_long((int *) BD_DATA(bdata), 
				 (int *) &CI_NUMBER(cinfo));
      break;
    case C_DISCRIM_TAG:
      advance_bytes += vget_float((float *) BD_DATA(bdata), 
				  &CI_DISCRIM(cinfo));
      break;
    case C_DEPTH_TAG:
      advance_bytes += vget_float((float *) BD_DATA(bdata), 
				  &CI_DEPTH(cinfo));
      break;
    case C_EV_TAG:
      advance_bytes += vget_floats((int *) BD_DATA(bdata), &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid ev data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_EVCOORDS(cinfo), data, sizeof(float)*2);
      free(data);
      break;
    case C_XY_TAG:
      advance_bytes += vget_floats((int *) BD_DATA(bdata), &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid xy data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_XYCOORDS(cinfo), data, sizeof(float)*2);
      free(data);
      break;
    case C_RFCENTER_TAG:
      advance_bytes += vget_floats((int *) BD_DATA(bdata), &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid rfcenter data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_RF_CENTER(cinfo), data, sizeof(float)*2);
      free(data);
      break;
    case C_TL_TAG:
      advance_bytes += vget_floats((int *) BD_DATA(bdata), &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid rfbox data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_RF_QUAD_UL(cinfo), data, sizeof(float)*2);
      free(data);
      break;
    case C_BL_TAG:
      advance_bytes += vget_floats((int *) BD_DATA(bdata), &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid rfbox data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_RF_QUAD_LL(cinfo), data, sizeof(float)*2);
      free(data);
      break;
    case C_BR_TAG:
      advance_bytes += vget_floats((int *) BD_DATA(bdata), &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid rfbox data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_RF_QUAD_LR(cinfo), data, sizeof(float)*2);
      free(data);
      break;
    case C_TR_TAG:
      advance_bytes += vget_floats((int *) BD_DATA(bdata), &n, &data);
      if (n != 2) {
	fprintf(stderr,"dfuFileToCellInfo: invalid rfbox data\n");
	free(data);
	status = DF_ABORT;
      }
      memcpy(CI_RF_QUAD_UR(cinfo), data, sizeof(float)*2);
      free(data);
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
  -----                     Output Functions                     -----
  -------------------------------------------------------------------*/

void dfuBufferToAscii(unsigned char *vbuf, int bufsize, FILE *OutFP)
{
  int c, dtype;
  int i, advance_bytes = 0;
  
  dfPushStruct(TOP_LEVEL, "TOP_LEVEL");

  if (!vconfirm_magic_number((char *)vbuf)) {
    fprintf(stderr,"dfutils: file not recognized as df format\n");
    exit(-1);
  }
  
  for (i = DF_MAGIC_NUMBER_SIZE; i < bufsize; i+=advance_bytes) {
    c = vbuf[i++];
    if (c == END_STRUCT) {
      fprintf(OutFP, "END:   %s\n", dfGetCurrentStructName());
      dfPopStruct();
      advance_bytes = 0;
      continue;
    }
    switch (dtype = dfGetDataType(c)) {
    case DF_STRUCTURE:
      fprintf(OutFP, "BEGIN: %s\n", dfGetTagName(c));
      dfPushStruct(dfGetStructureType(c), dfGetTagName(c));
      advance_bytes = 0;
      break;
    case DF_VERSION:
      advance_bytes = vread_version((float *) &vbuf[i], OutFP);
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
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      break;
    }
  }
}

void dfuFileToAscii(FILE *InFP, FILE *OutFP)
{
  int c, dtype;
  
  dfPushStruct(TOP_LEVEL, "TOP_LEVEL");

  if (!confirm_magic_number(InFP)) {
    fprintf(stderr,"dfutils: file not recognized as df format\n");
    exit(-1);
  }
  
  while((c = getc(InFP)) != EOF) {
    if (c == END_STRUCT) {
      fprintf(OutFP, "END:   %s\n", dfGetCurrentStructName());
      dfPopStruct();
      continue;
    }
    switch (dtype = dfGetDataType(c)) {
    case DF_STRUCTURE:
      fprintf(OutFP, "BEGIN: %s\n", dfGetTagName(c));
      dfPushStruct(dfGetStructureType(c), dfGetTagName(c));
      break;
    case DF_VERSION:
      read_version(InFP, OutFP);
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
    case DF_SHORT_ARRAY:
      read_shorts(c, InFP, OutFP);
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      break;
    }
  }
}


/*--------------------------------------------------------------------
  -----               dfu Functions (data file utils)            -----
  -------------------------------------------------------------------*/


/***********************************************************************
 *
 * dfuCreateDataFile()
 *
 *    Create DATA_FILE structure and initialize (if necessary).
 *
 ***********************************************************************/

DATA_FILE *dfuCreateDataFile(void)
{
  return( (DATA_FILE *) calloc(1, sizeof(DATA_FILE)) );
}

/***********************************************************************
 *
 * dfuCreateObsPeriods(DATA_FILE *df, int n)
 *
 *    Create n OBS_P strucures and initialize (if necessary).
 *
 ***********************************************************************/

int dfuCreateObsPeriods(DATA_FILE *df, int n)
{
  DF_NOBSP(df) = n;
  if (n) {
    DF_OBSPS(df) = (OBS_P *) calloc(n, sizeof(OBS_P));
    if (!DF_OBSPS(df)) return(0);
  }
  else DF_OBSPS(df) = NULL;
  return(n);
}

/***********************************************************************
 *
 * dfuCreateObsPeriod()
 *
 *   Allocate and return a single observation period structure.
 *
 ***********************************************************************/

OBS_P *dfuCreateObsPeriod(void)
{
  return((OBS_P *) calloc(1, sizeof(OBS_P)));
}

/***********************************************************************
 *
 * dfuCreateCellInfos(DATA_FILE *df, int n)
 *
 *    Create n CELL_INFO strucures and initialize (if necessary).
 *
 ***********************************************************************/

int dfuCreateCellInfos(DATA_FILE *df, int n)
{
  DF_NCINFO(df) = n;
  if (n) {
    DF_CINFOS(df) = (CELL_INFO *) calloc(DF_NCINFO(df), sizeof(CELL_INFO));
    if (!DF_CINFOS(df)) return(0);
  }
  else {
    DF_CINFOS(df) = NULL;
  }
  return(n);
}

/***********************************************************************
 *
 * dfuCreateSpikeChannels(SP_DATA *spdata, int n)
 *
 *    Create n spike channels in spdata and initialize.
 *
 ***********************************************************************/

int dfuCreateSpikeChannels(SP_DATA *spdata, int n)
{
  SP_NCHANNELS(spdata) = n;
  if (n) {
    SP_CHANNELS(spdata) = (SP_CH_DATA *) calloc(n, sizeof(SP_CH_DATA));
    if (!SP_CHANNELS(spdata)) return(0);
  }
  else {
    SP_CHANNELS(spdata) = NULL;
  }
  return(n);
}

/***********************************************************************
 *
 * dfuSetEm...(EM_DATA *spdata, ...)
 *
 *    Simple interface to em_data structure
 *
 ***********************************************************************/

void dfuSetEmFixPos(EM_DATA *emdata, int x, int y)
{
  EM_FIXPOS(emdata)[0] = x;
  EM_FIXPOS(emdata)[1] = y;
}

void dfuSetEmWindow(EM_DATA *emdata, int v0, int v1, int v2, int v3)
{
  EM_WINDOW(emdata)[0] = v0;
  EM_WINDOW(emdata)[1] = v1;
  EM_WINDOW(emdata)[2] = v2;
  EM_WINDOW(emdata)[3] = v3;
}

/***********************************************************************
 *
 * dfuSetSp...(SP_DATA *spdata, ...)
 *
 *    Simple interface to sp_data structure
 *
 ***********************************************************************/

void dfuSetSpChSource(SP_DATA *spdata, int channel, char source)
{
  SP_CH_SOURCE(SP_CHANNEL(spdata, channel)) = source;
}

void dfuSetSpChCellnum(SP_DATA *spdata, int channel, int cellnum)
{
  SP_CH_CELLNUM(SP_CHANNEL(spdata, channel)) = cellnum;
}

/*--------------------------------------------------------------------
  -----               dfu Free Functions                         -----
  -------------------------------------------------------------------*/

void dfuFreeDataFile(DATA_FILE *df)
{
  int i;
  for (i = 0; i < DF_NOBSP(df); i++) dfuFreeObsPeriod(DF_OBSP(df,i));
  if (DF_OBSPS(df)) free(DF_OBSPS(df));
  for (i = 0; i < DF_NCINFO(df); i++) dfuFreeCellInfo(DF_CINFO(df,i));
  if (DF_CINFOS(df)) free(DF_CINFOS(df));
  dfuFreeDFInfo(DF_DFINFO(df));
  if (df) free(df);
}

void dfuFreeDFInfo(DF_INFO *dfinfo)
{
  int i;

  if (!DF_AUXFILES(dfinfo)) return;

  /* free each auxfile string */
  for (i = 0; i < DF_NAUXFILES(dfinfo); i++)
    if (DF_AUXFILE(dfinfo, i)) free(DF_AUXFILE(dfinfo, i));

  /* free the array of auxfile char *'s */
  free(DF_AUXFILES(dfinfo));
}

void dfuFreeObsPeriod(OBS_P *obsp)
{
  dfuFreeObsInfo(OBSP_INFO(obsp));
  dfuFreeEvData(OBSP_EVDATA(obsp));
  dfuFreeSpData(OBSP_SPDATA(obsp));
  dfuFreeEmData(OBSP_EMDATA(obsp));
}

void dfuFreeObsInfo(OBS_INFO *obsinfo)
{
/* nothing is allocated so we do nothing */
}

void dfuFreeEvData(EV_DATA *evdata)
{
  dfuFreeEvList(EV_FIXON(evdata));
  dfuFreeEvList(EV_FIXOFF(evdata));
  dfuFreeEvList(EV_STIMON(evdata));
  dfuFreeEvList(EV_STIMOFF(evdata));
  dfuFreeEvList(EV_RESP(evdata));
  dfuFreeEvList(EV_PATON(evdata));
  dfuFreeEvList(EV_PATOFF(evdata));
  dfuFreeEvList(EV_STIMTYPE(evdata));
  dfuFreeEvList(EV_PATTERN(evdata));
  dfuFreeEvList(EV_REWARD(evdata));
  dfuFreeEvList(EV_PROBEON(evdata));
  dfuFreeEvList(EV_PROBEOFF(evdata));
  dfuFreeEvList(EV_SAMPON(evdata));
  dfuFreeEvList(EV_SAMPOFF(evdata));
  dfuFreeEvList(EV_FIXATE(evdata));
  dfuFreeEvList(EV_DECIDE(evdata));
  dfuFreeEvList(EV_STIMULUS(evdata));
  dfuFreeEvList(EV_DELAY(evdata));
  dfuFreeEvList(EV_ISI(evdata));
  dfuFreeEvList(EV_CUE(evdata));
  dfuFreeEvList(EV_TARGET(evdata));
  dfuFreeEvList(EV_DISTRACTOR(evdata));
  dfuFreeEvList(EV_UNIT(evdata));
  dfuFreeEvList(EV_INFO(evdata));
  dfuFreeEvList(EV_TRIALTYPE(evdata));
  dfuFreeEvList(EV_ABORT(evdata));
  dfuFreeEvList(EV_WRONG(evdata));
  dfuFreeEvList(EV_PUNISH(evdata));
  dfuFreeEvList(EV_SACCADE(evdata));
}

void dfuFreeEvList(EV_LIST *evlist)
{
  if (EV_LIST_N(evlist)) {
    free(EV_LIST_VALS(evlist));
    free(EV_LIST_TIMES(evlist));
  }
}

void dfuFreeSpData(SP_DATA *spdata)
{
  int i;
  for (i = 0; i < SP_NCHANNELS(spdata); i++)
    dfuFreeSpChData(SP_CHANNEL(spdata, i));
  if (SP_CHANNELS(spdata)) free(SP_CHANNELS(spdata));
}

void dfuFreeSpChData(SP_CH_DATA *spchdata)
{
  if (SP_CH_SPTIMES(spchdata)) free(SP_CH_SPTIMES(spchdata));
}

void dfuFreeEmData(EM_DATA *emdata)
{
  if (!EM_NSAMPS(emdata)) return;
  if (EM_SAMPS_H(emdata)) free(EM_SAMPS_H(emdata));
  if (EM_SAMPS_V(emdata)) free(EM_SAMPS_V(emdata));
}

void dfuFreeCellInfo(CELL_INFO *cinfo)
{

  /* check description field to see if it is allocated */
  
}

/*--------------------------------------------------------------------
  -----           Dynamic List Functions (data file utils)       -----
  --------------------------------------------------------------------*/

/***********************************************************************
 *
 * dfuCreateDynObsPeriods()
 *
 *    Create a dynamic "olist" for holding observation periods.
 *
 ***********************************************************************/

DYN_OLIST *dfuCreateDynObsPeriods(void)
{
  DYN_OLIST *dynolist = (DYN_OLIST *) calloc(1, sizeof(DYN_OLIST));
  if (!dynolist) return(NULL);
  
  DYN_OLIST_INCREMENT(dynolist) = 10;
  DYN_OLIST_MAX(dynolist) = 10;
  DYN_OLIST_VALS(dynolist) = 
    (OBS_P **) calloc(DYN_OLIST_MAX(dynolist), sizeof(OBS_P *));
  if (!DYN_OLIST_VALS(dynolist)) {
    free(dynolist);
    return(NULL);
  }
  DYN_OLIST_N(dynolist) = 0;
  
  return(dynolist);
}

/***********************************************************************
 *
 * dfuAddObsPeriod(DYN_OLIST *, OBS_P *)
 *
 *    Append an obs period to a dynamic list checking to ensure adequate
 *  storage.
 *
 ***********************************************************************/

void dfuAddObsPeriod(DYN_OLIST *dynolist, OBS_P *obsp)
{
  if (DYN_OLIST_N(dynolist) == DYN_OLIST_MAX(dynolist)) {
    DYN_OLIST_MAX(dynolist) += DYN_OLIST_INCREMENT(dynolist);
    DYN_OLIST_VALS(dynolist) = 
      (OBS_P **) realloc(DYN_OLIST_VALS(dynolist), 
			 sizeof(OBS_P *) * DYN_OLIST_MAX(dynolist));
  }
  DYN_OLIST_VALS(dynolist)[DYN_LIST_N(dynolist)] = obsp;
  DYN_OLIST_N(dynolist)++;
}

/***********************************************************************
 *
 * dfuCreateDynEvData()
 *
 *    Create a dynamic group for holding event info lists.  Twice
 * as many lists are created as there are event types.  One set is for
 * vals, the other for times.
 *
 ***********************************************************************/

DYN_GROUP *dfuCreateDynEvData(void)
{
  int i, nlists = 2*E_NEVENT_TAGS;
  
  DYN_GROUP *dg = dfuCreateDynGroup(nlists);
  
  for (i = 0; i < nlists; i++) 
    dfuAddDynGroupNewList(dg, "", DF_LONG, 4);
  
  return(dg);
}

/***********************************************************************
 *
 * dfuAddEvData()
 *
 *    Add event value and time for event types.
 *
 ***********************************************************************/

void dfuAddEvData(DYN_GROUP *evgroup, int type, int val, int time)
{
  dfuAddDynListLong(DYN_GROUP_LIST(evgroup,type*2), val);
  dfuAddDynListLong(DYN_GROUP_LIST(evgroup,type*2+1), time);
}


/***********************************************************************
 *
 * dfuAddEvData4Params()
 *
 *    Add event value and time for event types.
 *
 ***********************************************************************/

void dfuAddEvData4Params(DYN_GROUP *evgroup, int type, int val, int time,
			 int p1, int p2, int p3, int p4)
{
  dfuAddDynListLong(DYN_GROUP_LIST(evgroup,type*2+1), time);
  dfuAddDynListLong(DYN_GROUP_LIST(evgroup,type*2), val);
  dfuAddDynListLong(DYN_GROUP_LIST(evgroup,type*2), p1);
  dfuAddDynListLong(DYN_GROUP_LIST(evgroup,type*2), p2);
  dfuAddDynListLong(DYN_GROUP_LIST(evgroup,type*2), p3);
  dfuAddDynListLong(DYN_GROUP_LIST(evgroup,type*2), p4);
}

/***********************************************************************
 *
 * dfuCreateDynSpData()
 *
 *    Create a dynamic list for holding spike data (float).
 *
 ***********************************************************************/

DYN_GROUP *dfuCreateDynSpData(int nchannels)
{
  int i;
  DYN_GROUP *dg = dfuCreateDynGroup(nchannels);
  
  /* create lists for each channel */
  
  for (i = 0; i < nchannels; i++) 
    dfuAddDynGroupNewList(dg, "", DF_FLOAT, 25);
  
  return(dg);
}

/***********************************************************************
 *
 * dfuAddSpData()
 *
 *    Add spike data to a sp dyn_group
 *
 ***********************************************************************/

void dfuAddSpData(DYN_GROUP *spgroup, int channel, float time)
{
  /* could check DYN_GROUP_NLISTS() here but we'll assume spgroup's OK */
  dfuAddDynListFloat(DYN_GROUP_LIST(spgroup,channel), time);
}

/***********************************************************************
 *
 * dfuCreateDynEmData()
 *
 *    Create a dynamic list for holding eye movements (shorts).
 *
 ***********************************************************************/

DYN_GROUP *dfuCreateDynEmData(void)
{
  DYN_GROUP *dg = dfuCreateDynGroup(2);
  
  /* create list for horizontal and vertical data */
  
  dfuAddDynGroupNewList(dg, "H_EM", DF_SHORT, 200);
  dfuAddDynGroupNewList(dg, "V_EM", DF_SHORT, 200);
  
  return(dg);
}

/***********************************************************************
 *
 * dfuAddEmData()
 *
 *    Add an em sample to an em dyn_group
 *
 ***********************************************************************/

void dfuAddEmData(DYN_GROUP *emgroup, short hsamp, short vsamp)
{
  /* could check DYN_GROUP_NLISTS() here but we'll assume emgroup's OK */

  dfuAddDynListShort(DYN_GROUP_LIST(emgroup,0), hsamp);
  dfuAddDynListShort(DYN_GROUP_LIST(emgroup,1), vsamp);
}


/***********************************************************************
 *
 * dfuCreateDynList()
 *
 *    Create a dynamic list which grows and shrinks for holding events.
 *
 ***********************************************************************/

DYN_LIST *dfuCreateDynList(int datatype, int increment)
{
  return(dfuCreateNamedDynList("", datatype, increment));
}


/***********************************************************************
 *
 * dfuCreateNamedDynList()
 *
 *    Create a named dynamic list which grows and shrinks.
 *
 ***********************************************************************/

DYN_LIST *dfuCreateNamedDynList(char *name, int datatype, int increment)
{
  DYN_LIST *dynlist = (DYN_LIST *) calloc(1, sizeof(DYN_LIST));
  if (!dynlist) {
    fprintf(stderr,"dlsh/dlwish: out of memory\n");
    return(NULL);
  }

  if (name != DYN_LIST_NAME(dynlist))
    strncpy(DYN_LIST_NAME(dynlist), name, DYN_LIST_NAME_SIZE-1);

  /* Don't allow zero length allocs */
  if (!increment) increment++;

  DYN_LIST_FLAGS(dynlist) = 0;
  DYN_LIST_INCREMENT(dynlist) = increment;
  DYN_LIST_MAX(dynlist) = increment;
  DYN_LIST_DATATYPE(dynlist) = datatype;
  switch (DYN_LIST_DATATYPE(dynlist)) {
  case DF_LONG:
    DYN_LIST_VALS(dynlist) = 
      (int *)calloc(DYN_LIST_MAX(dynlist), sizeof(int));
    break;
  case DF_SHORT:
    DYN_LIST_VALS(dynlist) = 
      (short *)calloc(DYN_LIST_MAX(dynlist), sizeof(short));
    break;
  case DF_FLOAT:
    DYN_LIST_VALS(dynlist) = 
      (float *)calloc(DYN_LIST_MAX(dynlist), sizeof(float));
    break;
  case DF_CHAR:
    DYN_LIST_VALS(dynlist) = 
      (char *)calloc(DYN_LIST_MAX(dynlist), sizeof(char));
    break;
  case DF_STRING:
    DYN_LIST_VALS(dynlist) = 
      (char **)calloc(DYN_LIST_MAX(dynlist), sizeof(char *));
    break;
  case DF_LIST:
    DYN_LIST_VALS(dynlist) = 
      (DYN_LIST **)calloc(DYN_LIST_MAX(dynlist), sizeof(DYN_LIST *));
    break;
  }
  
  if (!DYN_LIST_VALS(dynlist)) {
    free(dynlist);
    fprintf(stderr,"dlsh/dlwish: out of memory\n");
    return(NULL);
  }
  DYN_LIST_N(dynlist) = 0;
  
  return(dynlist);
}

/***********************************************************************
 *
 * dfuCreateDynList()
 *
 *    Create a dynamic list which grows and shrinks.
 *
 ***********************************************************************/

DYN_LIST *dfuCreateDynListWithVals(int datatype, int n, void *vals)
{
  return(dfuCreateNamedDynListWithVals("", datatype, n, vals));
}

/***********************************************************************
 *
 * dfuCreateNamedDynListWithVals()
 *
 *    Create a named dynamic list which grows and shrinks.
 *
 ***********************************************************************/

DYN_LIST *dfuCreateNamedDynListWithVals(char *name, int t, int n, void *vals)
{
  DYN_LIST *dynlist;

//  if (!n) return NULL;

  dynlist = (DYN_LIST *) calloc(1, sizeof(DYN_LIST));
  if (!dynlist) {
    fprintf(stderr,"dlsh/dlwish: out of memory\n");
    return(NULL);
  }

  if (name != DYN_LIST_NAME(dynlist))
    strncpy(DYN_LIST_NAME(dynlist), name, DYN_LIST_NAME_SIZE-1);

  if (!n) {
	return dfuCreateNamedDynList(name, t, 10);
  }

  DYN_LIST_FLAGS(dynlist) = 0;
  DYN_LIST_INCREMENT(dynlist) = n > 1024 ? n / 2 : n;
  DYN_LIST_N(dynlist) = n;
  DYN_LIST_MAX(dynlist) = n;
  DYN_LIST_DATATYPE(dynlist) = t;
  DYN_LIST_VALS(dynlist) = vals;
  
  return(dynlist);
}

/***********************************************************************
 *
 * dfuCopyDynList()
 *
 *    Create a copy of a dynamic list.
 *
 ***********************************************************************/

DYN_LIST *dfuCopyDynList(DYN_LIST *old)
{
  int i, n;
  DYN_LIST *new;
  if (!old) return(NULL);

  new = (DYN_LIST *) calloc(1, sizeof(DYN_LIST));

  memcpy(new, old, sizeof(DYN_LIST));

  /* 
   * This is a strange situation, but something that we take care of
   *  by making sure that the copy actually allocates some space
   */
  if (DYN_LIST_MAX(old) == 0) {
    DYN_LIST_MAX(new) = 2;
    n = 2;
  }
  else n = DYN_LIST_MAX(old);

  if (DYN_LIST_INCREMENT(old) == 0) {
    DYN_LIST_INCREMENT(new) = 2;
  }
  

  switch (DYN_LIST_DATATYPE(old)) {
  case DF_LONG:
    DYN_LIST_VALS(new) = 
      (int *)calloc(n, sizeof(int));
    memcpy(DYN_LIST_VALS(new), DYN_LIST_VALS(old), 
	   sizeof(int)*DYN_LIST_N(old));
    break;
  case DF_SHORT:
    DYN_LIST_VALS(new) = 
      (short *)calloc(n, sizeof(short));
    memcpy(DYN_LIST_VALS(new), DYN_LIST_VALS(old), 
	   sizeof(short)*DYN_LIST_N(old));
    break;
  case DF_FLOAT:
    DYN_LIST_VALS(new) = 
      (float *)calloc(n, sizeof(float));
    memcpy(DYN_LIST_VALS(new), DYN_LIST_VALS(old), 
	   sizeof(float)*DYN_LIST_N(old));
    break;
  case DF_CHAR:
    DYN_LIST_VALS(new) = 
      (char *)calloc(n, sizeof(char));
    memcpy(DYN_LIST_VALS(new), DYN_LIST_VALS(old), 
	   sizeof(char)*DYN_LIST_N(old));
    break;
  case DF_STRING:
    {
      char **vals, **oldvals;
      vals = DYN_LIST_VALS(new) = 
	(char **)calloc(n, sizeof(char *));
      oldvals = (char **) DYN_LIST_VALS(old);
      for (i = 0; i < DYN_LIST_N(old); i++) {
	vals[i] = (char *) calloc(strlen(oldvals[i])+1, sizeof(char));
	strcpy(vals[i], oldvals[i]);
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals, **oldvals;
      vals = DYN_LIST_VALS(new) = 
	(DYN_LIST **)calloc(n, sizeof(DYN_LIST *));
      oldvals = (DYN_LIST **) DYN_LIST_VALS(old);
      for (i = 0; i < DYN_LIST_N(old); i++) {
	vals[i] = dfuCopyDynList(oldvals[i]);
      }
    }
    break;
  }
  
  return(new);
}


/***********************************************************************
 *
 * dfuCreateDynGroup()
 *
 *    Create a dynamic group for holding dynamic lists
 *
 ***********************************************************************/

DYN_GROUP *dfuCreateDynGroup(int n)
{
  return(dfuCreateNamedDynGroup("", n));
}


/***********************************************************************
 *
 * dfuCreateNamedDynGroup()
 *
 *    Create a dynamic group for holding dynamic lists
 *
 ***********************************************************************/

DYN_GROUP *dfuCreateNamedDynGroup(char *name, int n)
{
  DYN_GROUP *dg = (DYN_GROUP *) calloc(1, sizeof(DYN_GROUP));

  if (name != DYN_GROUP_NAME(dg))
    strncpy(DYN_GROUP_NAME(dg), name, DYN_GROUP_NAME_SIZE-1);
  
  if (!n) n++;
  DYN_GROUP_INCREMENT(dg) = DYN_GROUP_MAX(dg) = n;
  DYN_GROUP_N(dg) = 0;
  DYN_GROUP_LISTS(dg) = 
    (DYN_LIST **) calloc(DYN_GROUP_MAX(dg), sizeof(DYN_LIST *));
  return(dg);
}


/***********************************************************************
 *
 * dfuAddDynGroupList(char *name, int type, int n)
 *
 *    Create and add new named list to group
 *
 ***********************************************************************/

int dfuAddDynGroupNewList(DYN_GROUP *dg, char *name, int type, int increment)
{
  DYN_LIST *newlist = dfuCreateNamedDynList(name, type, increment);
  return(dfuAddDynGroupExistingList(dg,name,newlist));
}

/***********************************************************************
 *
 * dfuAddDynGroupExistingList(char *name, DYN_LIST *)
 *
 *    Add existing list to group
 *
 ***********************************************************************/

int dfuAddDynGroupExistingList(DYN_GROUP *dg, char *name, DYN_LIST *list)
{
  DYN_LIST **lists = DYN_GROUP_LISTS(dg);

  if (name != DYN_LIST_NAME(list))
    strncpy(DYN_LIST_NAME(list), name, DYN_LIST_NAME_SIZE-1);

  if (DYN_GROUP_N(dg) == DYN_GROUP_MAX(dg)) {
    DYN_GROUP_MAX(dg) += DYN_GROUP_INCREMENT(dg);
    lists = (DYN_LIST **) realloc(lists, sizeof(DYN_LIST *)*DYN_GROUP_MAX(dg));
  }

  lists[DYN_GROUP_N(dg)] = list;
  DYN_GROUP_N(dg)++;
  
  DYN_GROUP_LISTS(dg) = lists;
  return(DYN_GROUP_N(dg)-1);
}


/***********************************************************************
 *
 * DYN_GROUP *dfuCopyDynGroup(DYN_GROUP *, char *name)
 *
 *    Copy a group to a new group named name
 *
 ***********************************************************************/

DYN_GROUP *dfuCopyDynGroup(DYN_GROUP *dg, char *name)
{
  int i;
  DYN_GROUP *newgroup;
  DYN_LIST **lists = DYN_GROUP_LISTS(dg);
  
  if (!dg) return NULL;
  newgroup = dfuCreateNamedDynGroup(name, DYN_GROUP_N(dg));
  
  for (i = 0; i < DYN_GROUP_N(dg); i++)
    dfuCopyDynGroupExistingList(newgroup, DYN_LIST_NAME(lists[i]), lists[i]);

  return newgroup;
}

/***********************************************************************
 *
 * dfuCopyDynGroupExistingList(char *name, DYN_LIST *)
 *
 *    Copy an existing list to group
 *
 ***********************************************************************/

int dfuCopyDynGroupExistingList(DYN_GROUP *dg, char *name, DYN_LIST *list)
{
  DYN_LIST **lists = DYN_GROUP_LISTS(dg);

  if (DYN_GROUP_N(dg) == DYN_GROUP_MAX(dg)) {
    DYN_GROUP_MAX(dg) += DYN_GROUP_INCREMENT(dg);
    lists = (DYN_LIST **) realloc(lists, sizeof(DYN_LIST *)*DYN_GROUP_MAX(dg));
  }
  
  lists[DYN_GROUP_N(dg)] = dfuCopyDynList(list);
  strncpy(DYN_LIST_NAME(lists[DYN_GROUP_N(dg)]), name, DYN_LIST_NAME_SIZE-1);
  
  DYN_GROUP_N(dg)++;
  
  DYN_GROUP_LISTS(dg) = lists;
  return(DYN_GROUP_N(dg)-1);
}

/***********************************************************************
 *
 * dfuResetDynList(DYN_LIST *)
 *
 *    Reset dynamic list to zero
 *
 ***********************************************************************/

void dfuResetDynList(DYN_LIST *dynlist)
{
  int i;
  if (!dynlist) return;

  /* recursively free lists */

  if (DYN_LIST_DATATYPE(dynlist) == DF_LIST) {
    DYN_LIST **vals = DYN_LIST_VALS(dynlist);
    for (i = 0; i < DYN_LIST_N(dynlist); i++) {
      dfuFreeDynList(vals[i]);
    }
  }

  /* free any allocated strings */

  else if (DYN_LIST_DATATYPE(dynlist) == DF_STRING) {
    char **vals = DYN_LIST_VALS(dynlist);
    for (i = 0; i < DYN_LIST_N(dynlist); i++) {
      if (vals[i]) free(vals[i]);
    }
  }

  DYN_LIST_N(dynlist) = 0;
}


/***********************************************************************
 *
 * dfuResetDynListToType(DYN_LIST *, int type, int increment)
 *
 *    Reset dynamic list length to increment and type to type
 *
 ***********************************************************************/

DYN_LIST *dfuResetDynListToType(DYN_LIST *dynlist, int datatype, int increment)
{
  if (!dynlist) return NULL;

  dfuResetDynList(dynlist);

  /* Don't allow zero length allocs */
  if (!increment) increment++;

  DYN_LIST_N(dynlist) = 0;
  DYN_LIST_INCREMENT(dynlist) = increment;
  DYN_LIST_MAX(dynlist) = increment;
  DYN_LIST_DATATYPE(dynlist) = datatype;
  switch (DYN_LIST_DATATYPE(dynlist)) {
  case DF_LONG:
    DYN_LIST_VALS(dynlist) = 
      (int *)realloc(DYN_LIST_VALS(dynlist), increment*sizeof(int));
    break;
  case DF_SHORT:
    DYN_LIST_VALS(dynlist) = 
      (short *)realloc(DYN_LIST_VALS(dynlist), increment*sizeof(short));
    break;
  case DF_FLOAT:
    DYN_LIST_VALS(dynlist) = 
      (float *)realloc(DYN_LIST_VALS(dynlist), increment*sizeof(float));
    break;
  case DF_CHAR:
    DYN_LIST_VALS(dynlist) = 
      (char *)realloc(DYN_LIST_VALS(dynlist), increment*sizeof(char));
    break;
  case DF_STRING:
    DYN_LIST_VALS(dynlist) = 
      (char **)realloc(DYN_LIST_VALS(dynlist), increment*sizeof(char *));
    break;
  case DF_LIST:
    DYN_LIST_VALS(dynlist) = 
      (DYN_LIST **)realloc(DYN_LIST_VALS(dynlist), 
			   increment*sizeof(DYN_LIST *));
    break;
  }
  
  if (!DYN_LIST_VALS(dynlist)) {
    free(dynlist);
    fprintf(stderr,"dlsh/dlwish: out of memory\n");
    return(NULL);
  }
  
  return(dynlist);
}

/***********************************************************************
 *
 * dfuResetDynGroup(DYN_GROUP *)
 *
 *    Reset all dynamic lists in a group to zero
 *
 ***********************************************************************/

void dfuResetDynGroup(DYN_GROUP *dyngroup)
{
  int i;
  for (i = 0; i < DYN_GROUP_NLISTS(dyngroup); i++)
    dfuResetDynList(DYN_GROUP_LIST(dyngroup,i));
}



/***********************************************************************
 *
 * dfuAddDynListLong(DYN_LIST *, int val)
 *
 *    Append an int to a dynamic list checking to ensure adequate
 *  storage.
 *
 ***********************************************************************/

void dfuAddDynListLong(DYN_LIST *dynlist, int val)
{
  int *vals;
  if (!dynlist) return;

  vals = DYN_LIST_VALS(dynlist);

  if (DYN_LIST_N(dynlist) == DYN_LIST_MAX(dynlist)) {
    DYN_LIST_MAX(dynlist) += DYN_LIST_INCREMENT(dynlist);
    vals = (int *) realloc(vals, sizeof(int)*DYN_LIST_MAX(dynlist));
  }
  vals[DYN_LIST_N(dynlist)] = val;
  DYN_LIST_N(dynlist)++;
  
  DYN_LIST_VALS(dynlist) = vals;
}

/***********************************************************************
 *
 * dfuPrependDynListLong(DYN_LIST *, int val)
 *
 *    Prepend an int to a dynamic list checking to ensure adequate
 *  storage.
 *
 ***********************************************************************/

void dfuPrependDynListLong(DYN_LIST *dynlist, int val)
{
  dfuInsertDynListLong(dynlist, val, 0);
}

/***********************************************************************
 *
 * dfuInsertDynListLong(DYN_LIST *, int val, int pos)
 *
 *    Insert an int to a dynamic list checking to ensure adequate
 *  storage.
 *
 ***********************************************************************/

int dfuInsertDynListLong(DYN_LIST *dynlist, int val, int pos)
{
  int *vals;
  int i;

  if (!dynlist || pos > DYN_LIST_N(dynlist)) return 0;
  vals = DYN_LIST_VALS(dynlist);

  if (DYN_LIST_N(dynlist) == DYN_LIST_MAX(dynlist)) {
    DYN_LIST_MAX(dynlist) += DYN_LIST_INCREMENT(dynlist);
    vals = (int *) realloc(vals, sizeof(int)*DYN_LIST_MAX(dynlist));
  }

  for (i = DYN_LIST_N(dynlist); i > pos; i--) {
    vals[i] = vals[i-1];
  }
  vals[pos] = val;

  DYN_LIST_N(dynlist)++;
  DYN_LIST_VALS(dynlist) = vals;
  return 1;
}

/***********************************************************************
 *
 * dfuAddDynListShort(DYN_LIST *, short val)
 *
 *    Append a short int to a dynamic list checking to ensure adequate
 *  storage.
 *
 ***********************************************************************/

void dfuAddDynListShort(DYN_LIST *dynlist, short val)
{
  short *vals = DYN_LIST_VALS(dynlist);

  if (DYN_LIST_N(dynlist) == DYN_LIST_MAX(dynlist)) {
    DYN_LIST_MAX(dynlist) += DYN_LIST_INCREMENT(dynlist);
    vals = (short *) realloc(vals, sizeof(short)*DYN_LIST_MAX(dynlist));
  }
  vals[DYN_LIST_N(dynlist)] = val;
  DYN_LIST_N(dynlist)++;
  
  DYN_LIST_VALS(dynlist) = vals;
}

/***********************************************************************
 *
 * dfuPrependDynListShort(DYN_LIST *, short val)
 *
 *    Prepend a short int to a dynamic list checking to ensure adequate
 *  storage.
 *
 ***********************************************************************/

void dfuPrependDynListShort(DYN_LIST *dynlist, short val)
{
  dfuInsertDynListShort(dynlist, val, 0);
}

int dfuInsertDynListShort(DYN_LIST *dynlist, short val, int pos)
{
  short *vals;
  int i;

  if (!dynlist || pos > DYN_LIST_N(dynlist)) return 0;
  vals = DYN_LIST_VALS(dynlist);

  if (DYN_LIST_N(dynlist) == DYN_LIST_MAX(dynlist)) {
    DYN_LIST_MAX(dynlist) += DYN_LIST_INCREMENT(dynlist);
    vals = (short *) realloc(vals, sizeof(short)*DYN_LIST_MAX(dynlist));
  }
  for (i = DYN_LIST_N(dynlist); i > pos; i--) {
    vals[i] = vals[i-1];
  }
  vals[pos] = val;
  
  DYN_LIST_N(dynlist)++;
  DYN_LIST_VALS(dynlist) = vals;
  return 1;
}

/***********************************************************************
 *
 * dfuAddDynListFloat(DYN_LIST *, float val)
 *
 *    Append a float int to a dynamic list checking to ensure adequate
 *  storage.
 *
 ***********************************************************************/

void dfuAddDynListFloat(DYN_LIST *dynlist, float val)
{
  float *vals = DYN_LIST_VALS(dynlist);

  if (DYN_LIST_N(dynlist) == DYN_LIST_MAX(dynlist)) {
    DYN_LIST_MAX(dynlist) += DYN_LIST_INCREMENT(dynlist);
    vals = (float *) realloc(vals, sizeof(float)*DYN_LIST_MAX(dynlist));
  }
  vals[DYN_LIST_N(dynlist)] = val;
  DYN_LIST_N(dynlist)++;
  
  DYN_LIST_VALS(dynlist) = vals;
}

/***********************************************************************
 *
 * dfuPrependDynListFloat(DYN_LIST *, float val)
 *
 *    Prepend a float int to a dynamic list checking to ensure adequate
 *  storage.
 *
 ***********************************************************************/

void dfuPrependDynListFloat(DYN_LIST *dynlist, float val)
{
  dfuInsertDynListFloat(dynlist, val, 0);
}

int dfuInsertDynListFloat(DYN_LIST *dynlist, float val, int pos)
{
  int i;
  float *vals;

  if (!dynlist || pos > DYN_LIST_N(dynlist)) return 0;
  vals = DYN_LIST_VALS(dynlist);

  if (DYN_LIST_N(dynlist) == DYN_LIST_MAX(dynlist)) {
    DYN_LIST_MAX(dynlist) += DYN_LIST_INCREMENT(dynlist);
    vals = (float *) realloc(vals, sizeof(float)*DYN_LIST_MAX(dynlist));
  }

  for (i = DYN_LIST_N(dynlist); i > pos; i--) {
    vals[i] = vals[i-1];
  }
  vals[pos] = val;
  
  DYN_LIST_N(dynlist)++;
  DYN_LIST_VALS(dynlist) = vals;
  return 1;
}

/***********************************************************************
 *
 * dfuAddDynListChar(DYN_LIST *, char val)
 *
 *    Append a char int to a dynamic list checking to ensure adequate
 *  storage.
 *
 ***********************************************************************/

void dfuAddDynListChar(DYN_LIST *dynlist, unsigned char val)
{
  unsigned char *vals = DYN_LIST_VALS(dynlist);

  if (DYN_LIST_N(dynlist) == DYN_LIST_MAX(dynlist)) {
    DYN_LIST_MAX(dynlist) += DYN_LIST_INCREMENT(dynlist);
    vals = (unsigned char *) realloc(vals, sizeof(char)*DYN_LIST_MAX(dynlist));
  }
  vals[DYN_LIST_N(dynlist)] = val;
  DYN_LIST_N(dynlist)++;
  
  DYN_LIST_VALS(dynlist) = vals;
}

/***********************************************************************
 *
 * dfuPrependDynListChar(DYN_LIST *, char val)
 *
 *    Prepend a char int to a dynamic list checking to ensure adequate
 *  storage.
 *
 ***********************************************************************/

void dfuPrependDynListChar(DYN_LIST *dynlist, unsigned char val)
{
  dfuInsertDynListChar(dynlist, val, 0);
}

int dfuInsertDynListChar(DYN_LIST *dynlist, unsigned char val, int pos)
{
  unsigned char *vals;
  int i;

  if (!dynlist || pos > DYN_LIST_N(dynlist)) return 0;
  vals = DYN_LIST_VALS(dynlist);

  if (DYN_LIST_N(dynlist) == DYN_LIST_MAX(dynlist)) {
    DYN_LIST_MAX(dynlist) += DYN_LIST_INCREMENT(dynlist);
    vals = (unsigned char *) realloc(vals, sizeof(char)*DYN_LIST_MAX(dynlist));
  }

  for (i = DYN_LIST_N(dynlist); i > pos; i--) {
    vals[i] = vals[i-1];
  }
  vals[pos] = val;
  
  DYN_LIST_N(dynlist)++;
  DYN_LIST_VALS(dynlist) = vals;
  return 1;
}


/***********************************************************************
 *
 * dfuAddDynListString(DYN_LIST *, string *)
 *
 *    Append a string to a dynamic list checking to ensure adequate
 *  storage.
 *
 ***********************************************************************/

void dfuAddDynListString(DYN_LIST *dynlist, char *string)
{
  char **vals = (char **) DYN_LIST_VALS(dynlist);

  if (DYN_LIST_N(dynlist) == DYN_LIST_MAX(dynlist)) {
    DYN_LIST_MAX(dynlist) += DYN_LIST_INCREMENT(dynlist);
    vals = (char **) realloc(vals, sizeof(char *)*DYN_LIST_MAX(dynlist));
  }
  vals[DYN_LIST_N(dynlist)] = malloc(strlen(string)+1);
  strcpy(vals[DYN_LIST_N(dynlist)], string);

  DYN_LIST_N(dynlist)++;
  
  DYN_LIST_VALS(dynlist) = vals;
}

/***********************************************************************
 *
 * dfuPrependDynListString(DYN_LIST *, string *)
 *
 *    Prepend a string to a dynamic list checking to ensure adequate
 *  storage.
 *
 ***********************************************************************/

void dfuPrependDynListString(DYN_LIST *dynlist, char *val)
{
  dfuInsertDynListString(dynlist, val, 0);
}

int dfuInsertDynListString(DYN_LIST *dynlist, char *string, int pos)
{
  char **vals;
  int i;

  if (!dynlist || pos > DYN_LIST_N(dynlist)) return 0;
  vals = (char **) DYN_LIST_VALS(dynlist);
  if (DYN_LIST_N(dynlist) == DYN_LIST_MAX(dynlist)) {
    DYN_LIST_MAX(dynlist) += DYN_LIST_INCREMENT(dynlist);
    vals = (char **) realloc(vals, sizeof(char *)*DYN_LIST_MAX(dynlist));
  }

  for (i = DYN_LIST_N(dynlist); i > pos; i--) {
    vals[i] = vals[i-1];
  }
  vals[pos] = malloc(strlen(string)+1);
  strcpy(vals[pos], string);
  
  DYN_LIST_N(dynlist)++;
  DYN_LIST_VALS(dynlist) = vals;
  return 1;
}

/***********************************************************************
 *
 * dfuAddDynListList(DYN_LIST *, DYN_LIST *)
 *
 *    Append a dyn_list to a dynamic list checking to ensure adequate
 *  storage by copying the newlist into the dynlist.
 *
 ***********************************************************************/

void dfuAddDynListList(DYN_LIST *dynlist, DYN_LIST *newlist)
{
  DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dynlist);

  if (DYN_LIST_N(dynlist) == DYN_LIST_MAX(dynlist)) {
    DYN_LIST_MAX(dynlist) += DYN_LIST_INCREMENT(dynlist);
    vals = (DYN_LIST **) 
      realloc(vals, sizeof(DYN_LIST *)*DYN_LIST_MAX(dynlist));
  }

  if (!newlist) {
    fprintf(stderr, "Attempt to add null list\n");
    return;
  }

  vals[DYN_LIST_N(dynlist)] = dfuCopyDynList(newlist);
  DYN_LIST_N(dynlist)++;
  
  DYN_LIST_VALS(dynlist) = vals;
}

/***********************************************************************
 *
 * dfuMoveDynListList(DYN_LIST *, DYN_LIST *)
 *
 *    Append a dyn_list to a dynamic list checking to ensure adequate
 *  storage by moving the newlist into the dynlist.
 *
 ***********************************************************************/

void dfuMoveDynListList(DYN_LIST *dynlist, DYN_LIST *newlist)
{
  DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dynlist);

  if (DYN_LIST_N(dynlist) == DYN_LIST_MAX(dynlist)) {
    DYN_LIST_MAX(dynlist) += DYN_LIST_INCREMENT(dynlist);
    vals = (DYN_LIST **) 
      realloc(vals, sizeof(DYN_LIST *)*DYN_LIST_MAX(dynlist));
  }

  if (!newlist) {
    fprintf(stderr, "Attempt to add null list\n");
    return;
  }

  vals[DYN_LIST_N(dynlist)] = newlist;
  DYN_LIST_N(dynlist)++;
  
  DYN_LIST_VALS(dynlist) = vals;
}


/***********************************************************************
 *
 * dfuPrependDynListList(DYN_LIST *, DYN_LIST *)
 *
 *    Prepend a dyn_list to a dynamic list checking to ensure adequate
 *  storage.
 *
 ***********************************************************************/

void dfuPrependDynListList(DYN_LIST *dynlist, DYN_LIST *val)
{
  dfuInsertDynListList(dynlist, val, 0);
}

int dfuInsertDynListList(DYN_LIST *dynlist, DYN_LIST *newlist, int pos)
{
  int i;
  DYN_LIST **vals;

  if (!dynlist || pos > DYN_LIST_N(dynlist)) return 0;
  vals = (DYN_LIST **) DYN_LIST_VALS(dynlist);

  if (DYN_LIST_N(dynlist) == DYN_LIST_MAX(dynlist)) {
    DYN_LIST_MAX(dynlist) += DYN_LIST_INCREMENT(dynlist);
    vals = 
      (DYN_LIST **) realloc(vals, sizeof(DYN_LIST *)*DYN_LIST_MAX(dynlist));
  }

  for (i = DYN_LIST_N(dynlist); i > pos; i--) {
    vals[i] = vals[i-1];
  }
  vals[pos] =  dfuCopyDynList(newlist);
  
  DYN_LIST_N(dynlist)++;
  DYN_LIST_VALS(dynlist) = vals;
  return 1;
}

/***********************************************************************
 *
 * dfuSetObsPeriods(DATA_FILE *, DYN_OLIST *)
 *
 *   Copy data from the dynamic obsp_list into the df.
 *
 ***********************************************************************/

int dfuSetObsPeriods(DATA_FILE *df, DYN_OLIST *dynolist)
{
  int i;
  
  dfuCreateObsPeriods(df, DYN_OLIST_N(dynolist));
  for (i = 0; i < DYN_OLIST_N(dynolist); i++) {
    memcpy(DF_OBSP(df, i), DYN_OLIST_VALS(dynolist)[i], sizeof(OBS_P));
  }
  return(i);
}

/***********************************************************************
 *
 * dfuSetEvData(EV_LIST *, DYN_GROUP *)
 *
 *   Copy data from the dynamic group into event lists.  If any vals
 * are found in the event list, they are freed.
 *
 ***********************************************************************/

int dfuSetEvData(EV_DATA *evdata, DYN_GROUP *evlists)
{
  if (DYN_GROUP_NLISTS(evlists) != E_NEVENT_TAGS*2) {
    fprintf(stderr,"dfuSetEvData(): invalid event group argument\n");
    return(0);
  }
  dfuSetEvList(EV_FIXON(evdata), DYN_GROUP_LIST(evlists, 2*E_FIXON_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_FIXON_TAG+1));
  dfuSetEvList(EV_FIXOFF(evdata), DYN_GROUP_LIST(evlists, 2*E_FIXOFF_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_FIXOFF_TAG+1));
  dfuSetEvList(EV_STIMON(evdata), DYN_GROUP_LIST(evlists, 2*E_STIMON_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_STIMON_TAG+1));
  dfuSetEvList(EV_STIMOFF(evdata), DYN_GROUP_LIST(evlists, 2*E_STIMOFF_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_STIMOFF_TAG+1));
  dfuSetEvList(EV_RESP(evdata), DYN_GROUP_LIST(evlists, 2*E_RESP_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_RESP_TAG+1));
  dfuSetEvList(EV_PATON(evdata), DYN_GROUP_LIST(evlists, 2*E_PATON_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_PATON_TAG+1));
  dfuSetEvList(EV_PATOFF(evdata), DYN_GROUP_LIST(evlists, 2*E_PATOFF_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_PATOFF_TAG+1));
  dfuSetEvList(EV_STIMTYPE(evdata), DYN_GROUP_LIST(evlists, 2*E_STIMTYPE_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_STIMTYPE_TAG+1));
  dfuSetEvList(EV_PATTERN(evdata), DYN_GROUP_LIST(evlists, 2*E_PATTERN_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_PATTERN_TAG+1));
  dfuSetEvList(EV_REWARD(evdata), DYN_GROUP_LIST(evlists, 2*E_REWARD_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_REWARD_TAG+1));
  dfuSetEvList(EV_PROBEON(evdata), DYN_GROUP_LIST(evlists, 2*E_PROBEON_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_PROBEON_TAG+1));
  dfuSetEvList(EV_PROBEOFF(evdata), DYN_GROUP_LIST(evlists, 2*E_PROBEOFF_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_PROBEOFF_TAG+1));
  dfuSetEvList(EV_SAMPON(evdata), DYN_GROUP_LIST(evlists, 2*E_SAMPON_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_SAMPON_TAG+1));
  dfuSetEvList(EV_SAMPOFF(evdata), DYN_GROUP_LIST(evlists, 2*E_SAMPOFF_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_SAMPOFF_TAG+1));
  dfuSetEvList(EV_FIXATE(evdata), DYN_GROUP_LIST(evlists, 2*E_FIXATE_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_FIXATE_TAG+1));
  dfuSetEvList(EV_DECIDE(evdata), DYN_GROUP_LIST(evlists, 2*E_DECIDE_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_DECIDE_TAG+1));
  dfuSetEvList(EV_STIMULUS(evdata), DYN_GROUP_LIST(evlists, 2*E_STIMULUS_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_STIMULUS_TAG+1));
  dfuSetEvList(EV_DELAY(evdata), DYN_GROUP_LIST(evlists, 2*E_DELAY_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_DELAY_TAG+1));
  dfuSetEvList(EV_ISI(evdata), DYN_GROUP_LIST(evlists, 2*E_ISI_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_ISI_TAG+1));
  dfuSetEvList(EV_CUE(evdata), DYN_GROUP_LIST(evlists, 2*E_CUE_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_CUE_TAG+1));
  dfuSetEvList(EV_TARGET(evdata), DYN_GROUP_LIST(evlists, 2*E_TARGET_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_TARGET_TAG+1));
  dfuSetEvList(EV_DISTRACTOR(evdata), DYN_GROUP_LIST(evlists, 
						     2*E_DISTRACTOR_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_DISTRACTOR_TAG+1));
  dfuSetEvList(EV_CORRECT(evdata), DYN_GROUP_LIST(evlists, 2*E_CORRECT_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_CORRECT_TAG+1));
  dfuSetEvList(EV_UNIT(evdata), DYN_GROUP_LIST(evlists, 2*E_UNIT_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_UNIT_TAG+1));
  dfuSetEvList(EV_INFO(evdata), DYN_GROUP_LIST(evlists, 2*E_INFO_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_INFO_TAG+1));
  dfuSetEvList(EV_TRIALTYPE(evdata), 
	       DYN_GROUP_LIST(evlists, 2*E_TRIALTYPE_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_TRIALTYPE_TAG+1));
  dfuSetEvList(EV_ABORT(evdata), DYN_GROUP_LIST(evlists, 2*E_ABORT_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_ABORT_TAG+1));
  dfuSetEvList(EV_WRONG(evdata), DYN_GROUP_LIST(evlists, 2*E_WRONG_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_WRONG_TAG+1));
  dfuSetEvList(EV_PUNISH(evdata), DYN_GROUP_LIST(evlists, 2*E_PUNISH_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_PUNISH_TAG+1));
  dfuSetEvList(EV_SACCADE(evdata), DYN_GROUP_LIST(evlists, 2*E_SACCADE_TAG),
	       DYN_GROUP_LIST(evlists, 2*E_SACCADE_TAG+1));
  return(1);
}


/***********************************************************************
 *
 * dfuSetEvList(EV_LIST *, DYN_LIST *, DYN_LIST *)
 *
 *   Copy data from the dynamic list into an event list.  If any vals
 * are found in the event list, they are freed.
 *
 * NOTE:  The dynamic list must be of type DF_LONG (ints).
 *
 ***********************************************************************/

int dfuSetEvList(EV_LIST *evlist, DYN_LIST *vals, DYN_LIST *times)
{
  if (DYN_LIST_DATATYPE(vals) != DF_LONG ||
      DYN_LIST_DATATYPE(times) != DF_LONG) {
    fprintf(stderr,"dfuSetEvList(): incorrect data type\n");
    return 0;
  }
  
  if (!DYN_LIST_N(vals) || !DYN_LIST_N(times)) return(0);
  
  if (EV_LIST_N(evlist) && EV_LIST_VALS(evlist)) {
    free(EV_LIST_VALS(evlist));
    free(EV_LIST_TIMES(evlist));
  }
  
  EV_LIST_VALS(evlist) = (int *) calloc(DYN_LIST_N(vals), sizeof(int));
  if (!EV_LIST_VALS(evlist)) 
    return(0);
  
  EV_LIST_N(evlist) = DYN_LIST_N(vals);
  memcpy(EV_LIST_VALS(evlist), DYN_LIST_VALS(vals), 
	 sizeof(int)*DYN_LIST_N(vals));
  
  EV_LIST_TIMES(evlist) = (int *) calloc(DYN_LIST_N(times), sizeof(int));
  if (!EV_LIST_TIMES(evlist)) 
    return(0);

  EV_LIST_NTIMES(evlist) = DYN_LIST_N(times);
  memcpy(EV_LIST_TIMES(evlist), DYN_LIST_VALS(times), 
	 sizeof(int)*DYN_LIST_N(times));
  
  return(1);
}


/***********************************************************************
 *
 * dfuSetSpData(SP_DATA *, DYN_GROUP *)
 *
 *   Copy data from the dynamic lists into spike channels.  If any vals
 * are found in the spike list, they are freed.
 *
 ***********************************************************************/

int dfuSetSpData(SP_DATA *spdata, DYN_GROUP *sptimes)
{
  int i;
  SP_CH_DATA *spchannel;	/* temporary pntrs used for copying */
  DYN_LIST *dynlist;

  if (!DYN_GROUP_NLISTS(sptimes)) return(0);
  dfuCreateSpikeChannels(spdata, DYN_GROUP_NLISTS(sptimes));

  for (i = 0; i < SP_NCHANNELS(spdata); i++) {
    spchannel = SP_CHANNEL(spdata,i);
    dynlist = DYN_GROUP_LIST(sptimes, i);
    if (DYN_LIST_N(dynlist)) {
      SP_CH_SPTIMES(spchannel) = 
	(float *) calloc(DYN_LIST_N(dynlist), sizeof(float));
      if (!SP_CH_SPTIMES(spchannel))
	return(0);
    }
    else {
      SP_CH_SPTIMES(spchannel) = NULL;
    }
    
    SP_CH_NSPTIMES(spchannel) = DYN_LIST_N(dynlist);
    memcpy(SP_CH_SPTIMES(spchannel), DYN_LIST_VALS(dynlist), 
	   sizeof(float)*DYN_LIST_N(dynlist));
  }
  
  return(SP_NCHANNELS(spdata));
}

/***********************************************************************
 *
 * dfuSetEmData(EM_DATA *, DYN_GROUP *)
 *
 *   Copy data from two dynamic lists into em buffers.  If any vals
 * are found in the em lists, they are freed.
 *
 * NOTE:  The dynamic lists must be of type DF_SHORT (shorts).
 *
 ***********************************************************************/

int dfuSetEmData(EM_DATA *emdata, DYN_GROUP *emlists)
{
  DYN_LIST *dynlist_h = DYN_GROUP_LIST(emlists, 0);
  DYN_LIST *dynlist_v = DYN_GROUP_LIST(emlists, 1);

/* free old em data if exists */

  if (EM_NSAMPS(emdata) && EM_SAMPS_H(emdata)) free(EM_SAMPS_H(emdata));
  if (EM_NSAMPS(emdata) && EM_SAMPS_V(emdata)) free(EM_SAMPS_V(emdata));
  
/* allocate space for new samps */

  if (DYN_LIST_N(dynlist_h)) {
    EM_SAMPS_H(emdata) = 
      (short *) calloc(DYN_LIST_N(dynlist_h), sizeof(short));
    if (!EM_SAMPS_H(emdata)) return(0);
  }
  else EM_SAMPS_H(emdata) = NULL;

  if (DYN_LIST_N(dynlist_v)) {
    EM_SAMPS_V(emdata) = 
      (short *) calloc(DYN_LIST_N(dynlist_v), sizeof(short));
    if (!EM_SAMPS_V(emdata)) {
      free(EM_SAMPS_H(emdata));
      return(0);
    }
  }
  else EM_SAMPS_V(emdata) = NULL;

/* set the count to the number of samples */
  
  EM_NSAMPS(emdata) = DYN_LIST_N(dynlist_h);

/* copy both sets of samples */

  memcpy(EM_SAMPS_H(emdata), DYN_LIST_VALS(dynlist_h),
	 sizeof(short)*DYN_LIST_N(dynlist_h));
  memcpy(EM_SAMPS_V(emdata), DYN_LIST_VALS(dynlist_v),
	 sizeof(short)*DYN_LIST_N(dynlist_v));
  
  return(1);
}


/***********************************************************************
 *
 * dfuFreeDynOList(DYN_OLIST *)
 *
 *   Free dynamic obs_p list
 *
 ***********************************************************************/

void dfuFreeDynOList(DYN_OLIST *dynolist)
{
  int i;
  for (i = 0; i < DYN_OLIST_N(dynolist); i++)
    free(DYN_OLIST_VALS(dynolist)[i]); /* free the allocated obs period */
  
  free(DYN_OLIST_VALS(dynolist));      /* free array of pointers to obs */
  free(dynolist);                      /* free the dynolist structure   */
}


/***********************************************************************
 *
 * dfuFreeDynGroup(DYN_GROUP *)
 *
 *   Free all dyn_lists in dyn_group
 *
 ***********************************************************************/

void dfuFreeDynGroup(DYN_GROUP *dyngroup)
{
  int i;
  for (i = 0; i < DYN_GROUP_NLISTS(dyngroup); i++)
    if (DYN_GROUP_LIST(dyngroup,i))
      dfuFreeDynList(DYN_GROUP_LIST(dyngroup,i));
  
  if (DYN_GROUP_NLISTS(dyngroup)) free(DYN_GROUP_LISTS(dyngroup));
  free(dyngroup);
}

/***********************************************************************
 *
 * dfuFreeDynList(DYN_LIST *)
 *
 *   Free any vals found in the dynlist and the dynlist itself.
 *
 ***********************************************************************/

void dfuFreeDynList(DYN_LIST *dynlist)
{
  int i;

  if (!dynlist) return;

#ifdef DEBUG
  /* Should never get this message */
  if (!DYN_LIST_MAX(dynlist)) {
    fprintf(stderr, "dfuFreeDynList: received list with no allocated space\n");
    fprintf(stderr, "DYN_LIST_N(dynlist) = %d\n", DYN_LIST_N(dynlist));
    fprintf(stderr, "DYN_LIST_INC(dynlist) = %d\n", DYN_LIST_INCREMENT(dynlist));
    fprintf(stderr, "DYN_LIST_VALS(dynlist) = %x\n", DYN_LIST_VALS(dynlist));
    return;
  }
#endif

  /* recursively free lists */
  
  if (DYN_LIST_DATATYPE(dynlist) == DF_LIST) {
    DYN_LIST **vals = DYN_LIST_VALS(dynlist);
    for (i = 0; i < DYN_LIST_N(dynlist); i++) {
      dfuFreeDynList(vals[i]);
    }
  }

  /* free any allocated strings */

  else if (DYN_LIST_DATATYPE(dynlist) == DF_STRING) {
    char **vals = DYN_LIST_VALS(dynlist);
    for (i = 0; i < DYN_LIST_N(dynlist); i++) {
      if (vals[i]) free(vals[i]);
    }
  }

  if (DYN_LIST_VALS(dynlist)) free(DYN_LIST_VALS(dynlist));
  free(dynlist);
}




