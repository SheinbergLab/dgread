/*
 * dgread.c - read dlsh dgz files into Python
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "df.h"
#ifdef _WIN32
#define ZLIB_DLL
#define _WINDOWS
#endif
#include "zlib.h"
#include "dynio.h"

#define PY_SSIZE_T_CLEAN
#include "Python.h"

#if PY_MAJOR_VERSION >= 3
#include <numpy/arrayobject.h>
#include <numpy/npy_common.h>
#else
#include <numpy/arrayobject.h>
#include <numpy/npy_common.h>
//#include <numpy/noprefix.h>
#endif


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
#ifdef _WIN32
  static char *tmpdir = "c:/windows/temp";
  char *fname;
#else
  static char fname[L_tmpnam];
#endif

  if (!filename) return NULL;

  if (!(in = gzopen(filename, "rb"))) {
    sprintf(tempname, "file %s not found", filename);
    return 0;
  }

#ifdef _WIN32
  fname = tempnam(tmpdir, "dg");
#else
  tmpnam(fname);
#endif
  if (!(fp = fopen(fname,"wb"))) {
    strcpy(tempname, "error opening temp file for decompression");
    return 0;
  }

  gz_uncompress(in, fp);

  /* DONE in gz_uncompress!  fclose(fp); */

  fp = fopen(fname, "rb");
  if (tempname) strcpy(tempname, fname);


#ifdef _WIN32
  //  free(fname); ?? Apparently not, as it crashes when compiled with mingw
#endif

  return(fp);
}  


static
PyObject *
dynListToPyObject(DYN_LIST *dl) /* Create a list from a group of dl's */
{
  int length;
  int i;
  npy_intp dims[1];
  DYN_LIST **sublists;
  PyObject *retval = NULL, *cell;
  length = DYN_LIST_N(dl);

  switch(DYN_LIST_DATATYPE(dl)) {
  case DF_LIST:
    {
      retval=PyList_New(length);
      sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	cell = dynListToPyObject(sublists[i]);
	PyList_SetItem(retval, i, cell);
      }
      return retval;
    }
    break;
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      PyArrayObject *vector;
      dims[0] = DYN_LIST_N(dl);
      vector = (PyArrayObject *) PyArray_SimpleNew(1, dims, NPY_INT32);
      memcpy(vector->data, vals, dims[0]*sizeof(int));
      return PyArray_Return(vector);
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      PyArrayObject *vector;
      dims[0] = DYN_LIST_N(dl);
      vector = (PyArrayObject *) PyArray_SimpleNew(1, dims, NPY_INT16);
      memcpy(vector->data, vals, dims[0]*sizeof(short));
      return PyArray_Return(vector);
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      PyArrayObject *vector;
      dims[0] = DYN_LIST_N(dl);
      vector = (PyArrayObject *) PyArray_SimpleNew(1, dims, NPY_FLOAT32);
      memcpy(vector->data, vals, dims[0]*sizeof(float));
      return PyArray_Return(vector);
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      PyArrayObject *vector;
      dims[0] = DYN_LIST_N(dl);
      vector = (PyArrayObject *) PyArray_SimpleNew(1, dims, NPY_INT8);
      memcpy(vector->data, vals, dims[0]*sizeof(char));
      return PyArray_Return(vector);
    }
    break;
  case DF_STRING:
    {
      char **vals = (char **) DYN_LIST_VALS(dl);
      retval=PyList_New(length);
      for (i = 0; i < length; i++){
	PyList_SetItem(retval, i, Py_BuildValue("s", vals[i]));
      }
      return retval;
    }
    break;
  }
  return retval;
}

PyObject *
dynGroupFileToPyObject(char *filename)
{
  int i;
  DYN_GROUP *dg;
  FILE *fp;
  char *suffix;
  char tempname[128];
  PyObject *pygroup;
  char message[256];
  
  /* No need to uncompress a .dg file */
  if ((suffix = strrchr(filename, '.')) && strstr(suffix, "dg") &&
      !strstr(suffix, "dgz")) {
    fp = fopen(filename, "rb");
    if (!fp) {
      PyErr_SetString(PyExc_ValueError,
		      "dyngroup not found");
      return NULL;
    }
  }


  else if ((suffix = strrchr(filename, '.')) &&
	   strlen(suffix) == 4 &&
	   ((suffix[1] == 'l' && suffix[2] == 'z' && suffix[3] == '4') ||
	    (suffix[1] == 'L' && suffix[2] == 'Z' && suffix[3] == '4'))) {
    if (!(dg = dfuCreateDynGroup(4))) {
      sprintf(message, "dg_read: error creating new dyngroup");
      PyErr_SetString(PyExc_ValueError, message);
      return NULL;
    }
    if (dgReadDynGroup(filename, dg) == DF_OK) {
      goto process_dg;
    }
    else {
      sprintf(message, "dg_read: file %s not recognized as lz4/dg format", 
	      filename);
      PyErr_SetString(PyExc_ValueError, message);
      return NULL;
    }
  }

  
  else if ((fp = uncompress_file(filename, tempname)) == NULL) {
    char fullname[128];
    sprintf(fullname,"%s.dg", filename);
    if ((fp = uncompress_file(fullname, tempname)) == NULL) {
      sprintf(fullname,"%s.dgz", filename);
      if ((fp = uncompress_file(fullname, tempname)) == NULL) {
	PyErr_SetString(PyExc_ValueError,
			"dyngroup not found");
	return NULL;
      }
    }
  }
  
  dg = dfuCreateDynGroup(4);
  
  if (!dguFileToStruct(fp, dg)) {
    fclose(fp);
    if (tempname[0]) unlink(tempname);
    PyErr_SetString(PyExc_ValueError,
		    "dyngroup invalid");
    return NULL;
  }
  fclose(fp);
  if (tempname[0]) unlink(tempname);

 process_dg:
  pygroup = PyDict_New();

  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    PyDict_SetItemString(pygroup, 
			 DYN_LIST_NAME(DYN_GROUP_LIST(dg,i)),
			 dynListToPyObject(DYN_GROUP_LIST(dg,i)));
  }

  dfuFreeDynGroup(dg);
  return pygroup;
}

PyObject *
dynGroupBufferToPyObject(unsigned char *buf, int length)
{
  int i;
  DYN_GROUP *dg;
  PyObject *pygroup;
  
  dg = dfuCreateDynGroup(4);

  if (!dguBufferToStruct(buf, length, dg)) {
    PyErr_SetString(PyExc_ValueError,
		    "dyngroup invalid");
    return NULL;
  }
  
  pygroup = PyDict_New();
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    PyDict_SetItemString(pygroup, 
			 DYN_LIST_NAME(DYN_GROUP_LIST(dg,i)),
			 dynListToPyObject(DYN_GROUP_LIST(dg,i)));
  }
  dfuFreeDynGroup(dg);
  return pygroup;
}


#ifdef READY_FOR_THIS

PyObject *
PyObjectToDynGroupFile(PyObject *pydg)
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

#endif


static PyObject *
dgread_dgread(PyObject *self, PyObject *args)
{
  char *filename;
  if (!PyArg_ParseTuple(args, "s", &filename))
    return NULL;

  return (PyObject*) dynGroupFileToPyObject(filename);
}

static PyObject *
dgread_fromString(PyObject *self, PyObject *args)
{
  unsigned char *buf;
  Py_ssize_t len;
  if (!PyArg_ParseTuple(args, "s#", &buf, &len))
    return NULL;

  return (PyObject*) dynGroupBufferToPyObject(buf, len);
}



#define B64_WHITESPACE 64
#define B64_EQUALS     65
#define B64_INVALID    66
 
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
 
static int base64decode (char *in, unsigned int inLen, unsigned char *out, unsigned int *outLen) { 
  char *end = in + inLen;
  int buf = 1;
  unsigned int len = 0;
  
  while (in < end) {
    unsigned char c = d[*in++];
    
    switch (c) {
    case B64_WHITESPACE: continue;   /* skip whitespace */
    case B64_INVALID:    return 1;   /* invalid input, return error */
    case B64_EQUALS:                 /* pad character, end of data */
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

static PyObject *
dgread_fromString64(PyObject *self, PyObject *args)
{
  PyObject *retobj;
  unsigned char *buf, *buf64;
  Py_ssize_t len;
  int len64, result;
  if (!PyArg_ParseTuple(args, "s#", &buf, &len))
    return NULL;

  len64 = len;
  buf64 = (unsigned char *) calloc(len64, sizeof(char));
  
  result = base64decode (buf, len, buf64, &len64);  

  retobj = (PyObject*) dynGroupBufferToPyObject(buf64, len64);

  free(buf64);
  return retobj;
}

static PyMethodDef
DgreadMethods[] =
  {
    { "dgread", dgread_dgread, METH_VARARGS },
    { "fromString", dgread_fromString, METH_VARARGS },
    { "fromString64", dgread_fromString64, METH_VARARGS },
    { NULL, NULL },
  };

#if PY_MAJOR_VERSION >= 3
	struct PyModuleDef dgread_def = {
		PyModuleDef_HEAD_INIT,
		"dgread",
		NULL,
		-1,
		DgreadMethods,
		NULL, NULL, NULL, NULL
	};

int init_numpy()
{
  import_array();
  return 1;
}

int PyInit_dgread()
{
  init_numpy();
  return(PyModule_Create(&dgread_def));
}
#else

void initdgread()
{
  Py_InitModule("dgread", DgreadMethods);
  import_array();
}

#endif

