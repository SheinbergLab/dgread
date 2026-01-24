#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <df.h>
#include <dynio.h>


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
  int result;
  static char fname[9];
#endif

  if (!filename) return NULL;

  if (!(in = gzopen(filename, "rb"))) {
    sprintf(tempname, "file %s not found", filename);
    return 0;
  }

#ifdef WIN32
  fname = tempnam(tmpdir, "dg");
#else
  strncpy(fname, "dgXXXXXX", 8);
  result = mkstemp(fname);
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


int main(int argc, char *argv[])
{

  DYN_GROUP *dg;
  FILE *fp;
  char *newname = NULL, *suffix;
  char tempname[128];
  
  if (argc < 2) {
    printf("usage: %s dgfile\n", argv[0]);
    exit(-1);
  }

  if (!(dg = dfuCreateDynGroup(4))) {
    printf("dg_read: error creating new dyngroup\n");
    exit(-1);
  }

  /* No need to uncompress a .dg file */
  if ((suffix = strrchr(argv[1], '.')) && strstr(suffix, "dg") &&
      !strstr(suffix, "dgz")) {
    fp = fopen(argv[1], "rb");
    if (!fp) {
      printf("dg_read: file %s not found\n", argv[1]);
      exit(-1);
    }
    tempname[0] = 0;
  }
  
  else if ((suffix = strrchr(argv[1], '.')) &&
	   strlen(suffix) == 4 &&
	   ((suffix[1] == 'l' && suffix[2] == 'z' && suffix[3] == '4') ||
	    (suffix[1] == 'L' && suffix[2] == 'Z' && suffix[3] == '4'))) {
    if (dgReadDynGroup(argv[1], dg) == DF_OK) {
      goto process_dg;
    }
    else {
      printf("dg_read: error reading .lz4 file\n");
      exit(-1);
    }
  }

  else if ((fp = uncompress_file(argv[1], tempname)) == NULL) {
    char fullname[128];
    sprintf(fullname,"%s.dg", argv[1]);
    if ((fp = uncompress_file(fullname, tempname)) == NULL) {
      sprintf(fullname,"%s.dgz", argv[1]);
      if ((fp = uncompress_file(fullname, tempname)) == NULL) {
	if (tempname[0] == 'f') { /* 'f'ile not found...*/
	  printf("dg_read: file \"%s\" not found\n", argv[1]);
	  exit(-1);
	}
	else {
	  printf("dg_read: error opening tempfile\n");
	  exit(-1);
	}
	exit(-1);
      }
    }
  }

  if (!dguFileToStruct(fp, dg)) {
    printf("dg_read: file %s not recognized as dg format\n", 
	    argv[1]);
    fclose(fp);
    if (tempname[0]) unlink(tempname);
    exit(-1);
  }
  fclose(fp);
  if (tempname[0]) unlink(tempname);

 process_dg:
  printf("successfully read in %s\n", argv[1]);
  
  dfuFreeDynGroup(dg);

  return 0;
}
