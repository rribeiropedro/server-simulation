# Arena Server Engine — Build Plan

A guide for building a multiplayer game server engine in C++, designed for someone comfortable with C++/STL but new to threading and concurrency.

Each section has three parts:
- **Learn** — videos, reading, and conceptual understanding
- **Practice** — isolated exercises to drill the concepts before touching the project
- **Build** — features added to the server engine

---

# Weeks 1–8: Server Engine
Budget: 10-15 hrs/week

---

## Week 1: Learn Concurrency Fundamentals
**Goal:** Understand threads, mutexes, and race conditions before writing project code.

### Learn (4-5 hrs)
**Videos:**
- Bo Qian's C++ Threading playlist (YouTube: "Bo Qian C++ Concurrency")
  - Watch videos 1-6 covering `std::thread`, `std::mutex`, `std::lock_guard`, data races
  - ~15 min each, pause and code along
- CppCon 2017: "An Introduction to Multithreading in C++0x" by Anthony Williams
  - Search YouTube: "CppCon Anthony Williams multithreading introduction"

**Reading:**
- C++ Reference pages for `<thread>`, `<mutex>`, `<atomic>` (cppreference.com)
- Chapter 1-2 of "C++ Concurrency in Action" by Anthony Williams (the definitive book — get a copy or find the PDF, it's worth owning for your career)

### Practice (4-5 hrs)
**Exercise 1 — Thread basics:**
Write a program that spawns 4 threads, each printing its thread ID and a counter 1-100. Run it. Watch the output interleave chaotically. This is your first race condition. Fix it with `std::mutex`.

**Exercise 2 — Shared counter:**
Create a shared `int counter = 0`. Spawn 10 threads that each increment it 100,000 times. Print the final value. It won't be 1,000,000. Fix it three ways:
1. `std::mutex` + `std::lock_guard`
2. `std::atomic<int>`
3. `std::mutex` + `std::unique_lock`

Understand why each works and when you'd pick one over another.

**Exercise 3 — Thread lifecycle:**
Write a small program that creates a `std::thread`, moves it to another variable with `std::move`, and joins it. Then try copying a thread — observe the compile error. Understand why threads are movable but not copyable (what would it mean to duplicate a running execution context?).

### Build (1-2 hrs)
No server features yet — set up the project skeleton:
- Create the repo with `src/`, `src/common/`, `src/engine/`, `src/ingestion/`, `src/network/`, `src/race-control/`
- Add a `CMakeLists.txt` stub (just `project()` + `add_executable` with an empty `main.cpp`)
- Add `.gitignore` for build artifacts
- Verify it compiles with `cmake -B build && cmake --build build`

### Checkpoint
You can explain the difference between `lock_guard` and `unique_lock`, and you know why `atomic` is faster for simple operations. Your repo exists and compiles.

---

## Week 2: Build the Ring Buffer
**Goal:** Understand condition variables, then build the project's core data structure.

### Learn (3-4 hrs)
**Videos:**
- Bo Qian's condition variable video (YouTube: "Bo Qian condition variable")
- CppCon 2015: "C++ Atomics, From Basic to Advanced" by Fedor Pikus (watch first 30 min for mental model)

**Reading:**
- cppreference page for `std::condition_variable` — read every example
- "C++ Concurrency in Action" Chapter 4 (synchronizing concurrent operations)

### Practice (2-3 hrs)
**Exercise — Producer-consumer with raw primitives:**
Before building the ring buffer, write a minimal producer-consumer using just a `std::queue`, a `std::mutex`, and a `std::condition_variable`. One thread pushes integers 0-99. Another thread pops and prints them. Use `cv.wait(lock, predicate)` to block the consumer when the queue is empty. This isolates the condition variable pattern so you understand it before combining it with circular buffer logic.

Then break it on purpose: remove the predicate from `cv.wait()` and observe what happens with spurious wakeups. Add the predicate back. Understand why it's essentially `while (!pred()) { cv.wait(lock); }`.

### Build (6-8 hrs)
**Add the ring buffer to the project (`src/ingestion/RingBuffer.h`):**

Build in stages:

Stage 1 — Single-threaded circular buffer:
- Template class `RingBuffer<T>` with fixed capacity (1024)
- `push(T)` and `pop() -> T` using head/tail indices
- Test: push 10 items, pop 10 items, verify FIFO order

Stage 2 — Add mutex protection:
- Wrap push/pop with `std::mutex`
- Test: one thread pushes, another pops concurrently

Stage 3 — Replace busy-waiting with condition variables:
- `cv_not_full_` for producer (waits when buffer is full)
- `cv_not_empty_` for consumer (waits when buffer is empty)
- Use `std::unique_lock` + `cv.wait(lock, predicate)`

Stage 4 — Add graceful shutdown:
- `shutdown()` method sets `std::atomic<bool>` flag
- `cv.notify_all()` wakes blocked threads
- `push`/`pop` return `false` after shutdown

**Test harness:**
One producer thread pushes integers 0-9999 into the buffer. One consumer pops and verifies they arrive in order. Print throughput (items/sec). Run with different buffer sizes (16, 64, 256, 1024) and observe behavior.

### Checkpoint
Your ring buffer passes the ordering test and handles shutdown cleanly. You can explain why condition variables are better than `while(!empty) { yield; }`.

---

## Week 3: Game State & Tick Engine
**Goal:** Build a deterministic fixed-rate game loop producing game state snapshots.

### Learn (3-4 hrs)
**Reading:**
- Glenn Fiedler, "Fix Your Timestep!" — https://gafferongames.com/post/fix_your_timestep/
  - THE article on fixed-rate game loops. Read it twice. Understand the accumulator pattern.
- Glenn Fiedler, "Networked Physics" series on gafferongames.com (skim the first 2 articles)

**Videos:**
- GDC: "Overwatch Gameplay Architecture and Netcode" (YouTube, ~1hr)
  - Shows how a real AAA game structures its tick loop and entity system
  - You don't need to understand everything — focus on the tick/update/send cycle

### Practice (2-3 hrs)
**Exercise — Fixed-rate loop:**
Write a standalone program that runs a loop at exactly 64Hz using `std::chrono::steady_clock` and `sleep_until`. Each iteration, print the tick number and measure the actual elapsed time since the last tick. Observe how `sleep_until` maintains a steady cadence while `sleep_for` would drift over time. Run it for 10 seconds and verify you get exactly ~640 ticks.

Then stress it: add a random workload (0-10ms of busy work) inside each tick. Watch how the loop catches up when a tick runs long. This is the accumulator pattern from Glenn Fiedler's article — see it in action before building the real engine.

### Build (6-8 hrs)
**Add game state types and the tick engine to the project:**

Define your data structures (`src/common/types.h`):

Note: We use `Vec3` and `std::vector` from the start even though weeks 1-8 only use 2D movement with 10 players. This avoids a painful refactor later when Phase 2 adds 3D rendering and Phase 3 scales to 200+ entities. During weeks 1-8, just set `z = 0` everywhere and initialize with 10 players.

```cpp
struct Vec3 { float x, y, z; };

struct PlayerState {
    int id;
    Vec3 position;
    Vec3 velocity;
    float health;        // 0-100
    bool alive;
    int last_input_tick;  // for lag comp later
};

struct GameState {
    uint64_t tick_number;
    std::vector<PlayerState> players;  // dynamic — 10 now, 200+ later
    uint64_t timestamp_ns;
};

struct PlayerInput {
    int player_id;
    Vec3 move_direction;  // normalized
    bool shoot;
    uint64_t tick_number;  // which tick this input is for
};
```

**Build the tick engine (`src/engine/TickEngine.cpp`):**
- Fixed-rate loop at 64Hz (15.625ms per tick) using `std::chrono::steady_clock`
- Each tick: read input queue → update positions → resolve collisions → push GameState to ring buffer
- Simple arena: 500x500 unit square (z = 0 for now), players bounce off walls
- Simple combat: if two players are within 20 units, the one with higher speed deals 10 damage
- Initialize 10 players at random positions with random velocities
- Measure actual tick duration — print a warning if any tick exceeds 15ms
- Use `sleep_until` for timing, not `sleep_for` (avoids drift)

### Checkpoint
Your tick engine runs at steady 64Hz, produces GameState frames, and pushes them through the ring buffer to a consumer that prints position updates for all 10 players.

---

## Week 4: Terminal Dashboard + CMake
**Goal:** Visualize the simulation in real-time and finalize the build system.

### Learn (2-3 hrs)
**Reading:**
- ANSI escape codes reference (search "ANSI escape codes terminal colors")
  - `\033[H` = cursor home, `\033[2J` = clear screen, `\033[38;5;Xm` = 256-color
- CMake tutorial: "An Introduction to Modern CMake" — https://cliutils.gitlab.io/modern-cmake/
  - Read chapters 1-3 only. You need `project()`, `add_executable()`, `target_link_libraries(Threads::Threads)`

### Practice (1-2 hrs)
**Exercise — ANSI rendering:**
Write a standalone program that clears the terminal and draws a 10-row "leaderboard" that updates 5 times per second. Each row shows a player name and a health bar made of `█` characters that slowly decreases. Color the bars green/yellow/red based on value. This isolates the terminal rendering logic so you're not debugging ANSI codes and game state bugs at the same time.

### Build (7-9 hrs)
**Add the terminal dashboard to the project:**
The consumer thread reads GameState from the ring buffer and renders a live display:
- Clear screen each frame (ANSI `\033[H\033[2J`)
- Show all 10 players sorted by health (descending)
- Per player: ID, position (x,y), speed, health bar, alive/dead status
- Color-code health: green (>70), yellow (30-70), red (<30)
- Show tick number, real-time tick rate, buffer occupancy (how full the ring buffer is)
- Throttle display to ~15fps (you don't need to redraw every tick)

**Finalize CMake build system:**
```cmake
cmake_minimum_required(VERSION 3.16)
project(arena-server-engine LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(Threads REQUIRED)
add_executable(arena-server
    src/main.cpp
    src/engine/TickEngine.cpp
)
target_include_directories(arena-server PRIVATE src)
target_link_libraries(arena-server Threads::Threads)
```

**Write the initial README** — explain the architecture, not just how to compile.

### Checkpoint
Running `./arena-server` shows a live terminal display with 10 players moving, fighting, and dying. Buffer occupancy stays near 0 (consumer keeps up). You can build with `cmake -B build && cmake --build build`.

---

## Week 5: Network Simulator
**Goal:** Simulate unreliable network connections between the server and 10 players.

### Learn (4-5 hrs)
**Reading (this is the most important reading week):**
- Gabriel Gambetta, "Fast-Paced Multiplayer" series — https://www.gabrielgambetta.com/client-server-game-architecture.html
  - Read ALL 4 articles + study the live demo
  - Part 1: Client-Server Architecture (authoritative server model)
  - Part 2: Client-Side Prediction and Server Reconciliation
  - Part 3: Entity Interpolation
  - Part 4: Lag Compensation
  - **This series is the single best resource for what you're building. Bookmark it.**

**Videos:**
- "Valorant Netcode & 128-Tick Servers" (YouTube, Riot Games tech talk)
  - Practical example of how a real competitive game handles tick rate and networking

### Practice (2-3 hrs)
**Exercise — Delay queue:**
Write a standalone program that simulates message delivery with variable delay. Push 100 messages into a `std::priority_queue` sorted by arrival time, where each message has a random delay between 10ms and 200ms. Then drain the queue in a loop, only popping messages whose arrival time has passed. Print each message as it "arrives." Observe that messages sent in order 0-99 arrive out of order. This is the exact mechanism you'll use in the server, isolated from game logic.

### Build (5-7 hrs)
**Add network simulation to the project (`src/network/`):**

**Network profiles:**
Create 10 player profiles with realistic variety. Each profile has a base latency (15-120ms), jitter range (±3-40ms), packet loss rate (0.5-5%), and its own RNG for randomizing delay per input. Group them into archetypes: "fiber" (players 0-2, fast and stable), "cable" (players 3-5, moderate), "wifi" (players 6-7, jittery), and "unstable" (players 8-9, high latency with frequent drops).

**Input delay simulation:**
Each simulated player generates inputs every tick (random movement + occasional shots). Before an input reaches the tick engine, it gets delayed by `base_latency + uniform(-jitter, +jitter)` milliseconds. Some inputs are dropped entirely based on the loss rate. High-jitter inputs can arrive out of order — an input from tick 100 might arrive after tick 102's input.

**Priority queue timing mechanism:**
Use a `std::priority_queue` sorted by simulated arrival time (earliest first). When a player generates an input at tick N, compute its arrival time as `current_time_ns + delay_ms * 1e6` and push it into the queue. Each tick, before processing game logic, the engine drains all entries from the queue whose arrival time is ≤ the current simulation time. These are the inputs that have "arrived" this tick. Everything else stays in the queue for future ticks. This naturally handles out-of-order arrival — a low-jitter input from tick 101 can arrive before a high-jitter input from tick 100.

Track per-player stats: inputs sent, inputs received, inputs dropped, and effective latency (rolling average of actual delay between send and arrival).

**Update dashboard:**
- Add per-player latency display (effective ping)
- Add packet loss indicator
- Show when a player's input was dropped (brief flash)

### Checkpoint
You can see players with high latency reacting sluggishly — their positions update less frequently. Players with packet loss occasionally "skip" inputs. The dashboard shows each player's effective ping.

---

## Week 6: State History Buffer
**Goal:** Store a rolling window of past game states for lag compensation in Week 7.

### Learn (2-3 hrs)
**Reading:**
- Valve Developer Wiki: "Source Multiplayer Networking" (search this exact title)
  - Covers their lag compensation implementation in detail
  - Focus on the "Lag Compensation" section
- Re-read Gabriel Gambetta Part 4 (Lag Compensation) — now it'll click differently

**Videos:**
- "Lag Compensation in Watch Dogs 2" (GDC 2017, YouTube)
  - Shows how Ubisoft implemented server-side rewind for a different genre

### Practice (2-3 hrs)
**Exercise — `std::shared_mutex` reader-writer pattern:**
Write a program with one writer thread and 5 reader threads sharing a `std::vector<int>`. The writer appends a number every 100ms. Each reader reads the entire vector every 50ms and prints its size. Use `std::shared_mutex` with `std::shared_lock` for readers and `std::unique_lock` for the writer. Verify that readers never see a partially-written state (no crashes, no corrupt sizes). Then compare throughput against using a plain `std::mutex` for all access — observe that the shared version is faster when reads outnumber writes.

### Build (6-8 hrs)
**Add the state history buffer to the project (`src/engine/StateHistory.h`):**

Separate from the main ring buffer — this one stores `GameState` snapshots for lag compensation lookups.

```cpp
class StateHistory {
public:
    void store(const GameState& state);  // called by tick engine
    
    // Returns the game state closest to the given tick number
    // Returns nullopt if the tick is too old (outside our window)
    std::optional<GameState> get_state_at_tick(uint64_t tick) const;
    
    // Returns the state at a given time offset from now
    // e.g., get_state_at_offset_ms(45) returns state from ~45ms ago
    std::optional<GameState> get_state_at_offset_ms(float ms_ago) const;
    
private:
    std::array<GameState, 128> buffer_;
    std::atomic<size_t> write_pos_{0};
    mutable std::shared_mutex mutex_;  // allows concurrent reads
};
```

- Capacity: 128 ticks (~2 seconds at 64Hz)
- After each tick, the engine pushes the current GameState into history
- Thread-safe reads using `std::shared_mutex`: the lag compensator queries historical states while the engine writes new ones

**Integrate with the tick engine:** After each tick produces a GameState and pushes it to the ring buffer, also push it to the state history.

**Test:**
- Run the engine for 5 seconds
- Query state history for "where was player 3 at tick 150?"
- Verify the returned state matches what the engine produced at that tick
- Test edge cases: query a tick that's too old (outside window), query the current tick

### Checkpoint
State history stores the last 2 seconds of game states. You can query any past tick and get the correct snapshot. `shared_mutex` allows the lag compensator and the tick engine to operate concurrently without blocking each other.

---

## Week 7: Lag Compensation
**Goal:** Implement server-side rewind — validate player actions against what they actually saw.

### Learn (2-3 hrs)
**Reading:**
- Re-read Gambetta Part 4 one more time — you're implementing this now
- Glenn Fiedler, "Snapshot Interpolation" — gafferongames.com
  - Covers how to interpolate between discrete snapshots

**Concept to internalize:**
When player 8 (120ms ping) shoots at player 2 at tick 500, the server receives this input at ~tick 508. But player 8 was aiming at where player 2 was at tick 500, not tick 508. The lag compensator rewinds the world to tick 500, checks if the shot would have hit, and applies the result at tick 508. This is fair to the shooter but can feel unfair to the target — they might get hit after they thought they'd moved to safety. This tradeoff is the heart of game networking.

### Practice (2-3 hrs)
**Exercise — State rewind logic:**
Write a standalone program that stores 100 snapshots (each just an `int` position moving +1 per tick). Given a "current tick" of 100 and a "player claimed tick" of 92, look up the position at tick 92, check if a "shot" at position 92 would hit (is the target within range?), and print the result. Then try it with a tick that's too old (outside your 100-snapshot window) and verify it returns a rejection. This isolates the rewind-and-validate logic without threading or game state complexity.

### Build (6-8 hrs)
**Add the lag compensator to the project (`src/race-control/LagCompensator.h`):**

```cpp
class LagCompensator {
public:
    LagCompensator(StateHistory& history);
    
    struct CompensationResult {
        bool action_valid;
        int damage_dealt;
        int target_player_id;    // -1 if miss
        uint64_t original_tick;
        uint64_t applied_tick;
    };
    
    CompensationResult validate_action(
        const PlayerInput& input, 
        uint64_t current_tick
    );
    
private:
    bool check_hit(
        Vec3 shooter_pos, 
        const GameState& historical_state,
        int shooter_id,
        int& hit_player_id
    );
};
```

**Implementation:**
1. Input arrives with `tick_number` = the tick the player intended
2. Look up `StateHistory::get_state_at_tick(tick_number)`
3. If the tick is too old (>2 seconds), reject the action (anti-cheat boundary)
4. Check if the action was valid in the historical state (was the player alive? were they close enough to hit?)
5. Apply the result to the CURRENT tick (not the historical one)

**Desync detection:**
Each player's input carries their intended `tick_number`, which tells you what tick they thought it was when they acted. From the sequence of a player's inputs, you can reconstruct their expected position — they moved in `move_direction` every tick from their last acknowledged tick up to their intended tick. Compare this reconstructed position against where the server actually has them in the current game state. If the gap exceeds a threshold (e.g., 50 units), it means the player's local view and the server's authoritative state have diverged too far. Flag a desync warning on the dashboard for that player.

This matters because in Phase 2, desync detection becomes a real diagnostic tool — when a player complains "I was behind the wall but I still got shot," you can point at the desync log and explain exactly what each side saw.

**Update dashboard:**
- Show lag compensation events: "Player 8 shot player 2 (rewound 8 ticks)"
- Show desync warnings per player
- Show hit validation success/failure rates

### Checkpoint
Players with high latency can still land shots fairly. The dashboard shows rewind events. Desync detection catches cases where the simulation has diverged too far. You can explain the fairness tradeoffs to an interviewer.

---

## Week 8: Polish, Test, Observe, Document
**Goal:** Make the project interview-ready. Sanitizers, Prometheus metrics, clean README.

### Learn (3-4 hrs)
**Reading:**
- Thread Sanitizer (TSan) documentation: search "Clang ThreadSanitizer"
  - Learn the compiler flags: `-fsanitize=thread`
- Address Sanitizer (ASan) documentation: `-fsanitize=address`
- Prometheus exposition format: search "Prometheus exposition format" — it's just plain text lines like `metric_name{label="value"} 123.45`
- prometheus-cpp library: https://github.com/juliusc/prometheus-cpp (lightweight C++ client)

### Practice (2-3 hrs)
**Exercise — Sanitizer first contact:**
Take your ring buffer from week 2 and intentionally introduce a data race: remove the mutex from `push()` but keep it in `pop()`. Compile with `-fsanitize=thread` and run. Read the TSan output — it will tell you exactly which two threads accessed which variable, and from which lines of code. Fix it and rerun. This teaches you to read sanitizer output before you run it on the full project, where the reports will be longer and harder to parse.

Then try ASan: write a small program that does an out-of-bounds array access. Compile with `-fsanitize=address`. Read the output. Understand the difference between what TSan catches (data races) and what ASan catches (memory errors).

**Exercise — Minimal Prometheus endpoint:**
Write a tiny standalone HTTP server (or use a library) that serves one metric at `/metrics` in Prometheus text format. Something like `ring_buffer_occupancy 0.42`. Hit it with `curl localhost:9090/metrics` and see the output. This isolates the exposition format from your project so you know exactly what Prometheus expects before wiring it into the server.

### Build (8-10 hrs)
**Testing:**
- Add TSan to your CMake build (add a `sanitize` build target)
- Run your full simulation under TSan — fix any data race warnings
- This alone is a huge signal to interviewers. Most candidates never run sanitizers.
- Write at least 3 unit tests:
  1. Ring buffer ordering under concurrent push/pop
  2. State history query accuracy
  3. Lag compensation produces correct results for known inputs

**Prometheus metrics:**
Instrument the server with a `/metrics` HTTP endpoint exposing:
- `tick_engine_tick_duration_seconds` — histogram of per-tick processing time (shows p50/p95/p99)
- `tick_engine_ticks_total` — counter, total ticks processed
- `ring_buffer_occupancy_ratio` — gauge, how full the buffer is (0.0 to 1.0)
- `consumer_lag_ticks` — gauge, how many ticks behind the consumer is
- `network_inputs_received_total` — counter per player
- `network_inputs_dropped_total` — counter per player
- `lag_compensation_rewind_ticks` — histogram, how far back the compensator rewinds
- `desync_warnings_total` — counter per player

Use prometheus-cpp or roll a minimal text exporter (it's just string formatting served over a basic HTTP socket). The point is that anyone can point Prometheus + Grafana at your server and see live dashboards. This is a strong production-readiness signal — most portfolio projects have zero observability.

**Code cleanup:**
- Consistent naming, clear file organization
- Remove debug prints, add proper logging levels
- Ensure clean compilation with `-Wall -Wextra -Wpedantic`

**README — make it a design document:**
Write these sections:
1. **What this is** — 2 sentences. "A real-time multiplayer game server engine simulating 10 concurrent players with realistic network conditions. Built to explore concurrency, network compensation, and temporal state management in C++17."
2. **Architecture** — component diagram + 1-paragraph description of data flow
3. **Key design decisions** — why a ring buffer (bounded memory), why condition variables (no busy-wait), why `shared_mutex` for state history (concurrent reads), why 128-tick history window (2s covers most latency scenarios)
4. **Observability** — what metrics are exposed, how to scrape them
5. **How to build and run** — cmake commands, what to expect
6. **What I'd do next** — honest list of extensions (shows you think ahead)

**Git hygiene:**
- Clean commit history (squash messy WIP commits)
- Tag v1.0 when the core is done

### Checkpoint
All sanitizer warnings resolved. Prometheus endpoint serving live metrics at `/metrics`. README reads like a design document. Project is interview-ready.

---

# Phase 2: 3D Multiplayer Clients
Up to 10 real human players connecting to your server with a 3D Raylib client. The server engine from weeks 1-8 becomes the live backend — its tick engine, ring buffer, state history, and lag compensator stay nearly identical. You swap simulated inputs for real UDP packets and the terminal dashboard for a 3D renderer.

---

## Stage 1: UDP Networking Layer
**Goal:** Replace the network simulator with real UDP sockets so the server can talk to external clients.

### Learn
**Reading:**
- Beej's Guide to Network Programming — https://beej.us/guide/bgnet/
  - The classic. Read the UDP sections (chapters 5-6). Skip TCP for now.
- Glenn Fiedler, "Sending and Receiving Packets" — gafferongames.com
  - Why games use UDP, not TCP. How to handle packet loss at the application layer.

**Videos:**
- Ben Eater, "Networking tutorial" (YouTube) — visual explanation of how packets travel

### Practice
**Exercise — UDP echo server:**
Write two programs. A server that listens on a UDP port, receives a message, and echoes it back. A client that sends "hello" and prints the response. Run them on localhost. Then introduce artificial packet loss (just drop every 5th packet on the server side with `rand() % 5 == 0`) and observe the client receiving nothing for those messages. This gives you a feel for UDP's unreliability before you build the real protocol.

### Build
**Add UDP send/receive to the server (`src/network/UDPSocket.h`):**
- Wrap platform socket calls (socket, bind, sendto, recvfrom) in a clean class
- Non-blocking mode so the tick engine isn't stalled waiting for packets
- Server binds to a port (e.g., 7777) and listens for client connections
- Simple connection handshake: client sends a "join" packet, server assigns a player ID, sends back an acknowledgment
- Each tick, the server calls `recvfrom` in a loop to drain all arrived packets, parses them into `PlayerInput` structs, and feeds them to the tick engine (same path the simulator used)
- After each tick, serialize the `GameState` snapshot and `sendto` every connected client

**Serialization:**
Define a compact binary format for `GameState` and `PlayerInput`. Don't use JSON or protobufs — at 64Hz you need minimal overhead. Pack structs directly, or write simple serialize/deserialize functions that write fields into a byte buffer in order. Include a tick number and a checksum so the client can detect corruption.

**Keep the network simulator as a fallback:** Add a `--simulated` flag that runs the old simulated network path. This lets you test server logic without needing a real client.

### Checkpoint
The server accepts UDP connections on port 7777. You can test it with a simple command-line client that sends fake inputs and prints received game states. The `--simulated` flag still works for solo testing.

---

## Stage 2: Raylib Client — Window, Camera, Capsules
**Goal:** Get a basic 3D scene rendering all players as capsules in the arena.

### Learn
**Reading:**
- Raylib cheatsheet: https://www.raylib.com/cheatsheet/cheatsheet.html
  - All you need. Raylib's API is small and readable.
- Raylib 3D examples: search "raylib 3d first person example" on GitHub
  - Study how camera, models, and input work together

**Videos:**
- "Raylib in 10 minutes" style tutorials on YouTube (several exist, pick any)

### Practice
**Exercise — Raylib hello world:**
Install Raylib. Write a program that opens a window, places a 3D capsule at the origin, and lets you orbit around it with mouse + WASD. Draw a grid floor. Render at 60fps. This gets you comfortable with Raylib's init/update/draw loop, camera controls, and 3D primitives before connecting any networking.

### Build
**Create the client as a separate executable (`client/`):**
- Raylib window (1280x720) with a first-person camera
- Flat ground plane (500x500 to match the server arena)
- Draw every player from the latest received `GameState` as a colored capsule
  - Your player: distinct color
  - Other players: team color or default
  - Dead players: gray, lying flat
- Health bar floating above each capsule
- HUD overlay: your health, tick number, ping, server address
- Input capture: WASD movement, mouse look, left-click shoot
- Send `PlayerInput` to server via UDP every frame (or every tick if you match 64Hz)
- Receive `GameState` snapshots from server and update the scene

**CMake:** Add a second executable target for the client that links against Raylib.

### Checkpoint
You can run the server in one terminal and the client in another. The client connects, you see capsules moving based on server state. You can move with WASD and your capsule moves on screen. Other players (still simulated by the server in `--simulated` mode) appear as capsules.

---

## Stage 3: Client-Side Prediction & Interpolation
**Goal:** Make movement feel responsive despite network latency, and make other players move smoothly.

### Learn
**Reading:**
- Re-read Gambetta Parts 2 and 3 — you're implementing these now
  - Part 2: Client-Side Prediction and Server Reconciliation
  - Part 3: Entity Interpolation
- Glenn Fiedler, "State Synchronization" — gafferongames.com

### Practice
**Exercise — Interpolation visualizer:**
Write a small Raylib program that draws two dots. One teleports to a new random position every 100ms (simulating server snapshots). The other smoothly interpolates from the previous position to the new one over the 100ms window. See the difference visually. Then add a third dot that extrapolates beyond the latest snapshot. Watch it overshoot when the target changes direction — this shows why interpolation (with a small delay) is usually preferred over extrapolation.

### Build
**Add prediction and interpolation to the client:**

**Client-side prediction (your own player):**
When you press W, immediately move your capsule forward locally. Don't wait for the server to confirm. Store a buffer of your unacknowledged inputs (input + tick number). When the server sends back a GameState, compare your predicted position to the server's authoritative position for your player. If they match, discard the acknowledged inputs. If they differ, snap to the server position and re-apply all unacknowledged inputs on top — this is reconciliation. The result: your movement feels instant (0ms), but the server is still authoritative.

**Entity interpolation (other players):**
Buffer the two most recent server snapshots. Render other players at a position interpolated between snapshot N-1 and snapshot N, based on how much time has elapsed since snapshot N arrived. This introduces a small visual delay (~one tick, ~15ms) but eliminates the jerky teleporting you'd see if you just snapped to each new snapshot.

**Update HUD:** Show prediction error (distance between your predicted position and the server's authoritative position). Show interpolation delay.

### Checkpoint
Your own movement feels instant. Other players move smoothly. When you run with artificial latency (200ms), your movement still feels responsive and other players don't stutter. The prediction error stays near zero under normal conditions.

---

## Stage 4: Arena Geometry & Game Feel
**Goal:** Add walls, cover, and basic game mechanics to make it feel like a real shooter.

### Learn
**Reading:**
- Raylib collision detection functions: `CheckCollisionBoxes`, `GetRayCollisionBox`, `GetRayCollisionMesh`
- AABB (axis-aligned bounding box) collision detection concepts — any game dev tutorial

### Practice
**Exercise — Raycast visualization:**
Write a Raylib program that draws a few 3D boxes in a scene. Click to fire a ray from the camera. Visualize the ray as a line. If it hits a box, highlight that box red. Print the hit distance. This gives you visual intuition for raycasting before using it for shot detection.

### Build
**Add to the server:**
- Arena geometry: walls around the perimeter, 5-10 rectangular obstacles (cover) placed inside the arena
- Collision detection: players can't walk through walls or obstacles (AABB checks against player capsule)
- Raycast shooting: replace proximity-based combat with actual raycasts. When a player shoots, cast a ray from their position in their look direction. Check intersection with other player capsules and with arena geometry. Walls block shots.
- Health packs: 3-4 static positions on the map, respawn every 30 seconds
- Respawning: dead players respawn after 5 seconds at a random spawn point

**Add to the client:**
- Render walls and obstacles as textured boxes
- Render health packs as glowing spheres
- Crosshair in the center of the screen
- Hit marker flash when your shot connects
- Kill feed in the corner showing recent kills

### Checkpoint
You can play an actual deathmatch with 2+ people on a LAN. Walls block movement and shots. Health packs heal you. It feels like a real (bare-bones) shooter.

---

## Stage 5: Polish & Playtesting
**Goal:** Make it stable enough to demo.

### Learn
**Reading:**
- Delta compression concepts — search "game networking delta compression"
- UDP reliability layer concepts — Glenn Fiedler, "Building a Game Network Protocol"

### Practice
**Exercise — Bandwidth measurement:**
Add a bytes-sent/bytes-received counter to both server and client. Run a 2-player game for 60 seconds. Calculate average bandwidth per client. With 10 players and 64Hz snapshots, estimate total server upload bandwidth. Determine if delta compression is needed at this scale (spoiler: probably not for 10 players, but the calculation teaches you when it matters).

### Build
- Connection timeout: detect disconnected clients and remove them from the game
- Graceful join/leave: players can connect and disconnect mid-game without crashing
- Server browser or direct IP connect on client
- Basic scoreboard: kills, deaths, K/D ratio
- Docker compose: one command to start the server

### Checkpoint
You can give the client executable to a friend, they connect to your server by IP, and you play a 1v1 deathmatch without crashes.

---

# Phase 3: CUDA Simulation Mode
Scale the server to 200+ AI bots. CPU collision detection and raycasting become the bottleneck. CUDA earns its place.

---

## Stage 1: AI Bot System
**Goal:** Add simple AI bots that stress the tick engine at scale.

### Learn
**Reading:**
- Finite state machines for game AI — any introductory game AI tutorial
- "Programming Game AI by Example" by Mat Buckland — Chapter 2 (state machines) is all you need

### Practice
**Exercise — State machine:**
Write a standalone program with 3 states: PATROL (move in a random direction), CHASE (move toward a target), SHOOT (stop and fire). Transition rules: if a target is within 100 units → CHASE. If within 20 units → SHOOT. If target moves beyond 100 units → PATROL. Run 10 bots in a loop, print their state transitions. This isolates AI logic from the server.

### Build
**Add bot system to the server (`src/ai/BotManager.h`):**
- BotManager spawns N bots (configurable, default 200) alongside any real players
- Each bot runs a simple FSM: PATROL → CHASE → SHOOT → PATROL
- Bots generate `PlayerInput` structs and feed them into the same input pipeline as real players — the tick engine doesn't distinguish between bot and human inputs
- Add `--bots N` flag to the server. `--bots 0` disables them (pure multiplayer). `--bots 200` for stress testing.
- Profile the tick engine: at 200 entities, how long does collision detection take per tick? At 500? Find the point where a single tick exceeds the 15.6ms budget.

### Checkpoint
Server runs 200 bots smoothly (or you've identified exactly where it slows down). The terminal dashboard and Prometheus metrics show per-tick timing. You have concrete numbers: "at 200 entities, collision detection takes Xms per tick."

---

## Stage 2: Arena Geometry for Raycasting
**Goal:** Add enough world geometry to make raycasting non-trivial.

### Learn
**Reading:**
- AABB trees / spatial partitioning — search "bounding volume hierarchy game physics"
- "Real-Time Collision Detection" by Christer Ericson — Chapter 6 (spatial partitioning) if you want depth

### Practice
**Exercise — Brute-force raycast profiling:**
Write a program that generates 1000 random AABB boxes and 200 random rays. Brute-force test every ray against every box. Time it. Then implement a simple uniform grid: divide space into cells, assign boxes to cells, and only test rays against boxes in cells the ray passes through. Time it again. Compare. This gives you baseline numbers for the CPU path before CUDA enters the picture.

### Build
**Expand arena geometry on the server:**
- Generate a denser arena: 50-100 obstacles (walls, pillars, platforms) using AABB boxes
- All raycasts (shots from 200+ bots and real players) must test against this geometry
- CPU collision detection is now O(entities²) for entity-entity and O(entities × obstacles) for entity-world
- Implement a simple uniform spatial grid on CPU first: divide the arena into cells, assign entities to cells each tick, only check collisions between entities in the same or neighboring cells
- Profile again: grid-accelerated collision at 200 entities vs brute force. Note the improvement and where the remaining bottleneck is.

### Checkpoint
Server has a dense arena with 50-100 obstacles. CPU spatial grid makes 200 entities manageable. You have profiling data showing exactly how much time collision detection and raycasting take per tick.

---

## Stage 3: CUDA Spatial Hashing
**Goal:** Move broad-phase collision detection to the GPU.

### Learn
**Reading:**
- "CUDA C Programming Guide" — Chapters 1-3 (programming model, memory hierarchy, thread hierarchy)
  - https://docs.nvidia.com/cuda/cuda-c-programming-guide/
- "GPU Gems 3, Chapter 32: Broad-Phase Collision Detection with CUDA"
  - Available free online. This is exactly what you're implementing.
- CUDA by Example (book) — Chapters 1-5 for fundamentals

**Videos:**
- "CUDA Crash Course" on YouTube (several good ones, ~1hr)

### Practice
**Exercise — CUDA hello world:**
Write a CUDA kernel that takes an array of 1 million floats and doubles each one. Launch it, copy results back, verify. Time it vs a CPU loop. Understand kernel launch syntax (`<<<blocks, threads>>>`), device memory allocation (`cudaMalloc`, `cudaMemcpy`), and grid/block sizing. This is your first GPU program.

**Exercise — CUDA spatial hash:**
Write a CUDA program that takes 10,000 random 2D points, hashes each into a grid cell (`cell_x = (int)(x / cell_size)`, `cell_y = (int)(y / cell_size)`), and counts how many points land in each cell. Each CUDA thread processes one point. Use `atomicAdd` to increment cell counts. Compare against a CPU version for correctness and speed.

### Build
**Add CUDA spatial hashing to the server (`src/cuda/SpatialHash.cu`):**
- Each tick, upload all entity positions to GPU memory
- CUDA kernel 1: hash each entity into a grid cell (one thread per entity)
- CUDA kernel 2: for each entity, check all entities in the same and neighboring cells for collision candidates (one thread per entity)
- Download collision pairs back to CPU for resolution
- Use CUDA-host memory or pinned memory to minimize transfer overhead
- Add a `--cuda` flag. Without it, the server uses the CPU spatial grid from Stage 2. With it, collision detection runs on GPU.
- Benchmark: CPU grid vs CUDA grid at 200, 500, and 1000 entities. Produce a comparison table with wall-clock times per tick.

**CMake:** Add CUDA language support (`enable_language(CUDA)`), find CUDA toolkit, conditionally compile `.cu` files.

### Checkpoint
CUDA spatial hashing works and produces identical collision pairs to the CPU version. You have benchmark numbers showing the crossover point — the entity count where GPU becomes faster than CPU. You can explain why below that threshold, kernel launch overhead makes GPU slower.

---

## Stage 4: CUDA Batch Raycasting
**Goal:** Batch all per-tick raycasts and process them on the GPU.

### Learn
**Reading:**
- Ray-AABB intersection algorithm (slab method) — search "ray AABB intersection slab method"
- CUDA occupancy and warp efficiency — CUDA Programming Guide Chapter 5
- "Thinking Parallel" (NVIDIA blog series) — how to structure GPU workloads

### Practice
**Exercise — CUDA ray-AABB kernel:**
Write a CUDA kernel that takes one ray and 1000 AABBs and returns the closest hit (or miss). Each thread tests one AABB. Use a parallel reduction to find the minimum hit distance. Verify against a CPU reference. Then extend to 200 rays × 1000 AABBs — one block per ray, threads within the block test different AABBs.

### Build
**Add CUDA batch raycasting to the server (`src/cuda/BatchRaycast.cu`):**
- Each tick, collect all shoot actions from bots and players into a ray buffer (origin + direction per ray)
- Upload rays + arena geometry to GPU
- CUDA kernel: each thread processes one ray, traverses the spatial grid from Stage 3 to find AABB cells the ray passes through, tests intersection against entities and obstacles in those cells
- Download results: per-ray hit/miss, hit entity ID, hit distance
- Feed results back into the lag compensator / damage system
- Benchmark: CPU raycasting (one ray at a time) vs CUDA batch raycasting at 200, 500, 1000 simultaneous rays

### Checkpoint
All raycasts run on GPU when `--cuda` is enabled. Results match the CPU path exactly. Benchmark shows clear GPU advantage when 50+ rays fire in the same tick. Prometheus metrics now include `raycast_batch_size` and `raycast_gpu_time_seconds`.

---

## Stage 5: Benchmarking & Performance Analysis
**Goal:** Produce a performance report that proves you understand when GPU compute is worth it.

### Learn
**Reading:**
- NVIDIA Nsight Compute — profiling CUDA kernels
- "Roofline model" for GPU performance analysis — search "roofline model GPU"

### Practice
**Exercise — Profile a kernel:**
Run one of your CUDA kernels through `ncu` (Nsight Compute CLI). Read the output: occupancy, memory throughput, compute throughput, warp stall reasons. Identify whether your kernel is compute-bound or memory-bound. This teaches you to read profiling output before analyzing your real kernels.

### Build
**Write a benchmark suite and performance document:**
- Automated benchmark script that runs the server at entity counts of 10, 50, 100, 200, 500, 1000 with CPU-only and CUDA modes
- Measure per-tick time, collision detection time, raycasting time for each configuration
- Produce a results table and chart showing the crossover points
- Profile CUDA kernels with Nsight Compute. Report occupancy, memory bandwidth utilization, and warp efficiency.

**Write `PERFORMANCE.md`:**
1. Methodology — how you measured, what hardware, what conditions
2. Results table — entity count × mode (CPU/GPU) × time per component
3. Crossover analysis — at what entity count does GPU become faster, and why
4. Bottleneck analysis — what limits GPU performance (kernel launch overhead at low N, memory bandwidth at high N)
5. What you'd optimize next — shared memory, persistent threads, stream overlap

This document is the single strongest interview artifact from the entire project. It proves you don't just use CUDA — you understand when it helps and when it doesn't.

### Checkpoint
`PERFORMANCE.md` has real numbers from real benchmarks. You can walk an interviewer through the crossover chart and explain the hardware reasons behind every data point.

---

## Resource Summary

### Essential Reading (in order of priority)
1. Gabriel Gambetta, "Fast-Paced Multiplayer" (4 articles + live demo) — gabrielgambetta.com
2. Glenn Fiedler, "Fix Your Timestep!" — gafferongames.com
3. "C++ Concurrency in Action" by Anthony Williams — Chapters 1-4
4. Valve Developer Wiki, "Source Multiplayer Networking"
5. Beej's Guide to Network Programming — beej.us/guide/bgnet/
6. CUDA C Programming Guide — docs.nvidia.com/cuda/

### YouTube / Talks
1. Bo Qian's C++ Concurrency playlist (threading fundamentals)
2. CppCon "Introduction to Multithreading" by Anthony Williams
3. GDC "Overwatch Gameplay Architecture and Netcode"
4. Riot Games "Valorant Netcode & 128-Tick Servers"
5. GDC 2017 "Lag Compensation in Watch Dogs 2"

### Reference
- cppreference.com — `<thread>`, `<mutex>`, `<atomic>`, `<shared_mutex>`, `<condition_variable>`
- Clang ThreadSanitizer docs
- "An Introduction to Modern CMake" — cliutils.gitlab.io/modern-cmake/
- Raylib cheatsheet — raylib.com/cheatsheet/cheatsheet.html

### Curated Resource List
- github.com/0xFA11/MultiplayerNetworkingResources — comprehensive list of game networking talks, articles, and libraries