#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
    SmallShell::getInstance().handle_ctrl_z(sig_num);
}

void ctrlCHandler(int sig_num) {
  // TODO: Add your implementation
}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}

void sigchld_handler(int sig_num) {
    SmallShell::getInstance().handle_sigchld(sig_num);
}