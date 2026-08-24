// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
#ifdef LAB_LOCK
  int nts;
  int n;
#endif
};

#ifdef LAB_LOCK
// Reader-writer lock.
struct rwspinlock {
  // Replace this with your implementation.
  struct spinlock w; // reader and writer
  int readcount;      
  struct spinlock x; // readcount modify
  int writecount;
  struct spinlock y; // writecount modify
  struct spinlock z, r;  // block new readers when a writer is waiting
};
#endif
