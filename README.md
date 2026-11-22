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
- OpenMP performance experiments and OpenGL visualization are not implemented yet.
- Supervisor logging uses `olive.log`.
