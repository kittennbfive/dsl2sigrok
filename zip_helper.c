#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>
#include <zip.h>

#include "zip_helper.h"

/*
This is zip_helper, a small wrapper around libzip for lazy people like me.

(c) 2022 by kittennbfive

AGPLv3+ and NO WARRANTY!
*/

zip_t * open_zip_file(char const * const filename)
{
	zip_t * zip;
	int err;
	zip=zip_open(filename, ZIP_RDONLY, &err);
	if(!zip)
		errx(1, "open_zip_file(%s): zip_open failed with error %d", filename, err);
	return zip;
}

void close_zip_file(zip_t * zip)
{
	zip_close(zip);
}

bool get_file_size_from_zip(zip_t * zip, char const * const fname, zip_uint64_t * const filesize)
{
	zip_stat_t stat;
	if(zip_stat(zip, fname, 0, &stat))
	{
		zip_error_t * error=zip_get_error(zip);
		if(error->zip_err==ZIP_ER_NOENT)
		{
			*filesize=0;
			zip_error_fini(error);
			return false;
		}
		else
			errx(1, "get_file_size_from_zip(%s): zip_stat failed: %s", fname, zip_error_strerror(error));
	}
	
	if(!(stat.valid&ZIP_STAT_SIZE))
		errx(1, "get_file_size_from_zip(%s): zip_stat did not return a valid filesize", fname);

	*filesize=stat.size;

	return true;
}

zip_uint64_t get_file_from_zip(zip_t * zip, char const * const fname, uint8_t **buffer, const bool c_string)
{
	zip_uint64_t sz;
	
	if(!get_file_size_from_zip(zip, fname, &sz))
		return false;
	
	if(sz>SZ_MALLOC_MAX)
		errx(1, "get_file_from_zip: file %s is bigger than hardcoded limit SZ_MALLOC_MAX", fname);
	
	zip_file_t * file=zip_fopen(zip, fname, 0);
	if(!file)
		errx(1, "get_file_from_zip(%s): zip_open failed: %s", fname, zip_strerror(zip));
	
	*buffer=malloc(sz+(c_string?1:0));
	if(!(*buffer))
		err(1, "get_file_from_zip: malloc failed for %s", fname);
	
	zip_int64_t bytes_read=zip_fread(file, *buffer, sz);
	if(bytes_read<0 || (zip_uint64_t)bytes_read!=sz)
		errx(1, "get_file_from_zip(%s): zip_fread failed: %s", fname, zip_strerror(zip));
	
	if(zip_fclose(file))
		errx(1, "get_file_from_zip(%s): zip_fclose failed: %s", fname, zip_strerror(zip));
	
	if(c_string)
		(*buffer)[sz]='\0';
	
	return sz;
}

zip_t * create_zip_file(char const * const filename)
{
	zip_t * zip;
	int err;
	zip=zip_open(filename, ZIP_CREATE|ZIP_EXCL, &err);
	if(!zip)
	{
		if(err==ZIP_ER_EXISTS)
			errx(1, "create_zip_file(%s): file already exists, not overwriting", filename);
		else
			errx(1, "create_zip_file(%s) failed with error %d", filename, err);
	}
	return zip;
}

void add_buffer_to_zip_file(zip_t * zip, char const * const filename, void const * const buffer, const zip_uint64_t buffersize, const uint8_t compression_level)
{
	zip_source_t * zip_source;
	zip_error_t * err=NULL;
	zip_source=zip_source_buffer_create(buffer, buffersize, 0, err);
	if(!zip_source)
		errx(1, "add_buffer_to_zip_file(%s): zip_source_buffer_create failed: %s", filename, zip_error_strerror(err));
	
	zip_int64_t index=zip_file_add(zip, filename, zip_source, ZIP_FL_ENC_CP437);
	if(index==-1)
		errx(1, "add_buffer_to_zip_file(%s): zip_file_add failed: %s", filename, zip_strerror(zip));
	
	if(zip_set_file_compression(zip, index, ZIP_CM_DEFLATE, compression_level)<0)
		errx(1, "add_buffer_to_zip_file(%s): zip_set_file_compression failed: %s", filename, zip_strerror(zip));
	
}
