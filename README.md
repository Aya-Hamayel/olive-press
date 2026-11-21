# Olive Press Foundation

Build on Ubuntu/WSL:

```bash
make
./olive config.txt
```

`src/main.c` creates 6 stage child processes and 1 Quality Control child.
Five ordinary pipes connect the six stages. A shared status pipe sends
stage events to QC; an alert pipe sends QC errors to the supervisor.

This is a foundation/demo only. Stage processing, simulated sensors,
conveyor controls, OpenMP experiments and OpenGL visualization are not implemented.

Change `config.txt` and rerun without rebuilding. Press Ctrl+C to stop.


## Configuration Contract

- `container_kg`: Initial mass of olives available in the reception container.
- `batch_size_kg`: Mass of each batch transferred to the production line.
- `num_batches`: Number of batches explicitly configured by the user.

Current validation:

    num_batches * batch_size_kg <= container_kg

The current implementation allows leftover olives in the container.

Reception currently sends the configured number of batches.
Automatic batch calculation and the container-empty sensor are
not implemented yet.

The team must agree before changing these semantics.

## Contract for Person 2

Stage function:

    run_stage(stage_id_t stage,
              int in_fd,
              int out_fd,
              int status_fd,
              const config_t *cfg);

### File descriptors

- Reception: in_fd = -1, out_fd sends batches to Cleaning.
- Cleaning, Washing, Crushing, Malaxation:
  read from in_fd and forward batches to out_fd.
- Pressing: reads from in_fd, out_fd = -1.
- All stages send status_msg_t to status_fd.

Use read_full() and write_full() to transfer messages
and check their return values.

### Messages

- batch_msg_t: batch ID and mass in kilograms.
- status_msg_t: stage, batch ID, temperature,
  pressure, processing time and state.
- alert_msg_t: alert type, stage, batch ID and text.

Quality Control receives status messages and sends
alerts to Supervisor through alert_pipe.

### Conveyor Signals

- SIG_CONVEYOR_STOP = SIGUSR1
- SIG_CONVEYOR_RESUME = SIGUSR2

Proposed design:
Supervisor sends signals to the Cleaning process
to stop or resume its conveyor.

Signal handlers and conveyor control are not implemented yet.
Do not send these signals before handlers are installed.

### Important Team Rules

- Do not modify common.h or message structures
  without agreement.
- Do not change batch semantics independently.
- stage_time[] is parsed but stage delays are not implemented yet.
- Coordinate changes to shared pipes and signals.

## Current Limitations

- Stage processing is currently a simulation stub.
- Container-empty sensor is not implemented.
- Conveyor pause/resume is not implemented.
- OpenMP performance experiments are not implemented.
- OpenGL visualization is not implemented.
- QC currently prints messages to the terminal.
- Supervisor logging uses olive.log.
- The log file is opened with "w", so previous logs
  are overwritten on each run.
