/* libunwind - a platform-independent unwind library
   Copyright (C) 2008 CodeSourcery

This file is part of libunwind.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#include <stdlib.h>
#include <string.h>

#include "unwind_i.h"

#ifdef UNW_REMOTE_ONLY

/* unw_local_addr_space is a NULL pointer in this case.  */
PROTECTED unw_addr_space_t unw_local_addr_space;

#else /* !UNW_REMOTE_ONLY */

static struct unw_addr_space local_addr_space;

PROTECTED unw_addr_space_t unw_local_addr_space = &local_addr_space;

static inline void *
uc_addr (unw_tdep_context_t *uc, int reg)
{
  if (reg >= UNW_ARM_R0 && reg < UNW_ARM_R0 + 16)
    return &uc->regs[reg - UNW_ARM_R0];
  else
    return NULL;
}

# ifdef UNW_LOCAL_ONLY

HIDDEN void *
tdep_uc_addr (unw_tdep_context_t *uc, int reg)
{
  return uc_addr (uc, reg);
}

# endif /* UNW_LOCAL_ONLY */

HIDDEN unw_dyn_info_list_t _U_dyn_info_list;

/* XXX fix me: there is currently no way to locate the dyn-info list
       by a remote unwinder.  On ia64, this is done via a special
       unwind-table entry.  Perhaps something similar can be done with
       DWARF2 unwind info.  */

static int
get_dyn_info_list_addr (unw_addr_space_t as, unw_word_t *dyn_info_list_addr,
			void *arg)
{
  *dyn_info_list_addr = (unw_word_t) &_U_dyn_info_list;
  return 0;
}

static struct sigaction old_sigsegv_handler;
static volatile int sigsegv_protection = 0;
#define SIGSEGV_PROTECT 0x80000000
#define SIGSEGV_RAISED  0x00000001

#define PSR_J_BIT 0x01000000
#define PSR_T_BIT 0x00000020
#define PROCESSOR_MODE(x) (((x) & PSR_J_BIT) >> 23) | \
                          (((x) & PSR_T_BIT) >> 5)

static void
sigsegv_protect ()
{
  sigsegv_protection = SIGSEGV_PROTECT;
}

static int
sigsegv_raised ()
{
  int raised = (sigsegv_protection & SIGSEGV_RAISED) != 0;
  sigsegv_protection = 0;
  return raised;
}

static void
sigsegv_handler (int sig, siginfo_t* si, void* arg)
{
  if (sigsegv_protection & SIGSEGV_PROTECT) {
    sigsegv_protection |= SIGSEGV_RAISED;
    ucontext_t* uc = (ucontext_t*) arg;
    unw_word_t pc = uc->uc_mcontext.arm_pc;
    switch (PROCESSOR_MODE(uc->uc_mcontext.arm_cpsr)) {
      case 0: // ARM
        pc += 4; // each instruction is 32 bits
        break;
      case 1: // Thumb
        pc += 2; // each instruction is 16 bits
        break;
      case 2: // Jazelle
      case 3: // ThumbEE
        /* implement me! */
        break;
    }
    // skip over the faulty instruction
    uc->uc_mcontext.arm_pc = pc;
  } else {
    if ((old_sigsegv_handler.sa_flags & SA_SIGINFO) &&
        old_sigsegv_handler.sa_sigaction) {
      old_sigsegv_handler.sa_sigaction (sig, si, arg);
    } else if (old_sigsegv_handler.sa_handler) {
      old_sigsegv_handler.sa_handler (sig);
    }
  }
}

static void
install_sigsegv_handler ()
{
  struct sigaction sa;
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  sigemptyset (&sa.sa_mask);
  sa.sa_sigaction = sigsegv_handler;
  sigaction (SIGSEGV, &sa, &old_sigsegv_handler);
}

static int
access_mem (unw_addr_space_t as, unw_word_t addr, unw_word_t *val, int write,
	    void *arg)
{
  int valid_access;

  if (write)
    {
      Debug (16, "mem[%x] <- %x\n", addr, *val);

      sigsegv_protect();

      *(unw_word_t *) addr = *val;

      valid_access = !sigsegv_raised();
    }
  else
    {
      sigsegv_protect();

      *val = *(unw_word_t *) addr;

      valid_access = !sigsegv_raised();

      Debug (16, "mem[%x] -> %x\n", addr, *val);
    }
  return valid_access ? 0 : -UNW_EINVAL;
}

static int
access_reg (unw_addr_space_t as, unw_regnum_t reg, unw_word_t *val, int write,
	    void *arg)
{
  unw_word_t *addr;
  unw_tdep_context_t *uc = arg;

  if (unw_is_fpreg (reg))
    goto badreg;

Debug (16, "reg = %s\n", unw_regname (reg));
  if (!(addr = uc_addr (uc, reg)))
    goto badreg;

  if (write)
    {
      *(unw_word_t *) addr = *val;
      Debug (12, "%s <- %x\n", unw_regname (reg), *val);
    }
  else
    {
      *val = *(unw_word_t *) addr;
      Debug (12, "%s -> %x\n", unw_regname (reg), *val);
    }
  return 0;

 badreg:
  Debug (1, "bad register number %u\n", reg);
  return -UNW_EBADREG;
}

static int
access_fpreg (unw_addr_space_t as, unw_regnum_t reg, unw_fpreg_t *val,
	      int write, void *arg)
{
  unw_tdep_context_t *uc = arg;
  unw_fpreg_t *addr;

  if (!unw_is_fpreg (reg))
    goto badreg;

  if (!(addr = uc_addr (uc, reg)))
    goto badreg;

  if (write)
    {
      Debug (12, "%s <- %08lx.%08lx.%08lx\n", unw_regname (reg),
	     ((long *)val)[0], ((long *)val)[1], ((long *)val)[2]);
      *(unw_fpreg_t *) addr = *val;
    }
  else
    {
      *val = *(unw_fpreg_t *) addr;
      Debug (12, "%s -> %08lx.%08lx.%08lx\n", unw_regname (reg),
	     ((long *)val)[0], ((long *)val)[1], ((long *)val)[2]);
    }
  return 0;

 badreg:
  Debug (1, "bad register number %u\n", reg);
  /* attempt to access a non-preserved register */
  return -UNW_EBADREG;
}

static int
get_static_proc_name (unw_addr_space_t as, unw_word_t ip,
		      char *buf, size_t buf_len, unw_word_t *offp,
		      void *arg)
{
  return _Uelf32_get_proc_name (as, getpid (), ip, buf, buf_len, offp);
}

HIDDEN void
arm_local_addr_space_init (void)
{
  memset (&local_addr_space, 0, sizeof (local_addr_space));
  local_addr_space.caching_policy = UNW_CACHE_GLOBAL;
  local_addr_space.acc.find_proc_info = arm_find_proc_info;
  local_addr_space.acc.put_unwind_info = arm_put_unwind_info;
  local_addr_space.acc.get_dyn_info_list_addr = get_dyn_info_list_addr;
  local_addr_space.acc.access_mem = access_mem;
  local_addr_space.acc.access_reg = access_reg;
  local_addr_space.acc.access_fpreg = access_fpreg;
  local_addr_space.acc.resume = arm_local_resume;
  local_addr_space.acc.get_proc_name = get_static_proc_name;
  unw_flush_cache (&local_addr_space, 0, 0);

  install_sigsegv_handler ();
}

#endif /* !UNW_REMOTE_ONLY */