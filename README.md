# `throttleA`, syscalls throttler

## Abstract

`throttleA` is a Linux Kernel Module developed in the context of the 2026 edition of the _Advancd Operating Systems and System Security_ class held at _University of Rome, Tor Vergata_, it's main purpose is to delay execution of specific system calls for a set of users and/or program names.

## Installation

### Module build and installation

From the project root, run :

```bash
make all start
```

You'll eventually be prompted for the root password.

### Module management

The module's API is exposed through three `ioctl` nodes, whose default instances are:

- `/dev/throttleA-api` : `ioctl` based node that is controlled by `throttlectl` (see below).
- `/dev/throttleA-uids` : Readable file that allows sys admins to dump all the registered user ids.
- `/dev/throttleA-paths` : Readable file that allows sys admins to dump all the registered program names. `throttlectl` formats its raw input.

### `throttlectl`

`throttlectl` is a client program that allows sysadmins to configure the various aspects of the module through its API, which will now be described :

- `tthrottlectl on|off` : enables/disables the throttler.
- `tthrottlectl status` : checks if the throttler is enabled. 
- `tthrottlectl uid add|rm UID` : adds/removes an UID to/from the monitored list.
- `tthrottlectl path add|rm PATH` : adds/removes a Program Name to/from the monitored list.
- `tthrottlectl path dump` : reads the content from `/dev/throttleA-paths` and prettifies it.
- `tthrottlectl syscalls add|rm NAME1,NAME2,...` : adds/removes a list of syscalls to/from the monitored table.
- `tthrottlectl syscalls dump [-n]` : dumps the list of monitored syscalls, -n selects whether the list is numerical or it's a name list.
- `tthrottlectl stats` : prints the current MAX value and all the required stats.
- `tthrottlectl max MAX` : sets the new MAX value and resets the counters.


## Project structure and module architecture

- `api` : divided in two modules, `api.c` that contains the device driver implementations, and `ioctl.c` that actually validates and performs the operations.

- `hash_table/hash_table.c` : contains the implementations for the data structures and operations that manage the monitored uids. Implemented as an RCU-overflow list hash table that grants readers (the probes that decide whether the thread will sleep or not), a concurrent consultation.

- `hash_table/tree.c` : contains the implementations for the data structures and operations that manage the monitored paths. Implemented as an RCU trie that grants readers (the probes that decide whether the thread will sleep or not), a concurrent consultation. The trie was chosen to reduce memory footprint by sharing common path components.

- `syscalls` : contains the implementations for the data structures and operations that manage the monitored sycalls. Implemented as an RCU register.


- `preempt_kprobe` : contains utils for searching for the kprobe per-cpu location, that enable sleepable kprobes.

- `stats` : The contains all the other relevant module data and config, more precisely:
  - the maximum number of concurrent invocations per second,
  - the average and peak delays of the monitored threads, comprising the peak uid and program,
  - the average and peak number of threads that have been blocked in the various time windows.

  It's implemented as an RCU register.

- `throttler_status` : manages the activation/deactivation of the throttler as well as its status check.

- `timers` : implementation of actual sleeping and wake up mechanism. In order to guarantee fairness and progress, threads that need to sleep use a ticketing system which will give them a token that will be their number, and wakeup condition, in the sleep wait queue, thus creating a FIFO wait queue. Wakeup is performed by a periodic timer that, on activation, will evaluate the time-frame statistics and will unlock at most MAX threads from the sleeping queue by the means of a ticket window.

- `probing` : the heart of the module, its role is to probe a function in the syscall dispatcher stack, `x64_syscall`, and check, if throttling is on, whether the requested syscall and its invoking uid/program are registered, and in case so, if the window allows it, they will sleep.

- `oracles` : contains a util that resolves pathnames.

- `utils` : string utils.