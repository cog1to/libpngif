#ifndef PNGIF_ERRORS_HEADER
#define PNGIF_ERRORS_HEADER

/** Common errors **/

// Cannot open and read file.
static const int PNGIF_ERR_FILEIO = 49;
// Memory allocation error.
static const int PNGIF_ERR_UNKNOWN_FORMAT = 50;

/** GIF errors **/

// Cannot open and read file.
static const int GIF_ERR_FILEIO_CANT_OPEN = 1;
// Memory allocation error.
static const int GIF_ERR_MEMIO = 2;
// Wrong header.
static const int GIF_ERR_BAD_HEADER = 3;
// File read error.
static const int GIF_ERR_FILEIO_READ_ERROR = 4;
// File read error: failed to get file size.
static const int GIF_ERR_FILEIO_NO_SIZE = 5;
// Bad GIF data format.
static const int GIF_ERR_BAD_FORMAT = 6;
// Bad block size (i. e. bigger than 255).
static const int GIF_ERR_BAD_BLOCK_SIZE = 7;
// Encoding error.
static const int GIF_ERR_BAD_ENCODING = 8;
// Encountered an unknown block.
static const int GIF_ERR_UNKNOWN_BLOCK = 9;
// Didn't find a reset code while expecting one.
static const int GIF_ERR_NO_RESET = 10;
// No data in the block.
static const int GIF_ERR_NO_DATA = 11;

/** PNG errors **/

// Memory allocation error.
static const int PNG_ERR_MEMIO = 1;
// File read error.
static const int PNG_ERR_FILEIO = 2;
// Wrong file header error.
static const int PNG_ERR_WRONG_HEADER = 3;
// Unrecognized chunk format or order.
static const int PNG_ERR_CHUNK_FORMAT = 4;
// CRC checksum mismatch.
static const int PNG_ERR_CRC = 5;
// Memory allocation error.
static const int PNG_ERR_ZLIB = 8;
// Memory allocation error.
static const int PNG_ERR_NO_HEADER = 6;
// File read error.
static const int PNG_ERR_NO_DATA = 7;
// Unsupported data format.
static const int PNG_ERR_UNSUPPORTED_FORMAT = 10;
// Invalid data format.
static const int PNG_ERR_INVALID_FORMAT = 11;
// Mismatch of animation control and number of frame controls.
static const int PNG_ERR_BAD_FRAME_COUNT = 12;
// Bad frame data, i. e. wrong size or offset.
static const int PNG_ERR_BAD_FRAME_DATA = 13;


#endif
