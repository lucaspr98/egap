#include <stdio.h>                                  
#include <stdlib.h>                                 
#include <string.h>   
#include <getopt.h>
                    
#include "heap.h"
#include "src/malloc_count.h"
#include "lib/utils.h"


#ifndef DEBUG
#define DEBUG 1
#endif

#ifndef CHECK
#define CHECK 0
#endif

#ifndef HEAP_SIZE
#define HEAP_SIZE 256 //must be larger than 1
#endif

/**********************************************************************/

//Open a file and returns a pointer
FILE* file_open(char *c_file, const char * c_mode){
  
  FILE* f_in;
  f_in = fopen(c_file, c_mode);
  if (!f_in) perror ("file_open");
  fseek(f_in, 0, SEEK_SET);
  
return f_in;
}

/**********************************************************************/

int heap_sort_level(heap *h, FILE *f_lcp, size_t *sum, char* c_file, int level){
  
  #if CHECK == 2
    char c_pos[500];
    FILE* f_pos=NULL;
    if(!level){
      sprintf(c_pos, "%s.pos.lcp", c_file);
      f_pos = file_open(c_pos, "wb"); 
    }
  #endif
  
  #if CHECK == 1
    uint64_t curr=0;
  #endif
  
  #if DEBUG
    printf("**\n");
  #endif
  
  //pair sentinel; sentinel.pos=I_MAX; sentinel.lcp=I_MAX;
  pair sentinel; sentinel=MAX_KEY; //sentinel.lcp=I_MAX;
    
  while(key(0)!=sentinel){
  
    pair tmp = heap_delete_min(h);
    heap_write(h, f_lcp, tmp, level);
    (*sum)++;
    #if CHECK == 2
      uint64_t out = pos(tmp);
      if(!level) fwrite(&out, h->pos_size, 1, f_pos);
    #endif    
    
    #if CHECK == 1
      if(!level){
        if(pos(tmp)!=curr){
          printf("%lu != %lu\n", pos(tmp), curr);
          printf("isNotSorted!!\n");
          return 0;
        }
        curr++;
      }
    #endif
    
    #if DEBUG
      if(level) printf("<%lu, %lu [%llu]> ", lcp(tmp), pos(tmp), tmp);
      else printf("%lu, ", pos(tmp));
    #endif
  }
  
  if(level){
    heap_write(h, f_lcp, sentinel, 1);
    (*sum)++;
  }
  
  #if DEBUG
    printf("\n**\n");
  #endif
  
  #if CHECK == 2
    if(!level) fclose(f_pos);
  #endif

return 1;
}

/**********************************************************************/

void usage(char *name){
  printf("\n\tUsage: %s [options] FILE POS_SIZE LCP_SIZE \n\n",name);
  puts("Multiway k-merge sort for the lists of pairs <pos, lcp>.");
  puts("Input:\tFILE.pair.lcp with the lists and FILE.size.lcp");
  puts("with their start positions in FILE.pair.lcp");
  puts("Output:\tFILE.lcp contains <lcp> sorted by <pos>.\n");
  puts("Available options:");
  puts("\t-h\tthis help message");
  puts("\t-k\tHEAP_SIZE");
  puts("\t-t\ttime");
  puts("\t-v\tverbose\n");
  exit(EXIT_FAILURE);
}

/**********************************************************************/

int main(int argc, char **argv) {                   
  

printf("%zu bytes\n", sizeof(uint128_t));
  extern char *optarg;
  extern int optind, opterr, optopt;
  
  int c=0, time=0, verbose=0;
  char *c_file=NULL;
  int heap_size=HEAP_SIZE;
  int pos_size=4, lcp_size=4;
  
  while ((c=getopt(argc, argv, "k:vth")) != -1) {
    switch (c)
    {
      case 'k':
        heap_size=atoi(optarg); break;  // compute LCP and output in Gap format 
      case 'v':
        verbose++; break;
      case 't':
        time++; break;
      case 'h':
        usage(argv[0]); break;       // show usage and stop
      //case 'm':
        //RAM=(size_t)atoi(optarg)*KB; break;
      case '?':
        exit(EXIT_FAILURE);
    }
  }
  free(optarg);
  
  if(optind+3==argc) {
    c_file=argv[optind++];
    pos_size=atoi(argv[optind++]);
    lcp_size=atoi(argv[optind++]);
    
    if(pos_size+lcp_size>16) exit(0); 
    if(!pos_size) exit(0); 
    if(!lcp_size) exit(0); 
  }
  else{
    usage(argv[0]);
  }
  
  if(heap_size<2){
    puts("ERROR: k must be larger than 1");
    exit(0);
  }
  
  //config
  char c_lcp[500], c_size[500];
  char c_lcp_multi[500], c_size_multi[500]; //multilevel
  
  sprintf(c_lcp, "%s.pair.lcp",  c_file);
  sprintf(c_size, "%s.size.lcp", c_file);
  
  printf("INPUT:\t%s, %s\n", c_lcp, c_size);
  
  if(verbose){
    printf("sizeof(pos) = %d bytes\n", pos_size);
    printf("sizeof(lcp) = %d bytes\n", lcp_size);
  }

  // inits 
  time_t t_start=0, t_total=0;
  clock_t c_start=0, c_total=0;
   
  time_start(&t_start, &c_start);
  time_start(&t_total, &c_total);
  
  //alloc heap
  int level=0, onelevel=0;
  size_t i, blocks;
  FILE *f_size, *f_lcp;
  heap *h;
  size_t size, seek=0;
  size_t sum;
  
  //LEVEL 1 
  do{ // multilevel merging
  
    //printf("INPUT: %s\t%s\n", c_lcp, c_size);
    seek=0;
    
    blocks=1; i=0; sum=0;
    level++;
    h = heap_alloc(heap_size, c_lcp, level, pos_size, lcp_size);
    
    f_size = fopen(c_size, "rb");//header file
    
    //output
    sprintf(c_lcp_multi, "%s.pair.%d.lcp", c_file, level);  
    f_lcp = file_open(c_lcp_multi, "wb");//lists file
    
    sprintf(c_size_multi, "%s.size.%d.lcp", c_file, level);
    FILE* f_size_multi = file_open(c_size_multi, "wb");
    
    /**/
    while(fread(&size, sizeof(size_t), 1, f_size)){
      //init heap
      #if DEBUG
        printf("%zu (%zu): ", size, seek);
      #endif
      heap_insert(h, seek); //size == seek
      seek+=size;
      
      if(++i == heap_size){
            
        heap_sort_level(h, f_lcp, &sum, c_file, level);     
        fwrite(&sum, sizeof(size_t), 1, f_size_multi);
      
        //new heap
        heap_free(h, f_lcp, level);
        h = heap_alloc(heap_size, c_lcp, level, pos_size, lcp_size);
        
        blocks++;
        i=0;
        sum=0;
      }
    }
    
    if(i){//last block
      
      if(blocks==1 && level==1){
        onelevel=1;
        fclose(f_lcp); fclose(f_size); fclose(f_size_multi);
        //empty files
        remove(c_size_multi);
        remove(c_lcp_multi);
        break;
      }
      
      heap_sort_level(h, f_lcp, &sum, c_file, level);
      fwrite(&sum, sizeof(size_t), 1, f_size_multi);    
    }
        
    heap_free(h, f_lcp, level);
    fclose(f_lcp); fclose(f_size); fclose(f_size_multi);
    
    if(verbose)
      printf("%zux%d+%zu\t\n", blocks-1, heap_size, i);
    
    if(level>1){//do not remove the original inputs
      remove(c_lcp);
      remove(c_size);
    }
    
    //input for the next level
    sprintf(c_lcp, "%s.pair.%d.lcp", c_file, level);
    sprintf(c_size, "%s.size.%d.lcp", c_file, level);
  }
  while(blocks>heap_size);
  
  level = 0;

  if(onelevel){
    if(verbose)
      printf("%zux%d+%zu\t\n", blocks-1, heap_size, i);
  }
  else{
    /**/
    if(time){
      printf("## LEVEL 1 ##\n");
      //fprintf(stderr,"%.6lf\n", time_stop(t_start, c_start));
      time_stop(t_start, c_start);
      time_start(&t_start, &c_start);
    }
    /**/
    
    //LEVEL 2
    
    //printf("INPUT: %s\t%s\n", c_lcp_multi, c_size_multi);
    if(verbose)
      printf("%dx%zu\t\n", 1, blocks);
    
    f_size = file_open(c_size_multi, "rb");//header file
    h = heap_alloc(blocks, c_lcp_multi, level, pos_size, lcp_size);
    seek=0;
    
    while(fread(&size, sizeof(size_t), 1, f_size)){
      #if DEBUG
        printf("%zu (%zu): ", size, seek);
      #endif
      heap_insert(h, seek); //size == seek
      seek+=size;
    }
    
    fclose(f_size);
  }
  
  sprintf(c_lcp, "%s.%d.lcp", c_file, lcp_size);//linal
  f_lcp = file_open(c_lcp, "wb");
    
  size_t total=0;
  
  #if CHECK == 1
    if(heap_sort_level(h, f_lcp, &total, c_file, level)) printf("isSorted!!\n");
    else printf("isNotSorted!!\n");
  #else
    heap_sort_level(h, f_lcp, &total, c_file, level);
  #endif
  
  printf("%zu\n", total);
  
  heap_free(h, f_lcp, level);
  fclose(f_lcp);
  
  /**/
  if(time){
    printf("## LEVEL 2 ##\n");
    time_stop(t_start, c_start);
  }
  /**/
  
  if(time){
    printf("## TOTAL ##\n");
    fprintf(stderr,"%.6lf\n", time_stop(t_total, c_total));
  }
  
  printf("OUTPUT:\t%s\n", c_lcp);
  
  remove(c_lcp_multi);
  remove(c_size_multi);
  
  //checking
  #if CHECK == 2
    printf("CHECK:\t");
    char c_pos[500];
    sprintf(c_pos, "%s.pos.lcp", c_file);
    FILE* f_pos = file_open(c_pos, "rb");
    f_lcp = file_open(c_lcp, "rb");
    uint64_t pos=0,lcp=0;
    int true=1; 
    size_t previous=0;
    sum=0;
    while(fread(&pos, pos_size, 1, f_pos)){
      fread(&lcp, lcp_size, 1, f_lcp);
      #if DEBUG
        printf("<%lu, %lu> ", pos, lcp);
      #endif
      if(previous>pos){
        true=0; break;
      }
      previous=pos;
      sum++;
    }
    printf("\n");
    if(true) printf("isSorted!! (%zu)\n", sum);
    else printf("isNotSorted!!\n");

    fclose(f_pos);
    fclose(f_lcp);
  #endif

	/**/
  sprintf(c_lcp, "%s.pair.lcp",  c_file);
  sprintf(c_size, "%s.size.lcp", c_file);

	remove(c_lcp);
	remove(c_size);
	/**/


return 0;
}

