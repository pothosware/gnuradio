//possible fix for missing afxres.h header
//https://stackoverflow.com/questions/3566018/cannot-open-include-file-afxres-h-in-vc2010-express

#ifndef _AFXRES_H
#define _AFXRES_H
#if __GNUC__ >= 3
#pragma GCC system_header
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WINDOWS_H
#include <windows.h>
#endif

/* IDC_STATIC is documented in winuser.h, but not defined. */
#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

#ifdef __cplusplus
}
#endif
#endif   
