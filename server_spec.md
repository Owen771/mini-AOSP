## OS


PRETTY_NAME="Debian GNU/Linux 11 (bullseye)"
NAME="Debian GNU/Linux"
VERSION_ID="11"
VERSION="11 (bullseye)"
VERSION_CODENAME=bullseye
ID=debian 


## CPU 

Architecture:                            x86_64
CPU op-mode(s):                          32-bit, 64-bit
Byte Order:                              Little Endian
Address sizes:                           46 bits physical, 48 bits virtual
CPU(s):                                  48
On-line CPU(s) list:                     0-47
Thread(s) per core:                      2
Core(s) per socket:                      24
Socket(s):                               1
NUMA node(s):                            1
Vendor ID:                               GenuineIntel
CPU family:                              6
Model:                                   143
Model name:                              Intel(R) Xeon(R) Platinum 8488C
Stepping:                                8
CPU MHz:                                 3568.600
BogoMIPS:                                4800.00
Hypervisor vendor:                       KVM
Virtualization type:                     full
L1d cache:                               1.1 MiB
L1i cache:                               768 KiB
L2 cache:                                48 MiB
L3 cache:                                105 MiB
NUMA node0 CPU(s):                       0-47
Vulnerability Gather data sampling:      Not affected
Vulnerability Indirect target selection: Not affected
Vulnerability Itlb multihit:             Not affected
Vulnerability L1tf:                      Not affected
Vulnerability Mds:                       Not affected
Vulnerability Meltdown:                  Not affected
Vulnerability Mmio stale data:           Not affected
Vulnerability Reg file data sampling:    Not affected
Vulnerability Retbleed:                  Not affected
Vulnerability Spec rstack overflow:      Not affected
Vulnerability Spec store bypass:         Mitigation; Speculative Store Bypass disabled via prctl
Vulnerability Spectre v1:                Mitigation; usercopy/swapgs barriers and __user pointer sanitization
Vulnerability Spectre v2:                Mitigation; Enhanced / Automatic IBRS; IBPB conditional; PBRSB-eIBRS SW sequence; BHI BHI_DIS_S
Vulnerability Srbds:                     Not affected
Vulnerability Tsa:                       Not affected
Vulnerability Tsx async abort:           Not affected
Vulnerability Vmscape:                   Not affected
Flags:                                   fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ss ht 
                                         syscall nx pdpe1gb rdtscp lm constant_tsc arch_perfmon rep_good nopl xtopology nonstop_tsc cpuid aperfmp
                                         erf tsc_known_freq pni pclmulqdq monitor ssse3 fma cx16 pdcm pcid sse4_1 sse4_2 x2apic movbe popcnt tsc_
                                         deadline_timer aes xsave avx f16c rdrand hypervisor lahf_lm abm 3dnowprefetch cpuid_fault ssbd ibrs ibpb
                                          stibp ibrs_enhanced fsgsbase tsc_adjust bmi1 avx2 smep bmi2 erms invpcid avx512f avx512dq rdseed adx sm
                                         ap avx512ifma clflushopt clwb avx512cd sha_ni avx512bw avx512vl xsaveopt xsavec xgetbv1 xsaves avx_vnni 
                                         avx512_bf16 wbnoinvd ida arat avx512vbmi umip pku ospke waitpkg avx512_vbmi2 gfni vaes vpclmulqdq avx512
                                         _vnni avx512_bitalg tme avx512_vpopcntdq rdpid cldemote movdiri movdir64b md_clear serialize amx_bf16 av
                                         x512_fp16 amx_tile amx_int8 flush_l1d arch_capabilities


## Memory 

               total        used        free      shared  buff/cache   available
Mem:            92Gi        27Gi       8.4Gi       235Mi        56Gi        64Gi
Swap:             0B          0B          0B 

## storage 

Filesystem      Size  Used Avail Use% Mounted on
overlay         500G  416G   85G  84% /
tmpfs            64M     0   64M   0% /dev
/dev/nvme0n1p1  500G  416G   85G  84% /srv/runtime
/dev/loop2       30M   30M     0 100% /srv/runtime/67c7d31617700d7203e3f32b3b047499
tmpfs            90G   28K   90G   1% /vault/secrets
shm              64M  4.0K   64M   1% /dev/shm
tmpfs            90G   12K   90G   1% /srv/certs/mx-financial-sandbox
tmpfs            90G   12K   90G   1% /srv/certs/shared-doortest01
tmpfs            90G   12K   90G   1% /run/secrets/kubernetes.io/serviceaccount
tmpfs            47G     0   47G   0% /proc/acpi
tmpfs            47G     0   47G   0% /sys/firmware
/dev/loop1       81M   81M     0 100% /srv/dynamic-values/be8caa8cdc1cf3b6dcf94ef94cba09ab
/dev/loop3      1.0M  1.0M     0 100% /srv/dynamic-values-modern/55564c4279ea4b4d7fa14cf0bc1143b0 
