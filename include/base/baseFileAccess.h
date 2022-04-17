/* 
 *
 */
#ifndef __BASE_FILE_ACCESS_H__
#define __BASE_FILE_ACCESS_H__

/**
 * @brief File manipulation and access.
 */
#include <_baseTypes.h>

BASE_BEGIN_DECL 

/**
 * @defgroup BASE_FILE_ACCESS File Access
 * @ingroup BASE_IO
 * @{
 *
 */

/**
 * This structure describes file information, to be obtained by
 * calling #bfile_getstat(). The time information in this structure
 * is in local time.
 */
typedef struct bfile_stat
{
    boff_t        size;   /**< Total file size.               */
    btime_val     atime;  /**< Time of last access.           */
    btime_val     mtime;  /**< Time of last modification.     */
    btime_val     ctime;  /**< Time of last creation.         */
} bfile_stat;


/**
 * Returns non-zero if the specified file exists.
 *
 * @param filename      The file name.
 *
 * @return              Non-zero if the file exists.
 */
bbool_t bfile_exists(const char *filename);

/**
 * Returns the size of the file.
 *
 * @param filename      The file name.
 *
 * @return              The file size in bytes or -1 on error.
 */
boff_t bfile_size(const char *filename);

/**
 * Delete a file.
 *
 * @param filename      The filename.
 *
 * @return              BASE_SUCCESS on success or the appropriate error code.
 */
bstatus_t bfile_delete(const char *filename);

/**
 * Move a \c oldname to \c newname. If \c newname already exists,
 * it will be overwritten.
 *
 * @param oldname       The file to rename.
 * @param newname       New filename to assign.
 *
 * @return              BASE_SUCCESS on success or the appropriate error code.
 */
bstatus_t bfile_move( const char *oldname, 
                                   const char *newname);


/**
 * Return information about the specified file. The time information in
 * the \c stat structure will be in local time.
 *
 * @param filename      The filename.
 * @param stat          Pointer to variable to receive file information.
 *
 * @return              BASE_SUCCESS on success or the appropriate error code.
 */
bstatus_t bfile_getstat(const char *filename, bfile_stat *stat);


/** @} */

BASE_END_DECL


#endif

