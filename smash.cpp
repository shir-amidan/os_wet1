#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

using namespace std;


int main(int argc, char* argv[]) {
    if (signal(SIGTSTP , ctrlZHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if (signal(SIGCHLD , sigchld_handler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    // if(signal(SIGINT , ctrlCHandler)==SIG_ERR) {
    //     perror("smash error: failed to set ctrl-C handler");
    // }

    SmallShell& smash = SmallShell::getInstance();
    string cmd_line;
    do {
        cout << smash.name();
        getline(std::cin, cmd_line);
    } while (smash.executeCommand(cmd_line.c_str()));
    return 1;
}