#ifndef _PCN_PORTING_H
#define _PCN_PORTING_H

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/mman.h>
# include <sys/wait.h>
# include <fcntl.h>
# include <unistd.h>
# include <stdio.h>
# include <stdlib.h>
# include <time.h>
# include <string.h>
# include <assert.h>
# define __STDC_FORMAT_MACROS
# include <inttypes.h>
# include <sys/mman.h>
# include <sched.h>
# include <pthread.h>
# include <errno.h>
# include <poll.h>
# include <time.h>
# include <signal.h>
# include <assert.h>
# include <arch.h>
#else
# include <linux/kernel.h>
# include <linux/string.h>
# include <linux/slab.h>
# include <linux/sched.h>
# include <linux/kthread.h>
# include <linux/vmalloc.h>
# include <linux/module.h>
# include <linux/jiffies.h>
# include <linux/mm.h>
# include <linux/spinlock.h>
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

#define PCIE_CLOUD_NETWORK_DEBUG 1
#undef PCIE_CLOUD_NETWORK_DEBUG

/**
 * minimalistic porting layer for linux kernel and userspace
 * : some codes was took from pci-ring-buffer.
 */

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
typedef pthread_spinlock_t pcn_spinlock_t;
#else
typedef spinlock_t         pcn_spinlock_t;
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

static inline
void *pcn_malloc(size_t size)
{
        return malloc(size);
}

static inline
void pcn_free(void *ptr)
{
        free(ptr);
}

static inline
void *pcn_calloc(size_t nmemb, size_t size)
{
        return calloc(nmemb, size);
}

static inline
void pcn_spin_init(pthread_spinlock_t *lock)
{
	pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE);
}

static inline
void pcn_spin_deinit(pthread_spinlock_t *lock)
{
	pthread_spin_destroy(lock);
}

static inline
void pcn_spin_lock(pthread_spinlock_t *lock)
{
	pthread_spin_lock(lock);
}

static inline
void pcn_spin_unlock(pthread_spinlock_t *lock)
{
	pthread_spin_unlock(lock);
}

static inline
int pcn_mutex_init(pthread_mutex_t *mutex)
{
	return pthread_mutex_init(mutex, NULL);
}

static inline
int pcn_mutex_destroy(pthread_mutex_t *mutex)
{
	return pthread_mutex_destroy(mutex);
}

static inline
int pcn_mutex_lock(pthread_mutex_t *mutex)
{
	return pthread_mutex_lock(mutex);
}

static inline
int pcn_mutex_unlock(pthread_mutex_t *mutex)
{
	return pthread_mutex_unlock(mutex);
}

static inline
int pcn_wait_init(pthread_cond_t *wait)
{
	return pthread_cond_init(wait, NULL);
}

static inline
int pcn_wait_destroy(pthread_cond_t *wait)
{
	return pthread_cond_destroy(wait);
}

static inline
int pcn_wait_wake_up(pthread_cond_t *wait)
{
	return pthread_cond_signal(wait);
}

static inline
int pcn_wait_wake_up_all(pthread_cond_t *wait)
{
	return pthread_cond_broadcast(wait);
}

static inline
int pcn_wait_sleep_on(pthread_cond_t *wait, pthread_mutex_t *mutex)
{
	return pthread_cond_wait(wait, mutex);
}

static inline
int pcn_wait_sleep_on_timeout(pthread_cond_t *wait, pthread_mutex_t *mutex,
			      struct timespec *timeout)
{
	return pthread_cond_timedwait(wait, mutex, timeout);
}

static inline
void pcn_yield(void)
{
	smp_mb();
	sched_yield();
	smp_rmb();
}

static inline
int __conv_scif_ret(int rc)
{
        return (rc >= 0) ? rc : -errno;
}
#else /* PCIE_CLOUD_NETWORK_CONF_KERNEL */
static inline
void *pcn_malloc(size_t size)
{
        return kmalloc(size, GFP_KERNEL);
}

static inline
void pcn_free(void *ptr)
{
        kfree(ptr);
}

static inline
void *pcn_calloc(size_t nmemb, size_t size)
{
        return kzalloc(nmemb * size, GFP_KERNEL);
}

static inline
void pcn_spin_init(spinlock_t *lock)
{
	spin_lock_init(lock);
}

static inline
void pcn_spin_deinit(spinlock_t *lock)
{
	/* do nothing */
}

static inline
void pcn_spin_lock(spinlock_t *lock)
{
	spin_lock(lock);
}

static inline
void pcn_spin_unlock(spinlock_t *lock)
{
	spin_unlock(lock);
}

static inline
int pcn_mutex_init(struct mutex *mutex)
{
	mutex_init(mutex);
	return 0;
}

static inline
int pcn_mutex_destroy(struct mutex *mutex)
{
	mutex_destroy(mutex);
	return 0;
}

static inline
int pcn_mutex_lock(struct mutex *mutex)
{
	mutex_lock(mutex);
	return 0;
}

static inline
int pcn_mutex_unlock(struct mutex *mutex)
{
	mutex_unlock(mutex);
	return 0;
}

static inline
int pcn_wait_init(wait_queue_head_t *wait)
{
	init_waitqueue_head(wait);
	return 0;
}

static inline
int pcn_wait_destroy(wait_queue_head_t *wait)
{
	return 0;
}

static inline
int pcn_wait_wake_up(wait_queue_head_t *wait)
{
	wake_up(wait);
	return 0;
}

static inline
int pcn_wait_wake_up_all(wait_queue_head_t *wait)
{
	wake_up_all(wait);
	return 0;
}

static inline
int pcn_wait_sleep_on(wait_queue_head_t *wait, struct mutex *mutex)
{
	interruptible_sleep_on(wait);
	return 0;
}

static inline
int pcn_wait_sleep_on_timeout(wait_queue_head_t *wait, struct mutex *mutex,
			      struct timespec *timeout)
{
	unsigned long time_jiffie, timeout_jiffie;

	/* calc. timeout in jiffie */
	timeout_jiffie = timespec_to_jiffies(timeout);

	/* XXX: interruptible_sleep_on_timeout() is deprecated in 4.x kerenel */
	time_jiffie = interruptible_sleep_on_timeout(wait, timeout_jiffie);

	/* is the time over? */
	return (time_jiffie >= timeout_jiffie) ? ETIMEDOUT : 0;
}

static inline
void pcn_yield(void)
{
	smp_mb();
	if (need_resched())
		cond_resched();
	else
		schedule();
	smp_rmb();
}

static inline
int __conv_scif_ret(int rc)
{
        return rc;
}
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

#ifndef min
# define min(__x, __y)               ( ((__x) > (__y)) ? (__y) : (__x))
#endif

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
# define copy_to_user(__d,__s,__n)    ({	\
			memcpy(__d, __s,__n);	\
			0;			\
		})
# define copy_from_user(__d,__s,__n)  ({	\
			memcpy(__d, __s,__n);	\
			0;			\
		})
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */


/**
 * debugging and logging
 */
#define pcn_static_assert(__c, __m) typedef		\
	int ___pcn_static_assert___##__LINE__[(__c) ? 1 : -1]

#ifndef PCIE_CLOUD_NETWORK_CONF_KERNEL
# define print_out(...) fprintf(stdout, __VA_ARGS__)
# define print_err(...) fprintf(stderr, __VA_ARGS__)
#else
# define print_out(...) printk(KERN_INFO __VA_ARGS__)
# define print_err(...) printk(KERN_ERR  __VA_ARGS__)
#endif /* PCIE_CLOUD_NETWORK_CONF_KERNEL */

#ifndef PCIE_CLOUD_NETWORK_DEBUG
#define pcn_assert(__cond, __msg)
#define pcn_dbg(...)
#define pcn_dbg2(...)
#define pcn_here()
#else
#define pcn_assert(__cond, __msg) if (!(__cond)) {	\
		int *__p = NULL;			\
		print_err("\033[91m");			\
		print_err(				\
			"[PCN-ASSERT:%s:%04d] %s\n",	\
			__func__, __LINE__,		\
			__msg);				\
                print_err("\033[0m");			\
		assert(0);				\
		*__p = 0;				\
	}
#define pcn_dbg(__fmt, ...)				\
	print_out(					\
		"[PCN-dbg:%s:%04d] " __fmt,		\
		__func__, __LINE__, ##__VA_ARGS__)
#define pcn_dbg2(...)
#define pcn_here()				\
	print_err("[PCN-HERE:%s:%04d] <== \n",	\
		  __func__, __LINE__)
#endif /* RING_BUFFER_DEBUG */

#define pcn_log(__fmt, ...)				\
	print_out(					\
		"[PCN-LOG:%s:%04d] " __fmt,		\
		__func__, __LINE__, ##__VA_ARGS__)

#define pcn_err(__fmt, ...) do {			\
		print_err(					\
			"\033[91m[PCN-ERR:%s:%04d] " __fmt,	\
			__func__, __LINE__, ##__VA_ARGS__);	\
		print_out("\033[0m\n");				\
	} while(0)

#define pcn_wrn(__fmt, ...) do {			\
		print_err(					\
			"\033[91m[PCN-WRN:%s:%04d] " __fmt,	\
			__func__, __LINE__, ##__VA_ARGS__);	\
		print_out("\033[0m\n");				\
	} while(0)

#define pcn_test(__cond, ...) do {				\
		print_out(					\
			"%s[PCN-TEST:%s:%04d] [%s] ",		\
			(__cond) ? "\033[92m" : "\033[91m",	\
			__func__, __LINE__,			\
			(__cond) ? "PASS" : "FAIL");		\
		print_out(__VA_ARGS__);				\
		print_out("\033[0m\n");				\
	} while(0)

#define pcn_test_exit(__cond, ...) do {				\
 		print_out(					\
			"%s[PCN-TEST:%s:%04d] [%s] ",		\
			(__cond) ? "\033[92m" : "\033[91m",	\
			__func__, __LINE__,			\
			(__cond) ? "PASS" : "FAIL");		\
		print_out(__VA_ARGS__);				\
		print_out("\033[0m\n");				\
		if (!(__cond)) exit(1);				\
	} while(0)

#define pcn_trace(__fmt, ...) do {				\
		print_out("\033[93m");				\
		print_out(					\
			"[PCN-TRACE] " __fmt, ##__VA_ARGS__);	\
		print_out("\033[0m\n");				\
	} while(0)

#define pcn_yield_dmsg(__dmsg) do {		\
		pcn_yield();			\
		pcn_dbg(__dmsg);		\
	} while(0)

#ifndef POLLWRNORM
# define POLLWRNORM     0x0100
#endif
#ifndef POLLWRBAND
# define POLLWRBAND     0x0200
#endif
#ifndef POLLMSG
# define POLLMSG        0x0400
#endif
#ifndef POLLREMOVE
# define POLLREMOVE     0x1000
#endif
#ifndef POLLRDHUP
# define POLLRDHUP      0x2000
#endif
#endif /* PCN_PORTING_H */
