/*
 * dgread.c - read dlsh dgz files into Python
 */

#include <stdlib.h>
#include <stdio.h>

#include <ctype.h>
#include <string.h>
#include "df.h"
#include "zlib.h"
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
#ifdef WIN32
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

#ifdef WIN32
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


#ifdef WIN32
  //  free(fname); ?? Apparently not, as it crashes when compiled with mingw
#endif

  return(fp);
}  

int readdg(char *filename)
{
  int i;
  DYN_GROUP *dg;
  FILE *fp;
  char *suffix;
  char tempname[128];

  /* No need to uncompress a .dg file */
  if ((suffix = strrchr(filename, '.')) && strstr(suffix, "dg") &&
      !strstr(suffix, "dgz")) {
    fp = fopen(filename, "rb");
    if (!fp) {
      fprintf(stderr, "dyngroup not found");
      return 0;
    }
  }

  else if ((fp = uncompress_file(filename, tempname)) == NULL) {
    char fullname[128];
    sprintf(fullname,"%s.dg", filename);
    if ((fp = uncompress_file(fullname, tempname)) == NULL) {
      sprintf(fullname,"%s.dgz", filename);
      if ((fp = uncompress_file(fullname, tempname)) == NULL) {
	fprintf(stderr, "dyngroup not found [2]");
	return 0;
      }
    }
  }
  
  dg = dfuCreateDynGroup(4);
  
  if (!dguFileToStruct(fp, dg)) {
    fclose(fp);
    if (tempname[0]) unlink(tempname);
    fprintf(stderr, "error reading dyngroup");
    return 0;
  }
  fclose(fp);
  if (tempname[0]) unlink(tempname);

  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    printf("[%d] %s\n", i, DYN_LIST_NAME(DYN_GROUP_LIST(dg,i)));
  }

  
  dfuFreeDynGroup(dg);
  return 1;
}

void main (int argc, char *argv[])
{
  readdg("ints.dgz");
}
