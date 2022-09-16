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
      long *vals = (long *) DYN_LIST_VALS(dl);
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

static SEXP
dynGroupBufferToSexp(SEXP call)
{
  int i;
  DYN_GROUP *dg;
  SEXP retval, names;
  
  int length;
  unsigned char *buf;
  
  buf = R_CHAR(VECTOR_ELT(CADR(call),0));
  length = LENGTH(VECTOR_ELT(CADR(call),0));

  if (!(dg = dfuCreateDynGroup(4))) {
    error("error creating dyn group");
  }

  if (!dguBufferToStruct(buf, length, dg)) {
    error("dg_fromString: invalid arg");
  }
  
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
  {"dgFromString", (DL_FUNC) &dynGroupBufferToSexp, -1},
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
