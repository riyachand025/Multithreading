#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include <stdlib.h>
#include <assert.h>

#define MAX_THREADS 5
#define STACK_SIZE 4096
#define JB_SP 6
#define JB_PC 7

enum STATE { TERM, READY, RUN, NEW };

typedef struct tcb {
    jmp_buf env;
    char stack[STACK_SIZE];
    int id;
    enum STATE state;
} TCB;

typedef unsigned long address;

static int cur = -1;
static TCB threads[MAX_THREADS];

void alarm_handler(int sig) {
    printf("Thread %d running.\n", cur);
    signal(SIGALRM, alarm_handler);
    if (cur != -1) {
        threads[cur].state = READY;
    }
}

void dispatch(int sig) {
    int cnt = 0;
    signal(SIGALRM, SIG_IGN);

    if (threads[cur].state != TERM) {
        if (sigsetjmp(threads[cur].env, 1) == 1)
            return;

        threads[cur].state = READY;
    }

    for (int i = (cur + 1) % MAX_THREADS; cnt <= MAX_THREADS; i = (i + 1) % MAX_THREADS) {
        if (threads[i].state == READY) {
            cur = i;
            break;
        }
        cnt++;
    }

    if (cnt > MAX_THREADS) {
        exit(0);
    }

    threads[cur].state = RUN;
    signal(SIGALRM, alarm_handler);
    alarm(1);
    siglongjmp(threads[cur].env, 1);
}

void deleteThread(int tid) {
    threads[tid].state = TERM;
    printf("Thread %d deleted.\n", tid);
}

static void wrapper() {
    printf("Thread %d exited.\n", cur);
    deleteThread(cur);

    signal(SIGALRM, alarm_handler);
    alarm(1);
    dispatch(SIGALRM);
}

int create(void (*start_routine)(void)) {
    int id = -1;
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (threads[i].state == TERM) {
            id = i;
            break;
        }
    }

    if (id == -1) {
        printf("No available thread.\n");
        return id;
    }

    assert(id >= 0 && id < MAX_THREADS);
    threads[id].state = NEW;
    threads[id].id = id;

    address sp = (address)threads[id].stack + STACK_SIZE - sizeof(address);
    address pc = (address)wrapper;

    if (setjmp(threads[id].env) == 0) {
        threads[id].env->__jmpbuf[JB_SP] = sp;
        threads[id].env->__jmpbuf[JB_PC] = pc;
        sigemptyset(&(threads[id].env)->__saved_mask);

        threads[id].state = RUN;
        start_routine();
        threads[id].state = TERM;
        printf("Thread %d finished.\n", id);
    }

    return id;
}

void run(int tid) {
    threads[tid].state = READY;
    siglongjmp(threads[tid].env, 1);
}

void f() {
    printf("Inside f");
    int sum = 0;
    for(int i=0; i < 10; ++i) {
        printf("%d\n", sum);
        sum += i;
    }
}

void g() {
    printf("Inside g");
}

int main() {
    printf("In Main:\n");
    int tid, tid2;
    tid = create(f);
    tid2 = create(f);
    printf("Thread created with id = %d\n", tid);

    alarm(1);
    dispatch(SIGALRM);
    run(tid);
    run(tid2);

    return 0;
}
