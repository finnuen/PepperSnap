#pragma once
#include <windows.h>
#include <vector>

// Lossless WebP (VP8L) encoding.
//
// Windows can decode WebP but cannot encode it: WIC registers only a "Microsoft
// Webp Decoder", the WinRT BitmapEncoder API has no WebP encoder id, and the
// Store's "WebP Image Extensions" package is a decoder too.  So the only way
// PepperSnap can offer WebP is to write the bitstream itself - and since this
// project takes no third-party code, it does, in WebPEncode.cpp.
//
// `pixels` is a top-down 32-bit buffer in the layout GDI+ hands us for
// PixelFormat32bppARGB, that is B, G, R, A in memory.  `stride` is the distance
// between rows in bytes and may exceed width * 4.  `hasAlpha` is the caller's
// answer to "is any pixel not fully opaque", which becomes the format's
// alpha-is-used flag.
//
// On success `out` holds a complete RIFF/WEBP file image, ready to write to a
// .webp path.  Returns false if the image cannot be represented (too large, or
// no memory).
bool EncodeBGRAtoWebP(const BYTE* pixels, int width, int height, int stride,
                      bool hasAlpha, std::vector<BYTE>& out);
