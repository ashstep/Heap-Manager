//Ashka Stephen
/*
SOME NOTES
  header is 32 bytes due to boolean addition
  boolean is 4 bytes
*/


#include <stdio.h>  // needed for size_t
#include <unistd.h> // needed for sbrk
#include <assert.h> // needed for asserts
#include <limits.h> //int max val
#include "dmm.h"

/* You can improve the below metadata structure using the concepts from Bryant
 * and OHallaron book (chapter 9).
 */

typedef struct metadata {
  /* size_t is the return type of the sizeof operator. Since the size of an
   * object depends on the architecture and its implementation, size_t is used
   * to represent the maximum size of any object in the particular
   * implementation. size contains the size of the data object or the number of
   * free bytes
   */
  bool isTaken;  //NOTE I ADDED this, 0 indicates FREE and 1 is ALLOCATED
  size_t size;
  struct metadata* next;
  struct metadata* prev;
} metadata_t;

/* freelist -entire block- maintains all the blocks which are not in use; freelist is kept
 * sorted to improve coalescing efficiency
 */

static metadata_t* freelist = NULL;

void* dmalloc(size_t numbytes) {

  /* initialize through sbrk call first time*/
  if(freelist == NULL) {
    if(!dmalloc_init()){
      return NULL;
    }
  }

  assert(numbytes > 0);
  numbytes = ALIGN(numbytes);
  metadata_t* curr = freelist;
  metadata_t* adding_in_this_block = NULL;

  while(curr != NULL) {
    if( curr->size >= numbytes + METADATA_T_ALIGNED ) {
      if ( ( adding_in_this_block == NULL ) ||
      ( adding_in_this_block->size  > curr->size) ){
        adding_in_this_block = curr;
      }
    }
    curr = curr->next;
  }

  if(adding_in_this_block == NULL) {return NULL;}

  char* lastPointer = (char*) adding_in_this_block;
  metadata_t* newBlockPointer = (metadata_t*)(METADATA_T_ALIGNED + numbytes + lastPointer);

  //try deleting so the list only has free space
  newBlockPointer->size = adding_in_this_block->size - numbytes - METADATA_T_ALIGNED;
  newBlockPointer->isTaken = false;

  newBlockPointer->next = adding_in_this_block->next;
  newBlockPointer->prev = adding_in_this_block->prev;
  newBlockPointer->prev->next = newBlockPointer;
  newBlockPointer->next->prev = newBlockPointer;

  adding_in_this_block->size = numbytes;
  adding_in_this_block->isTaken = true;

  // NOTE: if parenthesis added, incorrect addition (adds header size); this returns pointer after the header (where data begins)
  return ((void*) adding_in_this_block + METADATA_T_ALIGNED);
}

/*  frees the requested block ptr and then merges adjacent free
blocks using the boundary-tags coalescing technique described in Section 9.9.11*/
void dfree(void* ptr) {
  // printf("  ==========================\n");
  // printf("  Dfree \n");
  metadata_t* newBlock = (metadata_t*) ((char*) ptr - METADATA_T_ALIGNED);
  metadata_t* curr = freelist;
  metadata_t* prevBlock = NULL;

  //iterate to find correct previous block -> setting in future
  //reconnect it into the list
  while( curr < ptr ) {
    prevBlock = curr;
    curr = curr->next;
  }

  //adding it into the loop
  newBlock->isTaken = false;
  newBlock->next = curr;
  newBlock->prev = prevBlock;
  newBlock->next->prev = newBlock;
  newBlock->prev->next = newBlock;

  //coalescing CASE 1
  if( !newBlock->next->isTaken ){
    if (  ( (void*)(newBlock->next) == ( newBlock->size + ptr ) )  ) {
      newBlock->size =  METADATA_T_ALIGNED + newBlock->size + newBlock->next->size; //size update
      newBlock->next = newBlock->next->next; // resetting pointers post-merge
      newBlock->next->prev = newBlock;
    }
  }
  //coalescing CASE TWO
  if( !newBlock->prev->isTaken ) {
    if ( ptr == ((void*)(newBlock->prev) + (2*METADATA_T_ALIGNED) + newBlock->prev->size) ) { //if they both point to same spot
      newBlock->prev->next = newBlock->next; // resetting pointers post-merge - disconnecting
      newBlock->prev->size = newBlock->prev->size + METADATA_T_ALIGNED + newBlock->size;
      newBlock->next->prev = newBlock->prev;
    }
  }
  //CASE THREE AND CASE FOUR ACCOUNTED FOR EARLIER (NEITHER IS FREE OR BOTH ARE FREE)
}

bool dmalloc_init() {

  /* Two choices:
   * 1. Append prologue and epilogue blocks to the start and the
   * end of the freelist
   *
   * 2. Initialize freelist pointers to NULL
   *
   * Note: We provide the code for 2. Using 1 will help you to tackle the
   * corner cases succinctly.
   */

  size_t max_bytes = ALIGN(MAX_HEAP_SIZE);
  /* returns heap_region, which is initialized to freelist */
  freelist = (metadata_t*) sbrk(max_bytes);
  /* Q: Why casting is used? i.e., why (void*)-1? ????*/
  if (freelist == (void *)-1)
    return false;

  metadata_t* prologue_block = freelist; //points to beginning of list

  prologue_block->next = (metadata_t*)( (char*) prologue_block + METADATA_T_ALIGNED);   //initial header
  prologue_block->prev = NULL;  //indicate start of the list
  prologue_block->size = 0;  //pointer should only skip metadata size  ->  0 size for block itself
  prologue_block->next->prev = prologue_block;  //set previous pointer for next block
  prologue_block->isTaken = true;  //cannot be used
  /*we use 3 METADATA_T_ALIGNED (prologue, epilogue, and inner free block) -> although inital size is max_bytes we subract these vals */
  prologue_block->next->size = max_bytes - (3 * METADATA_T_ALIGNED);

  /* Q: Why casting is used? i.e., why (void*)-1? ????*/
  char* epiloguePointer = (char*) freelist + max_bytes - METADATA_T_ALIGNED;
  metadata_t* epilogue_block = (metadata_t*) epiloguePointer;
  epilogue_block->next = NULL; //end block doesnt have a next
  epilogue_block->prev = prologue_block->next; //previous is what is after prologue (FREE SPACE)
  epilogue_block->size = 0; //as before, doesnt have a size
  epilogue_block->isTaken = true; //cannot be used

  prologue_block->next->next = epilogue_block;
  prologue_block->next->isTaken = false; //FREE space stored

  // PREVIOUSLY GIVEN CODE:
  // freelist->next = NULL;
  // freelist->prev = NULL;
  // freelist->size = max_bytes-METADATA_T_ALIGNED;
  // freelist->isTaken = false; //added this
  return true;
}
/* for debugging; can be turned off through -NDEBUG flag*/
void print_freelist() {
  metadata_t *freelist_head = freelist;
  while(freelist_head != NULL) {
    printf("\tFreelist Size:%zd, Taken?:%u, Head:%p, Prev:%p, Next:%p\t",
	  freelist_head->size,
    freelist_head->isTaken,
	  freelist_head,
	  freelist_head->prev,
	  freelist_head->next);
    //addede THIS
    // if (freelist_head->next!=NULL) {
    //   if (!freelist_head->isTaken  && !freelist_head->next->isTaken) {
    //     printf("-----------HERE-----------\n");
    //   }
    //   }
    freelist_head = freelist_head->next;
  }
  printf("\n");
}
