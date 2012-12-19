#include "lz78.h"


//******************************************************************************
//------------------------------DECOMPRESSORE-----------------------------------
//******************************************************************************
int lz78_decomp(struct decomp_state * ds, struct bitfile *in, FILE *out)
{
    int bits, need, len, i, l, ret;
    int match = 0;
    int jump = 0;
    unsigned char c_save = 0;
    unsigned char last = 0;
    uint32_t old_indice = 0;

    int d_size = 0;

    struct lz78_d * d = ds -> dec;      //riferimento al dizionario principale
    struct lz78_d * d2 = ds -> dec2;    //riferimento al dizionario secondario
    struct lz78_c * c = ds -> comp;     //riferimento al compressore di appoggio

    unsigned char * buf;
    buf = malloc(DIM_BUF * sizeof(unsigned char));
    
    if(buf == NULL){
        printf("Errore in allocazione di memoria.\n");
        exit(1);
    }
    
    
    bzero(buf, DIM_BUF * sizeof(unsigned char));

    d -> d_next = NULL_CHAR+1; //d_next inizialmente dalla prima posizione libera
    d -> buf_size = DIM_BUF;

   for(;;){

        bits = bitlen(d -> d_next);   //quanti bit mi servono per ricostruire l'indice
        need = bits - d -> n_bits;    //quanti bit devo ancora leggere per ricostruire l'indice


        if(d -> initialized == 0){
            need = 32; //per leggere dimensione dizionario
        }

       	/*
	*Se need è maggiore di zero vuol dire che devo leggere ancora bit dallo stream d'ingresso
	*/
	if(need > 0){
            update_bitbuf(&d -> bitbuf, &d -> n_bits, need, in);
	}


	/*
	*In uscita mi aspetto che bitbuf sia stato aggiornato con il numero di bit che mi servivano.
	*Se questo non è possibile mi ritroverò nel bitbuf meno bit di quelli che mi servono.
	*Il -1 di ritorno andrà interpretato da chi ha chiamato questa funzione in modo da poterla lanciare
	*nuovamente quando ci sono altri dati da leggere.
	*/
	if(d -> initialized == 1 && bits != d -> n_bits){
            printf("[decomp] esco per bits != n_bits\n");
            return -1;     //Mi devo sospendere
	}

	//----SUPERATO QUESTO PUNTO HO LETTO ABBASTANZA BIT PER INTERPRETARE L'INDICE-----

	//----GESTIONE INDICI PARTICOLARI-----

        d -> n_bits = 0;

	/*
	*Primo caso: ho trovato l'indice di End of FIle
	*Vuol dire che ho finito di decomprimere e ritorno zero per segnalare questa cosa.
	*/
	if(d -> bitbuf == D_EOF){ //End of File
            printf("[decomp] letto D_EOF\n");
            printf("[decomp] decompressione completata.\n");
            free_decomp(ds);
            free(buf);
            return 0; //Decompressione finita
	}

	/*
	*Secondo caso: ho trovato l'indice che mi segnala che mi sta per arrivare la dim del dizionario.
	*Dovrò mettere dentro d_next un valore che mi permette di leggere un numero di bit sufficienti.
	*/
	if(d -> bitbuf == D_DICT_SIZE){ //Dimensione Dizionario
            printf("[decomp] ricevuto simbolo D_DICT_SIZE..\n");
            continue;  //faccio ripartire il ciclo e faccio in modo dopo di capire la lunghezza del dizionario
	}


	/*
	*A questo punto possono succedere solo due cose:
	*1) Dizionario è stato allocato e quindi mi sta arrivando un codice valido.
	*2) Sono nella situazione in cui mi sta arrivando la lunghezza del dizionario.
	*Per discriminare questi due casi guardo se il dizionario è stato già allocato o meno.
	*/
	else if(d -> initialized == 0){
            printf("[decomp] ricevuta dimensione dizionario: %d\n", d -> bitbuf);
            d -> d_max = d -> bitbuf; 	//Setto la dimensione a quello che leggo da bitbuf
            d_size = d -> d_max;

            if(allocate_dict(d)){ 	//La funzione allocate_dict verifica se d_max è nel range giusto.
                return -2;   		//Errore Fatale
            }
            need = 0;
            continue;
	}

	//----GESTIONE INDICI NORMALI-----

	/*
	*build_string navigherà sull'albero e cercherà di restituire la stringa decodificata.
	*Inoltre ritorna il numero di caratteri nella stringa di uscita.
	*/
        bzero(buf, sizeof(d->buf_size));
	len = build_string(d -> dict, d -> bitbuf, buf, d -> d_next, &jump);

        


        //SE HO IL DIZIONARIO SECONDARIO ATTIVO SCORRO IL BUF E AGGIORNO
        if(d2 -> initialized == 1){
            //scorro la stringa decompressa
            for( i=0 ; i<len ; i++ ){
                ret = new_state(c, buf[i]); //aggiorno lo stato del compressore di appoggio passando il carattere corrente
                match = 1;

                if(ret == 0){ //se n_bits è maggiore di 0 vuol dire che ho qualcosa da scrivere sul dizionario secondario

                    c_save = buf[i]; //salvo l'ultimo carattere che non ha fatto match per un possibile aggiornamento in fase di scambio dizionario
                    
                    uint32_t indice = c -> bitbuf;

                    if(indice > d2 -> d_next){
                        printf("Errore in Aggiornamento Secondario: indice: %d > d_next: %d \n", indice, d2 -> d_next);
                        exit(1);
                    }

                    l = 0;
                    while(indice != NULL_CHAR){
                        old_indice = indice;
                        indice = d2 -> dict[indice].padre;
                        if(indice == 0){
                            printf("Errore in ciclo dizionario secondario\n");
                            exit(1);
                        }
                        l++;
                    }


                    if(d2 -> d_next > NULL_CHAR+1)
                        d2 -> dict[d2 -> d_next-1].etichetta = d2 -> dict[old_indice].etichetta;

                    //creo il buco
                    d2 -> dict[d2 -> d_next].padre = c -> bitbuf;
                    d2 -> d_next++;

                    c -> n_bits = 0;

                    match = 0; //Segnalo NO MATCH

                }

            }//for
        }

        if(d2->initialized == 1){
            last = buf[i-1]; //recupero quello che non ho ancora passato al dizionario
        }
        
        fwrite(buf, len, 1, out); 

        
        if(len > d->buf_size-1){ //ESPANDERE BUFFER di appoggio
            printf("[decomp] espando buffer...\n");
            d -> buf_size = d -> buf_size * 2;
            free(buf);
            buf = malloc(d->buf_size * sizeof(unsigned char));
            
            if(buf == NULL){
       	    	printf("Errore in allocazione di memoria.\n");
        	exit(1);
    	    }
            
            
            bzero(buf, d->buf_size * sizeof(unsigned char));
        }

	d -> dict[d -> d_next].padre = d -> bitbuf;	//Aggiornamento dizionario
        //printf("DEBUG dizionario[%d].padre = %d\n", o -> d_next, o -> bitbuf);
	d -> d_next++;


        //ATTIVAZIONE DIZIONARIO SECONDARIO
        if(d -> d_next == (d_size/2) && d2 -> initialized == 0){
            d2 -> d_next = NULL_CHAR+1; //d_next inizialmente dalla prima posizione libera
            d2 -> buf_size = DIM_BUF;
            d2 -> d_max = d -> d_max;
            printf("[decomp] attivato dizionario secondario con dimensione: %d\n", d2 -> d_max);
            allocate_dict(d2);
            comp_init(c, d2 ->d_max);
        }

        //GESTIONE SCAMBIO DIZIONARIO
        if(d -> d_next == d -> d_max){           

            d2 -> buf_size = d -> buf_size; //devo mantenere il mio bufsize 

            dict_reset(d);
            
            struct lz78_d * park;
            
            //SCAMBIO DI ISTANZE
            park = d2;
            d2 = d;
            d = park;
            
            //Ripulitura compressore
            hash_clean(c -> ht , c -> hash_size);
            c -> ht = hash_init(d2->d_max*2);

            c -> cur_node = NULL_CHAR;   //la ricerca dovrà partire dal nodo radice
            c -> d_next = NULL_CHAR + 1; //imposto il prossimo indice valido
            c -> n_bits = 0;

            
            if(match == 0){ //CASO NO MATCH = AGGIUNTA
                d -> dict[d -> d_next-1].etichetta = last;  
            }else{ //CASO MATCH - NO AGGIUNTA
                d -> dict[d -> d_next-1].etichetta = c_save;                
            }

            jump = 1; //devo far saltare il primo aggiornamento al dizionario principale

           printf("[decomp] dizionario scambiato. Dimensione: %d\n", d -> d_next);
     	}


    } //FINE FOR



}//FINE lz78_decomp

int update_bitbuf(uint32_t * bitbuf, uint * nb, int need, struct bitfile *i)
{
    int ret = 0;
    bzero(bitbuf, sizeof(uint32_t));
    ret = bit_read(i, (char *)bitbuf, need, 0);
    (*nb) += ret;
    return ret;
}

int allocate_dict(struct lz78_d *o)
{
    int i = 0;
    int dim = 0;

    dim = sizeof(struct nodo_d);

    o->dict = malloc(o -> d_max * dim);

    if(o->dict == NULL){
        printf("Errore in allocazione di memoria.\n");
        exit(1);
    }

    bzero(o->dict , o -> d_max * dim);

    //lascio l'indice zero libero
    for(i = 0; i <= VALID; i++){ //Per ogni locazione devo inserire i caratteri iniziali
        o->dict[i+1].padre = NULL_CHAR;
	o->dict[i+1].etichetta = i;
    }

    
    o -> initialized = 1;
    o -> n_bits = 0;

    return 0;
}

int build_string(struct nodo_d *dict, uint32_t bitbuf, unsigned char *buf, uint32_t d_next, int * jump)
{

    int i = 0;
    int k = 0;
    int j = 0;

    unsigned char park;

    uint32_t indice = bitbuf;

    if(indice >= d_next){
        printf("Errore in Build_String: indice: %d > d_next: %d \n", indice, d_next);
        exit(1);
    }


   //stampare l'etichetta corrispondente e risalire nel dizionario
    while(indice != NULL_CHAR){
        buf[i] = dict[indice].etichetta;
        indice = dict[indice].padre;
        if(indice == 0){
            printf("buf[i] %d - %c\n", buf[i], buf[i]);
            printf("Errore in ciclo Build_String\n");
            exit(1);
        }
        i++;
    }

    if(d_next > NULL_CHAR+1 && (*jump) == 0) //al primo giro non devo aggiornare il dizionario
        dict[d_next-1].etichetta = buf[i-1];

    *jump = 0; //primo giro effettuato


    if(bitbuf == d_next-1) //se mi arriva un indice non ancora completo lo completo
        buf[0] = buf[i-1];

 
    //CAPOVOLGIMENTO STRINGA RICOSTRUITA
    j=i-1;    
    while(j>k){
        park = buf[k];
        buf[k]=buf[j];
        buf[j]=park;
        k++;
        j--;

    }

    return i; //numero di caratteri da scrivere su file
}


int dict_reset(struct lz78_d *o)
{
    o -> d_next = NULL_CHAR+1;
    free(o->dict);
    allocate_dict(o);
    return 0;
}


//******************************************************************************
//------------------------------COMPRESSORE-------------------------------------
//******************************************************************************
int lz78_compress(struct comp_state * cs, FILE *in, struct bitfile *out)
{
    int ret, c_in_prec, err;
    int c_in = 0;

    struct lz78_c * c = cs -> d_main; //riferimento al compressore principale
    struct lz78_c * c2 = cs -> d_app; //riferimento al compressore secondario

    int d_size = c -> d_max;
    int h_size = d_size * 2; //tabella hash due volte il dizionario

    /*INIZIALIZZAZIONE lz78_c*/
    if(c -> initialized == 0){
	err = comp_init(c, d_size);
	if(err == -1){
		printf("Errore nell'inizializzazione di lz78_c\n");
	}

        c -> bitbuf = D_DICT_SIZE;
        c -> n_bits = 32;
        ret = bit_write(out, (const char *)&(c -> bitbuf), c -> n_bits, 0); //cerco di scriverli sul bitfile

        if(ret <= 0) return -1;	//scrittura fallita

        c -> bitbuf = d_size;
        ret = bit_write(out, (const char *)&(c -> bitbuf), c -> n_bits, 0); //cerco di scriverli sul bitfile

        c -> n_bits = 0;

    }


    for(;;){

	if(c -> n_bits > 0){	//se ho bit da inviare
             
            ret = bit_write(out, (const char *)&(c -> bitbuf), c -> n_bits, 0); //cerco di scriverli sul bitfile

            if(ret <= 0) return -1;	//scrittura fallita

            /*In caso che la bit_write sia andata a buon fine dobbiamo aggiornare la struttura.
            *Per primo slitto verso destra bitbuf di quanti bit ho scritto.
            *In seguito sottraiamo ad n_bits il numero di bit che ho scritto*/
            c -> bitbuf >>= ret;
            c -> n_bits -= ret;

            /*
             * Se il cur_node è il carattere di fine file (D_EOF) ho finito la compressione
             */
            if(c->cur_node == D_EOF && c->n_bits == 0){
                c -> n_bits = bitlen(c->d_next);
                bit_write(out, (const char *)&(c -> cur_node), c -> n_bits, 0); //Scrivo D_EOF sul file
                printf("[comp] scritto D_EOF\n");
                printf("[comp] compressione completata.\n");
                return 0;
            }

            /*Se n_bits è maggiore di zero vuol dire che devo fare almeno un altro ciclo*/
            if(c -> n_bits > 0) continue;
	}

        //Se sono alla soglie e C2 non è stato ancora inizializzato, lo inizializzo
        if(c -> d_next == (d_size/2) && c2 ->initialized == 0){
            err = comp_init(c2, d_size);
            if(err == -1){
		printf("Errore nell'inizializzazione di lz78_c secondario\n");
            }
        }


	/*Se siamo usciti dall'if vuol dire che devo leggere il carattere
	*Fgetc è una funzione che ritorna un intero da un file.
	*Ci segnala se siamo riusciti a leggere un carattere
	*oppure se c'è stato qualche evento particolare come un errore di lettura.
	*Se ritorna un valore minore di zero i caratteri non erano disponibili sul file.
	*/
        c_in_prec = c_in; //salvo il carattere precedente per il compressore secondario
        
        err = 0;
	c_in = fgetc(in);
	err = errno;           


	/*fgetc: distingue errore persistente (file finito)
	*da errore temporaneo (dati non disponibili)
	*con una variabile globale chiamata ERRNO.
	*/
	
	//Se c_in è maggiore di zero, non ci sono stati errori e devo calcolare il nuovo stato
	if(c_in >= 0){
            /*
             *Se ho il compressore secondario attivo, chiamo la new_state anche su lui
             * gli passo c_in_prec (un carattere indietro rispetto al compressore primario)
             * per tenerlo allineato con il compressore di appoggio lato decompressione
            */
            if(c2 ->initialized == 1)
                new_state(c2, c_in_prec);


            //chiamo la new state sul compressore primario (carattere avanti rispetto al secondario)
            new_state(c, c_in);


               if(c -> d_next == c -> d_max){ //dizionario principale PIENO
               
                c2 -> cur_node = c -> cur_node; //devo ripartire da dove ero arrivato
                c2 -> bitbuf = c -> bitbuf; //devo scrivere quello che stavo per scrivere
                c2 -> n_bits = c -> n_bits; //lo devo scrivere su un numero di bits giusti


                //pulizia dizionario pieno
                hash_clean( c -> ht , c2 -> hash_size );
                c -> ht = hash_init(h_size);

                //il compressore secondario riparte dall'inizio
                c -> d_next = NULL_CHAR + 1;
                c -> cur_node = NULL_CHAR;

                struct lz78_c * park;

                //scambio dizionari
                park = c;
                c = c2;
                c2 = park;

                printf("[comp] dizionario scambiato. Dimensione: %d\n", c -> d_next);
        }


            
	}else{ //Caso in cui c_in < 0 - End of File o ERRORE
	
		
		if(feof(in)){ //Se sono alla fine del file	
            		c -> bitbuf = c -> cur_node; //ultima sequenza matchata in bitbuf
            		c -> n_bits = bitlen(c -> d_next);	//setto n_bits alla lunghezza opportuna
            		c -> cur_node = D_EOF;	//codice End of File
            		continue;
            	}
            	
            	if(err == EAGAIN || err == EINTR) continue;
            	
            	printf("ERROR: %s\n", strerror(err));
            	exit(1);            		
            	

	}


    }//Fine FOR

    free_comp(cs);
}

int comp_init(struct lz78_c * c, int d_size)
{
    //azzero tutti i campi
    bzero(c, sizeof(struct lz78_c));

    c -> ht = hash_init(d_size*2); //alloco e setto la tabella hash su c -> ht
    c -> hash_size = d_size*2;
    c -> d_max = d_size;
    c -> cur_node = NULL_CHAR;
    c -> d_next = NULL_CHAR+1;
    c -> initialized = 1;
    c -> n_bits = 0;

    return 0;
}

/*
 * La new_state segnala l'aggiunta di un nuovo nodo ritornando 0
 * In caso contrario (MATCH) ritorna l'indice del nodo che ha fatto match
 */
int new_state(struct lz78_c * c, int c_in)
{

    unsigned int indice = hash_add(c->ht, c_in, c->cur_node, c->hash_size, c->d_next);
    if(  indice != 0 ){ //MATCH
        //Sposto cur_node sull'indice che ha fatto match
        c -> cur_node = indice;
        return indice; //Segnalo MATCH

    }else{ //NO MATCH - AGGIUNTA
        bzero(&(c->bitbuf), sizeof(uint32_t));
        c -> bitbuf = c -> cur_node; //preparo l'emissione di cur_node
        c -> n_bits = bitlen(c -> d_next); //imposto la dimensione in base ai bit necessari a rappresentare l'indice attuale
        c -> cur_node = c_in+1; //setto cur_node all'indice del char non trovato
        c -> d_next++;

    }

    return 0; //Segnalo AGGIUNTA
}

int bitlen(int dn)
{
    return log2(dn) + 1;
};

unsigned int PJWHash(unsigned char* str, int len)
{
    const unsigned int BitsInUnsignedInt = (unsigned int) (sizeof (unsigned int) * 8);
    const unsigned int ThreeQuarters = (unsigned int) ((BitsInUnsignedInt * 3) / 4);
    const unsigned int OneEighth = (unsigned int) (BitsInUnsignedInt / 8);
    const unsigned int HighBits = (unsigned int) (0xFFFFFFFF) << (BitsInUnsignedInt - OneEighth);
    unsigned int hash = 0;
    unsigned int test = 0;
    unsigned int i = 0;

    for (i = 0; i < len; str++, i++) {
        hash = (hash << OneEighth) + (*str);

        if ((test = hash & HighBits) != 0) {
            hash = ((hash ^ (test >> ThreeQuarters)) & (~HighBits));
        }
    }

    return hash;
}


unsigned int get_hash_index(unsigned char * string, int size)
{
    return PJWHash(string, HL) % size; //riadatto la lunghezza dell'indice alle dimensioni della hast table
}

struct nodo_c * hash_init(int size)
{
    //printf("[hash_init] started...\n");
    unsigned int i, dim, indice_padre;
    struct nodo_c * ht;

    //Calcolo la grandezza da allocare come Dimensione Dizionario * Ogni singolo nodo
    dim = size * sizeof (struct nodo_c);

    //Alloco e pulisco lo spazio di memoria necessario
    ht = malloc(dim);

    if(ht==NULL){
        printf("Errore in allocazione memoria\n");
        exit(1);
    }

    bzero(ht, dim);

    indice_padre = NULL_CHAR; //tutti i primi caratteri hanno come padre il carattere vuoto

    for(i = 0; i <= VALID; i++){
        hash_add(ht, i, indice_padre, size, i+1);
    }
    //printf("[hash_init] completed.\n");
    return ht;

}

int hash_add(struct nodo_c * ht, unsigned char etichetta, unsigned int ind_padre, int size, int indice)
{
    unsigned int indice_hash;

    //Buffer di appoggio per costruire la stringa da passare alla funzione di hashing
    unsigned char buf[HL];
    bzero(buf, HL);

    //copio l'etichetta nella prima parte della stringa
    buf[0] = etichetta;

    //copio l'indice del padre nella seconda parte della stringa
    if(memcpy(&buf[1], &ind_padre, sizeof(ind_padre)) == NULL){
        printf("[hash_add] Errore in copia memoria buf[1]\n");
        return -1;
    }

    indice_hash = get_hash_index(buf, size);

    if(ht[indice_hash].indice != 0){ //COLLISIONE oppure Nodo già in dizionario
        unsigned int ret = find_nodo(ht, indice_hash, etichetta, ind_padre);

        if(ret == 0){ //Nodo non trovato quindi COLLISIONE
            //--------GESTIONE COLLISIONE----------
            //creo un nuovo nodo
            struct nodo_c * new;
            new = malloc(sizeof(struct nodo_c));

             if(new==NULL){
                printf("Errore in allocazione memoria\n");
                exit(1);
            }

            bzero(new, sizeof(struct nodo_c));

            //lo riempio
            new -> etichetta = etichetta;
            new -> indice_padre = ind_padre;
            new -> indice = indice;
            //lo inserisco nella catena di collisioni
            new -> next = ht[indice_hash].next;
            ht[indice_hash].next = new;

            return 0; //Segnalo aggiunta - NO MATCH

        }else{ //Nodo trovato quindi nessuna aggiunta
            return ret; //Segnalo nessuna aggiunta - MATCH - Ritorno indice
        }

    }else{ //Posizione in Hash Table ancora libera
        //Aggiungo il nodo nell'hash table
        ht[indice_hash].etichetta = etichetta;
        ht[indice_hash].indice_padre = ind_padre;
        ht[indice_hash].indice = indice;
    }

    return 0; //Segnalo aggiunta - NO MATCH

}

int find_nodo(struct nodo_c * ht, int indice_hash, unsigned char etichetta, unsigned int ind_padre)
{
    struct nodo_c * iteratore = &ht[indice_hash]; //Iteratore per scorrere la lista concatenata di nodi
    while(iteratore != NULL){
        if(iteratore -> etichetta == etichetta && iteratore -> indice_padre == ind_padre){
            //Se il nodo corrente ha etichetta e padre uguale allora l'elemento esiste già nell'albero
            return iteratore -> indice; //segnalo che esiste ritornando il suo indice
        }
        iteratore = iteratore -> next; //avanzo l'iteratore
    }

    //se sono uscito dal while senza ritornare vuol dire che non ho trovato un elemento uguale
    //e quindi sono in presenza di una Collisione
    return 0;

}

int hash_clean(struct nodo_c * ht, int size)
{
    int i;
    struct nodo_c * app;

    for(i=0; i<size; i++){
        app = ht[i].next;
        while(app!=NULL){ //Ripulisco la lista concatenata delle colissione
            ht[i].next = app->next;
            free(app);
            app = ht[i].next;
        }
    }
    return 0;
}

int free_comp(void * c)
{
    struct comp_state * c_spec = (struct comp_state *) c;	//cast al tipo specifico di compressore
    struct lz78_c * main_tab = c_spec->d_main;
    struct lz78_c * app_tab = c_spec->d_app;

    hash_clean(main_tab->ht , main_tab->hash_size);
    free(main_tab->ht);

    if(app_tab -> initialized == 1){
        hash_clean(app_tab->ht , app_tab->hash_size);
        free(app_tab->ht);
    }

    return 0;
}

int free_decomp(void * d){
    struct decomp_state * d_spec = (struct decomp_state *) d;	//cast al tipo specifico di compressore
    struct lz78_d * main_tab = d_spec->dec;
    struct lz78_d * app_tab = d_spec->dec2;
    struct lz78_c * c_tab = d_spec->comp;

    free(main_tab->dict);

    if(app_tab -> initialized == 1){
        hash_clean(c_tab->ht , c_tab->hash_size);
        free(c_tab->ht);
        free(app_tab->dict);
    }

    return 0;
}
