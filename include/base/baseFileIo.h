/* 
/* 
 */
#ifndef __BASE_FILE_IO_H__
#define __BASE_FILE_IO_H__

/**
 * @brief Simple file I/O abstraction.
 */
#include <_baseTypes.h>

BASE_BEGIN_DECL 

/**
 * @defgroup BASE_FILE_IO File I/O
 * @ingroup BASE_IO
 * @{
 *
 * This file contains functionalities to perform file I/O. The file
 * I/O can be implemented with various back-end, either using native
 * file API or ANSI stream. 
 *
 * @section bfile_size_limit_sec Size Limits
 *
 * There may be limitation on the size that can be handled by the
 * #bfile_setpos() or #bfile_getpos() functions. The API itself
 * uses 64-bit integer for the file offset/position (where available); 
 * however some backends (such as ANSI) may only support signed 32-bit 
 * offset resolution.
 *
 * Reading and writing operation uses signed 32-bit integer to indicate
 * the size.
 *
 *
 */

/**
 * These enumerations are used when opening file. Values BASE_O_RDONLY,
 * BASE_O_WRONLY, and BASE_O_RDWR are mutually exclusive. Value BASE_O_APPEND
 * can only be used when the file is opened for writing. 
 */
enum bfile_access
{
    BASE_O_RDONLY     = 0x1101,   /**< Open file for reading.             */
    BASE_O_WRONLY     = 0x1102,   /**< Open file for writing.             */
    BASE_O_RDWR       = 0x1103,   /**< Open file for reading and writing. 
                                     File will be truncated.            */
    BASE_O_APPEND     = 0x1108    /**< Append to existing file.           */
};

/**
 * The seek directive when setting the file position with #bfile_setpos.
 */
enum bfile_seek_type
{
    BASE_SEEK_SET     = 0x1201,   /**< Offset from beginning of the file. */
    BASE_SEEK_CUR     = 0x1202,   /**< Offset from current position.      */
    BASE_SEEK_END     = 0x1203    /**< Size of the file plus offset.      */
};

/**
 * Open the file as specified in \c pathname with the specified
 * mode, and return the handle in \c fd. All files will be opened
 * as binary.
 *
 * @param pool          Pool to allocate memory for the new file descriptor.
 * @param pathname      The file name to open.
 * @param flags         Open flags, which is bitmask combination of
 *                      #bfile_access enum. The flag must be either
 *                      BASE_O_RDONLY, BASE_O_WRONLY, or BASE_O_RDWR. When file
 *                      writing is specified, existing file will be 
 *                      truncated unless BASE_O_APPEND is specified.
 * @param fd            The returned descriptor.
 *
 * @return              BASE_SUCCESS or the appropriate error code on error.
 */
bstatus_t bfile_open(bpool_t *pool,
                                  const char *pathname, 
                                  unsigned flags,
                                  bOsHandle_t *fd);

/**
 * Close an opened file descriptor.
 *
 * @param fd            The file descriptor.
 *
 * @return              BASE_SUCCESS or the appropriate error code on error.
 */
bstatus_t bfile_close(bOsHandle_t fd);

/**
 * Write data with the specified size to an opened file.
 *
 * @param fd            The file descriptor.
 * @param data          Data to be written to the file.
 * @param size          On input, specifies the size of data to be written.
 *                      On return, it contains the number of data actually
 *                      written to the file.
 *
 * @return              BASE_SUCCESS or the appropriate error code on error.
 */
bstatus_t bfile_write(bOsHandle_t fd,
                                   const void *data,
                                   bssize_t *size);

/**
 * Read data from the specified file. When end-of-file condition is set,
 * this function will return BASE_SUCCESS but the size will contain zero.
 *
 * @param fd            The file descriptor.
 * @param data          Pointer to buffer to receive the data.
 * @param size          On input, specifies the maximum number of data to
 *                      read from the file. On output, it contains the size
 *                      of data actually read from the file. It will contain
 *                      zero when EOF occurs.
 *
 * @return              BASE_SUCCESS or the appropriate error code on error.
 *                      When EOF occurs, the return is BASE_SUCCESS but size
 *                      will report zero.
 */
bstatus_t bfile_read(bOsHandle_t fd,
                                  void *data,
                                  bssize_t *size);

/**
 * Set file position to new offset according to directive \c whence.
 *
 * @param fd            The file descriptor.
 * @param offset        The new file position to set.
 * @param whence        The directive.
 *
 * @return              BASE_SUCCESS or the appropriate error code on error.
 */
bstatus_t bfile_setpos(bOsHandle_t fd,
                                    boff_t offset,
                                    enum bfile_seek_type whence);

/**
 * Get current file position.
 *
 * @param fd            The file descriptor.
 * @param pos           On return contains the file position as measured
 *                      from the beginning of the file.
 *
 * @return              BASE_SUCCESS or the appropriate error code on error.
 */
bstatus_t bfile_getpos(bOsHandle_t fd,
                                    boff_t *pos);

/**
 * Flush file buffers.
 *
 * @param fd		The file descriptor.
 *
 * @return		BASE_SUCCESS or the appropriate error code on error.
 */
bstatus_t bfile_flush(bOsHandle_t fd);


BASE_END_DECL

#endif


