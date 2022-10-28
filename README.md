# PNGIF: Decode animated PNGs and GIFs

Small C library to decode (A)PNG and GIF images into RGBA byte arrays.

*The code is not optimized and may have bugs. This is more of an learning
exercise than a production ready library. Use at your own risk.*

I created it because I didn't want to use third-party libraries with a ton of
extra functionality to simply read image files, and I was interested in how
various image formats are designed.

Right now only PNG and GIF are supported, both static and animated. I don't
currently have any plans to add more image formats.

## Interface

Decoders for both formats are structured into 3-4 "levels":

*Level 0*: Raw. Breaks the file into the minimal possible logical parts. For
PNG, it's individual "chunks".

For GIF, this layer is skipped because it didn't make much sense. In PNG, you
can split the file into individual chunks with 0 knowledge about content of
those chunks. In GIF, you have to know the format of some blocks to be able to
successfully skip them. So, because the block structure knowledge is already
required at this level, there's no point in simply "splitting" the file.

Look into `png_raw.h` header file for the interface.

*Level 1*: Parsed. Parses logical blocks from Level 0 from simple byte array
into a useful and structured data. For example, PLTE (color palette) chunk is
parsed into a color palette struct.

Look into `gif_parsed.h` and `png_parsed.h` headers.

*Level 2*: Decoded. Decodes parsed image data into more high-level image
containers. For example, raw image chunk data from GIF file is expanded,
deinteraced, translated from color table indices into actual RGBA values.

Some data might get thrown off at this stage. For example, I'm not interested
in looking at comment/text data, so it is ignored and not present in the final
data containers on this level.

Look into `gif_decoded.h` and `png_decoded.h` headers.

*Level 3*: Image. Final format-independent image container. Holds a list of
frames extracted from the image file, with some additional metadata (like frame
duration). Each frame is an RGBA-represented image ready to be rendered without
any additional transformations.

Look into `image.h` header.

On each level there are functions that take as an input either:

- previous level's output data
- byte array
- FILE pointer
- file path

So you can produce each level's output without manually going through all
previous levels.

## Example

You can look at `test/*.c` files for basic usage. Quick example going through
all abstraction levels:

```c
TODO
```

## Requirements

C compiler (GCC or Clang), C standard library. Some tests have graphical
interface that requires X11 on Linux or Cocoa on MacOS.

## Building & Installing

Use `make`. It has targets for both `static` and `dynamic` libraries.

## Tests

There are some basic test executables for each interface level that you can use
to verify that decoder is working correctly. You can build them with
`make tests` or individually with something like `make test_png_parsed`.

## Useful Links

[PNG documentation](https://www.w3.org/TR/png/#2-RFC-1951)
[APNG specification](https://wiki.mozilla.org/APNG_Specification)
[Zlib documentation](https://zlib.net/zlib_how.html)
[GIF 89a RFC](https://www.w3.org/Graphics/GIF/spec-gif89a.txt)
['What's In A GIF' articles from GIFLIB](https://giflib.sourceforge.net/whatsinagif/index.html)

## License

MIT
