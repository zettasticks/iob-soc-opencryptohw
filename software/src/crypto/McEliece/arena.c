#include "arena.h"

#include <stdlib.h>
#include <string.h>

#include "printf.h"

//Arena globalArenaInst = {};
Arena* globalArena = NULL;

Arena InitArena(int size){
  Arena arena = {};
  arena.ptr = (char*) malloc(size * sizeof(char));
  arena.allocated = size;

  return arena;
}

void* PushBytes(Arena* arena,int size){
  char* ptr = &arena->ptr[arena->used];

  size = (size + 3) & (~3); // Align to 4 byte boundary
  arena->used += size;

  if(arena->used > arena->allocated){
    printf("Arena overflow\n");
    printf("Size: %d,Used: %d, Allocated: %d\n",size,arena->used,arena->allocated);
  }

  return ptr;
}

void* PushAndZeroBytes(Arena* arena,int size){
  size = (size + 3) & (~3); // Align size
  char* ptr = PushBytes(arena,size);

  memset(ptr,0,size);
#if 0
  for (int i = 0; i < size; i++) {
    ptr[i] = 0;
  }
#endif

  int* asInt = (int*) ptr;
  for (int i = 0; i < size / 4; i++) {
    if(asInt[i] != 0){
      printf("Error on %d (%p): %d\n",i,&asInt[i],asInt[i]);
    }
  }

#if 0
  for (int i = 0; i < size; i++) {
    if(ptr[i] != 0){
      printf("Error on %d (%p): %d\n",i,&ptr[i],ptr[i]);
    }
  }
#endif

  return ptr;
}

int MarkArena(Arena* arena){
  return arena->used;
}

void PopArena(Arena* arena,int mark){
  arena->used = mark;
}
