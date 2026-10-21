// stb_implementations.cpp - Compilação das bibliotecas STB header-only
#ifdef _WIN32
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif
