#ifndef __ZIP_HELPER_H__
#define __ZIP_HELPER_H__

#include <stdint.h>
#include <stdbool.h>
#include <zip.h>

/*
This is zip_helper, a small wrapper around libzip for lazy people like me.

(c) 2022 by kittennbfive

AGPLv3+ and NO WARRANTY!
*/

#define SZ_MALLOC_MAX (100*1024*1024) //just as a sanity check to avoid nasty surprises, increase this if needed

zip_t * open_zip_file(char const * const filename);
void close_zip_file(zip_t * zip);
bool get_file_size_from_zip(zip_t * zip, char const * const fname, zip_uint64_t * const filesize); //returns false if file does not exist
zip_uint64_t get_file_from_zip(zip_t * zip, char const * const fname, uint8_t **buffer, const bool c_string); //will allocate needed memory, returns size
zip_t * create_zip_file(char const * const filename); //will NOT overwrite existing but exit with errormessage
void add_buffer_to_zip_file(zip_t * zip, char const * const filename, void const * const buffer, const zip_uint64_t buffersize, const uint8_t compression_level);
#endif
