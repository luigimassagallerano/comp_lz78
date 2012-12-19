#include "bit.h"

/* 
* Creates a bitfile structure with a buffer size 
* set to the value passed by parameter: bufsize
*/
struct bitfile * bit_open(const char *f, int mode, int bufsize){

	int fd;
    int dim;
    struct bitfile * b;
    int size = bufsize/8;

	/* Check on the size, if it fails: size set to AVG value */
    if( size < MIN_BUFFER || size > MAX_BUFFER )
    {
        printf("[bit_open] buffer size set to AVG value\n");
        size = (MIN_BUFFER + MAX_BUFFER) / 2;
    }

    bufsize = size * 8;

    dim = (sizeof(struct bitfile) + size);
    printf("[bit_open] buffer size: %d byte\n", size);

    fd = open(f,mode,S_IRWXU);
    if( fd == -1 )
    {
        printf("[bit_open] file not available in the selected mode\n");
        return NULL;
    }

    /* Memory allocation for structure bitfile */
    b = malloc(dim);
    bzero( b , dim ); // writing zero-valued bytes on the new struct
	/* -------------------------------------- */	

    /* Bitfile parameters initialization */
    b->fd = fd;
    b->mode = mode;
    b->bufsize = bufsize;
    b->n_bits = 0;
    b->ofs = 0;
	/* --------------------------------- */

    return b;
}

/* 
* Writes a number of bits in the buffer and eventually 
* in the file if the buffer is full.
*/
int bit_write(struct bitfile *fp, const char *base, int n_bits, int ofs)
{
	uint8_t mask;
	const char *p;
	char bit;
    int tot = n_bits;

	mask = 1 << ofs; // 1 left-shifted of "ofs" bits
	p = base;

	while(n_bits>0){
		/* 1) Extraction of bit */
		bit = (*p & mask) ? 1:0; 

		if(mask == 0x80){ // If mask is at the end of the byte
			mask=1;      // Setting mask to 1
			p++;  		// Moving pointer to the next byte
		}else{
			mask <<= 1;	// Shifting of the mask on the next bit
		}

		int pos = fp->ofs + fp->n_bits;

		/* 2) Adding of the bit to the I/O bitfile buffer */
		if(bit == 0){ 			// If I read zero
			fp -> buf[pos/8] &= ~ (1 << (pos%8)); // I write zero with complement operation
		}else{ 	// If i read one 
			fp -> buf[pos/8] |= (1 << (pos%8)); // I write one
		}

		fp -> n_bits++;

		/* 3) flush - if needed */
		if(fp->ofs + fp->n_bits == fp -> bufsize){
			bit_flush(fp);
		}

		n_bits--;
	}

    return tot;
}
/*
* The bit_read() reads a number of bits from the buffer
* eventually loading a new portion of the file if the buffer is empty.
*/
int bit_read(struct bitfile * fd ,char * base , int n_bits , int ofs)
{
    uint8_t w_mask = 1 << ofs;	// writing mask - user buf
    uint8_t mask;			//  writing mask - fp->buf
    int pos;
    char * p = base;
    int letti;
    int inseriti = 0;

    //---------------SECTION CONTROL ERRORS----------------------
    //Check on the Access Mode
    if(fd->mode != BIT_RD)
    {
        printf("[bit_read] Errore di accesso al bitfile\n");
        return -1;
    }

    if(n_bits < 0)
    {
        printf("[bit_read] Errore, il numero di bit non puo' essere negativo\n");
        return -1;
    }

    if(ofs<0 || ofs>7)
    {
        printf("[bit_read] Errore dimensione offset: %d\n",ofs);
        return -1;
    }
   
        /* ADD MORE ERRORS CHECKING */
    //--------------------------------------------------------------

    while( n_bits > 0 )
    {
        
        if( fd->n_bits == 0) // managing refill buffer
        {
            fd->ofs = 0;
            letti = read(fd->fd , fd->buf , fd->bufsize/8);

            if ( letti == 0 )
            {
                return inseriti;
            }
            if ( letti < 0 )
            {
                printf("[bit_read] errore di lettura dal descrittore\n");
                return -1;
            }

            fd->n_bits = letti*8;
        }

        //finding the byte on the I/O buffer from which read
        pos = fd->ofs / 8;
        //finding the bit on the I/O buffer from which read
        mask = 1 << fd->ofs % 8;

        //reading from I/O buffer and writing on the user buf
        if( fd->buf[pos] & mask )
        {
            *p |= w_mask;
        }
        else
        {
            *p &= ~w_mask;
        }

        //managing wrap around mask on the user-buf
        if( w_mask == 0x80 )
        {
            p++;
            w_mask = 1;
        }
        else
            w_mask <<= 1;

        inseriti++;
        fd->ofs++;
        fd->n_bits--;
        n_bits--;
    }
    
    return inseriti;
}


/* The bit_flush() writes the content in the buffer to the file
 * without waiting for the buffer to become full.
 */
int bit_flush(struct bitfile *fp)
{
	int n_byte = 0;
	int scritti = 0;

	//Check on n_bits is a multiple of 8 and handling if not 
	if((fp -> n_bits % 8) != 0){
		n_byte = ((fp -> n_bits)/8)+1; //rounding up 
		scritti = write(fp->fd, (fp->buf)+(fp->ofs), n_byte); //also flush the extra bits

	}else{
	//case it is a multiple of 8
		n_byte = ((fp -> n_bits)/8);
		scritti = write(fp->fd, fp->buf, n_byte);
	}

	if(scritti == -1){
		printf("Errore nella write\n");
		return -1;
	}

	fp -> n_bits = 0;

	return 0;

}

/* The bit_close() closes the file deleting the file descriptor
 * previously allocated, with its buffer
 */
int bit_close(struct bitfile * fp)
{
	/*
	* Recognizing the Access Mode
	* If it's in WR mode: forcing the write of what's in the buffer using the flush
	* freeing of the used memory 
	*/

	if(fp->mode == BIT_RD)
	{
		printf("[bit_close] Chiusura file in lettura\n");
		return 1;
	}
	if(fp->mode == BIT_WR)
	{
		printf("[bit_close] Chiusura file in scrittura\n");
		bit_flush(fp);
		return 1;
	}

	free(fp);

	return 0;
}

