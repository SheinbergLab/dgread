/************************************************************************/
/*                                                                      */
/*                           flipfuncs.h                                */
/*            declarations for byte flipping functions                  */
/*                                                                      */
/************************************************************************/

#ifndef __FLIPFUNCS_H__
#define __FLIPFUNCS_H__
#ifdef __cplusplus
extern "C" {
#endif

/** byte flipping utils **/

#include <stdint.h>

float  flipfloat(float);
double flipdouble(double);
short  flipshort(short);
int   fliplong(int);
int64_t flipint64(int64_t);
void fliplongs(int n, int *vals);
void flipshorts(int n, short *vals);
void flipfloats(int n, float *vals);
void flipint64s(int n, int64_t *vals);
void flipdoubles(int n, double *vals);

#ifdef __cplusplus
}
#endif
#endif /* !__FLIPFUNCS_H__ */
