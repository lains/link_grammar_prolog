#ifndef _LGP_H_INCLUDED
#define _LGP_H_INCLUDED

#if (defined(__MINGW32__) || defined(__MINGW64__) || defined(WIN32) || defined(_WIN32))
#include <windows.h>
#endif

#define BUILDING_LGP

#if !defined(LGP_STATICLIB)
        #if defined(BUILDING_LGP)
                #if   (defined(__MINGW32__) || defined(__MINGW64__))
                        #define LGP_API
                #elif (defined(WIN32) || defined(_WIN32))
                        #define LGP_API  __declspec(dllexport)
                #else
                        #define LGP_API
                #endif

                #define LGP_LIB_EXPORT
                #undef LGP_LIB_IMPORT
        #else
                #if   (defined(__MINGW32__) || defined(__MINGW64__))
                        #define LGP_API
                #elif (defined(WIN32) || defined(_WIN32))
                        #define LGP_API  __declspec(dllimport)
                #else
                        #define LGP_API
                #endif

                #define LGP_LIB_IMPORT
                #undef LGP_LIB_EXPORT
        #endif
#endif

LGP_API int main(int argc, char **argv);
LGP_API install_t install();

#endif	// _LGP_H_INCLUDED
