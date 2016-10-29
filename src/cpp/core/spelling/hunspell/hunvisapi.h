#ifndef _HUNSPELL_VISIBILITY_H_
#define _HUNSPELL_VISIBILITY_H_

// Unfortunately, my CMake changes are not working right.
#define HUNSPELL_STATIC
#if defined(HUNSPELL_STATIC)
#  define LIBHUNSPELL_DLL_EXPORTED
#elif defined(_MSC_VER)
#  if defined(BUILDING_LIBHUNSPELL)
#    define LIBHUNSPELL_DLL_EXPORTED __declspec(dllexport)
#  else
#    define LIBHUNSPELL_DLL_EXPORTED __declspec(dllimport)
#  endif
#elif BUILDING_LIBHUNSPELL && !defined(_MSC_VER)
#  define LIBHUNSPELL_DLL_EXPORTED __attribute__((__visibility__("default")))
#else
#  define LIBHUNSPELL_DLL_EXPORTED
#endif

#endif
