#pragma once

#include <lib/stdint.h>

typedef struct context
{
    uint32_t ebx;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t eip;
} context_t;

typedef struct trapframe
{
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t temp_esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    uint32_t ino;
    uint32_t eno;

    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t ss;
}trapframe_t;

typedef struct ktrapframe
{
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t temp_esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    uint32_t ino;
    uint32_t eno;

    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} ktrapframe_t;

void dump_trapframe(trapframe_t *tf);