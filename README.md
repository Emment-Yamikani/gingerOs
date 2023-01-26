# gingerOs
Minimal Unix-like hobby OS
#

## Architecture
x86-32bit
#

## Features
- Multitasking
    - kernel threads.
    - 1:1 User threads.
- Multiprocessing
    - supports up to 8 processor cores
- Virtual Filesystem
    - ramfs called gingerfs, a minimal filesystem.
    - devfs.
    - pipefs.
#

!NOTE:
-Code not fully tested, still a long way away from being a proper Operating System.

## REQUIREMENTS FOR BUILDING
- Latest GNU GCC cross compiler
- Latest Nasm
#

## Build
> make
#

## How to run
> make run
#
