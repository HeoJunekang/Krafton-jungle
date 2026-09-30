/*
 * Challenge 09 — strcpy 힙 오버플로 (심화: join 크기계산 off-by-one)
 *
 * [시나리오]
 *   여러 조각(parts)을 구분자 없이 이어 붙여 하나의 문자열을 만드는 join().
 *   필요한 크기를 먼저 계산(joined_size)해 malloc 한 뒤, 각 조각을 순서대로 복사한다.
 *
 * [기대 동작]
 *   모든 조각을 이어 붙인 결과 길이를 출력하고 정상 종료.
 *
 * [증상]
 *   크기 계산 함수 joined_size() 의 루프가 `i < n - 1` 이라, "마지막 조각"의 길이를
 *   더하지 않는다. 그런데 실제 복사 루프는 `i < n` 으로 마지막 조각까지 복사한다.
 *   마지막 조각이 크면(여기서는 큰 본문), 할당량보다 훨씬 많이 써서 힙을 크게 넘어간다.
 *   → 힙 메타데이터 손상(이후 free 에서 abort) 또는 매핑 밖 접근으로 SIGSEGV.
 *   크래시는 strcpy/free 에서 나지만, 원인은 "크기 계산의 off-by-one"이다.
 *
 * [gdb 로 잡기]
 *   make gdb NAME=09_strcpy_overflow
 *   (gdb) run                       → 크래시(SIGSEGV 또는 abort)
 *   (gdb) bt                        → join 의 strcpy 또는 free 근처
 *   (gdb) break joined_size ; run    → 반환값(need)과 실제 필요한 총합을 비교
 *   (gdb) print need                → 마지막 조각 길이가 빠져 need 가 부족함을 확인
 *
 * [printf(로그)로 잡기]
 *   계산한 크기와 실제로 복사한 바이트를 비교 출력:
 *     fprintf(stderr, "alloc=%zu copied=%zu\n", need, off);
 *   → copied 가 alloc 을 넘어서면 그 초과분이 힙을 침범한 것.
 *   (stdout 은 버퍼링되니 stderr 로 찍어야 크래시 직전 로그가 남는다)
 *
 * TODO: 크기 계산 루프를 `i < n` 으로 고쳐 모든 조각 길이와 종료 문자('\0') 자리를
 *       빠짐없이 더한다. "계산 루프와 복사 루프의 범위를 반드시 일치"시킨다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 필요한 총 바이트 수 = 모든 조각 길이 합 + 종료 문자 1 */
static size_t joined_size(const char *const *parts, int n) {
    size_t total = 1;                        /* '\0' 자리 */
    for (int i = 0; i < n; i++) {
        //printf("%zu bytes",strlen(parts[i]));
        total += strlen(parts[i]);
    }
    return total; //29
}

static char *join(const char *const *parts, int n) {
    size_t need = joined_size(parts, n);
    char *out = malloc(need);                
    if (!out) { perror("malloc"); exit(1); }

    size_t off = 0;
    for (int i = 0; i < n; i++) {           
        strcpy(out + off, parts[i]); //\0포함함 하지만 +off인 자리부터 메모리를 씀 
        off += strlen(parts[i]); //\0포함 안함
    }
    // out[off] = '\0';
    return out;
}

int main(void) {
    
    static char body[200000]; // 16부터 헤더 침범
    memset(body, 'x', sizeof body - 1);
    body[sizeof body - 1] = '\0';

    const char *parts[] = { "GET ", "/index.html", " HTTP/1.1\r\n\r\n", body };
    int n = (int)(sizeof(parts) / sizeof(parts[0]));

    char *msg = join(parts, n); 

    
    printf("joined length = %zu\n", strlen(msg));
    free(msg);
    return 0;
}


//char *out = malloc(need);                /* 마지막 조각 길이만큼 부족하게 할당됨 */
//for (int i = 0; i < n; i++) {            /* 복사는 마지막 조각까지 전부 → 오버플로 */
//char *msg = join(parts, n);              /* 복사 중 힙 오버플로 → 크래시 */

// 풀이과정
// 1. 문제 위치 파악
// 2.  strcpy(out + off, parts[i]); 여기서 i =3일때 마지막 요소가 들어갈때 오류가 발생
// 3. 왜? 일단 overflow라고 판단함.(오류 코드 본것은 아님)
// 4. 처음에 join함수만 봄 
// 5. need를 출력해봄 = 29 별생각안함 그냥 필요한 메모리겠거니 함
// 6. 근데 srtcpy 는 문자열을 복사할때 \0까지 복사함 그래서 이것때문에 계속 불필요하게 메모리를 하나씩 더 쓰는구나 마지막에만 써야하는데
// 7. strncpy를 사용 근데 같은 문제 발생 
// 8. 심지어 원래 그렇게면 i = 2에서 문제가 발생해야지 need가 29이기에 원래 생각대로라면 이때 strcpy 에서 메모리 31을 써서 이때 오류가 발생해야함
// 9. 함수 구조를 보니 off를 더함으로써 메모리 위치를 정함 strlen은 \0까지 포함을 안함. 그래서 이것 자체는 문제가 없구나
// 10. 그럼 왜 i =2에서 안나고 i=3? ..?하나가 더 있네
// 11. 넘겨주는 자료형 확인 20000짜리 배열있음 근데 joined_size는 n-1까지만 메모리를 할당하도록 함.
// 이때 joined_size여기서 넘겨주는 값 확인하려 20000이거.. 근데 그냥 숫자가 처음부터안나옴... 실패
// 12. 이후 n-1까지만 하는거 발견 
// 13. 수정

// 할일
// 메모리 양 줄이면 다른 곳에서 오류 발생-> 왜? 정확히 파악
// 그리고 메모리 양 늘어나면 왜 strcpy에서 오류 잡아줌 2번이랑 뭐가 다른데 3번도 파악해보시오

// malloc(): corrupted top size 이 오류가 뭔데
// 그래고 SIGABRT랑 SIGEGV차이 파악