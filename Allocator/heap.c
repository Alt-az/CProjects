#include "heap.h"
#include "tested_declarations.h"
#include "rdebug.h"

int heap_setup(void){
    memoryManager.memory_start = custom_sbrk(PAGE_SIZE);
    if(memoryManager.memory_start==NULL){
        memoryManager.flaga=-1;
        return -1;
    }
    memoryManager.first_memory_chunk = (struct memory_chunk_t*)memoryManager.memory_start;
    struct memory_chunk_t* current = memoryManager.first_memory_chunk;
    void * end = custom_sbrk(0);
    memoryManager.memory_size = (unsigned char*)end - (unsigned char*)memoryManager.memory_start;
    current->size = 0;
    current->free=0;
    current->prev = NULL;
    current->next = (struct memory_chunk_t*)((unsigned char*)current + CHUNKSIZE);
    current->flaga = sumControl(current);
    current->next->prev = current;
    current = current->next;
    current->free=1;
    current->size = (int)(memoryManager.memory_size - CHUNKSIZE * 3);
    current->next = (struct memory_chunk_t*)((unsigned char*)current + CHUNKSIZE + current->size);
    current->next->prev = current;
    current->flaga = sumControl(current);
    current = current->next;
    current->size = 0;
    current->free=0;
    current->next = NULL;
    current->flaga = sumControl(current);
    memoryManager.flaga=HEAPVALID;
    return 0;
}
void heap_clean(void){
    custom_sbrk(-(long)memoryManager.memory_size);
    memoryManager.flaga=-1;
}
void* heap_malloc(size_t size){
    if(size<=0){
        return NULL;
    }
    struct memory_chunk_t* current=memoryManager.first_memory_chunk;
    while(current->next!=NULL){
        if(current->size>=ALIGN(size+FENCE_LEN*2) && current->free==1){
            break;
        }
        current=current->next;
    }
    if(current->next==NULL){
        long ile=(((long)(size+FENCE_LEN*2+CHUNKSIZE)/4096)+1)*4096;
        void * s=custom_sbrk(ile);
        if(s==(void*)-1){
            return NULL;
        }
        memoryManager.memory_size+=ile;
        if(current->prev->free==1){
            current=current->prev;
        }
        current->size=current->size+(ile-CHUNKSIZE);
        current->free=1;
        current->next=(struct memory_chunk_t*)((unsigned char*)(current)+current->size+CHUNKSIZE);
        current->next->prev=current;
        current->flaga= sumControl(current);
        current->next->next=NULL;
        current->next->size=0;
        current->next->free=0;
        current->next->flaga= sumControl(current->next);
    }
    unsigned long actualsize= ALIGN(size+FENCE_LEN*2);
    unsigned long sizeleft;
    unsigned long currentsize=current->size;
    if(currentsize>actualsize) {
        sizeleft = current->size - actualsize;
        if (sizeleft >= CHUNKSIZE) {
            struct memory_chunk_t *next = current->next;
            current->next = (struct memory_chunk_t *) ((unsigned char *) current + actualsize + CHUNKSIZE);
            current->next->next = next;
            next->prev=current->next;
            next->flaga= sumControl(next);
            current->next->size = sizeleft - CHUNKSIZE;
            current->next->free = 1;
            current->next->prev = current;
            current->next->flaga = sumControl(current->next);
        }
    }
    current->size=size;
    current->free=0;
    current->flaga= sumControl(current);
    unsigned char* plotki = (unsigned char*)current;
    plotki+=CHUNKSIZE;
    memset(plotki,'#',FENCE_LEN);
    plotki+=current->size+FENCE_LEN;
    memset(plotki,'#',FENCE_LEN);
    return (void*)((unsigned char*)current+CHUNKSIZE+FENCE_LEN);
}
void* heap_calloc(size_t number, size_t size){
    if(number<=0 || size<=0){
        return NULL;
    }
    void * wsk=heap_malloc(size*number);
    unsigned char* ptr=(unsigned char*)wsk;
    if(ptr==NULL){
        return NULL;
    }
    size_t ile=(size*number);
    memset(ptr,0,ile);
    return (void*)wsk;
}
void* heap_realloc(void* memblock, size_t count){
    if(heap_validate()!=0 || memoryManager.flaga!=HEAPVALID){
        return NULL;
    }
    if(memblock==NULL){
        return heap_malloc(count);
    }
    if(get_pointer_type(memblock)!=pointer_valid){
        return NULL;
    }
    struct memory_chunk_t* current=(struct memory_chunk_t*)((unsigned char*)memblock-FENCE_LEN-CHUNKSIZE);
    size_t potentsize=((unsigned char*)current->next-((unsigned char*)current+CHUNKSIZE));//odleglosc do nastepnego bloku
    size_t potentsize2=((unsigned char*)current->next->next-((unsigned char*)current+CHUNKSIZE));//odleglosc do konca nexta
    size_t potentsize3=((unsigned char*)current->next-((unsigned char*)current->prev+CHUNKSIZE));//odleglosc od poprzedniego do konca currenta
    size_t potentsize4=((unsigned char*)current->next->next-((unsigned char*)current->prev+CHUNKSIZE));//odleglosc od poprzedniego do konca nexta
    if(count==0){
        heap_free(memblock);
        return NULL;
    }
    else if(count==current->size){
        return (void*)((unsigned char*)current+CHUNKSIZE+FENCE_LEN);
    }
    else if(count<current->size) {
        unsigned long actualsize = ALIGN(count + FENCE_LEN * 2);
        if (potentsize > actualsize) {
            unsigned long sizeleft = potentsize - actualsize;
            if (sizeleft >= CHUNKSIZE) {
                struct memory_chunk_t *next = current->next;
                current->next = (struct memory_chunk_t *) ((unsigned char *) current + actualsize + CHUNKSIZE);
                current->next->size = sizeleft - CHUNKSIZE;
                if (next->free == 1) {
                    current->next->size += CHUNKSIZE + next->size;
                    current->next->next=next->next;
                    next->next->prev=current->next;
                    current->next->next->flaga= sumControl(current->next->next);
                }
                else{
                    current->next->next = next;
                    next->prev = current->next;
                    next->flaga = sumControl(next);
                }
                current->next->free = 1;
                current->next->prev = current;
                current->next->flaga = sumControl(current->next);
            }
        }
        current->size = count;
        current->flaga = sumControl(current);
        unsigned char *ptr = (unsigned char *) current + CHUNKSIZE + FENCE_LEN + current->size;
        memset(ptr, '#', FENCE_LEN);
        return (void *) ((unsigned char *) current + CHUNKSIZE + FENCE_LEN);
    }
    else if((potentsize-2*FENCE_LEN)>=count){
        current->size=count;
        current->flaga= sumControl(current);
        unsigned char* ptr=(unsigned char*)current+CHUNKSIZE+FENCE_LEN+current->size;
        memset(ptr,'#',FENCE_LEN);
        return (void*)((unsigned char*)current+CHUNKSIZE+FENCE_LEN);
    }
    else if(current->next->free==1 && (potentsize2-2*FENCE_LEN)>=count){
        current->size=count;
        unsigned long actualsize = ALIGN(count + FENCE_LEN * 2);
        if(potentsize2>actualsize) {
            unsigned long sizeleft = potentsize2 - actualsize;
            if (sizeleft >= CHUNKSIZE) {
                struct memory_chunk_t *next = current->next->next;
                current->next = (struct memory_chunk_t *) ((unsigned char *) current + actualsize + CHUNKSIZE);
                current->next->next = next;
                next->prev=current->next;
                next->flaga= sumControl(next);
                current->next->size = sizeleft - CHUNKSIZE;
                current->next->free = 1;
                current->next->prev = current;
                current->next->flaga = sumControl(current->next);
            }
            else{
                current->next=current->next->next;
                current->next->prev=current;
                current->next->flaga= sumControl(current->next);
            }
        }
        else{
            current->next=current->next->next;
            current->next->prev=current;
            current->next->flaga= sumControl(current->next);
        }
        current->free=0;
        current->flaga= sumControl(current);
        unsigned char* ptr=(unsigned char*)current+CHUNKSIZE+FENCE_LEN+current->size;
        memset(ptr,'#',FENCE_LEN);
        return (void*)((unsigned char*)current+CHUNKSIZE+FENCE_LEN);
    }
    else if(current->prev->free==1 && (potentsize3-2*FENCE_LEN)>=count){
        struct memory_chunk_t* prev=current->prev;
        struct memory_chunk_t* next=current->next;
        prev->next=next;
        prev->size=((unsigned char*)next-((unsigned char*)prev+CHUNKSIZE));
        next->prev=prev;
        next->flaga= sumControl(next);
        current=prev;
        current->flaga= sumControl(current);
        unsigned long actualsize = ALIGN(count + FENCE_LEN * 2);
        if(potentsize3>actualsize) {
            unsigned long sizeleft = potentsize3 - actualsize;
            if (sizeleft >= CHUNKSIZE) {
                next = current->next;
                current->next = (struct memory_chunk_t *) ((unsigned char *) current + actualsize + CHUNKSIZE);
                current->next->next = next;
                next->prev=current->next;
                next->flaga= sumControl(next);
                current->next->size = sizeleft - CHUNKSIZE;
                current->next->free = 1;
                current->next->prev = current;
                current->next->flaga = sumControl(current->next);
            }
        }
        current->size=count;
        current->free=0;
        current->flaga= sumControl(current);
        unsigned char* ptr=(unsigned char*)current+CHUNKSIZE;
        memset(ptr,'#',FENCE_LEN);
        ptr+=current->size+FENCE_LEN;
        memset(ptr,'#',FENCE_LEN);
        return (void*)((unsigned char*)current+CHUNKSIZE+FENCE_LEN);
    }
    else if(current->prev->free==1 && current->next->free==1 && (potentsize4-2*FENCE_LEN)>=count){
        struct memory_chunk_t* prev=current->prev;
        struct memory_chunk_t* next=current->next;
        prev->next=next->next;
        prev->size=((unsigned char*)next->next-((unsigned char*)prev+CHUNKSIZE));
        current=prev;
        next=current->next;
        next->prev=current;
        next->flaga= sumControl(next);
        current->flaga= sumControl(current);
        unsigned long actualsize = ALIGN(count + FENCE_LEN * 2);
        if(potentsize4>actualsize) {
            unsigned long sizeleft = potentsize4 - actualsize;
            if (sizeleft >= CHUNKSIZE) {
                next = current->next;
                current->next = (struct memory_chunk_t *) ((unsigned char *) current + actualsize + CHUNKSIZE);
                current->next->next = next;
                next->prev=current->next;
                next->flaga= sumControl(next);
                current->next->size = sizeleft - CHUNKSIZE;
                current->next->free = 1;
                current->next->prev = current;
                current->next->flaga = sumControl(current->next);
            }
        }
        current->free=0;
        current->size=count;
        current->flaga= sumControl(current);
        unsigned char* ptr=(unsigned char*)current+CHUNKSIZE;
        memset(ptr,'#',FENCE_LEN);
        ptr+=FENCE_LEN+current->size;
        memset(ptr,'#',FENCE_LEN);
        return (void*)((unsigned char*)current+CHUNKSIZE+FENCE_LEN);
    }
    struct memory_chunk_t* newcurrent=memoryManager.first_memory_chunk;
    while(newcurrent->next!=NULL){
        if(newcurrent->free==1 && newcurrent->size >= ALIGN(count + FENCE_LEN * 2)){
            break;
        }
        newcurrent=newcurrent->next;
    }
    if(newcurrent->next==NULL) {
        long newsize=(long)count;
        if(current->next->next!=NULL && current->next->next->next==NULL){
            newsize-=(long)potentsize2;
        }
        long ile=(((long)(newsize+FENCE_LEN*2+CHUNKSIZE)/4096)+1)*4096;
        void * s=custom_sbrk(ile);
        if(s==(void*)-1){
            return NULL;
        }
        if(current->next->next!=NULL && current->next->next->next==NULL){
            current->size=potentsize2;
            current->next=current->next->next;
            current->next->prev=current;
        }
        else{
            unsigned char* ptrchrr=(unsigned char*)newcurrent+CHUNKSIZE;
            memset(ptrchrr,'#',FENCE_LEN);
            ptrchrr+=FENCE_LEN;
            memcpy(ptrchrr,(unsigned char*)memblock,current->size);
            heap_free(memblock);
            current=newcurrent;
        }
        memoryManager.memory_size+=ile;
        current->size=current->size+(ile-CHUNKSIZE);
        current->free=1;
        current->next=(struct memory_chunk_t*)((unsigned char*)(current)+current->size+CHUNKSIZE);
        current->next->prev=current;
        current->next->next=NULL;
        current->next->size=0;
        current->next->free=0;
        current->next->flaga= sumControl(current->next);
        unsigned long actualsize = ALIGN(count + FENCE_LEN * 2);
        if (current->size > actualsize) {
            unsigned long sizeleft = current->size - actualsize;
            if (sizeleft >= CHUNKSIZE) {
                struct memory_chunk_t *next = current->next;
                current->next = (struct memory_chunk_t *) ((unsigned char *) current + actualsize + CHUNKSIZE);
                current->next->next = next;
                next->prev = current->next;
                next->flaga = sumControl(next);
                current->next->size = sizeleft - CHUNKSIZE;
                current->next->free = 1;
                current->next->prev = current;
                current->next->flaga = sumControl(current->next);
            }
        }
        current->size = count;
        current->free = 0;
        current->flaga= sumControl(current);
        unsigned char* ptrchr=(unsigned char*)current+CHUNKSIZE+FENCE_LEN;
        ptrchr+=current->size;
        memset(ptrchr,'#',FENCE_LEN);
        return (void *) ((unsigned char *) current + CHUNKSIZE + FENCE_LEN);
    }
    unsigned long actualsize = ALIGN(count + FENCE_LEN * 2);
    if (newcurrent->size > actualsize) {
        unsigned long sizeleft = newcurrent->size - actualsize;
        if (sizeleft >= CHUNKSIZE) {
            struct memory_chunk_t *next = newcurrent->next;
            newcurrent->next = (struct memory_chunk_t *) ((unsigned char *) newcurrent + actualsize + CHUNKSIZE);
            newcurrent->next->next = next;
            next->prev = newcurrent->next;
            next->flaga = sumControl(next);
            newcurrent->next->size = sizeleft - CHUNKSIZE;
            newcurrent->next->free = 1;
            newcurrent->next->prev = newcurrent;
            newcurrent->next->flaga = sumControl(newcurrent->next);
        }
    }
    newcurrent->size = count;
    newcurrent->free=0;
    newcurrent->flaga = sumControl(newcurrent);
    unsigned char *ptr = (unsigned char *) newcurrent + CHUNKSIZE;
    memset(ptr, '#', FENCE_LEN);
    ptr+=FENCE_LEN+newcurrent->size;
    memset(ptr, '#', FENCE_LEN);
    heap_free(memblock);
    return (void *) ((unsigned char *) newcurrent + CHUNKSIZE + FENCE_LEN);
}
void heap_free(void* memblock){
    if(memblock==NULL || get_pointer_type(memblock)!=pointer_valid || heap_validate()!=0){
        return;
    }
    struct memory_chunk_t* current=(struct memory_chunk_t*)((unsigned char*)memblock-FENCE_LEN-CHUNKSIZE);
    struct memory_chunk_t* prev;
    struct memory_chunk_t* next;
    current->free=1;
    current->size=((unsigned char*)current->next-(unsigned char*)current-CHUNKSIZE);
    current->flaga= sumControl(current);
    while(current->prev->free == 1 || current->next->free == 1) {
        prev=current->prev;
        next=current->next;
        if (prev->free == 1 && next->free == 1) {
            prev->next=next->next;
            prev->size=((unsigned char*)next->next-((unsigned char*)prev+CHUNKSIZE));
            current=prev;
            next=current->next;
            next->prev=current;
            next->flaga= sumControl(next);
            current->flaga= sumControl(current);
        } else if (prev->free == 1) {
            prev->next=next;
            prev->size=((unsigned char*)next-((unsigned char*)prev+CHUNKSIZE));
            next->prev=prev;
            next->flaga= sumControl(next);
            current=prev;
            current->flaga= sumControl(current);
        } else if (next->free == 1) {
            struct memory_chunk_t* newnext=next->next;
            current->next=newnext;
            current->size=((unsigned char*)newnext-((unsigned char*)current+CHUNKSIZE));
            next=current->next;
            next->prev=current;
            next->flaga= sumControl(next);
            current->flaga= sumControl(current);
        }
    }
}
int heap_validate(void){
    if(memoryManager.flaga!=HEAPVALID){
        return 2;
    }
    struct memory_chunk_t* current=memoryManager.first_memory_chunk;
    unsigned char* ptr;
    unsigned char plotki[]="########";
    while(current!=NULL){
        if(current->flaga != sumControl(current) || current->size>33554432 || ((unsigned char*)current>(unsigned char*)current->next && current->next!=NULL)){
            return 3;
        }
        if(current->next!=NULL && current->next->prev!=current){
            return 3;
        }
        if(current->free==0 && current->size!=0){
            ptr=(unsigned char*)current+CHUNKSIZE;
            int w1=memcmp(ptr,plotki,FENCE_LEN);
            ptr+=current->size+FENCE_LEN;
            int w2=memcmp(ptr,plotki,FENCE_LEN);
            if(w1!=0 || w2!=0){
                return 1;
            }
        }
        current=current->next;
    }
    return 0;
}
size_t heap_get_largest_used_block_size(void){
    if(memoryManager.flaga!=HEAPVALID){
        return 0;
    }
    if(heap_validate()!=0){
        return 0;
    }
    struct memory_chunk_t* current=memoryManager.first_memory_chunk;
    size_t max=0;
    while(current!=NULL){
        if(current->size>max && current->free==0){
            max=current->size;
        }
        current=current->next;
    }
    return max;
}
enum pointer_type_t get_pointer_type(const void* const pointer){
    if(pointer==NULL){
        return pointer_null;
    }
    if(heap_validate()!=0){
        return pointer_heap_corrupted;
    }
    void* ptr=(unsigned char*)pointer;
    void* end=(void*)((unsigned char*)memoryManager.memory_start+memoryManager.memory_size);
    if(ptr<memoryManager.memory_start || ptr>end){
        return pointer_unallocated;
    }
    struct memory_chunk_t* current=(struct memory_chunk_t*)memoryManager.first_memory_chunk;
    unsigned char* ptrch=(unsigned char*)ptr;
    unsigned char* beg=(unsigned char*)current;
    unsigned char* ends=(unsigned char*)current->next;
    if(ptrch>beg && ptrch<ends){
        return pointer_unallocated;
    }
    current=current->next;
    while(current->next!=NULL){
        beg=(unsigned char*)current;
        ends=(unsigned char*)current->next;
        if(ptrch>=beg && ptrch<=ends){
            beg+=CHUNKSIZE;
            if(ptrch<beg || ptrch==ends){
                return pointer_control_block;
            }
            if(current->free==1){
                return pointer_unallocated;
            }
            beg+=FENCE_LEN;
            if(ptrch<beg){
                return pointer_inside_fences;
            }
            if(ptrch==beg){
                return pointer_valid;
            }
            beg+=current->size;
            if(ptrch<beg){
                return pointer_inside_data_block;
            }
            beg+=FENCE_LEN;
            if(ptrch<beg){
                return pointer_inside_fences;
            }
            return pointer_unallocated;
        }
        current=current->next;
    }
    beg=(unsigned char*)current;
    ends=(unsigned char*)end;
    if(ptrch>beg && ptrch<ends){
        return pointer_unallocated;
    }
    return pointer_valid;
}
int sumControl(struct memory_chunk_t* memoryChunk){
    if(memoryChunk==NULL){
        return 0;
    }
    int suma=0;
    unsigned char* t=(unsigned char*)memoryChunk;
    for(int i=0;i<28;i++,t++){
        suma+=*t;
    }
    return suma;
}

