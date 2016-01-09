#ifndef __PARISC_SPINLOCK_T_H
#define __PARISC_SPINLOCK_T_H

/* LDCW, the only atomic read-write operation PA-RISC has. *sigh*.
 *
 * Note that PA-RISC has to use `1' to mean unlocked and `0' to mean locked
 * since it only has load-and-zero.
 */
#define __ldcw(a) ({ \
	unsigned __ret; \
	__asm__ __volatile__("ldcw 0(%1),%0" : "=r" (__ret) : "r" (a)); \
	__ret; \
})

/*
 * Your basic SMP spinlocks, allowing only a single CPU anywhere
 */

typedef struct {
	volatile unsigned int __attribute__((aligned(16))) lock;
#ifdef CONFIG_DEBUG_SPINLOCK
	volatile unsigned long owner_pc;
	volatile unsigned long owner_cpu;
#endif
} spinlock_t;

#ifndef CONFIG_DEBUG_SPINLOCK
#define SPIN_LOCK_UNLOCKED (spinlock_t) { 1 }

/* Define 6 spinlock primitives that don't depend on anything else. */

#define spin_lock_init(x)       do { (x)->lock = 1; } while(0)
#define spin_is_locked(x)       ((x)->lock == 0)
#define spin_trylock(x)		(__ldcw(&(x)->lock) != 0)
#define spin_unlock(x)		do { (x)->lock = 1; } while(0)

#define spin_unlock_wait(x)     do { barrier(); } while(((volatile spinlock_t *)(x))->lock == 0)

#define spin_lock(x) do { \
	while (__ldcw (&(x)->lock) == 0) \
		while ((x)->lock == 0) ; \
} while (0)

#else

#define SPIN_LOCK_UNLOCKED (spinlock_t) { 1, 0, 0 }

/* Define 6 spinlock primitives that don't depend on anything else. */

#define spin_lock_init(x)       do { (x)->lock = 1; (x)->owner_cpu = 0; (x)->owner_pc = 0; } while(0)
#define spin_is_locked(x)       ((x)->lock == 0)
void spin_lock(spinlock_t *lock);
int spin_trylock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);
#define spin_unlock_wait(x)     do { barrier(); } while(((volatile spinlock_t *)(x))->lock == 0)

#endif

#endif /* __PARISC_SPINLOCK_T_H */
