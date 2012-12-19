/* 
 * Author: Luigi Massa Gallerano
 *
 * Library with input/output functions in order to perform buffered I/O on a per bit basis.
 * The library provides similar functions to the fread/fwrite family functions, 
 * but it has been optimized to work with bits instead of bytes.
 *
 */

#ifndef _BIT_H
#define	_BIT_H

#define MAX_BUFFER 5000
#define MIN_BUFFER 64
#define BIT_WR (O_WRONLY|O_CREAT)
#define BIT_RD O_RDONLY

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

/* File descriptor */
struct bitfile
{
	int fd; /* Intern file descriptor */
	int mode;  /* Access mode */
	int bufsize; /* Max num of bits stored in the I/O buffer */
	int n_bits; /* Current num of bits in the I/O buffer */
	int ofs;  /* Offset (in bits) of the first bit in the buffer */
	char buf[0]; /* No-a-priori size I/O buffer */
};


/* The bit_open() creates the bitfile structure
 * Parameters:
 * - file = file path to be opened
 * - mode = access mode
 * - bufsize = buffer size requested by the user in bits
 * Return value:
 * - pointer to the bitfile structure on success
 * - NULL on error (errno is set appropriately)
 */
struct bitfile * bit_open(const char *f, int mode, int bufsize);

/* The bit_close() closes the file deleting the file descriptor
 * previously allocated, with its buffer
 * Parameters:
 * - fp = pointer to the file descriptor
 * Return value:
 * - 0 on success
 * - -1 on failure
 */
int bit_close(struct bitfile *fp);

/* The bit_write() writes a number of bits in the buffer and
 * eventually in the file if the buffer is full.
 * Parameters:
 * - fp = pointer to the file descriptor
 * - base = pointer to the base of the user buffer containing bits to write
 * - n_bits = number of bits to write in the user buffer
 * - ofs = offset of the first bit in the first byte of the buffer
 * Return value:
 * - number of bits written
 * - -1 on error
 */
int bit_write(struct bitfile *fp, const char *base, int n_bits, int ofs);

/* The bit_read() reads a number of bits from the buffer
 * eventually loading a new portion of the file if the buffer is empty.
 * Parameters:
 * - fp = pointer to the file descriptor
 * - base = pointer to the base of the buffer where to write the read bits
 * - n_bits = number of bits to read
 * - ofs = offset of the first bit to write in the first byte of the buffer
 * Return value:
 * - number of bits read
 * - -1 on error
 */
int bit_read(struct bitfile *fp, char *base, int n_bits, int ofs);

/* The bit_flush() writes the content in the buffer to the file
 * without waiting for the buffer to become full.
 * Parameters:
 * - fp = pointer to the file descriptor
 * Return value:
 * - 0 on success
 * - -1 on failure
 */
int bit_flush(struct bitfile *fp);

#endif	/* _BIT_H */

