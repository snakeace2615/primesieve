///
/// @file   maintainLoad.cpp
/// @brief  Maintain a target system CPU utilization on Linux using
///         short, single-threaded prime counting tasks.
///
/// Copyright (C) 2026 Kim Walisch, <kim.walisch@gmail.com>
///
/// This file is distributed under the BSD License. See the COPYING
/// file in the top level directory.
///

#include "CmdOptions.hpp"

#include <primesieve/primesieve_error.hpp>

#if defined(__linux__)

#include <PrimeSieveClass.hpp>
#include <primesieve/macros.hpp>
#include <primesieve/pmath.hpp>
#include <primesieve/Vector.hpp>

#include <stdint.h>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace {

volatile std::sig_atomic_t stopRequested = 0;

void stopLoad(int)
{
  stopRequested = 1;
}

struct CpuSample
{
  primesieve::Array<uint64_t, 8> ticks;
  int cores = 0;
};

CpuSample readCpuSample()
{
  std::ifstream file("/proc/stat");
  std::string line;
  std::getline(file, line);
  std::istringstream fields(line);
  std::string label;
  CpuSample sample;

  if (!(fields >> label) || label != "cpu")
    throw primesieve::primesieve_error("cannot read CPU statistics from /proc/stat");

  for (auto& ticks : sample.ticks)
    if (!(fields >> ticks))
      throw primesieve::primesieve_error("invalid CPU statistics in /proc/stat");

  while (std::getline(file, line) &&
         line.size() > 3 &&
         line.compare(0, 3, "cpu") == 0 &&
         line[3] >= '0' && line[3] <= '9')
    sample.cores++;

  if (sample.cores == 0 || file.bad())
    throw primesieve::primesieve_error("cannot read CPU count from /proc/stat");

  return sample;
}

} // namespace

void maintainLoad(const CmdOptions& opts)
{
  using Clock = std::chrono::steady_clock;
  using Seconds = std::chrono::duration<double>;
  using primesieve::inBetween;
  using primesieve::primesieve_error;

  CpuSample previous = readCpuSample();
  stopRequested = 0;
  if (std::signal(SIGINT, stopLoad) == SIG_ERR ||
      std::signal(SIGTERM, stopLoad) == SIG_ERR)
    throw primesieve_error("cannot install load controller signal handlers");

  INDETERMINATE primesieve::PrimeSieve sieve;
  if (opts.sieveSize)
    sieve.setSieveSize(opts.sieveSize);

  double target = opts.cpuTarget / 100.0;
  double upper = std::min(1.0, target + 0.1);
  double duty = 0;
  double usage = 0;
  double steal = 0;
  double chunkSize = 1000;
  uint64_t batches = 0;
  uint64_t count = 0;
  auto beginning = Clock::now();
  auto lastSample = beginning;
  auto lastStatus = beginning;

  if (!opts.quiet)
    std::cout << "Maintaining system CPU at " << opts.cpuTarget
              << "% using one thread across " << previous.cores
              << " visible CPU(s). Timeout: " << opts.timeout
              << " seconds (0 = unlimited)." << std::endl;

  while (!stopRequested)
  {
    auto now = Clock::now();
    if (opts.timeout && Seconds(now - beginning).count() >= opts.timeout)
      break;

    if (Seconds(now - lastSample).count() >= 1)
    {
      CpuSample current = readCpuSample();
      if (current.cores != previous.cores)
        throw primesieve_error("CPU count changed while maintaining load");

      uint64_t total = 0;
      uint64_t busy = 0;
      uint64_t stolen = 0;
      for (unsigned i = 0; i < current.ticks.size(); i++)
      {
        // Linux iowait counters may decrease; guest time is already
        // included in user/nice and must not be counted again.
        if (current.ticks[i] < previous.ticks[i] && i != 4)
          throw primesieve_error("CPU statistics moved backwards");
        uint64_t delta = primesieve::checkedSub(current.ticks[i], previous.ticks[i]);
        total += delta;
        if (i < 3 || i == 5 || i == 6)
          busy += delta;
        if (i == 7)
          stolen = delta;
      }

      if (total == 0)
        throw primesieve_error("CPU statistics did not advance");

      usage = (double) busy / total;
      steal = (double) stolen / total;
      double maxDuty = std::min(1.0, upper * current.cores);
      duty = inBetween(0.0, duty + 0.5 * (target - usage) * current.cores, maxDuty);
      if (usage > upper)
        duty = 0;
      previous = current;
      lastSample = now;
    }

    if (opts.status && Seconds(now - lastStatus).count() >= 60)
    {
      std::cout << std::fixed << std::setprecision(1)
                << "Elapsed: " << Seconds(now - beginning).count()
                << " s, CPU: " << usage * 100 << "%, steal: " << steal * 100
                << "%, duty: " << duty * 100 << "%, batches: " << batches
                << ", accumulated prime count: " << count << std::endl;
      lastStatus = now;
    }

    auto frameEnd = Clock::now() + std::chrono::milliseconds(100);
    auto workEnd = frameEnd - std::chrono::duration_cast<Clock::duration>(Seconds(0.1 * (1 - duty)));
    while (!stopRequested && Clock::now() < workEnd)
    {
      auto start = Clock::now();
      double remaining = Seconds(workEnd - start).count();
      // Leave sub-millisecond tails for sleeping instead of repeatedly
      // initializing the sieve for tiny batches.
      if (remaining < 0.001)
        break;

      double batchSize = chunkSize * std::min(1.0, remaining / 0.01);
      batchSize = inBetween(1000.0, batchSize, 1e8);
      count += sieve.countPrimes(0, (uint64_t) batchSize);
      batches++;
      double elapsed = Seconds(Clock::now() - start).count();
      if (elapsed > 0)
      {
        double adjustment = inBetween(0.25, 0.01 / elapsed, 4.0);
        chunkSize = inBetween(1000.0, batchSize * adjustment, 1e8);
      }
    }

    if (!stopRequested)
      std::this_thread::sleep_until(frameEnd);
  }

  if (!opts.quiet)
    std::cout << "Stopped after " << Seconds(Clock::now() - beginning).count()
              << " seconds, batches: " << batches
              << ", accumulated prime count: " << count << std::endl;
}

#else

void maintainLoad(const CmdOptions&)
{
  throw primesieve::primesieve_error("--maintain-load requires Linux with /proc/stat");
}

#endif
