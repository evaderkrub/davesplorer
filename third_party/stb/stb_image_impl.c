// The one translation unit that carries stb_image's implementation. Built
// as its own library so the project's strict warning flags do not apply.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_LINEAR
#ifdef _WIN32
// stbi_load takes UTF-8 paths, the encoding the rest of the app speaks.
#define STBI_WINDOWS_UTF8
#endif
#include "stb_image.h"
