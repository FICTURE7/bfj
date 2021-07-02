#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#define MEMORY_SZ 128 * 1024
#define CODE_SZ   128 * 1024
#define LABEL_SZ    4 * 1024

FILE *fd;
int8_t* M;           /* Machine code. */
int8_t* m;           /* Machine code pointer. */

int8_t p;            /* Trace pointer. */
int8_t U[256];       /* Trace buffer. */
int8_t *u = U + 128;

int8_t *L[LABEL_SZ]; /* Label buffer. */
int32_t j;

uint8_t mem[MEMORY_SZ];

void panic(const char *message, ...)
{
  va_list ap;
  va_start(ap, message);
  fprintf(stderr, "error: ");
  vfprintf(stderr, message, ap);
  if (errno != 0)
    fprintf(stderr, ": %s", strerror(errno));
  fprintf(stderr, "\n");
  va_end(ap);
  abort();
}

void emit_(void *b, size_t n)
{
  if ((m + n - M) > CODE_SZ)
    panic("Exceeded code buffer size\n");
  memcpy(m, b, n);
  m += n;
}

#define emit(...)  emit_((uint8_t[]){__VA_ARGS__}, sizeof((uint8_t[]){__VA_ARGS__}))
#define emit_64(x) emit_((uint64_t[]){(uint64_t)(x)}, sizeof(uint64_t))

#define mrm(m, r, rm)  ((m) << 6 | (r) << 3 | (rm))
#define asm_mcmp()     emit(      0x80, mrm(0, 7, 3), 0)         /* cmp byte [rbx], 0 */
#define asm_madd(o, c) emit(      0x80, mrm(1, 0, 3), (o), (c))  /* add byte [rbx + o], c */
#define asm_mset(o, c) emit(      0x8B, mrm(1, 0, 3), (o), (c))  /* mov byte [rbx + o], byte c */
#define asm_padd(c)    emit(0x48, 0x83, mrm(3, 0, 3), (c))       /* add rbx, byte c */

#define asm_mout(o)                                                         \
  emit(0x0F, 0xB6, mrm(1, 7, 3), (o)),     /* movxz edi, byte [rbx + o] */  \
  emit(0x49, 0xFF, mrm(3, 2, 7))           /* call r15 */

#define asm_min(o)                                                          \
  emit(0x49, 0xFF, mrm(3, 2, 6)),          /* call r14 */                   \
  emit(      0x88, mrm(1, 0, 3), (o))      /* mov byte [rbx + o], al */

#define asm_prolog()                                                        \
  emit(0x48, 0xFF, mrm(3, 6, 3)),          /* push rbx */                   \
  emit(0x49, 0xFF, mrm(3, 6, 6)),          /* push r14 */                   \
  emit(0x49, 0xFF, mrm(3, 6, 7)),          /* push r15 */                   \
  emit(0x48, 0x8B, mrm(3, 3, 7)),          /* mov rbx, rdi */               \
  emit(0x49, 0xBE), emit_64(&getchar),     /* mov r14, &getchar */          \
  emit(0x49, 0xBF), emit_64(&putchar)      /* mov r15, &putchar */

#define asm_epilog()                                                        \
  emit(0x49, 0x8F, mrm(3, 0, 7)),          /* pop r15 */                    \
  emit(0x49, 0x8F, mrm(3, 0, 6)),          /* pop r14 */                    \
  emit(0x48, 0x8F, mrm(3, 0, 3)),          /* pop rbx */                    \
  emit(      0xC3)                         /* ret */

#define TRACE_ASM_FULL    0 /* Commit trace, including instruction pointer. */
#define TRACE_ASM_PARTIAL 1 /* Commit trace, w/o instruction pointer. */

void asm_trace(int k)
{
  for (int32_t i = -128; i < 127; i++) {
    if (u[i] != 0)
      asm_madd((int8_t)i, u[i]);
    u[i] = 0;
  }
  if (k == TRACE_ASM_FULL) {
    if (p != 0)
      asm_padd(p);
    p = 0;
  }
}

int main(int argc, char **argv)
{
  if (argc < 2)
    panic("Usage: bfj FILE");
  if (!(fd = fopen(argv[1], "r")))
    panic("Failed to open file");
  m = M = mmap(NULL, CODE_SZ, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (m == MAP_FAILED)
    panic("Failed to mmap memory");
  asm_prolog();
  while (!feof(fd)) {
    char c = fgetc(fd);
    switch (c) {
    case '+': u[p]++; break;
    case '-': u[p]--; break;
    case '>':
      if (p + 1 > 127)
        asm_trace(TRACE_ASM_FULL);
      p++;
      break;
    case '<':
      if (p - 1 < -128)
        asm_trace(TRACE_ASM_FULL);
      p--;
      break;
    case '.':
      asm_trace(TRACE_ASM_PARTIAL);
      asm_mout(p);
      break;
    case ',':
      asm_trace(TRACE_ASM_PARTIAL);
      asm_min(p);
      break;
    case '[':
      if (j + 1 >= LABEL_SZ)
        panic("Exceeded label buffer");
      asm_trace(TRACE_ASM_FULL);
      asm_mcmp();
      L[j++] = m;
      m += 6; /* jz L[j] */
      break;
    case ']':
      if (j - 1 < 0)
        panic("Invalid brainfuck program");
      asm_trace(TRACE_ASM_FULL);
      asm_mcmp();

      uint32_t off0 = L[--j] - m;
      emit(0x0F, 0x85), emit_(&off0, 4); /* jnz L[j] */

      int8_t* om = m; m = L[j];
      uint32_t off1 = om - L[j] - 6;
      emit(0x0F, 0x84), emit_(&off1, 4); 
      m = om;
      break;
    }
  }
  asm_trace(TRACE_ASM_PARTIAL);
  asm_epilog();
  if (j > 0)
    panic("Invalid brainfuck program");
  ((void (*)(uint8_t*))M)(mem); /* Run the compiled code. */
  return 0;
}
