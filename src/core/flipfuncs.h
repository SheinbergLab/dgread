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

float  flipfloat(float);
double flipdouble(double);
short  flipshort(short);
int   fliplong(int);
void fliplongs(int n, int *vals);
void flipshorts(int n, short *vals);
void flipfloats(int n, float *vals);

#ifdef __cplusplus
}
#endif
#endif /* !__FLIPFUNCS_H__ */
