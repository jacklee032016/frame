/* $Id: file.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include "testBaseTest.h"
#include <libBase.h>

#if INCLUDE_FILE_TEST

#define FILENAME                "testfil1.txt"
#define NEWNAME                 "testfil2.txt"
#define INCLUDE_FILE_TIME_TEST  0

static char buffer[11] = {'H', 'e', 'l', 'l', 'o', ' ',
		          'W', 'o', 'r', 'l', 'd' };

static int _fileTestInternal(void)
{
	enum { FILE_MAX_AGE = 1000 };
	bOsHandle_t fd = 0;
	bstatus_t status;
	char readbuf[sizeof(buffer)+16];
	bfile_stat stat;
	btime_val start_time;
	bssize_t size;
	boff_t pos;

	BASE_INFO("..file io test..");

	/* Get time. */
	bgettimeofday(&start_time);

	/* Delete original file if exists. */
	if (bfile_exists(FILENAME))
		bfile_delete(FILENAME);
	
	/*
	* Write data to the file.
	*/
	status = bfile_open(NULL, FILENAME, BASE_O_WRONLY, &fd);
	if (status != BASE_SUCCESS)
	{
		app_perror("...file_open() error", status);
		return -10;
	}

	size = sizeof(buffer);
	status = bfile_write(fd, buffer, &size);
	if (status != BASE_SUCCESS)
	{
		app_perror("...file_write() error", status);
		bfile_close(fd);
		return -20;
	}
	
	if (size != sizeof(buffer))
		return -25;

	status = bfile_close(fd);
	if (status != BASE_SUCCESS)
	{
		app_perror("...file_close() error", status);
		return -30;
	}

	/* Check the file existance and size. */
	if (!bfile_exists(FILENAME))
		return -40;

	if (bfile_size(FILENAME) != sizeof(buffer))
		return -50;

	/* Get file stat. */
	status = bfile_getstat(FILENAME, &stat);
	if (status != BASE_SUCCESS)
		return -60;

	/* Check stat size. */
	if (stat.size != sizeof(buffer))
		return -70;

#if INCLUDE_FILE_TIME_TEST
	/* Check file creation time >= start_time. */
	if (!BASE_TIME_VAL_GTE(stat.ctime, start_time))
		return -80;
	
	/* Check file creation time is not much later. */
	BASE_TIME_VAL_SUB(stat.ctime, start_time);
	if (stat.ctime.sec > FILE_MAX_AGE)
		return -90;

	/* Check file modification time >= start_time. */
	if (!BASE_TIME_VAL_GTE(stat.mtime, start_time))
		return -80;
	
	/* Check file modification time is not much later. */
	BASE_TIME_VAL_SUB(stat.mtime, start_time);
	if (stat.mtime.sec > FILE_MAX_AGE)
		return -90;

	/* Check file access time >= start_time. */
	if (!BASE_TIME_VAL_GTE(stat.atime, start_time))
		return -80;
	
	/* Check file access time is not much later. */
	BASE_TIME_VAL_SUB(stat.atime, start_time);
	if (stat.atime.sec > FILE_MAX_AGE)
		return -90;
#endif

	/*
	* Re-open the file and read data.
	*/
	status = bfile_open(NULL, FILENAME, BASE_O_RDONLY, &fd);
	if (status != BASE_SUCCESS)
	{
		app_perror("...file_open() error", status);
		return -100;
	}

	size = 0;
	while (size < (bssize_t)sizeof(readbuf))
	{
		bssize_t read;
		read = 1;
		status = bfile_read(fd, &readbuf[size], &read);
		if (status != BASE_SUCCESS)
		{
			BASE_ERROR("...error reading file after %d bytes (error follows)", size);
			app_perror("...error", status);
			return -110;
		}

		if (read == 0)
		{// EOF
			break;
		}
		size += read;
	}

	if (size != sizeof(buffer))
		return -120;

	/*
	if (!bfile_eof(fd, BASE_O_RDONLY))
	return -130;
	*/

	if (bmemcmp(readbuf, buffer, size) != 0)
		return -140;

	/* Seek test. */
	status = bfile_setpos(fd, 4, BASE_SEEK_SET);
	if (status != BASE_SUCCESS)
	{
		app_perror("...file_setpos() error", status);
		return -141;
	}

	/* getpos test. */
	status = bfile_getpos(fd, &pos);
	if (status != BASE_SUCCESS)
	{
		app_perror("...file_getpos() error", status);
		return -142;
	}
	
	if (pos != 4)
		return -143;

	status = bfile_close(fd);
	if (status != BASE_SUCCESS)
	{
		app_perror("...file_close() error", status);
		return -150;
	}

	/*
	* Rename test.
	*/
	status = bfile_move(FILENAME, NEWNAME);
	if (status != BASE_SUCCESS)
	{
		app_perror("...file_move() error", status);
		return -160;
	}

	if (bfile_exists(FILENAME))
		return -170;
	
	if (!bfile_exists(NEWNAME))
		return -180;

	if (bfile_size(NEWNAME) != sizeof(buffer))
		return -190;

	/* Delete test. */
	status = bfile_delete(NEWNAME);
	if (status != BASE_SUCCESS)
	{
		app_perror("...file_delete() error", status);
		return -200;
	}

	if (bfile_exists(NEWNAME))
		return -210;

	BASE_INFO("...file test success");
	return BASE_SUCCESS;
}


int testBaseFile(void)
{
	int rc = _fileTestInternal();

	/* Delete test file if exists. */
	if (bfile_exists(FILENAME))
		bfile_delete(FILENAME);

	return rc;
}

#else
int dummy_file_test;
#endif

