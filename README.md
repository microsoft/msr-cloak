About
=====
This repository contains the following basic experiments for determining low-level properties of TSX. (See the corresponding _Usenix Security Symposium 2017_ [paper](https://aka.ms/msr-cloak) for more details.)

* CodeSetSize: transactionally executes a blob of simple instructions of a given size (in MB). Depending on the configuration in the source, either _nop_ or _inc ecx; inc edx_ is executed many times. This experiment shows that more code than the LLC can hold can be executed transactionally.

* CodeProtectionSize: a victim thread running on core 0 transactionally executes a blob of simple instructions of a given size (in bytes). A synchronized attacker thread running on a given core evicts the victim's code (a) using _clflush_ or (b) by executing conflicting code. This experiment shows that transactions abort when code is evicted from the L1-I through external events. 

* CodeAbortTimings: repeatedly executes a range of experiments in which a victim thread reads/executes code while an attacker thread evicts corresponding memory. The results are written as CSV files. See source and stdout for a brief description of the experiments.

* ReadSetSize: transactionally reads a given amount of bytes; optionally attempts to use large pages. This experiment helps to determine the maximum size of the read set.

* RemainingLeakage: determines the remaining leakage for read/write/execution preloading for a tightly synchronized attacker using Flush+Reload. See Section 5.1.3 of the [paper](https://aka.ms/msr-cloak) for more details. 

_More to come._ 