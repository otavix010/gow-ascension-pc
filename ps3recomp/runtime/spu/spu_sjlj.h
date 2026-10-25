#ifndef SPU_SJLJ_H
#define SPU_SJLJ_H
#include <setjmp.h>
/* GOWA: the halt / stack-reset landing pad without the CRT's setjmp/longjmp.
 * On Windows x64 the CRT longjmp unwinds the abandoned frames through SEH
 * (RtlUnwindEx), ~0.5 us with 8 frames between (out/spudiff/teste_bsjlj.c) --
 * and God of War Ascension's SPURS kernels reset the guest stack ~130,000
 * times a second across the SPUs, each one a longjmp back here. Nothing in the
 * frames it abandons needs unwinding (C, no __try, no destructors), so this
 * pair just saves and restores the Win64 callee-saved state: rbx rbp rdi rsi
 * r12-r15, rsp, the return address, xmm6-xmm15. MXCSR stays what the thread
 * set (DAZ) and is not touched in between. SPU_CRT_SETJMP restores the CRT
 * pair. (clang's __builtin_setjmp faults on this target.) */
#if defined(_WIN64) && defined(__clang__) && !defined(SPU_CRT_SETJMP)
typedef struct { unsigned long long gpr[12]; unsigned long long xmm[20]; } spu_jmpbuf;
__attribute__((naked, returns_twice, noinline)) static int spu_setjmp(spu_jmpbuf* b)
{
    __asm__ volatile(
        "movq %rbx, 0(%rcx)\n\t"
        "movq %rbp, 8(%rcx)\n\t"
        "movq %rdi, 16(%rcx)\n\t"
        "movq %rsi, 24(%rcx)\n\t"
        "movq %r12, 32(%rcx)\n\t"
        "movq %r13, 40(%rcx)\n\t"
        "movq %r14, 48(%rcx)\n\t"
        "movq %r15, 56(%rcx)\n\t"
        "leaq 8(%rsp), %rdx\n\t"          /* rsp as the caller sees it after the return */
        "movq %rdx, 64(%rcx)\n\t"
        "movq (%rsp), %rdx\n\t"           /* return address */
        "movq %rdx, 72(%rcx)\n\t"
        "movdqu %xmm6, 96(%rcx)\n\t"
        "movdqu %xmm7, 112(%rcx)\n\t"
        "movdqu %xmm8, 128(%rcx)\n\t"
        "movdqu %xmm9, 144(%rcx)\n\t"
        "movdqu %xmm10, 160(%rcx)\n\t"
        "movdqu %xmm11, 176(%rcx)\n\t"
        "movdqu %xmm12, 192(%rcx)\n\t"
        "movdqu %xmm13, 208(%rcx)\n\t"
        "movdqu %xmm14, 224(%rcx)\n\t"
        "movdqu %xmm15, 240(%rcx)\n\t"
        "xorl %eax, %eax\n\t"
        "retq\n\t");
}
__attribute__((naked, noreturn, noinline)) static void spu_longjmp(spu_jmpbuf* b, int v)
{
    __asm__ volatile(
        "movq 0(%rcx), %rbx\n\t"
        "movq 8(%rcx), %rbp\n\t"
        "movq 16(%rcx), %rdi\n\t"
        "movq 24(%rcx), %rsi\n\t"
        "movq 32(%rcx), %r12\n\t"
        "movq 40(%rcx), %r13\n\t"
        "movq 48(%rcx), %r14\n\t"
        "movq 56(%rcx), %r15\n\t"
        "movdqu 96(%rcx), %xmm6\n\t"
        "movdqu 112(%rcx), %xmm7\n\t"
        "movdqu 128(%rcx), %xmm8\n\t"
        "movdqu 144(%rcx), %xmm9\n\t"
        "movdqu 160(%rcx), %xmm10\n\t"
        "movdqu 176(%rcx), %xmm11\n\t"
        "movdqu 192(%rcx), %xmm12\n\t"
        "movdqu 208(%rcx), %xmm13\n\t"
        "movdqu 224(%rcx), %xmm14\n\t"
        "movdqu 240(%rcx), %xmm15\n\t"
        "movl %edx, %eax\n\t"
        "movq 64(%rcx), %rsp\n\t"
        "jmpq *72(%rcx)\n\t");
}
#define SPU_HALT_SETJMP(b)     spu_setjmp(&(b))
#define SPU_HALT_LONGJMP(b, v) spu_longjmp(&(b), (v))
#else
typedef jmp_buf spu_jmpbuf;
#define SPU_HALT_SETJMP(b)     setjmp(b)
#define SPU_HALT_LONGJMP(b, v) longjmp((b), (v))
#endif
#endif
