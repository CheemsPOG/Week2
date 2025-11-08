#include "realtime-clock/fsm.hpp"
#include "realtime-clock/fsm_c_api.h"

extern "C" {

static ClockFSM g_clock_fsm;

void init_fsm() {
  g_clock_fsm.init();
}

void fsm_run() {
  g_clock_fsm.run();
}

} // extern "C"
