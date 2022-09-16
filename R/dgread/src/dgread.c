/*
 * dgread.c - read dlsh dgz files into R
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <R.h>
#include <Rdefines.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <R_ext/PrtUtil.h>
//#include "foreign.h"
#include <unistd.h>
#include "df.h"
#ifdef WIN32
#define ZLIB_DLL
#define _WINDOWS
#endif
#include <zlib.h>
#include "dynio.h"

/* ===========================================================================
 * Uncompress input to output then close both files.
 */
static void gz_uncompress(gzFile in, FILE *out)
{
    char buf[2048];
    int len;

    for (;;) {
        len = gzread(in, buf, sizeof(buf));
        if (len < 0) return;
        if (len == 0) break;

        if ((int)fwrite(buf, 1, (unsigned)len, out) != len) {
	  return;
	}
    }
    if (fclose(out)) return;
    if (gzclose(in) != Z_OK) return;
}

static FILE *uncompress_file(char *filename, char *tempname)
{
  FILE *fp;
  gzFile in;
  char *fname;
  static char *tmpdir = "c:/windows/temp";
  
  if (!filename) return NULL;
  
  if (!(in = gzopen(filename, "rb"))) {
    return 0;
  }
  
  fname = tempnam(tmpdir, "dg");
  
  if (!(fp = fopen(fname,"wb"))) {
    return 0;
  }
  gz_uncompress(in, fp);
  
  fp = fopen(fname, "rb");
  if (tempname) strcpy(tempname, fname);

  free(fname);

  return(fp);
}  

static
SEXP dynListToSexp(DYN_LIST *dl) /* Create a list from a group of dl's */
{
  int length;
  int i;
  DYN_LIST **sublists;
  SEXP retval = NULL, cell;
  length = DYN_LIST_N(dl);

  switch(DYN_LIST_DATATYPE(dl)) {
  case DF_LIST:
    PROTECT(retval=allocVector(VECSXP, length));
    sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      cell = dynListToSexp(sublists[i]);
      SET_VECTOR_ELT(retval, i, cell);
    }
    UNPROTECT(1);
    break;
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      PROTECT(retval=allocVector(INTSXP, length));
      for ( i = 0; i < DYN_LIST_N(dl); i++ ) 
	INTEGER(retval)[i] = vals[i];
      UNPROTECT(1);
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      PROTECT(retval=allocVector(INTSXP, length));
      for ( i = 0; i < DYN_LIST_N(dl); i++ ) 
	INTEGER(retval)[i] = vals[i];
      UNPROTECT(1);
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      PROTECT(retval=allocVector(REALSXP, length));
      for ( i = 0; i < DYN_LIST_N(dl); i++ ) 
	REAL(retval)[i] = vals[i];
      UNPROTECT(1);
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      PROTECT(retval=allocVector(INTSXP, length));
      for ( i = 0; i < DYN_LIST_N(dl); i++ ) 
	INTEGER(retval)[i] = vals[i];
      UNPROTECT(1);
    }
    break;
  case DF_STRING:
    {
      char **vals = (char **) DYN_LIST_VALS(dl);
      PROTECT(retval=allocVector(STRSXP,length));
      for (i = 0; i < length; i++){
	SET_STRING_ELT(retval,i,mkChar(vals[i]));
      }
      UNPROTECT(1);
    }
    break;
  }
  return retval;
}

static SEXP
dynGroupFileToSexp(SEXP call)
{
  int i;
  DYN_GROUP *dg;
  FILE *fp;
  char *suffix;
  char tempname[128];
  SEXP retval, names, fname;
  char *filename; 

  if (!isValidString(fname = CADR(call)))
    error("first argument must be a file name\n");
  
  filename = R_ExpandFileName(CHAR(STRING_ELT(fname,0)));

  /* No need to uncompress a .dg file */
  if ((suffix = strrchr(filename, '.')) && strstr(suffix, "dg") &&
      !strstr(suffix, "dgz")) {
    fp = fopen(filename, "rb");
    if (!fp) {
      error("error opening data file \"%s\".", filename);
    }
  }

  else if ((suffix = strrchr(filename, '.')) &&
	   strlen(suffix) == 4 &&
	   ((suffix[1] == 'l' && suffix[2] == 'z' && suffix[3] == '4') ||
	    (suffix[1] == 'L' && suffix[2] == 'Z' && suffix[3] == '4'))) {
    if (!(dg = dfuCreateDynGroup(4))) {
      error("dg_read: error creating new dyngroup");
    }
    if (dgReadDynGroup(filename, dg) == DF_OK) {
      goto process_dg;
    }
    else {
      error("dg_read: file %s not recognized as lz4/dg format", 
	      filename);
    }
  }


  else if ((fp = uncompress_file(filename, tempname)) == NULL) {
    char fullname[128];
    sprintf(fullname,"%s.dg", filename);
    if ((fp = uncompress_file(fullname, tempname)) == NULL) {
      sprintf(fullname,"%s.dgz", filename);
      if ((fp = uncompress_file(fullname, tempname)) == NULL) {
	error("dg_read: file %s not found", filename);
      }
    }
  }

  if (!(dg = dfuCreateDynGroup(4))) {
    error("error creating dyn group");
  }
  
  if (!dguFileToStruct(fp, dg)) {
    fclose(fp);
    if (tempname[0]) unlink(tempname);
    error("dg_read: file %s not recognized as dg format", 
	  filename);
  }
  fclose(fp);
  if (tempname[0]) unlink(tempname);


 process_dg:
  PROTECT(retval=allocVector(VECSXP, DYN_GROUP_NLISTS(dg)));
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    SET_VECTOR_ELT(retval, i, dynListToSexp(DYN_GROUP_LIST(dg, i)));
  }
  
  PROTECT(names=allocVector(STRSXP, DYN_GROUP_NLISTS(dg)));
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    SET_STRING_ELT(names, i, mkChar(DYN_LIST_NAME(DYN_GROUP_LIST(dg,i))));
  }
  setAttrib(retval, R_NamesSymbol, names);
  UNPROTECT(1);
  
  dfuFreeDynGroup(dg);
  UNPROTECT(1);
  
  return retval;
}


/*
 * Taken from http://en.wikibooks.org/wiki/Algorithm_Implementation/Miscellaneous/Base64#C_2
 */
#define WHITESPACE 64
#define EQUALS     65
#define INVALID    66
 
static const unsigned char d[] = {
    66,66,66,66,66,66,66,66,66,64,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,62,66,66,66,63,52,53,
    54,55,56,57,58,59,60,61,66,66,66,65,66,66,66, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,66,66,66,66,66,66,26,27,28,
    29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66
};
 
int base64decode (char *in, size_t inLen, unsigned char *out, size_t *outLen) { 
    char *end = in + inLen;
    size_t buf = 1, len = 0;
 
    while (in < end) {
        unsigned char c = d[*in++];
 
        switch (c) {
        case WHITESPACE: continue;   /* skip whitespace */
        case INVALID:    return 1;   /* invalid input, return error */
        case EQUALS:                 /* pad character, end of data */
            in = end;
            continue;
        default:
            buf = buf << 6 | c;
 
            /* If the buffer is full, split it into bytes */
            if (buf & 0x1000000) {
                if ((len += 3) > *outLen) return 1; /* buffer overflow */
                *out++ = buf >> 16;
                *out++ = buf >> 8;
                *out++ = buf;
                buf = 1;
            }   
        }
    }
 
    if (buf & 0x40000) {
        if ((len += 2) > *outLen) return 1; /* buffer overflow */
        *out++ = buf >> 10;
        *out++ = buf >> 2;
    }
    else if (buf & 0x1000) {
        if (++len > *outLen) return 1; /* buffer overflow */
        *out++ = buf >> 4;
    }
 
    *outLen = len; /* modify to reflect the actual output size */
    return 0;
}


static SEXP
dynGroupBufferToSexp(SEXP call)
{
  int i;
  DYN_GROUP *dg;
  SEXP retval, names;
  
  size_t in_length, out_length;
  int res;
  unsigned char *in_buf, *out_buf;
  
  if (!isValidString(CADR(call)))
    error("input argument must be a string\n");
  in_buf = CHAR(STRING_ELT(CADR(call),0));
  in_length = LENGTH(STRING_ELT(CADR(call),0));

  if (!(out_buf = malloc(in_length))) {
    error("error allocating memory for decoded dg\n");
  }

  out_length = in_length;
  res = base64decode (in_buf, in_length, out_buf, &out_length);

  if (!(dg = dfuCreateDynGroup(4))) {
    error("error creating dyn group");
  }

  if (!dguBufferToStruct(out_buf, out_length, dg)) {
    free(out_buf);
    error("dg_fromString64: invalid arg");
  }
  
  free(out_buf);

  PROTECT(retval=allocVector(VECSXP, DYN_GROUP_NLISTS(dg)));
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    SET_VECTOR_ELT(retval, i, dynListToSexp(DYN_GROUP_LIST(dg, i)));
  }
  
  PROTECT(names=allocVector(STRSXP, DYN_GROUP_NLISTS(dg)));
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    SET_STRING_ELT(names, i, mkChar(DYN_LIST_NAME(DYN_GROUP_LIST(dg,i))));
  }
  setAttrib(retval, R_NamesSymbol, names);
  UNPROTECT(1);
  
  dfuFreeDynGroup(dg);
  UNPROTECT(1);
  
  return retval;
}

static DYN_LIST *SexpToDynList(SEXP sexp)
{
  int i, n;
  DYN_LIST *retlist = NULL;

  switch (TYPEOF(sexp)) {
  case REALSXP:
    {
      double *v = REAL(sexp);
      float *fvals;
      n = length(sexp);

      if (!n) return dfuCreateDynList(DF_FLOAT, 5);

      fvals = (float *) calloc(n, sizeof(float));
      for (i = 0; i < n; i++) fvals[i] = v[i];
      retlist = dfuCreateDynListWithVals(DF_FLOAT, n, fvals);
    }
    break;
  case LGLSXP:
  case INTSXP:
    {
      int *v = INTEGER(sexp);
      int *ivals;
      n = length(sexp);

      if (!n) return dfuCreateDynList(DF_LONG, 5);

      ivals = (int *) calloc(n, sizeof(int));
      for (i = 0; i < n; i++) ivals[i] = v[i];
      retlist = dfuCreateDynListWithVals(DF_LONG, n, ivals);
    }
    break;
  case STRSXP:
    {
      n = length(sexp);

      if (!n) return dfuCreateDynList(DF_STRING, 5);

      retlist = dfuCreateDynList(DF_STRING, n);
      for (i = 0; i < n; i++) { 
	dfuAddDynListString(retlist, (char *) CHAR(STRING_ELT(sexp, i)));
      }
    }
    break;
  case VECSXP:
    {
      DYN_LIST *newsub;
      if (!isNewList(sexp)) return NULL;
      n = length(sexp);

      if (!n) return dfuCreateDynList(DF_LIST, 5);

      retlist = dfuCreateDynList(DF_LIST, n);
      for (i = 0; i < n; i++) {
	newsub = SexpToDynList(VECTOR_ELT(sexp, i));
	if (newsub) {
	  dfuMoveDynListList(retlist, newsub);
	} 
	else {
	  dfuFreeDynList(retlist);
	  return NULL;
	}
      }
    }
    break;
  }
  return retlist;
}

static DYN_GROUP *SexpToDynGroup(SEXP sexp, char *name)
{
  DYN_GROUP *dg;
  DYN_LIST *dl;
  SEXP l, names;
  int i;
  char buf[64];

  if (!isNewList(sexp)) {
    return NULL;
  }
  
  dg = dfuCreateNamedDynGroup(name, length(sexp));
  names = getAttrib(sexp, R_NamesSymbol);
  for (i = 0; i < length(sexp); i++) {				
    l = VECTOR_ELT(sexp, i);
    if (TYPEOF(names) == STRSXP) {
      name = (char *) CHAR(STRING_ELT(names, i));
    }
    else {
      name = buf;
      sprintf(buf, "list%d", i);
    }
    dl = SexpToDynList(l);
    if (!dl) {
      dfuFreeDynGroup(dg);
      return NULL;
    }
    dfuAddDynGroupExistingList(dg, name, dl);
  }

  return dg;
}

static SEXP
SexpToDynGroupFile(SEXP call)
{
  DYN_GROUP *dg;
  char *filename;
  SEXP fname, s, ans;

  if (!isNewList(s = CADR(call)))
    error("first argument must be a list\n");

  if (!isValidString(fname = CADDR(call)))
    error("second argument must be a file name\n");
  filename = R_ExpandFileName(CHAR(STRING_ELT(fname,0)));
  
  dg = SexpToDynGroup(s, "dg");

  dgInitBuffer();
  dgRecordDynGroup(dg);
  if (!dgWriteBufferCompressed(filename)) {
    error("error writing data (permissions? / disk full?)\n");
  }
  dgCloseBuffer();
  dfuFreeDynGroup(dg);
  
  PROTECT(ans = allocVector(INTSXP, 1));
  INTEGER(ans)[0] = 1;
  UNPROTECT(1);
  return ans;
}

static const
R_ExternalMethodDef ExternEntries[] = {
  {"dgRead", (DL_FUNC) &dynGroupFileToSexp, -1},
  {"dgFromString64", (DL_FUNC) &dynGroupBufferToSexp, -1},
  {"dgWrite", (DL_FUNC) &SexpToDynGroupFile, -1},
  {NULL}
};

void
R_init_dgread(DllInfo *info)
{
  R_registerRoutines(info,
		     NULL, 
		     NULL,
		     NULL, ExternEntries);
}

void
R_unload_dgread(DllInfo *info)
{
  /* Release resources. */
} 
