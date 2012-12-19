#ifndef _LZ78_H
#define	_LZ78_H

#include "bit.h"
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <errno.h>


#define VALID 255   //numero di cifre valide nel dizionario iniziale
#define HL 5        //numero di caratteri da considerare nel calcolo della hash

#define DIM_BUF 100

#define D_EOF 257
#define D_DICT_SIZE 258
#define NULL_CHAR 259 //Carattere vuoto - Radice dell'albero



struct nodo_d
{
	uint8_t etichetta;
	uint32_t padre;
};

struct lz78_d
{
	uint32_t d_max;		//dimensione massima del dizionario
        uint32_t buf_size;      //dimensione attuale del buffer di appoggio
	struct nodo_d * dict;	//testa di dizionario
	uint32_t d_next;	//prossimo elemento da riempire nel dizionario
	int initialized;	//flag di inizializzazione
	uint32_t bitbuf;	//buffer di bit letti ma non ancora processati
	uint n_bits;		//numeri di bit validi già letti
};

struct decomp_state
{
	int state;		//[0]:da inizializzare
	struct lz78_d * dec;
        struct lz78_d * dec2;
	struct lz78_c * comp;
};

struct nodo_c
{
	char etichetta;
	unsigned int indice;
	unsigned int indice_padre;
	struct nodo_c * next;
};

struct lz78_c
{
	int d_max;		//dimensione max albero
	int hash_size;		//dimensione tabella hash (albero)
	struct nodo_c * ht;	//puntatore alla tabella hash allocata dinamicamente
	int cur_node;		//indica su quale nodo ci troviamo
	int d_next;		//punto di riempimento dell'albero
	uint32_t bitbuf;	//numero massimo di bit
	int n_bits;		//bit da trasferire rimasti pendenti
	int initialized;	//Se 0: da inizializzare
};

struct comp_state
{
	int state;		//[0]:da inizializzare
	struct lz78_c * d_main;
	struct lz78_c * d_app;
};

int lz78_decomp(struct decomp_state * ds, struct bitfile *in, FILE *out);
int lz78_compress(struct comp_state * cs, FILE * in, struct bitfile *out);

int allocate_dict(struct lz78_d *o);
int bitlen(int dn);
int update_bitbuf(uint32_t * bitbuf, uint * nb, int need, struct bitfile *i);
int build_string(struct nodo_d *dict, uint32_t bitbuf, unsigned char *buf, uint32_t d_next, int * jump);
int dict_reset(struct lz78_d *o);

int new_state(struct lz78_c *c, int c_in);

int comp_init(struct lz78_c * c, int d_size);

struct nodo_c * hash_init(int size);

int hash_add(struct nodo_c * ht, unsigned char etichetta, unsigned int ind_padre, int size, int indice);
int find_nodo(struct nodo_c * ht, int indice_hash, unsigned char etichetta, unsigned int ind_padre);

int free_decomp(void * d);
int free_comp(void * c);
int hash_clean(struct nodo_c * ht, int size);

#endif	/* _LZ78_H */

