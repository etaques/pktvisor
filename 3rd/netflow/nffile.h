/*
 *  Copyright (c) 2009-2020, Peter Haag
 *  Copyright (c) 2008, SWITCH - Teleinformatikdienste fuer Lehre und Forschung
 *  All rights reserved.
 *  
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions are met:
 *  
 *   * Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice, 
 *     this list of conditions and the following disclaimer in the documentation 
 *     and/or other materials provided with the distribution.
 *   * Neither the name of the author nor the names of its contributors may be 
 *     used to endorse or promote products derived from this software without 
 *     specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 *  POSSIBILITY OF SUCH DAMAGE.
 *  
 */

#ifndef _NFFILE_H
#define _NFFILE_H 1

#include <stddef.h>
#include <sys/types.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#define IDENTLEN	128
#define IDENTNONE	"none"

#define NF_EOF		 	 0
#define NF_ERROR		-1
#define NF_CORRUPT		-2

#define NF_DUMPFILE         "nfcapd.current"

#define NOT_COMPRESSED 0
#define LZO_COMPRESSED 1
#define BZ2_COMPRESSED 2
#define LZ4_COMPRESSED 3

/* 
 * output buffer max size, before writing data to the file 
 * used to cache flows before writing to disk. size: tradeoff between
 * size and time to flush to disk. Do not delay collector with long I/O
 */
#define WRITE_BUFFSIZE 1048576

/*
 * use this buffer size to allocate memory for the output buffer
 * data other than flow records, such as histograms, may be larger than
 * WRITE_BUFFSIZE and have potentially more time to flush to disk
 */
#define BUFFSIZE (5*WRITE_BUFFSIZE)

/* if the output buffer reaches this limit, it gets flushed. This means,
 * that 0.5MB input data may produce max 1MB data in output buffer, otherwise
 * a buffer overflow may occur, and data does not get processed correctly.
 * However, every Process_vx function checks buffer boundaries.
 */

/*
 * nfdump binary file layout 1
 * ===========================
 * Each data file starts with a file header, which identifies the file as an nfdump data file.
 * The magic 16bit integer at the beginning of each file must read 0xA50C. This also guarantees 
 * that endian dependant files are read correct.
 *
 * Principal layout, recognized as LAYOUT_VERSION_1:
 *
 *   +-----------+-------------+-------------+-------------+-----+-------------+
 *   |Fileheader | stat record | datablock 1 | datablock 2 | ... | datablock n |
 *   +-----------+-------------+-------------+-------------+-----+-------------+
 */


typedef struct file_header_s {
	uint16_t	magic;				// magic to recognize nfdump file type and endian type
#define MAGIC 0xA50C

	uint16_t	version;			// version of binary file layout, incl. magic
#define LAYOUT_VERSION_1	1

	uint32_t	flags;				
#define NUM_FLAGS		4
#define FLAG_NOT_COMPRESSED	0x0		// records are not compressed
#define FLAG_LZO_COMPRESSED	0x1		// records are LZO compressed
#define FLAG_ANONYMIZED 	0x2		// flow data are anonimized 
#define FLAG_UNUSED			0x4		// unused
#define FLAG_BZ2_COMPRESSED 0x8		// records are BZ2 compressed
#define FLAG_LZ4_COMPRESSED 0x10	// records are LZ4 compressed
#define COMPRESSION_MASK	0x19	// all compression bits
// shortcuts

#define FILE_IS_NOT_COMPRESSED(n) (((n)->file_header->flags & COMPRESSION_MASK) == 0)
#define FILE_IS_LZO_COMPRESSED(n) ((n)->file_header->flags & FLAG_LZO_COMPRESSED)
#define FILE_IS_BZ2_COMPRESSED(n) ((n)->file_header->flags & FLAG_BZ2_COMPRESSED)
#define FILE_IS_LZ4_COMPRESSED(n) ((n)->file_header->flags & FLAG_LZ4_COMPRESSED)
#define FILE_COMPRESSION(n) (FILE_IS_LZO_COMPRESSED(n) ? LZO_COMPRESSED : (FILE_IS_BZ2_COMPRESSED(n) ? BZ2_COMPRESSED : (FILE_IS_LZ4_COMPRESSED(n) ? LZ4_COMPRESSED : NOT_COMPRESSED)))

#define BLOCK_IS_COMPRESSED(n) ((n)->flags == 2 )
#define IP_ANONYMIZED(n) ((n)->file_header->flags & FLAG_ANONYMIZED)

							
	uint32_t	NumBlocks;			// number of data blocks in file
	char		ident[IDENTLEN];	// string identifier for this file
} file_header_t;

/* 
 * In file layout format 1: After the file header an 
 * inplicit stat record follows, which contains the statistics 
 * information about all netflow records in this file.
 */

typedef struct stat_record_s {
	// overall stat
	uint64_t	numflows;
	uint64_t	numbytes;
	uint64_t	numpackets;
	// flow stat
	uint64_t	numflows_tcp;
	uint64_t	numflows_udp;
	uint64_t	numflows_icmp;
	uint64_t	numflows_other;
	// bytes stat
	uint64_t	numbytes_tcp;
	uint64_t	numbytes_udp;
	uint64_t	numbytes_icmp;
	uint64_t	numbytes_other;
	// packet stat
	uint64_t	numpackets_tcp;
	uint64_t	numpackets_udp;
	uint64_t	numpackets_icmp;
	uint64_t	numpackets_other;
	// time window
	uint32_t	first_seen;
	uint32_t	last_seen;
	uint16_t	msec_first;
	uint16_t	msec_last;
	// other
	uint32_t	sequence_failure;
} stat_record_t;


// legacy nfdump 1.5.x data block type
#define DATA_BLOCK_TYPE_1		1

// nfdump 1.6.x data block type
#define DATA_BLOCK_TYPE_2		2

/*
 *
 * Block type 2:
 * =============
 * Each data block start with a common data block header, which specifies the size, type and the number of records
 * in this data block
 */

typedef struct data_block_header_s {
	uint32_t	NumRecords;		// number of data records in data block
	uint32_t	size;			// size of this block in bytes without this header
	uint16_t	id;				// Block ID == DATA_BLOCK_TYPE_2
	uint16_t	flags;			// 0 - compatibility
								// 1 - block uncompressed
								// 2 - block compressed
} data_block_header_t;

/*
 * Generic file handle for reading/writing files
 * if a file is read only writeto and block_header are NULL
 */
typedef struct nffile_s {
	file_header_t		*file_header;	// file header
#define NUM_BUFFS 2
	void				*buff_pool[NUM_BUFFS];	// buffer space for read/write/compression 
	size_t				buff_size;
	data_block_header_t	*block_header;	// buffer ptr
	void				*buff_ptr;		// pointer into buffer for read/write blocks/records
	stat_record_t 		*stat_record;	// flow stat record
	int					fd;				// file descriptor
} nffile_t;

/* 
 * The block type 2 contains a common record and multiple extension records. This allows a more flexible data
 * storage of netflow v9 records and 3rd party extension to nfdump.
 * 
 * A block type 2 may contain different record types, as described below.
 * 
 * Record description:
 * -------------------
 * A record always starts with a 16bit record id followed by a 16bit record size. This record size is the full size of this
 * record incl. record type and size fields and all record extensions. 
 * 
 * Know record types:
 * Type 0: reserved
 * Type 1: Common netflow record incl. all record extensions
 * Type 2: Extension map
 * Type 3: xstat - port histogram record
 * Type 4: xstat - bpp histogram record
 */

#define CommonRecordV0Type	1
#define ExtensionMapType	2
#define PortHistogramType	3
#define BppHistogramType	4

// Legacy records
#define LegacyRecordType1	5
#define LegacyRecordType2	6

// exporter/sampler types
#define ExporterInfoRecordType	7
#define ExporterStatRecordType	8
#define SamplerInfoRecordype	9

typedef struct record_header_s {
 	// record header
 	uint16_t	type;
 	uint16_t	size;
} record_header_t;


/*
 * for the detailed description of the record definition see nfx.h
*/


void SumStatRecords(stat_record_t *s1, stat_record_t *s2);

nffile_t *OpenFile(char *filename, nffile_t *nffile);

nffile_t *OpenNewFile(char *filename, nffile_t *nffile, int compress, int anonymized, char *ident);

nffile_t *AppendFile(char *filename);

int ChangeIdent(char *filename, char *Ident);

void PrintStat(stat_record_t *s);

void QueryFile(char *filename);

stat_record_t *GetStatRecord(char *filename, stat_record_t *stat_record);

nffile_t *DisposeFile(nffile_t *nffile);

void CloseFile(nffile_t *nffile);

int CloseUpdateFile(nffile_t *nffile, char *ident);

int ReadBlock(nffile_t *nffile);

int WriteBlock(nffile_t *nffile);

int RenameAppend(char *from, char *to);

void ModifyCompressFile(char * rfile, char *Rfile, int compress);


#endif //_NFFILE_H

