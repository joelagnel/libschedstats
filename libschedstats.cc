#include "libschedstats.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#define PROC_SYSCTL_SCHEDSTATS "/proc/sys/kernel/sched_schedstats"
#define PROC_SCHEDSTATS "/proc/schedstat"

// SchedStats member functions
void SchedStats::SchedStatsEnable(void) {
  std::ofstream sched_debug_file;
  sched_debug_file.open(PROC_SYSCTL_SCHEDSTATS);
  if (!sched_debug_file.is_open()) {
    std::cerr << "ERROR: Could not open file: " << PROC_SYSCTL_SCHEDSTATS << " "
              << std::endl;
    return;
  }
  sched_debug_file << "1\n";
  sched_debug_file.close();
}

void SchedStats::SchedStatsDisable(void) {
  std::ofstream sched_debug_file;
  sched_debug_file.open(PROC_SYSCTL_SCHEDSTATS);
  if (!sched_debug_file.is_open()) {
    std::cerr << "ERROR: Could not open file: " << PROC_SYSCTL_SCHEDSTATS << " "
              << std::endl;
    return;
  }

  sched_debug_file << "0\n";
  sched_debug_file.close();
}

void SchedStats::DoSample(void) {
  prev = std::move(cur);
  cur.clear();
  delta.clear();

  std::ifstream ss(PROC_SCHEDSTATS);
  std::string line;
  bool first = true;

  if (!ss.is_open()) {
    std::cerr << "ERROR: Could not open sched stats\n";
    return;
  }

  while (std::getline(ss, line)) {
    // Confirm /proc/schedstats version number
    if (first) {
      if (line.find("version 15") == std::string::npos) {
        std::cerr << "ERROR: schedstats version mismatch. Expected 15, Got: "
                  << line << std::endl;
        return;
      }
      first = false;
    }

    // cpu line looks like this:
    // cpu0 0 0 39536349 6522616 21294273 15161519 18204 87789 128307
    if (line.find("cpu") == 0) {
      // SchedStatsSample knows how to parse it.
      auto s = SchedStatsSample(line);

      if (s.cpu == -1) {
        std::cerr << "Could not parse schedstat CPU line: " << line
                  << std::endl;
        clear();
        return;
      }
      cur.emplace_back(s);
      // std::cout << "Created sample " << s << std::endl;
    }
    // std::cout << line << '\n';
  }
  ss.close();

  // For first time, don't calc delta.
  if (prev.size() == 0) return;

  if (prev.size() != cur.size()) {
    std::cout << "INFO: Unexpected change in CPU list, start over."
              << std::endl;
    clear();
    return;
  }

  for (int i = 0; i < cur.size(); i++) {
    if (cur[i].cpu != prev[i].cpu) {
      std::cout << "INFO: Unexpected change in CPU list, start over."
                << std::endl;
      clear();
      return;
    }

    auto del = cur[i] - prev[i];
    if (del.cpu == -1) {
      std::cout << "INFO: Stats seem to be moving backward, start over."
                << std::endl;
      clear();
      return;
    }

    delta.emplace_back(del);
  }
}

void SchedStats::clear(void) {
  prev.clear();
  cur.clear();
  delta.clear();
}

std::ostream& operator<<(std::ostream& os, const SchedStats& s) {
  for (auto& o : s.delta) std::cout << o;
  return os;
}

#define SCHEDSTATS_PARSE_TOKEN(name)                                \
  name = std::stoi(tmp);                                            \
  if (name < 0) {                                                   \
    std::cerr << "Invalid " #name " parsed: " << name << std::endl; \
    cpu_nr = -1;                                                    \
  }                                                                 \
  break;

// SchedStatsSample member functions
SchedStatsSample::SchedStatsSample(std::string line) {
  std::vector<std::string> tokens;
  std::stringstream tokstream(line);
  std::string tmp;
  int i = 0;
  cpu = -1;

  // cpu line looks like this:
  // cpu0 0 0 39536349 6522616 21294273 15161519 18204 87789 128307
  if (line.find("cpu") != 0) {
    std::cerr << "ERROR: Incorrect line passed to create SchedStatsSample: "
              << line << std::endl;
    return;
  }

  // 3 means skip 'cpu'
  int cpu_nr = std::stoi(line.substr(3, line.find(' ') - 3));
  if (cpu_nr < 0 || cpu_nr > 1024) {
    std::cerr << "ERROR: Couldn't parse CPU number from line: " << line
              << std::endl;
    return;
  }

  // On errors, the parse macro assigns cpu_nr = -1;
  while (std::getline(tokstream, tmp, ' ')) {
    switch (i++) {
      case 0:
        // Skip the 'cpuXX' token
        continue;
      case 1:
        SCHEDSTATS_PARSE_TOKEN(yld_count);
      case 2:
        // Ignore 2, legacy for O(1) scheduler.
        break;
      case 3:
        SCHEDSTATS_PARSE_TOKEN(sched_count);
      case 4:
        SCHEDSTATS_PARSE_TOKEN(sched_goidle);
      case 5:
        SCHEDSTATS_PARSE_TOKEN(ttwu_count);
      case 6:
        SCHEDSTATS_PARSE_TOKEN(ttwu_local);
    }
  }
  sampling_time = std::chrono::steady_clock::now();

  // Assign cpu to signify that object constructed successfully
  cpu = cpu_nr;
}

#define DIFF_ENTRY(name)         \
  if (name < s.name) return err; \
  ret.name = name - s.name;

#define PR_ENTRY(name) os << " " #name << ": " << s.name;

SchedStatsSample SchedStatsSample::operator-(SchedStatsSample const& s) {
  SchedStatsSample ret(cpu), err(-1);

  if (s.cpu != cpu) {
    std::cerr << "ERROR: Invalid operands passed to SchedStatsSample.";
    return err;
  }

  DIFF_ENTRY(yld_count);
  DIFF_ENTRY(sched_count);
  DIFF_ENTRY(sched_goidle);
  DIFF_ENTRY(ttwu_count);
  DIFF_ENTRY(ttwu_local);

  ret.delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                     sampling_time - s.sampling_time)
                     .count();
  return ret;
}

std::ostream& operator<<(std::ostream& os, const SchedStatsSample& s) {
  os << "CPU: " << s.cpu << " delta_ms: " << s.delta_ms;
  PR_ENTRY(yld_count);
  PR_ENTRY(sched_count);
  PR_ENTRY(sched_goidle);
  PR_ENTRY(ttwu_count);
  PR_ENTRY(ttwu_local);
  os << std::endl;

  return os;
}
