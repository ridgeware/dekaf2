#ifndef __TEXTHANDLINGTUTILSDEFS_H_
#define __TEXTHANDLINGTUTILSDEFS_H_

#ifdef TEXTHANDLINGUTILS_LIBRARY
#ifdef _WIN32
#ifndef TEXTHANDLINGUTILS_STATIC_LIBRARY
#ifdef TEXTHANDLINGUTILS_BUILDING_LIBRARY
#define TEXTHANDLINGUTILS_EXPORT __declspec(dllexport)
#else
#define TEXTHANDLINGUTILS_EXPORT __declspec(dllimport)
#endif
#else
#define TEXTHANDLINGUTILS_EXPORT
#endif
#else
#if __GNUC__ >= 4
#define TEXTHANDLINGUTILS_EXPORT __attribute__((visibility("default")))
#else
#define TEXTHANDLINGUTILS_EXPORT
#endif
#endif
#else
#define TEXTHANDLINGUTILS_EXPORT
#endif

#endif // __TEXTHANDLINGTUTILSDEFS_H_
