# Olive Press Production Line Simulation

Build on Ubuntu/WSL:

```bash
make
./olive config.txt
```

`src/main.c` creates 6 stage child processes and 1 Quality Control child. Five ordinary pipes connect the six stages. A shared status pipe sends stage events to QC; an alert pipe sends QC errors to the supervisor.

Change `config.txt` and rerun without rebuilding. Press Ctrl+C to stop.

## Configuration Contract

- `container_kg`: Initial mass of olives available in the reception container.
- `batch_size_kg`: Mass of each batch transferred to the production line.

Current validation:

    container_kg > 0 && batch_size_kg > 0

Reception automatically calculates and dispatches batches of size `batch_size_kg` (and a final partial batch if `container_kg` is not an exact multiple) until the container-empty sensor detects `0.00 kg` remaining.

## Stage Pipeline & Responsibilities

Stage function:

    run_stage(stage_id_t stage, int in_fd, int out_fd, int status_fd, const config_t *cfg);

### File descriptors

- Reception: `in_fd = -1`, `out_fd` sends batches to Cleaning.
- Cleaning, Washing, Crushing, Malaxation: read from `in_fd` and forward batches to `out_fd`.
- Pressing: reads from `in_fd`, `out_fd = -1`.
- All stages send `status_msg_t` to `status_fd`.

### Conveyor Signals

- `SIG_CONVEYOR_STOP = SIGUSR1`
- `SIG_CONVEYOR_RESUME = SIGUSR2`

Implemented flow:
1. Reception monitors the olive container level and sends `SIG_CONVEYOR_STOP` / `SIG_CONVEYOR_RESUME` to the Supervisor (`getppid()`).
2. Supervisor catches these signals and forwards them to the Cleaning process (`children[CLEANING]`).
3. Cleaning installs signal handlers to pause/resume conveyor intake while allowing in-flight batches already inside the downstream pipeline (`Washing` through `Pressing`) to finish processing cleanly.

## Current Status & Remaining Work

- Stage processing delays (`stage_time[]`), simulated sensor readings, container-empty detection, and conveyor signal controls are implemented.
- OpenMP computational benchmarking is implemented; OpenGL visualization is pending.
- Supervisor logging uses `olive.log`.



## Quality Control and Fault Monitoring (Member 3)

The Quality Control process monitors the production stages
using status messages received through a pipe.

Implemented features:
- Temperature monitoring using configured minimum and maximum limits.
- Pressure monitoring using configured minimum and maximum limits.
- Random sensor fault simulation using `fault_probability`.
- Actual processing time measurement using `CLOCK_MONOTONIC`.
- Processing delay detection with a tolerance of 20% or 0.1 seconds,
  whichever is greater.
- Monitoring of stage states: idle, running, stopped, completed, and error.
- Alert messages sent from Quality Control to the Supervisor.
- Supervisor records received alerts in `olive.log`.

## OpenMP Performance Benchmark

An independent computational benchmark was implemented in
`src/openmp_benchmark.c` to compare sequential and parallel execution.

Compile the benchmark:

```bash
gcc -O2 -fopenmp src/openmp_benchmark.c -o openmp_benchmark -lm
```

Run with different thread counts:

```bash
OMP_NUM_THREADS=1 ./openmp_benchmark
OMP_NUM_THREADS=4 ./openmp_benchmark
OMP_NUM_THREADS=8 ./openmp_benchmark
```

### Experimental Results

| Threads | Parallel Time (s) | Speedup |
|---------|-------------------|---------|
| 1       | 4.199165          | 1.22x   |
| 4       | 1.576607          | 2.96x   |
| 8       | 1.214332          | 3.71x   |

Speedup = Sequential Time / Parallel Time

The benchmark showed that parallel execution improved
performance for the independent computational workload.

Among the tested configurations, 8 threads achieved
the lowest parallel execution time.

Sequential and parallel numerical results were consistent
within floating-point rounding differences.

Note: These results are from individual benchmark runs.
They measure a simulated computational workload, not
the complete olive press pipeline throughput.
