# Phase 1 Plan Refinement — Design Spec

> **Status:** Draft
> **Date:** 2026-03-22
> **Context:** AOSP cross-check (`docs/reference/05-aosp-cross-check.md`) revealed gaps between our stage plans and real AOSP architecture. This spec defines all corrections needed before Phase 1 implementation begins, plus a high-level Phase 2 roadmap.

---

## Problem

The existing Phase 1 stage docs (S0–S8) have several issues that would cause incorrect architecture or block implementation:

1. All stage docs reference C++ (`.cpp`, `class`, `std::string`) but the codebase uses pure C
2. servicemanager startup uses `usleep(500000)` instead of a proper ready signal
3. Proxy/Stub naming doesn't match AOSP convention (`Bp`/`Bn`)
4. No Looper↔Binder integration step — the two core subsystems are built in isolation
5. PMS needs a manifest format in Stage 4, but it's not defined until Stage 7
6. C↔Kotlin Binder interop (the actual cross-language path) is under-specified
7. Stage 6 app attach sends PID instead of a binder handle (breaks bidirectional IPC)
8. lmkd shown inside system_server in Stage 8 diagram but must be a separate daemon
9. No macOS development strategy despite the project running on Darwin
10. No Phase 2 roadmap for deferred items

## Approach

Surgical fixes to each stage doc. No stage renumbering. No structural reorganization. Fix what's wrong, add what's missing, defer what belongs in Phase 2.

---

## Phase 1 Changes

### Stage 0: Looper + Parcel

**Change: macOS portability strategy**

All native code uses `poll()` as the default event-wait mechanism. `epoll` is a Linux-only optimization that can be swapped in via `#ifdef __linux__`. This decision applies to all stages — Looper, servicemanager, Binder transport, lmkd.

Rationale: the project developer is on macOS (Darwin). `poll()` is POSIX-portable, sufficient for Phase 1 workloads, and conceptually identical to `epoll` (just O(n) instead of O(1) amortized). Real AOSP uses `epoll` because system_server has 100+ fds; we have <20.

**Change: C++ → C references**

Update all doc references from C++ to C:
- `.cpp` → `.c`, `.hpp` → `.h`
- `class Looper` → `struct Looper` + function pointers or plain functions
- `std::string` → `char*` / `const char*`
- `std::unordered_map` → simple array or hand-rolled hash map
- `std::vector<uint8_t>` → `uint8_t*` + `size_t`
- Remove `namespace`, `auto`, `new`/`delete` references

This affects: `stage-0.md` (28 refs), `stage-1.md` (7), `stage-2.md` (18), `stage-3.md` (8), `stage-5.md` (4), `stage-8.md` (4), `summary.md` (4).

---

### Stage 1: init Hardening

**Change: servicemanager.ready mechanism**

Replace `usleep(500000)` with a pipe-based ready signal:

1. Before forking servicemanager, init creates a pipe: `pipe(ready_fd)`
2. init passes the write-end fd to servicemanager (via environment variable `READY_FD` or command-line arg)
3. servicemanager binds its socket, enters event loop, then writes `READY\n` to the pipe
4. init blocks on `read(ready_fd[0])` — proceeds to start the next service only after receiving `READY`
5. If servicemanager crashes before writing READY, init's `read()` returns 0 (EOF) → restart servicemanager

This matches the spirit of AOSP's `property:servicemanager.ready=true` trigger without requiring the full property trigger system (which is a Phase 2 item).

The same pattern can be reused for Zygote readiness (write `READY` after binding `zygote.sock`).

---

### Stage 2: Binder IPC

**Change 1: Bp/Bn naming convention**

All generated and hand-written Proxy/Stub classes use AOSP naming:
- Client-side proxy: `BpXxx` (Binder Proxy)
- Server-side stub: `BnXxx` (Binder Native)
- Interface: `IXxx`

Example: `ICalculator`, `BpCalculator`, `BnCalculator`.

The existing stage docs already mention this convention in the explanation sections but use `XxxProxy`/`XxxStub` in code examples. Update all code examples to use `Bp`/`Bn`.

**Change 2: New Step 2E — Looper↔Binder integration**

After Step 2D (Thread Pool + linkToDeath), add Step 2E:

Goal: servicemanager uses Looper to manage its binder socket instead of raw `poll()`/`select()`.

```
// servicemanager main loop (after Stage 2E):
looper_init(&looper);
looper_add_fd(&looper, binder_socket_fd, on_binder_request, NULL);
looper_loop(&looper);  // blocks, dispatches callbacks
```

This connects Stage 0's Looper infrastructure to Stage 2's Binder transport. Without this step, the two subsystems are islands — Looper exists but nothing uses it until Stage 6.

Verification: servicemanager handles Binder transactions via Looper callbacks (not a manual `accept()` loop).

**Change 3: C↔Kotlin Parcel interop test**

Add an explicit cross-language test in Step 2A:

1. Kotlin system_server sends a Binder transaction (via binary Parcel) to C servicemanager
2. C servicemanager reads the Parcel, processes the request, writes a reply Parcel
3. Kotlin system_server reads the reply

This is the actual production path (Kotlin apps → C servicemanager) and must be verified before Stage 3.

The Stage 0 Parcel interop test (file-based) validates the wire format. This test validates it works over sockets in a real Binder transaction.

---

### Stage 3: servicemanager on Binder

**Change: ready notification**

After servicemanager binds its Binder socket and is ready to accept transactions, it signals readiness to init using the pipe mechanism from Stage 1.

Additionally, servicemanager sets property `servicemanager.ready=true` via the property store (Stage 1B). This allows other services to query readiness without the pipe.

Update the AOSP cross-check table: "ready 通知 🔴 需要加" → resolved.

---

### Stage 4: system_server + Managers

**Change: minimal AndroidManifest.json defined here (not Stage 7)**

PMS stub in Step 4B needs to scan app directories and assign UIDs. It needs a manifest format to read.

Define the minimal format in Stage 4:

```json
{
  "package": "com.miniaosp.helloapp",
  "versionCode": 1,
  "application": {
    "label": "HelloApp",
    "mainClass": "HelloApp"
  }
}
```

This is enough for PMS to:
- Discover installed packages
- Assign UIDs (10000+)
- Map package name → JAR path

Stage 7 **extends** this format with:
- `activities[]` with `intentFilters`
- `services[]` with `exported` flag
- `receivers[]` with `intentFilters`
- `permissions[]`

This eliminates the bootstrap gap where Stage 4 references a format that doesn't exist yet.

---

### Stage 5: Zygote

**Change: explicit fork+exec limitation documentation**

Add a prominent callout at the top of Stage 5:

> **Phase 1 limitation:** We use `fork() + exec("java -jar app.jar")` which starts a fresh JVM for each app process. This means:
> - No copy-on-write memory sharing (the primary benefit of Zygote in real AOSP)
> - ~1-2s startup per app (vs ~10ms in real Android)
> - preload() is effectively a no-op
>
> The Zygote architecture (socket protocol, forkSystemServer, specialize) is structurally identical to real AOSP. Only the fork mechanism differs.
>
> **Phase 2** adds an embedded JVM via `JNI_CreateJavaVM` in the Zygote C process, enabling real fork-without-exec and copy-on-write sharing.

No other structural changes to Stage 5.

---

### Stage 6: App Process + Lifecycle

**Change: app attach passes binder, not just PID**

Update Step 6A (ActivityThread) so that `attachApplication` passes an `IApplicationThread` binder object:

```kotlin
// App side (ActivityThread.kt):
val appThread = ApplicationThread()  // implements IApplicationThread
val ams = ServiceManager.getService("activity") as IActivityManager
ams.attachApplication(appThread)  // passes binder, not PID

// AMS side:
fun attachApplication(appThread: IApplicationThread) {
    val callerPid = Binder.getCallingPid()  // from SO_PEERCRED
    val callerUid = Binder.getCallingUid()
    val proc = ProcessRecord(pid = callerPid, uid = callerUid, appThread = appThread)
    // Store appThread handle — use it to push lifecycle events back
    appThread.scheduleLaunchActivity(activityName, savedState)
}
```

This is how real AOSP works: AMS holds each app's binder handle and uses it for bidirectional communication. Without this, AMS has no way to call back into the app.

The `IApplicationThread.aidl` should be defined:
```
interface IApplicationThread {
    void scheduleLaunchActivity(String activityName, byte[] savedState);
    void schedulePauseActivity();
    void scheduleStopActivity(boolean saveState);
    void scheduleDestroyActivity();
    void scheduleReceiver(String action, byte[] extras);
}
```

---

### Stage 8: AMS + lmkd + BroadcastReceiver

**Change 1: lmkd architecture fix**

lmkd is a separate C daemon started by init. Update the Stage 8 diagram:

```
init
 ├─ servicemanager    (C, handle 0)
 ├─ lmkd              (C, separate daemon)  ← NOT inside system_server
 ├─ zygote            (C)
 └─ [zygote forks]
     └─ system_server  (Kotlin)
         ├─ AMS
         ├─ PMS
         └─ PropertyManager
```

**Change 2: lmkd↔AMS communication via socket**

lmkd and AMS communicate via a dedicated Unix socket (`/tmp/mini-aosp/lmkd.sock`), not Binder. Reason: lmkd must function even if Binder infrastructure (servicemanager) is down.

Protocol:
- AMS → lmkd: `SET_ADJ <pid> <adj>\n` (on every oom_adj change)
- AMS → lmkd: `PROC_REMOVE <pid>\n` (process exited normally)
- lmkd → AMS: `KILL <pid>\n` (lmkd killed a process)

lmkd maintains an in-memory table of `pid → oom_adj`. On memory pressure, it kills the highest-adj (lowest priority) process first, breaking ties by least-recently-used.

---

## Phase 2 Roadmap (High-Level)

Phase 2 takes the working Phase 1 system and evolves it toward real modern Android.

### P2-1: Embedded JVM + Real Zygote
Replace `fork+exec("java -jar")` with `JNI_CreateJavaVM` embedded in Zygote's C process. Enables real fork-without-exec, copy-on-write memory sharing, and class preloading. The single biggest architectural upgrade.

### P2-2: Binder Kernel Driver
Replace Unix socket transport with a minimal `/dev/mini_binder` kernel module. Enables single-copy mmap transfers, kernel-managed handle table, and native `BR_DEAD_BINDER` notifications. Hardest Phase 2 item.

### P2-3: HAL (Hardware Abstraction Layer)
Simulated hardware services (Camera, Audio, Sensors) behind AIDL interfaces. Apps → Binder → HAL service → fake hardware (file/socket). Demonstrates the HIDL/AIDL HAL architecture.

### P2-4: ContentProvider
Cross-app data sharing with URI-based access (`content://`), cursor-based queries, and permission grants. Builds on Binder + PMS.

### P2-5: Security Hardening
- Simplified SELinux-like MAC (label-based access control on Binder transactions)
- `seccomp` filter on Zygote children
- UID persistence across reboots (`packages.xml`)
- Permission checking at Binder call boundaries

### P2-6: Parcel UTF-16 + FD Passing
Match real AOSP wire format (UTF-16 strings). Add `FileDescriptor` passing via `SCM_RIGHTS` on Unix sockets.

### P2-7: Property Triggers
`on property:X=Y` action trigger system in init. Replaces pipe-based ready signal with property-driven mechanism. Enables `on boot`, `on property:sys.boot_completed=1` patterns.

### P2-8: CI Per-Stage Test Harness
Automated `make test-stage-N` that runs all verification scenarios. Enables regression testing as later stages modify earlier components.

### P2-9: Transaction Pattern for Lifecycle
`ClientTransaction` + `LaunchActivityItem` + `TransactionExecutor` batching. Matches real AOSP's lifecycle dispatch. Optimization, not architecture change.

---

## What's NOT Changing

- Stage numbering (0-8) stays the same
- Stage dependency graph structure stays the same (S0→S1/S2→S3→S4/S5→S6→S7→S8)
- Overall Phase 1 scope stays the same (boot → Binder → services → apps → lifecycle → intents → broadcasts → memory)
- Language split: C for native, Kotlin for framework/apps
- Unix socket as Binder transport (Phase 1)
- fork+exec for Zygote (Phase 1)

## Summary of All Phase 1 Changes

| # | Change | Stage | Type |
|---|--------|-------|------|
| 1 | macOS: `poll()` default, `epoll` via `#ifdef` | S0 | Strategy |
| 2 | C++ → C in all stage docs (73 refs) | S0–S8 | Doc fix |
| 3 | servicemanager.ready via pipe | S1, S3 | New mechanism |
| 4 | Bp/Bn naming convention in all code examples | S2 | Rename |
| 5 | Step 2E: Looper↔Binder integration | S2 | New step |
| 6 | C↔Kotlin Binder transaction interop test | S2 | New test |
| 7 | Minimal AndroidManifest.json defined in S4 | S4 | Move earlier |
| 8 | fork+exec limitation callout | S5 | Doc addition |
| 9 | App attach passes IApplicationThread binder | S6 | Architecture fix |
| 10 | lmkd as separate daemon (fix diagram) | S8 | Architecture fix |
| 11 | lmkd↔AMS via socket protocol | S8 | Architecture fix |
