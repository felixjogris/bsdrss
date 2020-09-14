#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <time.h>

#include <fcntl.h>
#include <kvm.h>
#include <sys/param.h>
#include <sys/sysctl.h>

#ifdef __DragonFly__
#include <sys/kinfo.h>
#include <sys/thread.h>
#endif

#ifdef __FreeBSD__
#include <sys/user.h>
#endif

void bsdrss(int pid)
{
  int cnt;
  long rss;
  time_t startsec;
  struct tm *start;
  char state_buf[22];

#ifdef __OpenBSD__
  struct kinfo_proc kip;
  int mib[6] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, 0, sizeof(kip), 1 };
  const char *state_abbrev[] = {
    "", "start", "run", "sleep", "stop", "zomb", "dead", "onproc"
  };
  int ncpu, mib_ncpu[2] = { CTL_HW, HW_NCPU };
  size_t size;

  mib[3] = pid;
  size = sizeof(kip);
  cnt = sysctl(mib, 6, &kip, &size, NULL, 0);
  if (cnt == -1 || size/sizeof(kip) != 1)
    errx(1, "no kip");

  rss = kip.p_vm_rssize;
  startsec = kip.p_ustart_sec;

  size = sizeof(ncpu);
  if (sysctl(mib_ncpu, 2, &ncpu, &size, NULL, 0) == -1)
    errx(1, "no ncpu");

  if (kip.p_wmesg[0])
    snprintf(state_buf, sizeof(state_buf), "%s", kip.p_wmesg);
  else if ((ncpu > 1) && (kip.p_cpuid != KI_NOCPU))
    snprintf(state_buf, sizeof(state_buf), "%s/%llu",
             state_abbrev[kip.p_stat], kip.p_cpuid);
  else
    snprintf(state_buf, sizeof(state_buf), "%s", state_abbrev[kip.p_stat]);

#elif defined __NetBSD__
  struct kinfo_proc2 *kip;
  kvm_t *kvm;
  const char *state_abbrev[] = {
    "", "IDLE", "RUN", "SLEEP", "STOP", "ZOMB", "DEAD", "CPU"
  };
  int ncpu, mib_ncpu[2] = { CTL_HW, HW_NCPU };
  size_t size;

  kvm = kvm_open(NULL, "/dev/null", NULL, O_RDONLY | KVM_NO_FILES, "bsdrss");
  if (!kvm)
    errx(1, "no kvm");

  kip = kvm_getproc2(kvm, KERN_PROC_PID, pid, sizeof(*kip), &cnt);
  if (!kip || cnt != 1)
    errx(1, "no kip");

  rss = kip->p_vm_rssize;
  startsec = kip->p_ustart_sec;

  size = sizeof(ncpu);
  if (sysctl(mib_ncpu, 2, &ncpu, &size, NULL, 0) == -1)
    errx(1, "no ncpu");

  if (kip->p_cpuid != KI_NOCPU && ncpu > 1) {
    if (kip->p_stat == LSSLEEP) {
        snprintf(state_buf, sizeof(state_buf), "%.6s/%lu", 
                 kip->p_wmesg, kip->p_cpuid);
    } else {
        snprintf(state_buf, sizeof(state_buf), "%.6s/%lu",
                 state_abbrev[(unsigned)kip->p_stat], kip->p_cpuid);
    }
  } else if (kip->p_stat == LSSLEEP) {
    snprintf(state_buf, sizeof(state_buf), "%s", kip->p_wmesg);
  } else {
    snprintf(state_buf, sizeof(state_buf), "%s",
             state_abbrev[(unsigned)kip->p_stat]);
  }

  if (kvm_close(kvm))
    errx(1, "cant close kvm");

#elif defined __DragonFly__
  struct kinfo_proc *kip;
  kvm_t *kvm;
  const char *state_abbrev[] = {
    "", "RUN", "STOP", "SLEEP",
  };
  size_t state;

  kvm = kvm_open(NULL, "/dev/null", NULL, O_RDONLY, "bsdrss");
  if (!kvm)
    errx(1, "no kvm");

  kip = kvm_getprocs(kvm, KERN_PROC_PID, pid, &cnt);
  if (!kip || cnt != 1)
    errx(1, "no kip");

  rss = kip->kp_vm_rssize;
  startsec = kip->kp_start.tv_sec;

  if (kip->kp_stat == SZOMB) {
    snprintf(state_buf, sizeof(state_buf), "%s", "ZOMB");
  } else {
    switch (state = kip->kp_lwp.kl_stat) {
      case LSRUN:
        if (kip->kp_lwp.kl_tdflags & TDF_RUNNING)
          snprintf(state_buf, sizeof(state_buf),
                   "CPU%d", kip->kp_lwp.kl_cpuid);
        else
          snprintf(state_buf, sizeof(state_buf), "%s", "RUN");
        break;
      case LSSLEEP:
        if (kip->kp_lwp.kl_wmesg != NULL) {
          snprintf(state_buf, sizeof(state_buf),
                   "%.8s", kip->kp_lwp.kl_wmesg);
          break;
        }
      /* fall through */
      default:
        if (state < sizeof(state_abbrev)/sizeof(state_abbrev[0]))
          snprintf(state_buf, sizeof(state_buf), "%.6s", state_abbrev[state]);
        else
          snprintf(state_buf, sizeof(state_buf), "?%5lu", state);
        break;
    }
  }

  if (kvm_close(kvm))
    errx(1, "cant close kvm");

#else
  struct kinfo_proc *kip;
  kvm_t *kvm;
  const char *state_abbrev[] = {
    "", "START", "RUN", "SLEEP", "STOP", "ZOMB", "WAIT", "LOCK"
  };
  size_t state;
  int ncpu, mib_ncpu[2] = { CTL_HW, HW_NCPU };
  size_t size;

  kvm = kvm_open(NULL, "/dev/null", NULL, O_RDONLY, "bsdrss");
  if (!kvm)
    errx(1, "no kvm");

  kip = kvm_getprocs(kvm, KERN_PROC_PID, pid, &cnt);
  if (!kip || cnt != 1)
    errx(1, "no kip");

  rss = kip->ki_rssize;
  startsec = kip->ki_start.tv_sec;

  size = sizeof(ncpu);
  if (sysctl(mib_ncpu, 2, &ncpu, &size, NULL, 0) == -1)
    errx(1, "no ncpu");

  switch (state = kip->ki_stat) {
    case SRUN:
      if (ncpu > 1 && kip->ki_oncpu != NOCPU)
        snprintf(state_buf, sizeof(state_buf), "CPU%d", kip->ki_oncpu);
      else
        snprintf(state_buf, sizeof(state_buf), "%s", "RUN");
      break;
    case SLOCK:
      if (kip->ki_kiflag & KI_LOCKBLOCK) {
        snprintf(state_buf, sizeof(state_buf), "*%s", kip->ki_lockname);
        break;
      }
      /* fall through */
    case SSLEEP:
      snprintf(state_buf, sizeof(state_buf), "%.6s", kip->ki_wmesg);
      break;
    default:
      if (state < sizeof(state_abbrev)/sizeof(state_abbrev[0])) {
        snprintf(state_buf, sizeof(state_buf), "%.6s", state_abbrev[state]);
      } else {
        snprintf(state_buf, sizeof(state_buf), "?%5zu", state);
      }
      break;
  }

  if (kvm_close(kvm))
    errx(1, "cant close kvm");

#endif

  rss *= sysconf(_SC_PAGESIZE);
  start = localtime(&startsec);

  printf("pid:%i rss:%li rss:%li KiB state:%s "
         "start:%04u/%02u/%02u "
         "%02u:%02u:%02u\n",
         pid, rss, rss / 1024, state_buf,
         start->tm_year + 1900, start->tm_mon + 1, start->tm_mday,
         start->tm_hour, start->tm_min, start->tm_sec);
}

int main(int argc, char **argv)
{
  if (argc != 2)
    errx(1, "Usage: bsdrss <pid>");

  bsdrss(atoi(argv[1]));

  return 0;
}
