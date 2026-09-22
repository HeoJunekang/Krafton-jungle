#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Widget Widget;

typedef struct {
    void (*render)(Widget *self);
    void (*on_event)(Widget *self, int code);
} VTable;

struct Widget {
    const VTable *vtbl;  // 수정할 수 없는 VTable 구조체 포인터(가리키는 대상은 바꿀 수 있음)
    int id;
    int closed;
    char label[24];
};

#define MAX_WIDGETS 8
typedef struct {
    Widget *items[MAX_WIDGETS];
    int count;
} Screen;

/* ── 위젯 종류별 동작 ─────────────────────────────────────────── */
static void button_render(Widget *self) { //static = 다른 .c파일에서는 직접 호출 불가능 현재 파일에서만 사용가능
    printf("  [Button #%d] \"%s\"\n", self->id, self->label);
}
static void label_render(Widget *self) {
    printf("  Label #%d: %s\n", self->id, self->label);
}
static void dialog_render(Widget *self) {
    printf("  <<Dialog #%d>> %s\n", self->id, self->label);
}

static void widget_noop_event(Widget *self, int code) { (void)self; (void)code; }


static void dialog_on_event(Widget *self, int code);

static const VTable BUTTON_VT = { button_render, widget_noop_event }; //const = 내용변경 불가능
static const VTable LABEL_VT  = { label_render,  widget_noop_event };
static const VTable DIALOG_VT = { dialog_render, dialog_on_event  };

static Widget *widget_new(const VTable *vt, int id, const char *label) {

 
    Widget *w = malloc(sizeof *w);
    if (!w) { perror("malloc"); exit(1); } //perror() :현재 발생한 시스템 에러 내용을 출력하는 함수
    w->vtbl = vt;
    w->id = id;
    w->closed = 0;
    strncpy(w->label, label, sizeof(w->label) - 1); // 문자열을 n글자까지만 복사하는 함수, strncpy(w->label, label, sizeof(w->label) - 1);
    w->label[sizeof(w->label) - 1] = '\0';
    return w;
}

static void widget_destroy(Widget *w) {
    free(w);
}


/* ── Screen ──────────────────────────────────────────────────── */
static void screen_add(Screen *s, Widget *w) {
    if (s->count < MAX_WIDGETS) s->items[s->count++] = w;
}

static void screen_dispatch(Screen *s, int code) {
    for (int i = 0; i < s->count; i++) {
        Widget *w = s->items[i];
        w->vtbl->on_event(w, code);
        // if(w->closed == 1){
        //     s->items[i] = NULL;
        //     widget_destroy(w);
        // }
    }
}

static void screen_render(Screen *s) {
    for (int i = 0; i < s->count; i++) {
        Widget *w = s->items[i];
        w->vtbl->render(w);
        // if(w == NULL){
        //     continue;
        // }else{
        //     w->vtbl->render(w);
        // }
    }
}

static void dialog_on_event(Widget *self, int code) {
    if (code == 1) {
        self->closed = 1;
        widget_destroy(self);
    }
}

static char *app_build_status(const char *text) {
    char *msg = malloc(sizeof(Widget));   
    if (!msg) exit(1);


    memset(msg, 0xAB, sizeof(Widget)); //msg부터 Widget사이즈만큼 AB로 메모리를 채움
    snprintf(msg, sizeof(Widget), "STATUS: %s", text); //msg 버퍼에 최대 Widget 크기만큼 형식에 맞춰 text를 덮어씀
    return msg;
}

int main(void) {
    Screen s = { .count = 0 };

    screen_add(&s, widget_new(&LABEL_VT,  10, "Welcome"));
    screen_add(&s, widget_new(&BUTTON_VT, 11, "OK"));
    screen_add(&s, widget_new(&DIALOG_VT, 12, "Are you sure?"));  /* items[2] */
    screen_add(&s, widget_new(&BUTTON_VT, 13, "Cancel"));

    printf("frame 1:\n");
    screen_render(&s);
    screen_dispatch(&s, 1); //items[2] 없어짐

   
    char *status = app_build_status("dialog closed");
    printf("%s\n", status);

    printf("frame 2:\n");
    screen_render(&s);           

    free(status);
    for (int i = 0; i < s.count; i++) free(s.items[i]);
    return 0;
}

// 문제풀이 과정
// 1. 문제 파악(메모리 접근을 못함)
// 2. 문제 있는 코드 파악 및 지역변수 확인
// 3. 이전 함수들 확인하면서 코드의 의도를 파악(배열이 free()하고 싶은것은?)
// 4. pointer관계를 파악하면서 해결(free(w)를 하면 w가 쓰레기 값을 가리키게 됨 != NULL)
