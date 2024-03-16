#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <err.h>
#include <inttypes.h>

#include "zip_helper.h"

/*
dsl2sigrok - converts an output file of DSView (.dsl) to a Sigrok file (.sr)

Please read the fine manual.

WARNING: Depending on your input file this tool can eat quite a lot of RAM.

(c) 2022 by kittennbfive

AGPLv3+ and NO WARRANTY!
*/

#define CHUNK_VERBOSE 0 //show current chunk being made

#define SZ_CHUNK_OUTPUT_MB 4 //4MB seems to be the default, but Pulseview won't complain even for 100MB and it makes this tool (a little) faster

#define MAX_PROBES 32 //should be enough...

#define MAX_LENGTH_PROBE_NAME 50 //should be enough but you can always increase this...

#define SZ_METADATA 500 //value?

#define SZ_CHANNELS 500 //value?

//-- do not change anything below this line --

#define ENTRY_TOTAL_PROBES 0
#define ENTRY_SAMPLERATE 1
#define ENTRY_TOTAL_SAMPLES 2
#define ENTRY_TOTAL_BLOCKS 3

#define SZ_CHUNK_OUTPUT_BYTES (SZ_CHUNK_OUTPUT_MB*1024*1024)

typedef struct
{
	uint64_t block;
	uint64_t sample_in_block;
} blockpos_t;

void print_usage(void)
{
	printf("usage: dsl2sigrok $input_file $output_file [$compression_ratio]\n$compression_ratio is optional and must be between 1 and 9 (fastest to best compression)\n");
}

int main(int argc, char **argv)
{
	printf("This is dsl2sigrok version 2 (c)2022 by kittennbfive\nThis tool is released under AGPLv3+ and comes WITHOUT ANY WARRANTY!\n\n");
	
	if(argc!=3 && argc!=4)
	{
		print_usage();
		return 0;
	}
	
	uint8_t compression_ratio=99;
	
	if(argc==4)
	{
		compression_ratio=atoi(argv[3]);
		if(compression_ratio<1 || compression_ratio>9)
			errx(1, "invalid compression ratio or not a number");
	}
	else
		compression_ratio=0; //default
	
	printf("input file: \"%s\"\noutput file: \"%s\"\ncompression_ratio: %u\n\n", argv[1], argv[2], compression_ratio);
	
	zip_t * input=open_zip_file(argv[1]);
	
	char * header=NULL;
	get_file_from_zip(input, "header", (uint8_t**)&header, true);
		
	uint16_t total_probes;
	char samplerate[10];
	uint64_t total_samples, total_blocks;
	bool found_entry[4]={false, false, false, false};
	
	char probe_names[MAX_PROBES][MAX_LENGTH_PROBE_NAME];
	char probe_name[MAX_LENGTH_PROBE_NAME];
	uint16_t probe_number;
	
	char * line=strtok(header, "\n");	
	do
	{
		if(sscanf(line, "total probes = %hu", &total_probes)==1)
		{
			found_entry[ENTRY_TOTAL_PROBES]=true;
			if(total_probes>MAX_PROBES)
				errx(1, "maximum %u probes supported", MAX_PROBES);
		}
		else if(sscanf(line, "samplerate = %[^NULL]", samplerate)==1)
			found_entry[ENTRY_SAMPLERATE]=true;
		else if(sscanf(line, "total samples = %" SCNu64, &total_samples)==1)
			found_entry[ENTRY_TOTAL_SAMPLES]=true;
		else if(sscanf(line, "total blocks = %" SCNu64, &total_blocks)==1)
			found_entry[ENTRY_TOTAL_BLOCKS]=true;
		else if(sscanf(line, "probe%hu = %[^NULL]", &probe_number, probe_name)==2)
			strncpy(probe_names[probe_number], probe_name, MAX_LENGTH_PROBE_NAME);
	} while((line=strtok(NULL, "\n")));
	
	if(!found_entry[ENTRY_TOTAL_PROBES])
		errx(1, "total number of probes not found in header");
	if(!found_entry[ENTRY_SAMPLERATE])
		errx(1, "samplerate not found in header");
	if(!found_entry[ENTRY_TOTAL_SAMPLES])
		errx(1, "total number of samples not found in header");
	if(!found_entry[ENTRY_TOTAL_BLOCKS])
		errx(1, "total number of blocks not found in header");
	
	uint8_t unitsize=(total_probes+7)/8;
	char unitsize_str[20];
	sprintf(unitsize_str, "unitsize=%u\n", unitsize);
	
	printf("Header of input file successfully parsed.\n");
	printf("probes: %u\nsamples %" PRIu64 "\nsamplerate %s\n\n", total_probes, total_samples, samplerate);
	
	zip_t * output=create_zip_file(argv[2]);
	
	char version[1]={'2'};
	add_buffer_to_zip_file(output, "version", version, 1, compression_ratio);
	
	char * metadata=malloc(SZ_METADATA);
	
	sprintf(metadata, "[device 1]\ncapturefile=logic-1\ntotal probes=%u\nsamplerate=%s\ntotal analog=0\n", total_probes, samplerate);
	
	char channel[12+MAX_LENGTH_PROBE_NAME];
	char * channels=malloc(SZ_CHANNELS);
	memset(channels, '\0', SZ_CHANNELS);
	
	uint16_t i;
	for(i=0; i<total_probes; i++)
	{
		sprintf(channel, "probe%u=%s\n", i+1, probe_names[i]);
		strcat(channels, channel);
	}
	
	strcat(metadata, channels);
	strcat(metadata, unitsize_str);
	
	add_buffer_to_zip_file(output, "metadata", metadata, strlen(metadata), compression_ratio);
	
	uint64_t samples_per_output_chunk=(SZ_CHUNK_OUTPUT_BYTES)/unitsize;
	uint64_t nb_chunks_output=ceil((double)total_samples/samples_per_output_chunk);
	
	//Because libzip only writes to disk at the end when the actual file is closed we need to keep all data
	//in RAM until the end. This needs quite a bit of RAM, maybe use temp-files instead if you are low on RAM?
	//There is no function like "flush changes to disk" in libzip, we just could close and reopen the archive
	//but this would certainly be quite inefficient.
	uint8_t ** chunkdata_arr=malloc(nb_chunks_output*sizeof(uint8_t*));
	memset(chunkdata_arr, 0, nb_chunks_output*sizeof(uint8_t*));
	
	uint64_t sample_in_chunk=0;
	uint64_t chunk;
	char chunkname[40];
	
	blockpos_t * blockpos=malloc(SZ_CHANNELS*sizeof(blockpos_t));
	memset(blockpos, 0, SZ_CHANNELS*sizeof(blockpos_t));

	zip_uint64_t blocksize;
	uint16_t probe;
	char blockname[20];
	uint8_t * blockdata;
	
	printf("converting data, %" PRIu64 " samples per chunk, %" PRIu64 " chunks in output...", samples_per_output_chunk, nb_chunks_output); fflush(stdout);

#if CHUNK_VERBOSE
	printf("\n");
#endif

	for(chunk=1; chunk<=nb_chunks_output; chunk++)
	{
#if CHUNK_VERBOSE
		printf("creating chunk %lu\r", chunk); fflush(stdout);
#endif
		chunkdata_arr[chunk-1]=malloc(SZ_CHUNK_OUTPUT_BYTES);	
		memset(chunkdata_arr[chunk-1], 0x00, SZ_CHUNK_OUTPUT_BYTES);
		
		for(probe=0; probe<total_probes; probe++)
		{
			sprintf(blockname, "L-%u/%" PRIu64, probe, blockpos[probe].block);
			blocksize=get_file_from_zip(input, blockname, &blockdata, false);
			
			for(sample_in_chunk=0; sample_in_chunk<samples_per_output_chunk; sample_in_chunk++)
			{
				if(blockdata[blockpos[probe].sample_in_block/8]&(1<<blockpos[probe].sample_in_block%8))
					chunkdata_arr[chunk-1][unitsize*sample_in_chunk+probe/8]|=1<<(probe%8);
				
				blockpos[probe].sample_in_block++;
				
				if(blockpos[probe].sample_in_block/8>=blocksize)
				{
					free(blockdata);
					blockpos[probe].block++;
					blockpos[probe].sample_in_block=0;
					sprintf(blockname, "L-%u/%" PRIu64, probe, blockpos[probe].block);
					blocksize=get_file_from_zip(input, blockname, &blockdata, false);
					if(blocksize==0)
						break;
				}
			}
			if(blocksize)
				free(blockdata);
		}
		
		sprintf(chunkname, "logic-1-%" PRIu64, chunk);
		add_buffer_to_zip_file(output, chunkname, chunkdata_arr[chunk-1], unitsize*sample_in_chunk, compression_ratio);
	}

#if CHUNK_VERBOSE
	printf("\n\n");
#else
	printf("done\n\n");
#endif
	
	printf("closing input file\n");
	close_zip_file(input);
	printf("writing output file to disk..."); fflush(stdout);
	close_zip_file(output);
	printf("done\n\n");
	printf("freeing remaining memory\n\n");
	for(chunk=0; chunk<nb_chunks_output; chunk++)
		free(chunkdata_arr[chunk]);
	free(blockpos);
	
	printf("all done!\n");
	
	return 0;
}

