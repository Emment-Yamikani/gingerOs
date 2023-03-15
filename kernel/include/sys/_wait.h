/* Copyright (C) 1991-2022 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/*
 *	POSIX Standard: 3.2.1 Wait for Process Termination	<sys/wait.h>
 */

#pragma once

#include <lib/types.h>

/* This will define the `W*' macros for the flag
   bits to `waitpid', `wait3', and `wait4'.  */
# include <bits/waitflags.h>

/* This will define all the `__W*' macros.  */
# include <bits/waitstatus.h>

#define WEXITSTATUS(status)	__WEXITSTATUS (status)
#define WTERMSIG(status)	__WTERMSIG (status)
#define WSTOPSIG(status)	__WSTOPSIG (status)
#define WIFEXITED(status)	__WIFEXITED (status)
#define WIFSIGNALED(status)	__WIFSIGNALED (status)
#define WIFSTOPPED(status)	__WIFSTOPPED (status)
# ifdef __WIFCONTINUED
#  define WIFCONTINUED(status)	__WIFCONTINUED (status)
# endif

#define WCOREFLAG		__WCOREFLAG
#define WCOREDUMP(status)	__WCOREDUMP (status)
#define W_EXITCODE(ret, sig)	__W_EXITCODE (ret, sig)
#define W_STOPCODE(sig)	__W_STOPCODE (sig)

/* Wait for a child to die.  When one does, put its status in *STAT_LOC
   and return its process ID.  For errors, return (pid_t) -1.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern pid_t wait (int *__stat_loc);

/* Special values for the PID argument to `waitpid' and `wait4'.  */
#define WAIT_ANY	(-1)	/* Any process.  */
#define WAIT_MYPGRP	0	/* Any process in my process group.  */

/* Wait for a child matching PID to die.
   If PID is greater than 0, match any process whose process ID is PID.
   If PID is (pid_t) -1, match any process.
   If PID is (pid_t) 0, match any process with the
   same process group as the current process.
   If PID is less than -1, match any process whose
   process group is the absolute value of PID.
   If the WNOHANG bit is set in OPTIONS, and that child
   is not already dead, return (pid_t) 0.  If successful,
   return PID and store the dead child's status in STAT_LOC.
   Return (pid_t) -1 for errors.  If the WUNTRACED bit is
   set in OPTIONS, return status for stopped children; otherwise don't.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern pid_t waitpid (pid_t __pid, int *__stat_loc, int __options);