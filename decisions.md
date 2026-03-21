# Mini-AOSP Decision Log

Architectural decisions and their rationale. Review this when revisiting past choices.

---

## DEC-001: Use AIDL for Service Interfaces (not gRPC)

**Date**: 2026-03-16
**Status**: Decided
**Context**: We need an IDL for defining service interfaces across processes. gRPC with protobuf was considered as an alternative — it's easier to set up, works natively in C++ and Kotlin, and has great tooling.

**Decision**: Use `.aidl` files like real AOSP.

**Alternatives Considered**:

| Option | Pros | Cons |
|---|---|---|
| **AIDL (chosen)** | Mirrors AOSP exactly; teaches Parcel serialization, handle-based addressing, Proxy/Stub pattern as AOSP implements it | Must build our own codegen and transport |
| **gRPC + protobuf** | Clean `.proto` files; cross-language C++/Kotlin; well-documented; easy to debug | Misses Parcel format, handle-based addressing, caller UID/PID stamping, `linkToDeath`; HTTP/2 transport is heavier than needed |
| **Hybrid (protobuf serialization + custom transport)** | Clean serialization + learn transport layer | Neither fully AOSP nor fully standard tooling |

**Rationale**: The primary goal is learning AOSP internals. AIDL is core to how Android services communicate — the Proxy/Stub codegen, Parcel wire format, and handle model are all important concepts. Using gRPC would teach general RPC but skip the Android-specific patterns.

**Revisit if**: The AIDL codegen becomes a bottleneck for development velocity. Could swap to protobuf for serialization while keeping custom transport.

---

## DEC-002: OpenJDK 17+ as Runtime (not ART)

**Date**: 2026-03-16
**Status**: Decided
**Context**: Real Android uses ART (Android Runtime) with DEX bytecode, JIT/AOT compilation, and custom GC. Building even a toy ART would be a massive standalone project.

**Decision**: Use OpenJDK 17+ for Phase 1. ART is deferred to Phase 2.

**Rationale**: The lifecycle contract, process model, and IPC are identical regardless of runtime. OpenJDK 17+ also gives us built-in Unix domain socket support (JEP 380). ART's value is in execution optimization, not architecture.

**Revisit if**: We reach Phase 2 and want to explore bytecode execution, GC design, or JIT/AOT compilation.

---

## DEC-003: Unix Domain Sockets for Binder Transport

**Date**: 2026-03-16
**Status**: Decided
**Context**: Real Binder uses a kernel driver (`/dev/binder`) with mmap for single-copy IPC. We need cross-process communication between C++ and Kotlin processes.

**Decision**: Use Unix domain sockets (`AF_UNIX`) with `SO_PEERCRED` for caller identity.

**Alternatives Considered**:

| Option | Pros | Cons |
|---|---|---|
| **Unix sockets (chosen)** | Works in both C++ (POSIX) and Kotlin (JEP 380); `SO_PEERCRED` gives caller UID/PID; no kernel module needed | Double-copy (user→kernel→user); no priority inheritance |
| **Real Binder driver** | Authentic; single-copy; priority inheritance | Requires kernel module loading or ACK kernel |
| **Shared memory + eventfd** | Near-zero-copy | Complex synchronization; no caller identity |

**Rationale**: Unix sockets work on stock Ubuntu, both languages support them natively, and `SO_PEERCRED` provides unforgeable caller identity — the key security property of Binder. The double-copy overhead is irrelevant at our scale.

**Revisit if**: Phase 2 kernel work enables real `/dev/binder`; or if performance profiling shows socket overhead matters.

---

## DEC-004: Real AOSP Names (not m-prefixed)

**Date**: 2026-03-16
**Status**: Decided
**Context**: Originally used `minit`, `mservicemanager`, `mzygote`, `ActivityManagerLite` etc. to distinguish from real AOSP.

**Decision**: Use exact AOSP names (`init`, `servicemanager`, `zygote`, `ActivityManagerService`) with explicit "What's Missing vs Real AOSP" documentation for each component.

**Rationale**: The learning goal is AOSP. Using real names means navigating our codebase teaches real AOSP navigation. The "What's Missing" tables make divergences explicit rather than hidden behind a name change.

---

## DEC-005: Plain Makefile for Build System

**Date**: 2026-03-16
**Status**: Decided
**Context**: Need to build both C++ binaries and Kotlin JARs in a monorepo.

**Alternatives Considered**:

| Option | Pros | Cons |
|---|---|---|
| **Makefile (chosen)** | Simplest; zero framework; works everywhere | No dependency management; manual classpath |
| **Bazel** | Polyglot monorepo; hermetic; AOSP uses similar (Soong) | Steep learning curve; heavy setup |
| **CMake + Gradle** | Best-in-class per language | Two build systems to maintain |

**Rationale**: Educational project — minimize build system complexity so focus stays on OS concepts. Can migrate to Bazel later if needed.

---

## DEC-006: JSON Manifests (not binary AndroidManifest.xml)

**Date**: 2026-03-16
**Status**: Decided
**Context**: Real Android uses binary XML in APK files, parsed by `PackageParser`.

**Decision**: Use JSON manifest files per app.

**Rationale**: Binary XML parsing is mechanical complexity that doesn't teach architecture. JSON is human-readable, easy to edit, and trivial to parse in both C++ and Kotlin. The manifest *schema* (package name, components, permissions, intent filters) is what matters, not the format.

**Revisit if**: We want to learn APK format internals (Phase 2+).

---

## DEC-007: C++ for Native Layer, Kotlin for Framework/Apps

**Date**: 2026-03-16
**Status**: Decided
**Context**: Real AOSP uses C/C++ for everything below the framework and Java/Kotlin above.

**Decision**: Same split. C++ for `init`, `servicemanager`, `zygote`, Binder library, Looper, lmkd. Kotlin for `system_server`, all framework services, all apps.

**Boundary**: Zygote (C++) launches Kotlin processes via `fork()` + `exec("java", "-jar", "process.jar")`.

**Rationale**: Mirrors AOSP. C++ is necessary for process control (fork/exec), low-level IPC, and PID-1 init. Kotlin is productive for service logic and app model.

---

## DEC-008: Phase Structure

**Date**: 2026-03-16
**Status**: Decided

| Phase | Scope |
|---|---|
| **Phase 1 — Core** | Boot-to-app loop. All sample apps work. Mobile phone only. 4GB/4core/200proc limits. |
| **Phase 2 — Advanced** | HAL + simulated hardware, ART, ContentProvider, custom kernel modifications, other form factors |

**Rationale**: Phase 1 captures the essential Android architecture without getting bogged down in hardware abstraction or runtime internals. Phase 2 adds depth.

---

## DEC-009: Full AOSP Lifecycle Model (7 callbacks + save/restore)

**Date**: 2026-03-16
**Status**: Decided
**Context**: Originally planned 6 simplified callbacks (no `onRestart`, no `onSaveInstanceState`).

**Decision**: Implement the full AOSP Activity lifecycle:
- 7 callbacks: `onCreate`, `onStart`, `onResume`, `onPause`, `onStop`, `onRestart`, `onDestroy`
- `onSaveInstanceState` / `onRestoreInstanceState` for surviving process death
- Killable-state rules (only after `onStop`)
- A-starts-B ordering (`onPause(A)` before `onCreate(B)`)
- Service restart policies (`START_STICKY` / `START_NOT_STICKY` / `START_REDELIVER_INTENT`)
- Priority inheritance via bindings

**What's still simplified**: No task/back stack, no launch modes, no config change handling, no ANR detection.

**Rationale**: `onSaveInstanceState` is essential — without it, lmkd kills lose all transient state and the kill/restart flow doesn't work properly. Service restart policies are needed for AMS to know what to do after process death.

---

## DEC-010: Resource Constraints — Mid-Range 2024 Phone

**Date**: 2026-03-16
**Status**: Decided
**Context**: Need to simulate realistic Android phone constraints.

**Decision**: Target a 2024 mid-range phone (Pixel 8 / Galaxy A56 class):
- 4 GB RAM (hard limit via cgroups v2)
- 4 CPU cores
- 200 max processes
- Enforced via cgroups v2 or `systemd-run`

**Rationale**: 4 GB is tight enough to exercise memory management (real mid-range has 8 GB, but ~4 GB is consumed by the OS). Forces lmkd to actually make kill decisions, which is where the learning happens.

---

## DEC-011: Kernel — ACK or Ubuntu with Binder Module

**Date**: 2026-03-16
**Status**: Decided
**Context**: Need a kernel that supports our userspace. Three options identified.

**Decision**: Try Ubuntu kernel + `modprobe binder_linux` first (simplest). Fall back to building ACK x86_64 from source if needed. Phase 2 will modify a pure upstream Linux kernel.

**Rationale**: Start with least friction. If the Ubuntu kernel already has the binder module, we get real kernel Binder with zero build time. ACK build is the fallback for full Android kernel features.

---

## DEC-012: Skip HAL in Phase 1

**Date**: 2026-03-16
**Status**: Decided
**Context**: HAL is the abstraction layer between framework and hardware. No real hardware to abstract on Ubuntu.

**Decision**: Skip entirely in Phase 1. Phase 2 adds HAL interfaces with simulated hardware (Camera, Audio, Sensors).

**Rationale**: HAL is architecturally important but not required for the core boot→app lifecycle loop. Deferring it keeps Phase 1 focused.
