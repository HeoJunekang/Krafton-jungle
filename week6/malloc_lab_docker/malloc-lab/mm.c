/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "5team",
    /* First member's full name */
    "Kang mingu",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "Kim mingi",
    /* Second member's email address (leave blank if none) */
    "kimmingi@gmail.com"};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7) // 8의 배수로 올림 하기, 8의 배수보다 1이라도 더 크면 7을 더햇을때 하위 4비트부터 값이 바뀜

#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // 올림한 값

/* Basic constants and macros */
#define WSIZE 4  //헤더/푸터 용 word
#define DSIZE 8
#define CHUNKSIZE (1<<12) //1을 12칸 옮김 = 4096바이트 (힙 확장할때 사용하는 기본크기)

#define MAX(x,y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc)) //할당표시

/*Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p)) //p값 얻기
#define PUT(p, val) (*(unsigned int *)(p) = (val)) //p값 value로 수정

/*Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)     //  블록 크기만 보기(블록 전체 헤더 푸터까지)
#define GET_ALLOC(p) (GET(p) & 0x1)     // 할당표시 보기

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE) //HeaderPointer 4바이트 뒤로 이동 (char *로 캐스팅 이유 = 1바이트씩 이동하기 위해서)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //FooterPointer = 시작주소 + 전체 블록크기 - 8(헤더랑 푸터 빼기)



/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //다음 블록 시작주소
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //이전 푸터 시작주소에서 getsize = 이전 블록의 크기 알 수 있음 => 이전 블록 시작주소 



static void *heap_listp;
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static  void *extend_heap(size_t words);

//explicit
#define NEXT_FBLKP(bp) (*(char **)(FTRP(bp)-8)) //다음 가용리스트 bp의 주소
static void *free_heap_listp;
static void *insert_free_heaplist(void *bp);
void *delete_free_heaplist(void *bp);
static int size;

/*
 * mm_init - initialize the malloc package.
 */


int mm_init(void)
{

    /* Create the intial empty heap */ 
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) // mem_sbrk = 힙공간 확장 이전 brk 리턴
        return -1;
    PUT(heap_listp, 0); /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /*prologue header*/
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /*prologue footer*/
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));  /*epilogue header*/
    heap_listp += (2*WSIZE);  //
    size = 0;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;

}

 static  void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; //짝수에 4바이트를 곱함 그만큼 할당 8바이트 배수
    if((long)(bp = mem_sbrk(size)) == -1) // 힙공간 size만큼 확장하고 시작주소 반환
        return NULL;

    /* INitialize free block header/footer and the eilogue header */
    PUT(HDRP(bp),PACK(size, 0)); //header
    PUT(FTRP(bp),PACK(size, 0)); //footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); //다음 블록의 헤더 전

    /* Calesce if the previous block was free */
    return coalesce(bp);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}
//*******************explicit*****************************
void *insert_free_heaplist(void *bp)
{
    if(size == 0){ //가용리스트 하나도 없을때
        free_heap_listp = bp; //가용리스트 처음 갱신
        *(char **)free_heap_listp = NULL; 
        *(char **)(FTRP(bp)-8) = NULL;// 이전, 다음 블록 NULL로 만들기
    }else{
        *(char **)(FTRP(bp)-8) = free_heap_listp; //새 가용리스트 다음포인터에 이전 가용리스트 주소 넣기
        *(char **)free_heap_listp = bp;//이전 가용리스트 이전포인터에 새 가용리스트 넣기
        *(char **)bp = NULL; //새 가용리스트 이전 포인터 비우기
        free_heap_listp = bp; //가용리스트 처음 주소 갱신
    }
    size += 1; // 가용리스트 size 증가
    return bp;
}

void *delete_free_heaplist(void *bp){
    if(*(char **)bp == NULL && *(char **)(FTRP(bp)-8) == NULL){ //둘 다 없음
        free_heap_listp = NULL;
    }else if(*(char **)bp == NULL){ //이전 가용리스트가 없음
        free_heap_listp = *(char **)(FTRP(bp)-8);
        *(char **)free_heap_listp = NULL;
    }else if(*(char **)(FTRP(bp)-8) == NULL){ //다음 가용리스트가 없음
        char *temp = *(char **)bp;
        *(char **)(FTRP(temp)-8) = NULL;
    }else{ // 다 있음
        char *pretemp = *(char **)bp; //이전 블록
        char *nextTemp = *(char **)(FTRP(bp)-8);
        *(char **)(FTRP(pretemp)-8) = nextTemp; //이전블록의 다음 블록 갱신
        *(char **)nextTemp = pretemp;
    }
    size -= 1;
    return bp;
}


static void *coalesce(void *bp) // 앞뒤의 가용한 블록에 대해서 연결 후 새로운블록의 시작주소 리턴
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록 할당여부
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록 할당여부
    size_t size = GET_SIZE(HDRP(bp)); //현재 블록의 크기
    

    if(prev_alloc && next_alloc) { //둘다 할당되어있는 경우
        
    }

    else if(prev_alloc && !next_alloc){ //다음은 free
        delete_free_heaplist(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); //size  값 갱신
        PUT(HDRP(bp), PACK(size, 0)); //헤더에 덮어씌우기 , 할당x
        PUT(FTRP(bp), PACK(size, 0)); // 헤더에 사이즈 만큼 이동 후 footer값 갱신 즉, 새로운 블록 footer 갱신, 할당x
    }

    else if(!prev_alloc && next_alloc){ //이전이 free
        delete_free_heaplist(PREV_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0)); // footer 갱신
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); //헤더정보 갱신
        bp = PREV_BLKP(bp); //갱신된 블록의 시작지점으로 bp 갱신
    }

    else{ //둘다 free인 경우
        delete_free_heaplist(NEXT_BLKP(bp));
        delete_free_heaplist(PREV_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert_free_heaplist(bp);
    return bp; //새로 갱신한 블록의 시작주소(헤더 앞)
}

static void *recoalesce(void *bp) // 앞뒤의 가용한 블록에 대해서 연결 후 새로운블록의 시작주소 리턴
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록 할당여부
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록 할당여부
    size_t size = GET_SIZE(HDRP(bp)); //현재 블록의 크기
    

    if(prev_alloc && next_alloc) { //둘다 할당되어있는 경우
        
    }

    else if(prev_alloc && !next_alloc){ //다음은 free
        delete_free_heaplist(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); //size  값 갱신
        PUT(HDRP(bp), PACK(size, 1)); //헤더에 덮어씌우기 , 할당x
        PUT(FTRP(bp), PACK(size, 1)); // 헤더에 사이즈 만큼 이동 후 footer값 갱신 즉, 새로운 블록 footer 갱신, 할당x
    }

    else if(!prev_alloc && next_alloc){ //이전이 free
        delete_free_heaplist(PREV_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 1)); // footer 갱신
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 1)); //헤더정보 갱신
        bp = PREV_BLKP(bp); //갱신된 블록의 시작지점으로 bp 갱신
    }

    else{ //둘다 free인 경우
        delete_free_heaplist(NEXT_BLKP(bp));
        delete_free_heaplist(PREV_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 1));
        bp = PREV_BLKP(bp);
    }
    return bp; //새로 갱신한 블록의 시작주소(헤더 앞)
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) //재할당하고싶은 블록 포인터, 설정하고 싶은 크기
{
    void *oldptr_fd = ptr; //블록 포인터 저장(데이터 읽기용)
    void *oldptr = ptr; //블록 포인터 저장
    void *newptr; // 새로운 포인터 선언
    size_t copySize; // 기존 크기 

    if(size == 0)
        return NULL;

    //copySize = *(size_t *)((char *)oldbp - SIZE_T_SIZE); //기존 size
    copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;
    if(size <= copySize){
        oldptr = recoalesce(ptr);
    }

    copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;
    if(size <= copySize){
        //줄인만큼 메모리 복사
        copySize = size;
        newptr = oldptr;
        memcpy(newptr, oldptr_fd, copySize); //어차피 해당 블록의 크기는 바뀌지 않고 데이터 양만 줄음
        size_t asize;
        //받은 size 정제과정
        if(size <= DSIZE)
            asize = 3*DSIZE; 
        else
            asize = DSIZE * ((size + (DSIZE)+ (DSIZE-1)) / DSIZE); 

        if(copySize > asize +16){ // 기존 size가 정제된 사이즈를 빼도 24바이트보다 클때 분할
            PUT(HDRP(oldptr), PACK(asize, 1));
            PUT(FTRP(oldptr), PACK(asize, 1));
            PUT(HDRP(NEXT_BLKP(oldptr)), PACK(copySize - asize, 0));
            PUT(FTRP(NEXT_BLKP(oldptr)), PACK(copySize - asize, 0));
            insert_free_heaplist(NEXT_BLKP(oldptr));
        }
        return newptr;
    }
    // 더 늘리는 경우
    newptr = mm_malloc(size); //size만큼 재 할당 -> 가용블록 리스트 내에서 찾고 없으면 재할당
    if (newptr == NULL)// 할당 불가능 NULL반환
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);//기존 데이터 크기
    if (size < copySize)
        copySize = size; //더작은것을 골라야 실제로 할당된 데이터를 다 읽으므로 아니면 잘리거나, 더 크게 읽으면 없는 데이터도 읽게됨
    memcpy(newptr, oldptr_fd, copySize);
    mm_free(oldptr);
    return newptr;
}


void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if(size == 0)
        return NULL;
    
    /* Adjuist block size to include overhead and alignment reqs. */
    if(size <= DSIZE)
        asize = 3*DSIZE; //8의 배수로 맞춤 8바이트는 헤더/푸터 나머지 8바이트는 할당 + 패딩
    else
        asize = DSIZE * ((size + (DSIZE)+ (DSIZE-1)) / DSIZE); // 정수 나누고 곱해서 8의 배수로 만들어버림 (DSIZE-1은 올림을 하기 위함) 헤더/푸터 포함
    //=>asize = 8의배수, 필요한 size + 헤더/푸터 포함한 값, 최소 24
    /*Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL){ //asize가 들어갈 수 있는 가용블록 찾기 -> 블록의 시작 주소 리턴
        place(bp, asize); // 2가지 기능: 1. 
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;


    //기본코드
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
    //     return NULL;
    // else
    // {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
}

//asize는 할당하고픈 바이트를 받아서 24바이트 이상의 8의 배수바이트로 만든것
static void *find_fit(size_t asize) //776 
{
    char *bp = free_heap_listp; //heap의 시작주소
    while(true){
        if(bp == NULL){
            break;
        }

        char *p = HDRP(bp);
        if(GET_ALLOC(p) == 1 && GET_SIZE(p) == 0){
            break;
        }

        if(GET_ALLOC(p) == 0 && (GET_SIZE(p) > asize+16 || GET_SIZE(p) == asize)){ // 남은것도 24바이트 이상 이어야함. 그래서 16
            return bp;
        }
        bp = NEXT_FBLKP(bp);
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{

    if(GET_SIZE(HDRP(bp))< asize){
        return;
    }
 
    delete_free_heaplist(bp);
    size_t size = GET_SIZE(HDRP(bp));
    if(GET_SIZE(HDRP(bp)) == asize){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
    }else if(GET_SIZE(HDRP(bp)) > asize + 16){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(size - asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size - asize, 0));
        insert_free_heaplist(NEXT_BLKP(bp));
    }else{ //분할했을때 남는공간이 부족..
        PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
        PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
    }

}


// // *******************implicit*****************************

// static void *coalesce(void *bp) // 앞뒤의 가용한 블록에 대해서 연결 후 새로운블록의 시작주소 리턴
// {
//     size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록 할당여부
//     size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록 할당여부
//     size_t size = GET_SIZE(HDRP(bp)); //현재 블록의 크기
    

//     if(prev_alloc && next_alloc) { //둘다 할당되어있는 경우
        
//     }

//     else if(prev_alloc && !next_alloc){ //다음은 free
//         size += GET_SIZE(HDRP(NEXT_BLKP(bp))); //size  값 갱신
//         PUT(HDRP(bp), PACK(size, 0)); //헤더에 덮어씌우기 , 할당x
//         PUT(FTRP(bp), PACK(size, 0)); // 헤더에 사이즈 만큼 이동 후 footer값 갱신 즉, 새로운 블록 footer 갱신, 할당x
//     }

//     else if(!prev_alloc && next_alloc){ //이전이 free
//         size += GET_SIZE(HDRP(PREV_BLKP(bp)));
//         PUT(FTRP(bp), PACK(size, 0)); // footer 갱신
//         PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); //헤더정보 갱신
//         bp = PREV_BLKP(bp); //갱신된 블록의 시작지점으로 bp 갱신
//     }

//     else{ //둘다 free인 경우
//         size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
//         PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
//         PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
//         bp = PREV_BLKP(bp);
//     }
//     return bp; //새로 갱신한 블록의 시작주소(헤더 앞)
// }

// /*
//  * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
//  */
// void *mm_realloc(void *ptr, size_t size)
// {
//     void *oldptr = ptr;
//     void *newptr;
//     size_t copySize;

//     newptr = mm_malloc(size);
//     if (newptr == NULL)
//         return NULL;
//     copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
//     if (size < copySize)
//         copySize = size;
//     memcpy(newptr, oldptr, copySize);
//     mm_free(oldptr);
//     return newptr;
// }
// /*
//  * mm_malloc - Allocate a block by incrementing the brk pointer.
//  *     Always allocate a block whose size is a multiple of the alignment.
//  */
// void *mm_malloc(size_t size)
// {
//     size_t asize;
//     size_t extendsize;
//     char *bp;

//     /* Ignore spurious requests */
//     if(size == 0)
//         return NULL;
    
//     /* Adjuist block size to include overhead and alignment reqs. */
//     if(size <= DSIZE)
//         asize = 2*DSIZE; //8의 배수로 맞춤 8바이트는 헤더/푸터 나머지 8바이트는 할당 + 패딩
//     else
//         asize = DSIZE * ((size + (DSIZE)+ (DSIZE-1)) / DSIZE); // 정수 나누고 곱해서 8의 배수로 만들어버림 (DSIZE-1은 올림을 하기 위함) 헤더/푸터 포함
//     //=>asize = 8의배수, 필요한 size + 헤더/푸터 포함한 값
//     /*Search the free list for a fit */
//     if ((bp = find_fit(asize)) != NULL){ //asize가 들어갈 수 있는 가용블록 찾기 -> 블록의 시작 주소 리턴
//         place(bp, asize); // 2가지 기능: 1. 
//         return bp;
//     }

//     /* No fit found. Get more memory and place the block */
//     extendsize = MAX(asize, CHUNKSIZE);
//     if((bp = extend_heap(extendsize/WSIZE)) == NULL)
//         return NULL;
//     place(bp, asize);
//     return bp;


//     //기본코드
//     // int newsize = ALIGN(size + SIZE_T_SIZE);
//     // void *p = mem_sbrk(newsize);
//     // if (p == (void *)-1)
//     //     return NULL;
//     // else
//     // {
//     //     *(size_t *)p = size;
//     //     return (void *)((char *)p + SIZE_T_SIZE);
//     // }
// }


// static void *find_fit(size_t asize)
//     //설계
// // 1. 힙의 처음 헤더 정보를 읽음. 할당 여부 파악

// // 1-1. 할당되어 있으면 크기정보를 통해 다음 블록 이동

// // 1-2. 할당 안되어있으면 크기정보를 통해 asize보다 큰지 판별
// // 1-2-1. asize보다 크거나 같으면 리턴
// // 1-2-2. 작으면 다시 탐색
// {
//     //왜 mem_heap_lo를 쓰면 안됨..?

//     char *bp = heap_listp; //heap의 시작주소
//     while(true){
//         char *p = HDRP(bp);
//         if(GET_ALLOC(p) == 1 && GET_SIZE(p) == 0){
//             break;
//         }

//         if(GET_ALLOC(p) == 0 && (GET_SIZE(p) > asize+8 || GET_SIZE(p) == asize)){
//             return bp;
//         }
//         bp = NEXT_BLKP(bp);
//     }
//     return NULL;
// }

// static void place(void *bp, size_t asize)
// //설계
// // 0. 만약 작으면 오류..
// // 1. 같으면 배치 
// // 2. 크면 배치후 다음 블록 헤더 푸터 배치(size는 이전 크기 - asize)(size는 헤더 푸터 포함된 값이어야함)
// {

//     if(GET_SIZE(HDRP(bp))< asize){
//         return;
//     }
 
//     size_t size = GET_SIZE(HDRP(bp));
//     if(GET_SIZE(HDRP(bp)) == asize){
//         PUT(HDRP(bp), PACK(asize, 1));
//         PUT(FTRP(bp), PACK(asize, 1));
//     }else{
//         PUT(HDRP(bp), PACK(asize, 1));
//         PUT(FTRP(bp), PACK(asize, 1));
//         PUT(HDRP(NEXT_BLKP(bp)), PACK(size - asize, 0));
//         PUT(FTRP(NEXT_BLKP(bp)), PACK(size - asize, 0));
//     }

// } 
// // implicit 해답
// // static void *find_fit(size_t asize)
// // {
// //     /* First-fit search */
// //     void *bp;

// //     for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
// //         if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
// //             return bp;
// //         }
// //     }
// //     return NULL; /* No fit */
// // }

// // static void place(void *bp, size_t asize)
// // {
// //     size_t csize = GET_SIZE(HDRP(bp));

// //     if((csize - asize) >= (2*DSIZE)){
// //         PUT(HDRP(bp), PACK(asize, 1));
// //         PUT(FTRP(bp), PACK(asize, 1));
// //         bp = NEXT_BLKP(bp);
// //         PUT(HDRP(bp), PACK(csize-asize, 0));
// //         PUT(FTRP(bp), PACK(csize-asize, 0));
// //     }
// //     else{
// //         PUT(HDRP(bp), PACK(csize, 1));
// //         PUT(FTRP(bp), PACK(csize, 1));
// //     }
// // }