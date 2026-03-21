## Mini AOSP

Learn Android internals (AOSP) by building a minimal, educational Android-shaped OS from scratch.

this only can run on a server `./server_spec.md`, and the design is around it

### Role

Read `./role.md` first — it defines the tutor/learning-coach interaction model.

### Project Files

| File | Description |
|---|---|
| [`phase-1-core.md`](./phase-1-core.md) | **Phase 1 — Core**: Scoping, architecture, file structure, build system, and the boot-to-app-lifecycle loop (Stages 0–8). This is the active working document. |
| [`phase-2-advanced.md`](./phase-2-advanced.md) | **Phase 2 — Advanced**: HAL layer, ART, simulated hardware (Camera, Audio, Sensors), custom kernel modifications, ContentProvider. To be designed after Phase 1 is complete. |
| [`decisions.md`](./decisions.md) | **Decision Log**: All architectural decisions with rationale, alternatives considered, and conditions for revisiting. |
| [`role.md`](./role.md) | Tutor role definition — guides the AI interaction style. |

### Goal

Build a minimum set of core components that lets apps run on top, mirroring AOSP's real architecture as closely as possible for learning purposes.

### Workflow

Tasks and results are recorded in the phase files. Follow-up questions go here or in the terminal. Once a task is answered and reviewed, it gets marked done.

---

### Task List

### Task 1: Scope and define mini-AOSP ✅

Mirror AOSP as closely as possible, but separate non-essential features into Phase 2. → See `phase-1-core.md` Task 1

### Task 2: Define file structure ✅

Design a directory layout for C++, Kotlin, and build files that works for both developers and AI agents. → See `phase-1-core.md` Task 2

### Task 3: Empty prototype that compiles, builds, and layers can interact ✅ (research done, implementation next)

- Q1: What Linux kernel does AOSP use, and why?
- Q2: Does AOSP modify the kernel, or just use it?

→ See `phase-1-core.md` Task 3 for answers and prototype plan


### Task X : write up guidline and tutorials for learner 

we should 

1. breakdown to different steps, with clean goals on each step, e.g we can do 1 feature top down across layers, or a component is important and we need to get it done first
2. probably we need to think about what's easy, then essential, based on that to generate a DAG learning path 
3. each step should have proper guideline to follow, and good pointer, like what material / concept need to figure out by the learner, you can just put the term there, if the learner dont know what's it, let them figure it out by then 
4. some hints would be nice 
5. never write the exact code / the full answer, instead you can point out how AOSP do it, leave room for learner to explore and think about the concrete design 

