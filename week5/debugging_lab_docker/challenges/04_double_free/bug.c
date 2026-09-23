/*
 * Challenge 04 — Double Free (심화: 두 인덱스가 같은 객체를 가리키는 별칭)
 *
 *
 * [gdb 로 잡기]
 *   make gdb NAME=04_double_free
 *   (gdb) run                       → abort
 *   (gdb) bt                        → directory_free() 의 두 번째 free 루프
 *   (gdb) frame N ; print d->by_name[i] → 이 주소가 앞서 by_id 로 이미 free 됐는지 확인
 *   (gdb) print d->by_id[0]          
 *
 * [printf(로그)로 잡기]
 *   free 직전마다 주소를 찍어 같은 주소가 두 번 나오는지 본다:
 *     fprintf(stderr, "free rec=%p (%s)\n", (void*)r, tag);
 *   → by_id 루프와 by_name 루프에서 동일 주소가 각각 나오면 이중 해제.
 *   (stdout 은 버퍼링되니 stderr 로 찍어야 크래시 직전 로그가 남는다)
 *
 * TODO: 소유권은 한 곳만 갖게 한다. 예) by_id 를 "소유 인덱스"로 정하고 여기서만 해제,
 *       by_name 은 "관찰용(빌려온) 인덱스"로 두어 절대 free 하지 않는다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int   id;
    char *name;      
} Rec;

#define MAXN 16
typedef struct {
    Rec *by_id[MAXN];     
    Rec *by_name[MAXN];    
    int  count;
} Directory;

static Rec *rec_new(int id, const char *name) {
    Rec *r = malloc(sizeof *r);
    if (!r) { perror("malloc"); exit(1); }
    r->id = id;
    r->name = malloc(strlen(name) + 1);
    if (!r->name) { perror("malloc"); exit(1); }
    strcpy(r->name, name);
    return r;
}

static void directory_add(Directory *d, int id, const char *name) {
    Rec *r = rec_new(id, name);
    d->by_id[d->count]   = r;
    d->by_name[d->count] = r;      /* 같은 포인터를 두 인덱스에 함께 등록 */
    d->count++;
}

/* 이름 순 인덱스를 사전순으로 정렬(포인터만 재배치, 객체는 공유 그대로) */
static void directory_sort_by_name(Directory *d) {
    for (int i = 0; i < d->count; i++) {
        for (int j = i + 1; j < d->count; j++) {
            if (strcmp(d->by_name[i]->name, d->by_name[j]->name) > 0) {
                Rec *t = d->by_name[i];
                d->by_name[i] = d->by_name[j];
                d->by_name[j] = t;
            }
        }
    }
}

static Rec *find_by_id(Directory *d, int id) {
    for (int i = 0; i < d->count; i++)
        if (d->by_id[i]->id == id) return d->by_id[i];
    return NULL;
}

static void directory_dump(Directory *d) {
    printf("by id:  ");
    for (int i = 0; i < d->count; i++) printf("%d:%s ", d->by_id[i]->id, d->by_id[i]->name);
    printf("\nby name:");
    for (int i = 0; i < d->count; i++) printf(" %s(%d)", d->by_name[i]->name, d->by_name[i]->id);
    printf("\n");
}

static void directory_free(Directory *d) {
    for (int i = 0; i < d->count; i++) {
        free(d->by_id[i]->name);
        free(d->by_id[i]);                 
    }
    // for (int i = 0; i < d->count; i++) {
    //     free(d->by_name[i]);               
    // }
    d->count = 0;
}

int main(void) {
    Directory dir = { .count = 0 };

    directory_add(&dir, 3, "carol");
    directory_add(&dir, 1, "alice");
    directory_add(&dir, 4, "dave");
    directory_add(&dir, 2, "bob");

    directory_sort_by_name(&dir);
    directory_dump(&dir);

    Rec *r = find_by_id(&dir, 2);
    if (r) printf("lookup id=2 -> %s\n", r->name);

    directory_free(&dir);                  
    printf("done\n");
    return 0;
}

// 풀이과정
// 코드해석(이때 잘 못된 줄 모름)
// gdb를 활용해서 directory_free함수를 하나씩 돌려봄 -> 이때 두번째 for문 시작하자마자 문제가 생긴다는것을 발견
// 근데 왜..?
// 기존에 알고있던 개념으로 free는 예를들어 free(w)면 w에 포함되어있는 연결관계를 끊는것으로 이해함.
// 하지만 그렇게 생각하면 코드는 잘못된것이 없음. 그래서 코드 해석때 문제를 파악 못함.
// free에서 연결관계가 끊어지는 것은 결과임.
// free를 하면 연결되어 있는 메모리 자체가 자유로워 지는것. 그래서 id를 보면 쓰레기 값이 들어가있음 & 접근이 안됨 그래서 연결관계가 끊어지는것임
// so, by_id를 통해 free를 하면 이미 그 메모리는 자유로워 지는 것임. by_name으로 가리키는 것은 순서는 다르지만 같은 메모리를 보고있기 때문에 오류가 발생
