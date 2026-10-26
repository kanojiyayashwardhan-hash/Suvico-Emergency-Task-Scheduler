# Suvico Emergency Task Scheduler

A C++17 backend and browser-based dashboard demonstrating core Data Structures and Algorithms. This is an academic prototype with local sample data, not a real emergency dispatch system.

## Features

- Create and persist tasks in `data/tasks.json`
- Priority scheduling using a custom binary max-heap
- Expected O(1) task-ID lookup using `std::unordered_map`
- Undo the latest status change using a stack
- Sort by priority, deadline, or arrival time
- Dijkstra shortest-path calculation on a sample weighted graph
- Dashboard statistics and task lifecycle actions

## DSA policy

Severity: Critical = 4, High = 3, Medium = 2, Low = 1.
Pending tasks are ranked by severity descending, deadline ascending, then arrival sequence ascending. This is a simple demonstration policy and must not be treated as a validated emergency dispatch policy.

## Requirements

- Windows 10/11, macOS, or Linux
- C++17 compiler (Visual Studio 2022 C++ workload, or GCC/Clang)
- CMake 3.20+
- Git and an internet connection on the first configure/build, because CMake downloads `cpp-httplib` and `nlohmann/json`

## Build and run on Windows (PowerShell)

1. Install **Visual Studio 2022 Build Tools** or Visual Studio with the **Desktop development with C++** workload.
2. Install **CMake** and **Git**. During installation, allow CMake and Git to be added to PATH.
3. Extract this project folder.
4. Open PowerShell in the extracted `Suvico-Emergency-Task-Scheduler` directory.
5. Run:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\suvico.exe
```

If your generator places the executable directly in `build`, run `.\build\suvico.exe` instead.

6. Open `http://localhost:8080` in your browser.
7. Stop the server with `Ctrl+C`.

### Build on macOS/Linux

Install a C++ compiler, CMake and Git, then run:

```bash
cmake -S . -B build
cmake --build build -j
./build/suvico
```

Open `http://localhost:8080`.

## Project structure

```text
Suvico-Emergency-Task-Scheduler/
├── CMakeLists.txt
├── README.md
├── src/
│   └── main.cpp
├── public/
│   ├── index.html
│   ├── style.css
│   └── app.js
└── data/
    └── tasks.json
```

## API endpoints

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/api/tasks?sort=priority` | List tasks |
| GET | `/api/tasks?sort=deadline` | Sort by deadline |
| GET | `/api/tasks?sort=arrival` | Sort by arrival |
| GET | `/api/tasks/SV1` | Search by ID |
| POST | `/api/tasks` | Add task |
| POST | `/api/tasks/SV1/status` | Change status |
| GET | `/api/stats` | Dashboard counts |
| GET | `/api/next` | Return the highest-ranked pending task |
| POST | `/api/undo` | Undo last status change |
| GET | `/api/route?from=Control%20Center&to=Hospital` | Dijkstra route |

Example create-task request:

```json
{
  "title": "Electrical hazard reported",
  "location": "Industrial Area",
  "severity": 4,
  "deadlineMinutes": 10
}
```

## Complexity analysis

Let `n` be the number of stored tasks, `V` the number of road-network vertices and `E` the number of edges.

| Operation | Time | Notes |
|---|---:|---|
| Add task | Expected O(1) | Vector append and hash-table insertion; occasional resize |
| Search by ID | Expected O(1) | `unordered_map`; worst case O(n) |
| Build scheduler heap | O(n log n) | This version inserts each pending task individually |
| Pop highest-priority task | O(log n) | Custom binary heap |
| List and sort tasks | O(n log n) | Sorting dominates |
| Change status | O(n) | Hash lookup plus update of history vector |
| Undo status change | O(n) | Stack access O(1), history synchronization O(n) |
| Dijkstra | O((V + E) log V) | Adjacency list and min-priority queue |
| Persist all tasks | O(n) | Rewrite local JSON file |

## Manual test plan

1. Add a Critical task with a 10-minute deadline.
2. Add a Low task with a 2-minute deadline. Confirm Critical ranks first under the documented severity-first policy.
3. Add two Critical tasks with different deadlines. Confirm the earlier deadline ranks first.
4. Search for a valid ID such as `SV1`, then search for an invalid ID.
5. Change a task from Pending to In Progress, press Undo, and confirm the prior status returns.
6. Complete a task and confirm dashboard counts change.
7. Sort by deadline and arrival.
8. Calculate a route from Control Center to Hospital; confirm a route and estimated minutes appear.
9. Stop and restart the server; confirm tasks persist.
10. Try an empty title and invalid deadline; confirm validation errors appear.

## Limitations and next improvements

- Data is local and designed for a single local user; there is no authentication or concurrent-user coordination.
- Route data is fictional and has no live traffic or geolocation.
- Undo covers status changes only, not task creation or deletion.
- The prototype does not assign real responders or guarantee safety-critical scheduling.
- For a stronger submission, add unit tests, CSV export, resource constraints, and a benchmark comparing the custom heap with sorting or `std::priority_queue`.
