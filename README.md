# PNGIF: Decode animated PNGs and GIFs

Small C library to decode (A)PNG and GIF images into RGBA byte arrays.

*The code is not optimized and may have bugs. The formats are not fully
supported. This is more of an learning exercise than a production ready
library. Use at your own risk.*

I created it because I didn't want to use third-party libraries with a ton of
extra functionality to simply read image files, and I was interested in how
various image formats are designed.

Right now only PNG and GIF are supported, both static and animated. I don't
currently have any plans to add more image formats.

Not every feature of PNG and GIF is supported either. For PNGs, I don't do
anything with `gAMA`, `iCCP`, `sPLT`, and a bunch of other ancilliary chunks.
For GIFs, I ignore plain text chunks. Gamma correction for PNGs might come in
at some point; everything else - not necessarily.

## Interface

Decoders for both formats are structured into 3-4 "levels":

**Level 0**: Raw. Breaks the file into the minimal useful logical parts.

For PNG, it's individual "chunks". Each chunk has a length field embedded in it
so you can always just read the chunk's header, length, and skip the data to
get to the next chunk.

Look into `png_raw.h` header file for the interface.

In GIF, some blocks don't have the size embedded in them, and you have to know
the block's format and length to be able to correctly read/skip it. Because the
block structure knowledge is already kind of required at this level, there's no
point in simply "splitting" the file into raw byte blocks.

**Level 1**: Parsed. Parses logical blocks from Level 0 from simple byte array
into a useful and structured data. For example, PLTE (color palette) chunk in
a PNG file is parsed into a proper color palette struct.

Look into `gif_parsed.h` and `png_parsed.h` headers.

**Level 2**: Decoded. Decodes parsed image data into more high-level image
containers. For example, raw image chunk data from GIF file is decompressed,
deinteraced, translated from color table indices into actual RGBA values.

Some data might get thrown off at this stage. For example, I'm not interested
in looking at comment/text data, so it is ignored and not present in the final
data containers on this level.

Look into `gif_decoded.h` and `png_decoded.h` headers.

**Level 3**: Image. Final format-independent image container. Holds a list of
frames extracted from the image file, with some additional metadata (like frame
duration). Each frame is an RGBA-represented image ready to be rendered without
any additional transformations.

Look into `image.h` header.

On each level there are functions that take as an input either:

- previous level's output data
- byte array
- FILE pointer
- file path

The first one allows you to insert your logic into the decoding process. If you
are interested in some PNG chunk that is ignored at the decoding stage, you can
get it's raw data at parsing or a lower level.

The latter ones allow you to conveniently get each level's output without
manually going through all of the previous levels.

## Examples

You can look at `test/*.c` files for basic usage. Quick example going through
all abstraction levels:

```c
#include <pngif/png_raw.h>
#include <pngif/png_parsed.h>
#include <pngif/png_decoded.h>
#include <pngif/image.h>

int error = 0;

// Error checks after each call are assumed.
png_raw_t *raw = png_raw_from_path("sample.png", &error);
png_parsed_t *parsed = png_parsed_from_raw(raw, &error);
png_decoded_t *decoded = png_decoded_from_parsed(parsed, &error);
animated_image_t *image = image_from_decoded_png(decoded, &error);

for (int idx = 0; idx < image->frame_count; idx++) {
  image_frame_t frame = image->frames[idx];
  // Draw it or something, I don't know.
}

// Cleanup.
png_raw_free(raw);
png_parsed_free(parsed);
png_decoded_free(decoded);
animated_image_free(image);
```

Image interface can detect image format from the file's header, so on the
highest level you can just use this:

```c
#include <pngif/image.h>

int error = 0;
animated_image_t *image = image_from_path("sample.png", 1, &error);
// Do stuff with your image.
animated_image_free(image);
```

## Requirements

C compiler (GCC or Clang), C standard library. Zlib for PNG decoding. Some
tests have graphical interface that requires X11 on Linux or Cocoa on MacOS.

## Building & Installing

Use `make`. It has targets for both `static` and `dynamic` libraries.

## Tests

There are some basic test executables for each interface level that you can use
to verify that decoder is working correctly. You can build them with
`make tests` or individually with something like `make test_png_parsed`.

## Useful Links

- [PNG documentation](https://www.w3.org/TR/png/#2-RFC-1951)
- [APNG specification](https://wiki.mozilla.org/APNG_Specification)
- [Zlib documentation](https://zlib.net/zlib_how.html)
- [GIF 89a RFC](https://www.w3.org/Graphics/GIF/spec-gif89a.txt)
- ['What's In A GIF' articles from GIFLIB](https://giflib.sourceforge.net/whatsinagif/index.html)

## License

MIT
