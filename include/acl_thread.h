// Copyright (C) 2015-2021 Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause

#ifndef ACL_THREAD_H
#define ACL_THREAD_H

#include "acl.h"
#include "acl_context.h"
#include "acl_types.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef __linux__
#include <signal.h>
#define ACL_TLS __thread
#else // WINDOWS
#define ACL_TLS __declspec(thread)
#endif

extern ACL_TLS int acl_global_lock_count;
extern ACL_TLS int acl_inside_sig_flag;
extern ACL_TLS int acl_inside_sig_old_lock_count;

// -- signal handler functions --
// When we enter a signal handler, we save "acl_global_lock_count" to
// "acl_inside_sig_old_lock_count" temporarily. This is because the signal
// handler will run inside one of the existing threads randomly and so will get
// the value of the lock count that that thread had. However, it's misleading
// because conceptually the signal handler doesn't ever really have the lock.
// Therefore we temporarily change the lock count to 0 while inside the signal
// handler so that things like "acl_assert_locked()" will operate as expected.
// If a function needs an assert that passes if either the lock is held or
// inside a signal handler, it can use "acl_assert_locked_or_sig()".

static inline int acl_is_inside_sig() { return acl_inside_sig_flag; }

static inline void acl_assert_inside_sig() { assert(acl_is_inside_sig()); }

static inline void acl_assert_outside_sig() { assert(!acl_is_inside_sig()); }

static inline void acl_sig_started() {
  assert(!acl_inside_sig_flag);
  acl_inside_sig_flag = 1;
  acl_inside_sig_old_lock_count = acl_global_lock_count;
  acl_global_lock_count = 0;
}

static inline void acl_sig_finished() {
  assert(acl_inside_sig_flag);
  acl_inside_sig_flag = 0;
  acl_global_lock_count = acl_inside_sig_old_lock_count;
}

// Blocking/Unblocking signals (Only implemented for Linux)
#ifdef __linux__
extern ACL_TLS sigset_t acl_sigset;
static inline void acl_sig_block_signals() {
  sigset_t mask;
  if (sigfillset(&mask))
    assert(0 && "Error in creating signal mask in status handler");
  if (pthread_sigmask(SIG_BLOCK, &mask, &acl_sigset))
    assert(0 && "Error in blocking signals in status handler");
}
static inline void acl_sig_unblock_signals() {
  if (pthread_sigmask(SIG_SETMASK, &acl_sigset, NULL))
    assert(0 && "Error in unblocking signals in status handler");
}
#endif

// -- global lock functions --

void acl_lock();
void acl_unlock();
int acl_suspend_lock();
void acl_resume_lock(int lock_count);
void acl_wait_for_device_update(cl_context context);
void acl_signal_device_update();

static inline int acl_is_locked() { return (acl_global_lock_count > 0); }

// Used by dynamically loaded libs to check lock status.
int acl_is_locked_callback(void);

static inline void acl_assert_locked() { assert(acl_is_locked()); }

static inline void acl_assert_locked_or_sig() {
  assert(acl_is_locked() || acl_is_inside_sig());
}

static inline void acl_assert_unlocked() { assert(!acl_is_locked()); }

// -- misc functions --

int acl_get_thread_id();
int acl_get_pid();
void acl_yield_lock_and_thread();

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif // ACL_THREAD_H
