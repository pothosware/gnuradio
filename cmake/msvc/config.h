#ifndef _MSC_VER // [
#error "Use this header only with Microsoft Visual C++ compilers!"
#endif // _MSC_VER ]

#ifndef _MSC_CONFIG_H_ // [
#define _MSC_CONFIG_H_

#include <ciso646> //and, or, xor

////////////////////////////////////////////////////////////////////////
// enable M_* constants by setting _USE_MATH_DEFINES before math.h
////////////////////////////////////////////////////////////////////////
#ifdef _MSC_VER
  #ifndef _USE_MATH_DEFINES
    #define _USE_MATH_DEFINES //math.h M_* constants
  #endif //_USE_MATH_DEFINES
#endif //_MSC_VER

//INFINITY will not be defined, but HUGE_VAL will as part of math.h
#define INFINITY HUGE_VAL

////////////////////////////////////////////////////////////////////////
// enable inline functions for C code
////////////////////////////////////////////////////////////////////////
#ifndef __cplusplus
#  define inline __inline
#endif

////////////////////////////////////////////////////////////////////////
// signed size_t
////////////////////////////////////////////////////////////////////////
#include <stddef.h>
typedef ptrdiff_t ssize_t;

////////////////////////////////////////////////////////////////////////
// rint functions
// these functions are now available through C99 support in MSVC 2013
////////////////////////////////////////////////////////////////////////
#if defined(_MSC_VER) && (_MSC_VER <= 1700)

#include <math.h>
static inline long lrint(double x){return (long)(x > 0.0 ? x + 0.5 : x - 0.5);}
static inline long lrintf(float x){return (long)(x > 0.0f ? x + 0.5f : x - 0.5f);}
static inline long long llrint(double x){return (long long)(x > 0.0 ? x + 0.5 : x - 0.5);}
static inline long long llrintf(float x){return (long long)(x > 0.0f ? x + 0.5f : x - 0.5f);}
static inline double rint(double x){return (x > 0.0)? floor(x + 0.5) : ceil(x - 0.5);}
static inline float rintf(float x){return (x > 0.0f)? floorf(x + 0.5f) : ceilf(x - 0.5f);}

#endif

////////////////////////////////////////////////////////////////////////
// random and srandom
////////////////////////////////////////////////////////////////////////
#include <stdlib.h>
static inline long int random (void) { return rand(); }
static inline void srandom (unsigned int seed) { srand(seed); }

#endif // _MSC_CONFIG_H_ ]
