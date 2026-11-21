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
