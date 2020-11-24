#include <unistd.h>

#include <iostream>

#include "libschedstats.cc"

int main(int argc, char **argv) {
  SchedStats::SchedStatsEnable();
  SchedStats s;

  s.DoSample();
  sleep(1);
  s.DoSample();

  std::cout << "Samples:\n" << s << std::endl;
  SchedStats::SchedStatsDisable();

  return 0;
}
