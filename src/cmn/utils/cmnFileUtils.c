
#define _GNU_SOURCE	/* for mkostemp in stdlib.h */

#include <cmnOsPort.h>

#include <limits.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <ctype.h>

#define strcpy_safe(dst, src) \
	(dst[0] = '\0', strncat(dst, src, sizeof(dst) - 1))

#if 0
void cmnHexDump(const uint8_t *buf, int size)
{
	int len, i, j, c;

	for (i = 0; i < size; i += 16)
	{
		len = size - i;
		if (len > 16)
			len = 16;

		fprintf(stderr, "%08x ", i);
		for (j = 0; j < 16; j++)
		{
			if (j < len)
				fprintf(stderr, " %02x", buf[i + j]);
			else
				fprintf(stderr, "   ");
		}
		
		fprintf(stderr, " ");

		for (j = 0; j < len; j++)
		{
			c = buf[i + j];
			if (c < ' ' || c > '~')
				c = '.';
			fprintf(stderr, "%c", c);
		}
		
		fprintf(stderr, "\n");
	}
}
#endif

/* Display a buffer into a HEXA formated output */
void cmnDumpBuffer(const char *buff, size_t count, FILE* fp, int indent)
{
	size_t i, j, c;
	bool printnext = true;

	if (count % 16)
		c = count + (16 - count % 16);
	else
		c = count;

	for (i = 0; i < c; i++)
	{
		if (printnext)
		{
			printnext = false;
			fprintf(fp, "%*s%.4zu ", indent, "", i & 0xffff);
		}
		
		if (i < count)
		{
			fprintf(fp, "%3.2x", (unsigned char)buff[i] & 0xff);
		}
		else
		{
			fprintf(fp, "   ");
		}
		
		if (!((i + 1) % 8))
		{
			if ((i + 1) % 16)
			{
				fprintf(fp, " -");
			}
			else
			{
				fprintf(fp, "   ");
				for (j = i - 15; j <= i; j++)
				{
					if (j < count)
					{
						if ((buff[j] & 0xff) >= 0x20 && (buff[j] & 0xff) <= 0x7e)
						{
							fprintf(fp, "%c", buff[j] & 0xff);
						}
						else
						{
							fprintf(fp, ".");
						}
					}
					else
						fprintf(fp, " ");
				}
				
				fprintf(fp, "\n");
				printnext = true;
			}
		}
	}
}


mode_t umask_val = S_IXUSR | S_IRWXG | S_IRWXO;

/* We need to use O_NOFOLLOW if opening a file for write, so that a non privileged user can't
 * create a symbolic link from the path to a system file and cause a system file to be overwritten. */
FILE *fopen_safe(const char *path, const char *mode)
{
	int fd;
	FILE *file;
#ifdef ENABLE_LOG_FILE_APPEND
	int flags = O_NOFOLLOW | O_CREAT | O_CLOEXEC;
#endif
	int sav_errno;
	char file_tmp_name[PATH_MAX];

	if (mode[0] == 'r')
		return fopen(path, mode);

	if ( (mode[0] != 'a' && mode[0] != 'w') || 
		(mode[1] && (mode[1] != '+' || mode[2])) )
	{
		errno = EINVAL;
		return NULL;
	}

	if (mode[0] == 'w')
	{
		/* If we truncate an existing file, any non-privileged user who already has the file
		 * open would be able to read what we write, even though the file access mode is changed.
		 *
		 * If we unlink an existing file and the desired file is subsequently created via open,
		 * it leaves a window for someone else to create the same file between the unlink and the open.
		 *
		 * The solution is to create a temporary file that we will rename to the desired file name.
		 * Since the temporary file is created owned by root with the only file access permissions being
		 * owner read and write, no non root user will have access to the file. Further, the rename to
		 * the requested filename is atomic, and so there is no window when someone else could create
		 * another file of the same name.
		 */
		strcpy_safe(file_tmp_name, path);
		if (strlen(path) + 6 < sizeof(file_tmp_name))
		{
			strcat(file_tmp_name, "XXXXXX");
		}
		else
		{
			strcpy(file_tmp_name + sizeof(file_tmp_name) - 6 - 1, "XXXXXX");
		}
		
		fd = mkostemp(file_tmp_name, O_CLOEXEC);
	}
	else
	{
		/* Only allow append mode if debugging features requiring append are enabled. Since we
		 * can't unlink the file, there may be a non privileged user who already has the file open
		 * for read (e.g. tail -f). If these debug option aren't enabled, there is no potential
		 * security risk in that respect. */
#ifndef ENABLE_LOG_FILE_APPEND
		CMN_WARN( "BUG - shouldn't be opening file for append with current build options");
		errno = EINVAL;
		return NULL;
#else
		flags = O_NOFOLLOW | O_CREAT | O_CLOEXEC | O_APPEND;

		if (mode[1])
			flags |= O_RDWR;
		else
			flags |= O_WRONLY;

		fd = open(path, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
#endif
	}

	if (fd == -1)
	{
		sav_errno = errno;
		CMN_ERROR( "Unable to open '%s' - errno %d (%m)", path, errno);
		errno = sav_errno;
		return NULL;
	}

#ifndef ENABLE_LOG_FILE_APPEND
	/* Change file ownership to root */
	if (mode[0] == 'a' && fchown(fd, 0, 0))
	{
		sav_errno = errno;
		CMN_ERROR("Unable to change file ownership of %s- errno %d (%m)", path, errno);
		close(fd);
		errno = sav_errno;
		return NULL;
	}
#endif

	/* Set file mode, default rw------- */
	if (fchmod(fd, (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) & ~umask_val))
	{
		sav_errno = errno;
		CMN_ERROR("Unable to change file permission of %s - errno %d (%m)", path, errno);
		close(fd);
		errno = sav_errno;
		return NULL;
	}

	if (mode[0] == 'w')
	{
		/* Rename the temporary file to the one we want */
		if (rename(file_tmp_name, path))
		{
			sav_errno = errno;
			CMN_ERROR("Failed to rename %s to %s - errno %d (%m)", file_tmp_name, path, errno);
			close(fd);
			errno = sav_errno;
			return NULL;
		}
	}

	file = fdopen (fd, "w");
	if (!file)
	{
		sav_errno = errno;
		CMN_ERROR("fdopen(\"%s\") failed - errno %d (%m)", path, errno);
		
		close(fd);
		errno = sav_errno;
		return NULL;
	}

	return file;
}


