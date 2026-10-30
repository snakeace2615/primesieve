# Controlled CPU load on Ubuntu

This configuration targets a Linux VPS with one vCPU and 1 GiB RAM. It aims
for 30% total CPU utilization, allowing normal variation within 20%-40%.
It does not guarantee any hosting provider's retention policy.

Install the [build prerequisites](BUILD.md#prerequisites) first (a C++14
compiler and CMake). Build with one compiler job on a small VPS to limit peak
build memory:

```sh
cmake -S . -B build-load -DCMAKE_BUILD_TYPE=Release
cmake --build build-load --parallel 1
sudo cmake --install build-load
sudo ldconfig
```

First run a finite foreground trial:

```sh
primesieve --maintain-load --cpu-target=30 --timeout=5m
```

The controller measures all CPUs reported by `/proc/stat`, adjusts its duty
cycle once per second, and uses 100 ms work/sleep periods. Counting batches
adapt toward 10 ms or the remaining work budget. Computation pauses above
target + 10 percentage points. Allow several seconds to settle; evaluate
utilization over 10-second windows. Individual samples and instantaneous
utilization can exceed the desired range. Other processes, stolen CPU time
and quotas can prevent convergence. The controller cannot lower other
processes' CPU usage. One thread cannot consume more than one CPU, even when
the target refers to the entire machine. Containers with host-wide CPU
statistics are not an appropriate environment for this mode.

The workload repeatedly counts primes in a bounded interval no larger than
`[0, 100000000]`. Counts in logs are accumulated across repeated computations,
not counts of distinct primes. No result files are created. Logs report
system utilization, stolen CPU time and controller duty once per minute.
`--no-status` suppresses periodic status, and `--quiet` suppresses all normal
output. Sampling errors terminate the program with a nonzero exit status.
`--timeout=0` (the default for this mode) runs indefinitely. Ctrl+C and SIGTERM
request a clean exit after the current small batch.

## Optional systemd service

Review [primesieve-load.service](primesieve-load.service) before installing.
The example assumes the default `/usr/local/bin/primesieve` installation path
and uses an unprivileged dynamic user. Adjust `ExecStart` for other prefixes.
It limits this service to 40% of one CPU and 64 MiB memory, and gives it low
scheduling priority. These limits do not apply to other programs. `CPUQuota`
is relative to one CPU, whereas the application's target is system-wide.

The following commands install and start the service when you choose to deploy:

```sh
sudo install -m 644 doc/primesieve-load.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now primesieve-load.service
systemctl status primesieve-load.service
journalctl -u primesieve-load.service -n 20
```

To stop it and disable automatic startup:

```sh
sudo systemctl disable --now primesieve-load.service
```

## Validation on the VPS

Observe the foreground trial with an independent CPU monitor such as `top`.
Check 10-second averages after startup; a mostly idle one-vCPU machine should
settle near 30%. During normal application activity, the controller should
reduce its own work and resume when that activity ends. High system load
alone is not a failure if other processes account for it.

Check that Ctrl+C and service stop finish promptly, and that a finite timeout
exits successfully. During a 24-hour observation, check resident memory stays
below the initial 50 MiB target without an upward trend, and that logs remain
limited to approximately one status line per minute. Results on development
hardware do not replace this VPS-specific validation.
