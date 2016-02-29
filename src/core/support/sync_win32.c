/*
 *
 * Copyright 2015-2016, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* Win32 code for gpr synchronization support. */

#include <grpc/support/port_platform.h>

#ifdef GPR_WIN32
#include <grpc/support/log.h>
#include <grpc/support/sync.h>
#include <grpc/support/time.h>
#include <stdio.h>

void gpr_mu_init(gpr_mu *mu) {
  InitializeCriticalSection(&mu->cs);
  mu->locked = 0;
}

void gpr_mu_destroy(gpr_mu *mu) { DeleteCriticalSection(&mu->cs); }

void gpr_mu_lock(gpr_mu *mu) {
  EnterCriticalSection(&mu->cs);
  GPR_ASSERT(!mu->locked);
  mu->locked = 1;
}

void gpr_mu_unlock(gpr_mu *mu) {
  mu->locked = 0;
  LeaveCriticalSection(&mu->cs);
}

int gpr_mu_trylock(gpr_mu *mu) {
  int result = TryEnterCriticalSection(&mu->cs);
  if (result) {
    if (mu->locked) {                /* This thread already holds the lock. */
      LeaveCriticalSection(&mu->cs); /* Decrement lock count. */
      result = 0;                    /* Indicate failure */
    }
    mu->locked = 1;
  }
  return result;
}

/*----------------------------------------*/
static void impl_cond_do_signal(gpr_cv *cond, int broadcast);
static BOOL impl_cond_do_wait(gpr_cv *cond, CRITICAL_SECTION *mtx, DWORD timeout_max_ms);

void gpr_cv_init(gpr_cv *cv) { 
	cv->blocked = 0;
	cv->gone = 0;
	cv->to_unblock = 0;
	cv->sem_queue = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
	cv->sem_gate = CreateSemaphore(NULL, 1, 1, NULL);
	InitializeCriticalSection(&cv->monitor);

}

void gpr_cv_destroy(gpr_cv *cv) {
  /* Condition variables don't need destruction in Win32. */
	CloseHandle(cv->sem_queue);
	CloseHandle(cv->sem_gate);
}

int gpr_cv_wait(gpr_cv *cv, gpr_mu *mu, gpr_timespec abs_deadline) {
  int timeout = 0;
  DWORD timeout_max_ms;
  mu->locked = 0;
  if (gpr_time_cmp(abs_deadline, gpr_inf_future(abs_deadline.clock_type)) ==
      0) {
		impl_cond_do_wait(cv, &mu->cs, INFINITE);
  } else {
    abs_deadline = gpr_convert_clock_type(abs_deadline, GPR_CLOCK_REALTIME);
    gpr_timespec now = gpr_now(abs_deadline.clock_type);
    int64_t now_ms = (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
    int64_t deadline_ms =
        (int64_t)abs_deadline.tv_sec * 1000 + abs_deadline.tv_nsec / 1000000;
    if (now_ms >= deadline_ms) {
      timeout = 1;
    } else {
      if ((deadline_ms - now_ms) >= INFINITE) {
        timeout_max_ms = INFINITE - 1;
      } else {
        timeout_max_ms = (DWORD)(deadline_ms - now_ms);
      }
      timeout = (impl_cond_do_wait(cv, &mu->cs, timeout_max_ms) == 0 &&
                 GetLastError() == ERROR_TIMEOUT);
    }
  }
  mu->locked = 1;
  return timeout;
}

void gpr_cv_signal(gpr_cv *cv) {
	impl_cond_do_signal(cv, 0);
}

void gpr_cv_broadcast(gpr_cv *cv) {
	impl_cond_do_signal(cv, 1);
}

/*----------------------------------------*/

void gpr_once_init(gpr_once *flag, void(*init_function)(void)) {
  if (InterlockedCompareExchange(&flag->status, 1, 0) == 0) {
	  (init_function)();
	  InterlockedExchange(&flag->status, 2);
  } else {
	  while (flag->status == 1) {
		  SwitchToThread();
	  }
  }

}



static void impl_cond_do_signal(gpr_cv *cond, int broadcast)
{
	int nsignal = 0;

	EnterCriticalSection(&cond->monitor);
	if (cond->to_unblock != 0) {
		if (cond->blocked == 0) {
			LeaveCriticalSection(&cond->monitor);
			return;
		}
		if (broadcast) {
			cond->to_unblock += nsignal = cond->blocked;
			cond->blocked = 0;
		} else {
			nsignal = 1;
			cond->to_unblock++;
			cond->blocked--;
		}
	} else if (cond->blocked > cond->gone) {
		WaitForSingleObject(cond->sem_gate, INFINITE);
		if (cond->gone != 0) {
			cond->blocked -= cond->gone;
			cond->gone = 0;
		}
		if (broadcast) {
			nsignal = cond->to_unblock = cond->blocked;
			cond->blocked = 0;
		} else {
			nsignal = cond->to_unblock = 1;
			cond->blocked--;
		}
	}
	LeaveCriticalSection(&cond->monitor);

	if (0 < nsignal)
		ReleaseSemaphore(cond->sem_queue, nsignal, NULL);
}

static BOOL impl_cond_do_wait(gpr_cv *cond, CRITICAL_SECTION *mtx, DWORD timeout_max_ms)
{
	int nleft = 0;
	int ngone = 0;
	BOOL timeout = 0;
	DWORD w;

	WaitForSingleObject(cond->sem_gate, INFINITE);
	cond->blocked++;
	ReleaseSemaphore(cond->sem_gate, 1, NULL);

	EnterCriticalSection(mtx);

	w = WaitForSingleObject(cond->sem_queue, timeout_max_ms);
	timeout = (w == WAIT_TIMEOUT);

	EnterCriticalSection(&cond->monitor);
	if ((nleft = cond->to_unblock) != 0) {
		if (timeout) {
			if (cond->blocked != 0) {
				cond->blocked--;
			} else {
				cond->gone++;
			}
		}
		if (--cond->to_unblock == 0) {
			if (cond->blocked != 0) {
				ReleaseSemaphore(cond->sem_gate, 1, NULL);
				nleft = 0;
			} else if ((ngone = cond->gone) != 0) {
				cond->gone = 0;
			}
		}
	} else if (++cond->gone == INT_MAX / 2) {
		WaitForSingleObject(cond->sem_gate, INFINITE);
		cond->blocked -= cond->gone;
		ReleaseSemaphore(cond->sem_gate, 1, NULL);
		cond->gone = 0;
	}
	LeaveCriticalSection(&cond->monitor);

	if (nleft == 1) {
		while (ngone--)
			WaitForSingleObject(cond->sem_queue, INFINITE);
		ReleaseSemaphore(cond->sem_gate, 1, NULL);
	}

	LeaveCriticalSection(mtx);
	return timeout;
}


#endif /* GPR_WIN32 */
