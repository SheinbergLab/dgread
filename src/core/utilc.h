/************************************************************************/
/*                                                                      */
/*                             utilc.h                                  */
/*               declarations for utility functions                     */
/*                                                                      */
/************************************************************************/

#ifndef __UTILC_H__
#define __UTILC_H__
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * declarations of the global routines
 */
extern void WaitForReturn (void);
extern void InitString (char *s, int size);
extern char *SetExtension (char *, char *, char *);
extern void ParseFileName (char *, char *, char *);

extern int ranget(void);
extern int ranset(int);
extern int raninit(void);                    /* initialize random seed */
extern int ran(int);
extern int ran_range(int, int);              /* uniform int in [lo,hi] */
extern float frand(void);                    /* uniform distribution   */
extern float zrand(void);                    /* normal distribution    */
extern void rand_fill(int, int *);
extern void rand_choose(int m, int n,  int *);

extern int file_size(char *);

extern int  find_file(char *,char *, char*);
extern int  find_matching_files(char *, char *);
extern char *next_matching_file(void);
extern char *first_matching_file(void);
extern char **all_matching_files(void);
extern int  n_matching_files(void);
/* bounded filename component helpers: size = dst bytes incl. NUL;
   always NUL-terminate, truncate if needed */
extern int  file_pathname_n(char *dst, size_t size, const char *name);
extern int  file_basename_n(char *dst, size_t size, const char *name);
extern int  file_rootname_n(char *dst, size_t size, const char *name);

/* legacy unbounded forms, kept for ABI compatibility only; do not use */
extern int  file_pathname(char *,char *);
extern int  file_basename(char *,char *);
extern int  file_rootname(char *rootname, char *name);

extern char *SetNewExtension(char *, char *, char *);

/** byte flipping utils **/

extern float  flipfloat(float);
extern double flipdouble(double);
extern short  flipshort(short);
extern int   fliplong(int);
void fliplongs(int n, int *vals);
void flipshorts(int n, short *vals);
void flipfloats(int n, float *vals);
extern int64_t flipint64(int64_t);	/* int64_t: must match flipfuncs.h */
void flipint64s(int n, int64_t *vals);
void flipdoubles(int n, double *vals);

extern float canonicalize_angle(float);


/* Evarts conversion stuff */

void CartesianToEvarts(float x, float y, float *inner, float *outer);
void EvartsToCartesian(float inner, float outer, float *xp, float *yp);


/***********************************************************************/
/* FileList:  structure for matching filenames to a specific pattern   */
/*  The structure holds the pattern (a regular expression) and path as */
/*  well as the number of matches and the current match                */
/***********************************************************************/

typedef struct _filelist {
  char path[128];
  char match_pattern[128];
  char mangled_pattern[128];
  int  nmatches;
  char **filenames;
  int  current_match;
} FileList;

#define FL_PATH(f)             ((f)->path)
#define FL_PATTERN(f)          ((f)->match_pattern)
#define FL_MANGLED_PATTERN(f)  ((f)->mangled_pattern)
#define FL_NMATCHES(f)         ((f)->nmatches)
#define FL_FILENAMES(f)        ((f)->filenames)
#define FL_FILENAME(f, i)      (((f)->filenames)[i])
#define FL_CURRENT_MATCH(f)    ((f)->current_match)


#ifdef __cplusplus
}
#endif
#endif /* !__UTILC_H__ */
