/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstream.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FSTREAM_H
#define FSTREAM_H

#include "predef.h"

/**
 * @struct ostream_t
 *
 * @extends object_t
 *
 * @brief An append-only data stream.
 */
struct ostream_t {
	object_t base;

	int (*append)(ostream_t *strm, const void *data, size_t size);

	int (*append_sparse)(ostream_t *strm, size_t size);

	int (*flush)(ostream_t *strm);

	const char *(*get_filename)(ostream_t *strm);
};

/**
 * @struct null_ostream_t
 *
 * @extends ostream_t
 *
 * @brief An ostream that discards all data and counts the bytes written.
 */
struct null_ostream_t {
	ostream_t base;

	uint64_t bytes_written;
};

/**
 * @struct istream_t
 *
 * @extends object_t
 *
 * @brief A sequential, read-only data stream.
 */
struct istream_t {
	object_t base;

	size_t buffer_used;
	size_t buffer_offset;
	bool eof;

	uint8_t *buffer;

	int (*precache)(struct istream_t *strm);

	const char *(*get_filename)(struct istream_t *strm);
};


enum {
	OSTREAM_OPEN_OVERWRITE = 0x01,
	OSTREAM_OPEN_SPARSE = 0x02,
};

enum {
	ISTREAM_LINE_LTRIM = 0x01,
	ISTREAM_LINE_RTRIM = 0x02,
	ISTREAM_LINE_SKIP_EMPTY = 0x04,
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an output stream that writes to a file.
 *
 * @memberof ostream_t
 *
 * If the file does not yet exist, it is created. If it does exist this
 * function fails, unless the flag OSTREAM_OPEN_OVERWRITE is set, in which
 * case the file is opened and its contents are discarded.
 *
 * If the flag OSTREAM_OPEN_SPARSE is set, the underlying implementation tries
 * to support sparse output files. If the flag is not set, holes will always
 * be filled with zero bytes.
 *
 * @param path A path to the file to open or create.
 * @param flags A combination of flags controling how to open/create the file.
 *
 * @return A pointer to an output stream on success, NULL on failure.
 */
ostream_t *ostream_open_file(const char *path, int flags);

/**
 * @brief Create an input stream that reads from a file.
 *
 * @memberof istream_t
 *
 * @param path A path to the file to open or create.
 *
 * @return A pointer to an output stream on success, NULL on failure.
 */
istream_t *istream_open_file(const char *path);

/**
 * @brief Create an input stream from an existing file decriptor.
 *
 * @memberof istream_t
 *
 * If the function succeeds, it takes over ownership of the file decriptor.
 *
 * @param path A path to the file to open or create.
 * @param fd The file descriptor to read from.
 *
 * @return A pointer to an output stream on success, NULL on failure.
 */
istream_t *istream_open_fd(const char *path, int fd);

/**
 * @brief Create an output stream that transparently transforms data.
 *
 * @memberof ostream_t
 *
 * This function creates an output stream that transparently
 * transforms (e.g. compresses) all data appended to it and writes the
 * result data to an underlying, wrapped output stream.
 *
 * @param strm A pointer to another stream that should be wrapped.
 * @param xfrm A pointer to the transformation stream to use for
 *             processing all data written to the stream.
 *
 * @return A pointer to an output stream on success, NULL on failure.
 */
ostream_t *ostream_xfrm_create(ostream_t *strm, xfrm_stream_t *xfrm);

/**
 * @brief Create an input stream that transparently transforms data.
 *
 * @memberof istream_t
 *
 * This function creates an input stream that wraps an underlying input stream
 * that is encoded/compressed/etc... and transparently transforms the data when
 * reading from it.
 *
 * @param strm A pointer to another stream that should be wrapped.
 * @param xfrm A pointer to the transformation stream to feed all data through.
 *
 * @return A pointer to an input stream on success, NULL on failure.
 */
istream_t *istream_xfrm_create(istream_t *strm, xfrm_stream_t *xfrm);

/**
 * @brief Append a block of data to an output stream.
 *
 * @memberof ostream_t
 *
 * @param strm A pointer to an output stream.
 * @param data A pointer to the data block to append.
 * @param size The number of bytes to append.
 *
 * @return Zero on success, -1 on failure.
 */
int ostream_append(ostream_t *strm, const void *data, size_t size);

/**
 * @brief Append a number of zero bytes to an output stream.
 *
 * @memberof ostream_t
 *
 * If the unerlying implementation supports sparse files, this function can be
 * used to create a "hole". If the implementation does not support it, a
 * fallback is used that just appends a block of zeros manualy.
 *
 * @param strm A pointer to an output stream.
 * @param size The number of zero bytes to append.
 *
 * @return Zero on success, -1 on failure.
 */
int ostream_append_sparse(ostream_t *strm, size_t size);

/**
 * @brief Process all pending, buffered data and flush it to disk.
 *
 * @memberof ostream_t
 *
 * If the stream performs some kind of transformation (e.g. transparent data
 * compression), flushing caues the wrapped format to insert a termination
 * token. Only call this function when you are absolutely DONE appending data,
 * shortly before destroying the stream.
 *
 * @param strm A pointer to an output stream.
 *
 * @return Zero on success, -1 on failure.
 */
int ostream_flush(ostream_t *strm);

/**
 * @brief Get the underlying filename of a output stream.
 *
 * @memberof ostream_t
 *
 * @param strm The output stream to get the filename from.
 *
 * @return A string holding the underlying filename.
 */
const char *ostream_get_filename(ostream_t *strm);

/**
 * @brief Read a line of text from an input stream
 *
 * @memberof istream_t
 *
 * The line returned is allocated using malloc and must subsequently be
 * freed when it is no longer needed. The line itself is always null-terminated
 * and never includes the line break characters (LF or CR-LF).
 *
 * If the flag @ref ISTREAM_LINE_LTRIM is set, leading white space characters
 * are removed. If the flag @ref ISTREAM_LINE_RTRIM is set, trailing white space
 * characters are remvoed.
 *
 * If the flag @ref ISTREAM_LINE_SKIP_EMPTY is set and a line is discovered to
 * be empty (after the optional trimming), the function discards the empty line
 * and retries. The given line_num pointer is used to increment the line
 * number.
 *
 * @param strm A pointer to an input stream.
 * @param out Returns a pointer to a line on success.
 * @param line_num This is incremented if lines are skipped.
 * @param flags A combination of flags controling the functions behaviour.
 *
 * @return Zero on success, a negative value on error, a positive value if
 *         end-of-file was reached without reading any data.
 */
int istream_get_line(istream_t *strm, char **out, size_t *line_num, int flags);

/**
 * @brief Read data from an input stream
 *
 * @memberof istream_t
 *
 * @param strm A pointer to an input stream.
 * @param data A buffer to read into.
 * @param size The number of bytes to read into the buffer.
 *
 * @return The number of bytes actually read on success, -1 on failure,
 *         0 on end-of-file.
 */
int32_t istream_read(istream_t *strm, void *data, size_t size);

/**
 * @brief Adjust and refill the internal buffer of an input stream
 *
 * @memberof istream_t
 *
 * This function resets the buffer offset of an input stream (moving any unread
 * data up front if it has to) and calls an internal callback of the input
 * stream to fill the rest of the buffer to the extent possible.
 *
 * @param strm A pointer to an input stream.
 *
 * @return 0 on success, -1 on failure.
 */
int istream_precache(istream_t *strm);

/**
 * @brief Get the underlying filename of an input stream.
 *
 * @memberof istream_t
 *
 * @param strm The input stream to get the filename from.
 *
 * @return A string holding the underlying filename.
 */
const char *istream_get_filename(istream_t *strm);

/**
 * @brief Skip over a number of bytes in an input stream.
 *
 * @memberof istream_t
 *
 * @param strm A pointer to an input stream.
 * @param size The number of bytes to seek forward.
 *
 * @return Zero on success, -1 on failure.
 */
int istream_skip(istream_t *strm, uint64_t size);

/**
 * @brief Create a dummy sink stream that discards all data.
 *
 * @memberof null_ostream_t
 *
 * This is mainly usefull to figure out how many bytes would have been
 * written, but making sure they don't go anywhere.
 *
 * @return A null-stream instance on success, NULL on failure.
 */
null_ostream_t *null_ostream_create(void);

#ifdef __cplusplus
}
#endif

#endif /* FSTREAM_H */
