#include "lz78.h"
#include "bit.h"

#include <getopt.h>

int main(int argc, char** argv)
{

    int c;
    FILE * in;
    char * in_name;
    FILE * decompresso;
    char * dec_name;
    
    struct comp_state cs;
    struct lz78_c m;
    struct lz78_c app; 
       
    struct bitfile * out;
    
    
    struct decomp_state ds;
    struct lz78_d dec;
    struct lz78_d dec2;
    struct lz78_c com;
    
    struct bitfile * compresso;
    
    int bufsize = 0;
    int size = 0;
    
    if(argc == 1){
    
    	printf("\nOptions:\n");
    	printf("-b <buffer size> - alloca un buffer di buffer_size bit\n");
    	printf("-s <dict size> - alloca un dizionario di dict_size elementi\n");
    	printf("-c <nome file> - comprime il file specificato nel file compresso\n");
    	printf("-d <nome file> - decomprime il file compresso nel file specificato\n\n");
    	
    	printf("use: ./lz78 [-b bufsize] [-s dictsize] -c original_file --> per comprimere\n");
    	printf("use: ./lz78 [-b bufsize] -d end_file --> per decomprimere\n\n");    	
    	
    	return -1;
    
    }  
    
                  
	
    while((c = getopt(argc, argv, "b:s:c:d:")) != -1){
	
        switch(c){
            case 'b':
            	bufsize = atoi(optarg); //atoi in caso di errore ritorna zero, quindi sono protetto perché bufsize resta zero
            	break;
            	
            case 's':
            	size = atoi(optarg);
            	break;
            	
            case 'c':
                
                bzero(&cs, sizeof(struct comp_state));                
                bzero(&m, sizeof(struct lz78_c));                
                bzero(&app, sizeof(struct lz78_c));

                               
                if(size == 0 || size < 5000){
                    size = 50000;
                }

                m.d_max = size;

                cs.d_main = &m;
                                
                cs.d_app = &app;
                cs.state = 0;

                in_name = optarg;

                in = fopen(in_name, "r");

		if(bufsize == 0){
                	out = bit_open("./compresso" , BIT_WR , 1024);
                }else{
                	out = bit_open("./compresso" , BIT_WR , bufsize);
                }

                lz78_compress(&cs,in,out);

                bit_close(out);
                fclose(in);
                break;

            case 'd':

                bzero(&ds, sizeof(struct decomp_state));
                bzero(&dec, sizeof(struct lz78_d));
                bzero(&dec2, sizeof(struct lz78_d));
                bzero(&com, sizeof(struct lz78_c));

                ds.dec = &dec;
                ds.dec2 = &dec2;
                ds.comp = &com;
                ds.state = 0;

                dec_name = optarg;

                decompresso = fopen(dec_name, "w");
                
                if(bufsize == 0){
                	compresso = bit_open("./compresso" , 0 , 1024);
                }else{
                	compresso = bit_open("./compresso" , 0 , bufsize);
                }

                lz78_decomp(&ds,compresso,decompresso);

                bit_close(compresso);
                fclose(decompresso);
                break;

            case '?':
                fprintf (stderr, "opzione sconosciuta `-%c'.\n", optopt);
                return 1;

            default:
                abort();

        }
	
    }

    return 0;
}




