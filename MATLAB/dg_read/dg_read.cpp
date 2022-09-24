/*=================================================================
 * dg_read.cpp
 *
 * Read a dgz file into matlab and create a matlab structure with
 * fields corresponding to individual elements.
 *
 *=================================================================*/

#include "mex.hpp"
#include "mexAdapter.hpp"

#include <string.h>
#include <df.h>

#ifdef WINDOWNS
#define ZLIB_DLL
#define _WINDOWS
#else
#include <unistd.h>
#endif

#include <zlib.h>
#include "dynio.h"

using namespace matlab::data;
using matlab::mex::ArgumentList;

/*****************************************************************************
 *
 * FUNCTION
 *    tclReadDynGroup
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_read
 *
 * DESCRIPTION
 *    Reads in a dynGroup
 *
 *****************************************************************************/

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
}

static FILE *uncompress_file(char *filename, char tempname[256])
{
  FILE *fp;
  int fd;
  gzFile in;
  
#ifdef WINDOWS
  static char *tmpdir = "c:/windows/temp/dgzXXXXXX";
#else
  static char *tmpdir = "/tmp/dgzXXXXXX";
#endif

  if (!filename) return NULL;

  if (!(in = gzopen(filename, "rb"))) {
    return 0;
  }

  strncpy(tempname, tmpdir, 255);
  
  fd = mkstemp(tempname);
  printf("opening %s\n", tempname);
  if (fd < 1) return NULL;
  fp = fdopen(fd, "wb");
  gz_uncompress(in, fp);
  gzclose(in);
  fclose(fp);

  fp = fopen(tempname, "rb");
  return(fp);
}  

class MexFunction : public matlab::mex::Function {
public:
    void operator()(matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs) {
        checkArguments(outputs, inputs);
        double multiplier = inputs[0][0];
        matlab::data::TypedArray<double> in = std::move(inputs[1]);
        arrayProduct(in, multiplier);
        outputs[0] = std::move(in);
    }

    void arrayProduct(matlab::data::TypedArray<double>& inMatrix, double multiplier) {
        
        for (auto& elem : inMatrix) {
            elem *= multiplier;
        }
    }

    void checkArguments(matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs) {
        std::shared_ptr<matlab::engine::MATLABEngine> matlabPtr = getEngine();
        matlab::data::ArrayFactory factory;

        if (inputs.size() != 2) {
            matlabPtr->feval(u"error", 
                0, std::vector<matlab::data::Array>({ factory.createScalar("Two inputs required") }));
        }

        if (inputs[0].getType() != matlab::data::ArrayType::DOUBLE ||
            inputs[0].getType() == matlab::data::ArrayType::COMPLEX_DOUBLE ||
            inputs[0].getNumberOfElements() != 1) {
            matlabPtr->feval(u"error", 
                0, std::vector<matlab::data::Array>({ factory.createScalar("Input multiplier must be a scalar") }));
        }

        if (inputs[1].getType() != matlab::data::ArrayType::DOUBLE ||
            inputs[1].getType() == matlab::data::ArrayType::COMPLEX_DOUBLE) {
            matlabPtr->feval(u"error", 
                0, std::vector<matlab::data::Array>({ factory.createScalar("Input matrix must be type double") }));
        }

        if (inputs[1].getDimensions().size() != 2) {
            matlabPtr->feval(u"error", 
                0, std::vector<matlab::data::Array>({ factory.createScalar("Input must be m-by-n dimension") }));
        }
    }
};

#if 0
mxArray *dynListToCellArray(DYN_LIST *dl)
{
  mwSize dims;
  int i;
  DYN_LIST **sublists;
  mxArray *retval = NULL, *cell;
  double *d;

  switch(DYN_LIST_DATATYPE(dl)) {
  case DF_LIST:
    dims = DYN_LIST_N(dl);
    retval = mxCreateCellArray(1, &dims);
    sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      cell = dynListToCellArray(sublists[i]);
      mxSetCell(retval, i, cell);
    }
    break;
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      retval = mxCreateDoubleMatrix(DYN_LIST_N(dl), 1, mxREAL);
      d = mxGetPr(retval);
      for ( i = 0; i < DYN_LIST_N(dl); i++ ) 
	d[i] = (double) vals[i];
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      retval = mxCreateDoubleMatrix(DYN_LIST_N(dl), 1, mxREAL);
      d = mxGetPr(retval);
      for ( i = 0; i < DYN_LIST_N(dl); i++ ) 
	d[i] = (double) vals[i];
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      retval = mxCreateDoubleMatrix(DYN_LIST_N(dl), 1, mxREAL);
      d = mxGetPr(retval);
      for ( i = 0; i < DYN_LIST_N(dl); i++ ) 
	d[i] = (double) vals[i];
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      retval = mxCreateDoubleMatrix(DYN_LIST_N(dl), 1, mxREAL);
      d = mxGetPr(retval);
      for ( i = 0; i < DYN_LIST_N(dl); i++ ) 
	d[i] = (double) vals[i];
    }
    break;
  case DF_STRING:
    {
      const char **vals = (const char **) DYN_LIST_VALS(dl);
      retval = mxCreateCharMatrixFromStrings(DYN_LIST_N(dl), vals);
    }
  }
  return retval;
}

void
mexFunction(int nlhs,mxArray *plhs[],int nrhs,const mxArray *prhs[])
{
  mwSize dims[2] = {1, 1 };
  int i, buflen, status;
  char *filename;
  DYN_GROUP *dg;
  FILE *fp;
  int newentry;
  char *newname = NULL, *suffix;
  char tempname[256];
  char message[256];
  char **field_names;
  mxArray *field_value;

  /* Check for proper number of input and  output arguments */    
  if (nrhs !=1) {
    mexErrMsgTxt("usage: dg_read('filename')");
  } 
  if(nlhs > 1){
    mexErrMsgTxt("Too many output arguments.");
  }

  buflen = (mxGetM(FILE_IN) * mxGetN(FILE_IN)) + 1;
  filename = mxCalloc(buflen, sizeof(char));
  status = mxGetString(FILE_IN, filename, buflen);

  /* No need to uncompress a .dg file */
  if ((suffix = strrchr(filename, '.')) && strstr(suffix, "dg") &&
      !strstr(suffix, "dgz")) {
    fp = fopen(filename, "rb");
    if (!fp) {
      sprintf(message, "Error opening data file \"%s\".", filename);
      mexErrMsgTxt(message);
      tempname[0] = 0;
    }
  }
  
  else if ((suffix = strrchr(filename, '.')) &&
	   strlen(suffix) == 4 &&
	   ((suffix[1] == 'l' && suffix[2] == 'z' && suffix[3] == '4') ||
	    (suffix[1] == 'L' && suffix[2] == 'Z' && suffix[3] == '4'))) {
    if (!(dg = dfuCreateDynGroup(4))) {
      sprintf(message, "dg_read: error creating new dyngroup");
      mexErrMsgTxt(message);
    }
    if (dgReadDynGroup(filename, dg) == DF_OK) {
      goto process_dg;
    }
    else {
      sprintf(message, "dg_read: file %s not recognized as lz4/dg format", 
	      filename);
      mexErrMsgTxt(message);
    }
  }

  else if ((fp = uncompress_file(filename, tempname)) == NULL) {
    char fullname[128];
    sprintf(fullname,"%s.dg", filename);
    if ((fp = uncompress_file(fullname, tempname)) == NULL) {
      sprintf(fullname,"%s.dgz", filename);
      if ((fp = uncompress_file(fullname, tempname)) == NULL) {
	sprintf(message, "dg_read: file %s not found", filename);
	mexErrMsgTxt(message);
      }
    }
  }
  
  if (!(dg = dfuCreateDynGroup(4))) {
    mexErrMsgTxt("Error creating dyn group.");
  }

  if (!dguFileToStruct(fp, dg)) {
    sprintf(message, "dg_read: file %s not recognized as dg format", 
	    filename);
    fclose(fp);
    if (tempname[0]) unlink(tempname);
    mexErrMsgTxt(message);
  }
  fclose(fp);
  if (tempname[0]) unlink(tempname);

 process_dg:
  dims[1] = 1;

  field_names = mxCalloc(DYN_GROUP_NLISTS(dg), sizeof (char *));
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    field_names[i] = DYN_LIST_NAME(DYN_GROUP_LIST(dg,i));
  }

  plhs[0] = mxCreateStructArray(2, dims, DYN_GROUP_NLISTS(dg),
				(const char **) field_names);

  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
   field_value = dynListToCellArray(DYN_GROUP_LIST(dg, i));
   if (!field_value) {
     sprintf(message, "dg_read: error reading data file \"%s\"", filename);
     dfuFreeDynGroup(dg);
     mexErrMsgTxt(message);
   }
   mxSetFieldByNumber(plhs[0], 0, i, field_value);
  }

  dfuFreeDynGroup(dg);
}
#endif