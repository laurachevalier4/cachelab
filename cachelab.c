/* 
 * cachelab.c - A cache simulator that can replay traces of memory accesses
 *     and output statistics such as number of hits, misses, and
 *     evictions.  The replacement policy is LRU.
 *
 * Implementation and assumptions:
 *  1. Each load/store can cause at most one cache miss. (I examined the trace,
 *  the largest request I saw was for 8 bytes).
 *  2. Instruction loads (I) are ignored.
 *  3. data modify (M) is treated as a load followed by a store to the same
 *  address. Hence, ****an M operation can result in two cache hits, or a miss and a
 *  hit plus a possible eviction****. 
 */
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

//#define DEBUG_ON 
#define ADDRESS_LENGTH 64

/* Type: Memory address */
typedef unsigned long long int mem_addr_t;

/* A single cache block with pointer to prev and next block (doubly linked list), with tag and valid fields */
typedef struct block
	{
	  int tag;
	  int valid;
	  struct block *next;
	  struct block *prev;
	} cache_block;

	/* struct for set, made up of blocks */
typedef struct set
	{
	  cache_block *LRU_head; // for each set, keep track of a list of LRU blocks (with the most recently used at the head)
	  cache_block *LRU_tail; 
	} cache_set;

	/* struct for cache, made up of sets */
typedef struct cache
{
  cache_set *sets;
} my_cache;

/**************************************************************************/
/* Declarations of the functions currently implemented in this file */

/* 
 * printSummary - This function provides a standard way for your cache
 * simulator * to display its final hit and miss statistics
 */ 
void printSummary(int hits,  /* number of  hits */
				  int misses, /* number of misses */
				  int evictions); /* number of evictions */

/*
 * replayTrace - replays the given trace file against the cache 
 */
void replayTrace(char* trace_fn);

/* 
 * accessData - Access data at memory address addr. 
 * This is the one that you need to implement.
 */
void accessData(mem_addr_t addr);

/*
 * printUsage - Print usage info
 */
void printUsage(char* argv[]);

void printList(cache_block *head, int set);
/**************************************************************************/


/**************************************************************************/
/* Declarations of the global variable currently used. */
/* Do NOT modify any of the following globals. 
 * But you can add your own globals if you want 
 */
/* Globals set by command line args */
int s = 0; /* set index bits */
int b = 0; /* block offset bits */
int E = 0; /* associativity */
char* trace_file = NULL;
my_cache c1;

/* Counters used to record cache statistics */
int miss_count = 0;     /* cache miss */
int hit_count = 0;      /* cache hit */
int eviction_count = 0; /* A block is evicted from the cache */
int *size;
int set_index;
/**************************************************************************/


/* 
 * accessData - Access data at memory address addr.
 *   If it is already in cache, increast hit_count
 *   If it is not in cache, bring it in cache, increase miss count.
 *   Also increase eviction_count if a line is evicted.
 * 
 *   If you need to evict a line (i.e. block) apply least recently used 
 *   replacement policy.
 */
void accessData(mem_addr_t addr)
{  
  // 1) search for data in cache by going to correct set, c1->sets[set_index] 
  // 2) search for correct block by comparing the tag of each block in the set to the tag in the given address; ALSO make sure that valid bit is set
  // 3) if the data exists already, increase hit count and adjust c1->LRU_head (move the block from its position in the linked list to the head)
  // 4) if the data does not exist or it exists but is invalid, increase the miss count and put the new block at the head of the LRU list, kicking out the tail if set full (increase the eviction count) and changing valid bit to 0
 
  set_index = (addr >> b) & ((1<<s) - 1);
  int tag = (addr >> (s + b)); //get just the tag bits
  cache_block *curr = c1.sets[set_index].LRU_head;
  
  // loop through c1->sets[set_index] and compare the tag in the given addr to the tag in each block of the set
  while (curr->next != NULL && curr->tag != tag)
  {
    // loop through the linked list of blocks in the set until the tag is found
    curr = curr->next;
  }

  if (curr->tag == tag && curr->valid == 1) // the correct data is already in the cache, so delete and reinsert
  { 
    hit_count++; // increase the hit count
    if (size[set_index] > 1)
    {
      if (curr->prev != NULL) // if the hit is the head, we don't need to do anything
      {
        if (curr->next != NULL)
        {
          curr->prev->next = curr->next; // delete the block
          curr->next->prev = curr->prev;
          curr->next = c1.sets[set_index].LRU_head; // and put it at the head
          curr->prev = NULL;
          c1.sets[set_index].LRU_head = curr;
        }
        else if (curr->next == NULL)
        {
          curr->prev->next = NULL;
          c1.sets[set_index].LRU_tail =  curr->prev;
          curr->prev = NULL;
          c1.sets[set_index].LRU_head->prev = curr;
          curr->next = c1.sets[set_index].LRU_head;
          c1.sets[set_index].LRU_head = curr;
        }
      } 
      // no change in set size
    }
  }
  else // the correct data is not in the cache, so make a new block and insert into the cache and update LRU list
  {
    if (size[set_index] == 0) // when set empty
    {
      curr->tag = tag;
      curr->valid = 1;
      c1.sets[set_index].LRU_head = curr;
      c1.sets[set_index].LRU_tail = curr;
      size[set_index]++;
    }
    else // set not empty
    {
      if (size[set_index] >=  E) // if the number of blocks in the set would exceed the associativity, delete the tail
      {
        cache_block *temp = c1.sets[set_index].LRU_tail;
        c1.sets[set_index].LRU_tail = c1.sets[set_index].LRU_tail->prev;
	free(temp);
        c1.sets[set_index].LRU_tail->next = NULL;
        eviction_count++;
        size[set_index]--;
      }
      // insert a new block before the head
      cache_block *temp = (cache_block *)malloc(sizeof(cache_block));
      if(!temp) printf("error in allocating block");
      c1.sets[set_index].LRU_head->prev = temp;
      temp->tag = tag;
      temp->next = c1.sets[set_index].LRU_head; 
      temp->prev = NULL;
      temp->valid = 1; // set valid bit to 1
      c1.sets[set_index].LRU_head = temp; // make b the new head of the list, pointing to the previous head
      size[set_index]++; // without this line, I don't get a segfault; hit count and miss count are correct
    }
    miss_count++;
  } 
} // end of accessData


void printList(cache_block *head, int set)
{
  printf("LIST of set %d: ", set);
  while(head->valid == 1)
  {
    printf("tag : %d -> ", head->tag);
    if (head->next == NULL) break;
    head = head->next;
  }
}

/*
 * main - Main routine 
 */
int main(int argc, char* argv[])
{
    char c;
    //printf("here");
    /* Do NOT modify anything from this point till the following comment */
    while( (c=getopt(argc,argv,"s:E:b:t:vh")) != -1){
        switch(c){
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            trace_file = optarg;
            break;
        case 'h':
            printUsage(argv);
            exit(0);
        default:
            printUsage(argv);
            exit(1);
        }
    }

    /* Make sure that all required command line args were specified */
    if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        printUsage(argv);
        exit(1);
    }
    /* From here you can make any modification, 
     * except removing the call to replayTrace */

    /* My code */

    /* Need to do this computation once we actually have s... */
	int num_sets = pow(2.0, s); // num_sets is number of sets
        
	/* Initialize cache, given global variables */
	c1.sets = (cache_set*)malloc(sizeof(cache_set) * num_sets); //initialize cache with 2^s sets
	size = (int *)malloc(sizeof(int) * num_sets);
	int i;
	for (i = 0; i < num_sets; i++)
	{
	  size[i] = 0;
	}

	/* Initialize each set with number of blocks per set (E) */
	for (i = 0; i < num_sets; i++)
	{
	  c1.sets[i].LRU_head = (cache_block *)malloc(sizeof(cache_block)); // initialize each set with a head block
	  c1.sets[i].LRU_head->valid = 0;
	  c1.sets[i].LRU_head->tag = -1;
	  c1.sets[i].LRU_head->next = NULL;
	  c1.sets[i].LRU_head->prev = NULL;
	  // will need to malloc for each node added to the list
	}
    
    /* Do not remove this line as it is the one calls your cache access function */
    replayTrace(trace_file);
    
  
    /* Do not modify anything from here till end of main() function */
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}


/****** Do NOT modify anything below this point ******/

/* 
 * printSummary - Summarize the cache simulation statistics. Student cache simulators
 *                must call this function in order to be properly autograded. 
 */
void printSummary(int hits, int misses, int evictions)
{
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
    
}

/*
 * printUsage - Print usage info
 */
void printUsage(char* argv[])
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of blocks per set (i.e. associativity).\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t ls.trace\n", argv[0]);
    exit(0);
}

/*
 * replayTrace - replays the given trace file against the cache 
 */
void replayTrace(char* trace_fn)
{
    char buf[1000];
    mem_addr_t addr=0;
    unsigned int len=0;
    FILE* trace_fp = fopen(trace_fn, "r");

    if(!trace_fp){
        fprintf(stderr, "%s: %s\n", trace_fn, strerror(errno));
        exit(1);
    }

    while( fgets(buf, 1000, trace_fp) != NULL) {
        if(buf[1]=='S' || buf[1]=='L' || buf[1]=='M') {
            sscanf(buf+3, "%llx,%u", &addr, &len);

            accessData(addr);

            /* If the instruction is R/W then access again */
            if(buf[1]=='M')
                accessData(addr);
            
        }
    }

    fclose(trace_fp);
}
