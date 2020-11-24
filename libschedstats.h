#ifndef LIBSCHEDSTATS_H
#define LIBSCHEDSTATS_H
#include <chrono>
#include <fstream>
#include <vector>

#define NR_MAX_CPUS 16

class SchedStatsSample {
  // When adding entries here, make sure to update:
  // 1. operators - and <<.
  // 2. String constructor.

  // sys_sched_yield() stats
  unsigned int yld_count;

  // schedule() stats
  unsigned int sched_count;
  unsigned int sched_goidle;

  // try_to_wake_up() stats
  unsigned int ttwu_count;
  unsigned int ttwu_local;

  std::chrono::time_point<std::chrono::steady_clock> sampling_time;
  // 'delta_ms' is for SchedStatsSample deltas, calc'd from sampling_time
  // of 2 other samples.
  int64_t delta_ms;

 public:
  int cpu;
  // Create a bare minimum sample. cpu == -1 indicates an error object.
  SchedStatsSample(int cpu) : cpu(cpu), delta_ms(0) {}

  // Populate the object with a sample.
  SchedStatsSample(std::string line);

  // Pretty print.
  friend std::ostream& operator<<(std::ostream& os, const SchedStatsSample& s);

  // Difference between 2 samples.
  SchedStatsSample operator-(SchedStatsSample const& s);
};

class SchedStats {
  std::vector<SchedStatsSample> prev, cur, delta;

 public:
  void ParseSchedStats(void);

  static void SchedStatsEnable(void);
  static void SchedStatsDisable(void);

  // SampleIt - Construct a list of samples of all CPUs in cpu_list, while
  // calculating delta with previous samples. Must be called atleast once
  // for delta list to be populated.
  // SchedStatsEnabled() must be called to avoid getting zeroed samples.
  void DoSample(void);

  // Pretty print.
  friend std::ostream& operator<<(std::ostream& os, const SchedStats& s);

  // Clear all samples
  void clear(void);
};
#endif
