#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <signal.h>
#include <iomanip>
#include "Commands.h"

using namespace std;
const std::string WHITESPACE = " \n\r\t\f\v";

// #define DBUG

#if defined(DBUG)
#define FUNC_ENTRY()  \
  func_entry_c __func(__PRETTY_FUNCTION__);
#else
#define FUNC_ENTRY()
#endif

class func_entry_c {
    const char* _func;
public:
    func_entry_c(const char* func): _func(func) {
        cerr << _func << " --> " << endl;
    }
    ~func_entry_c() {
        cerr << _func << " <-- " << endl;
    }
};

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
//   FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

/* -------------- Command -------------- */

Command::Command(const char* cmd_line) {
    _smash = &SmallShell::getInstance();
    _cmd_line = new char[COMMAND_ARGS_MAX_LENGTH];
    strcpy(_cmd_line, cmd_line);
}

const char *Command::cmd_line() {
    return _cmd_line;
}

int Command::pid() {
    return _pid;
}

/* -------------- Command::CommandError -------------- */

Command::CommandError::CommandError(const std::string& message) {
    _message = message;
}

const std::string& Command::CommandError::what() const {
    return _message;
}

/* -------------- SmallShell -------------- */
SmallShell::SmallShell():
    _name("smash> ") {
    _cwd = new char[COMMAND_ARGS_MAX_LENGTH];
    _cd_called = false;
    _running_cmd = nullptr;
}

SmallShell &SmallShell::getInstance() {
    static SmallShell instance;
    return instance;
}


Command *SmallShell::CreateCommand(const char* cmd_line) {

    char* args[COMMAND_MAX_ARGS + 1];
    _parseCommandLine(cmd_line, args);
    string firstWord(args[0]);

    if (firstWord.compare("chprompt") == 0) {
        return new ChpromptCommand(cmd_line, args);
    } else if (firstWord.compare("showpid") == 0) {
        return new ShowPidCommand(cmd_line, args);
    } else if (firstWord.compare("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line, args);
    } else if (firstWord.compare("cd") == 0) {
        return new ChangeDirCommand(cmd_line, args);
    } else if (firstWord.compare("jobs") == 0) {
        return new JobsCommand(cmd_line, &_job_list);
    } else if (firstWord.compare("fg") == 0) {
        return new ForegroundCommand(cmd_line, args, &_job_list);
    } else if (firstWord.compare("bg") == 0) {
        return new BackgroundCommand(cmd_line, args, &_job_list);
    } else if (firstWord.compare("quit") == 0) {
        return new QuitCommand(cmd_line, args, &_job_list);
    }
    return new ExternalCommand(cmd_line);
}

bool SmallShell::executeCommand(const char *cmd_line) {
    try {
        Command* cmd = CreateCommand(cmd_line);
        if (_isBackgroundComamnd(cmd_line)) {
            _job_list.addJob(cmd);
        }
        cmd->execute();

        if (dynamic_cast<QuitCommand *>(cmd)) {
            return false;
        }
    } catch (const Command::CommandError& e) {
        cerr << "smash error: " << e.what() << endl;
    }
    return true;
}

const std::string& SmallShell::name() const {
    return _name;
}

void SmallShell::handle_ctrl_z(int sig_num) {
    if (_running_cmd) {
        kill(_running_cmd->pid(), sig_num);
        _job_list.addJob(_running_cmd, true);
        _running_cmd = nullptr;
    }
}

void SmallShell::handle_sigchld(int sig_num) {
    FUNC_ENTRY()
    _job_list.removeFinishedJobs();
}

/* -------------- BuiltInCommand -------------- */

BuiltInCommand::BuiltInCommand(const char* cmd_line):
    Command(cmd_line) {
    // smash will run this code
    _pid = getpid();
}

std::string& BuiltInCommand::smash_name() {
    return _smash->_name;
}

char *BuiltInCommand::smash_cwd() {
    return _smash->_cwd;
}

bool &BuiltInCommand::smash_cd_called() {
    return _smash->_cd_called;
}

Command* &BuiltInCommand::smash_running_cmd() {
    return _smash->_running_cmd;
}

/* -------------- ExternalCommand -------------- */

ExternalCommand::ExternalCommand(const char* cmd_line):
    Command(cmd_line) {
}

void ExternalCommand::execute() {
    char cmd_line[COMMAND_ARGS_MAX_LENGTH];
    strcpy(cmd_line, this->cmd_line());

    bool bg_cmd = _isBackgroundComamnd(cmd_line);
    if (bg_cmd) {
        _removeBackgroundSign(cmd_line);
    }
    char* args[COMMAND_MAX_ARGS + 1];
    _parseCommandLine(cmd_line, args);

    int pid = fork();
    if (pid < 0) {
        perror("smash error: fork failed");
    } else if (pid == 0) {
        execvp(args[0], args);
        perror("smash error: execvp failed");
    } else {
        _pid = pid;
        if (!bg_cmd) {
            _smash->_running_cmd = this;
            if (waitpid(pid, nullptr, WUNTRACED) < 0) {
                perror("smash error: waitpid failed");
            }
            _smash->_running_cmd = nullptr;
        }
    }
}

/* -------------- ChpromptCommand -------------- */

ChpromptCommand::ChpromptCommand(const char* cmd_line, char* args[]):
    BuiltInCommand(cmd_line) {
    _new_name = "smash";
    if (args[1]) {
        _new_name = args[1];
    }
    _new_name.append("> ");
}

void ChpromptCommand::execute() {
    smash_name() = _new_name;
}

/* -------------- ShowPidCommand -------------- */

ShowPidCommand::ShowPidCommand(const char* cmd_line, char* args[]):
    BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
    // TODO: consider changing smash to _smash->name();
    cout << "smash pid is " << _pid << endl;
}

/* -------------- GetCurrDirCommand -------------- */

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line, char* args[]):
    BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
    char cwd[COMMAND_ARGS_MAX_LENGTH];
    cout << getcwd(cwd, sizeof(cwd)) << endl;
}

/* -------------- ChangeDirCommand -------------- */

ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char* args[]):
    BuiltInCommand(cmd_line) {
    if (args[2]) {
        throw Command::CommandError("cd: too many arguments");
    }
    _new_dir = args[1];
    if (strcmp(args[1], "-") == 0) {
        if (!smash_cd_called()) {
            throw Command::CommandError("cd: OLDPWD not set");
        }
        strcpy(_new_dir, smash_cwd());
    }
}

void ChangeDirCommand::execute() {
    char cwd[COMMAND_ARGS_MAX_LENGTH];
    getcwd(cwd, sizeof(cwd));

    if (chdir(_new_dir) != 0) {
        perror("smash error: chdir failed");
        return;
    }
    strcpy(smash_cwd(), cwd);
    smash_cd_called() = true;
}

/* -------------- JobsList::JobEntry -------------- */

JobsList::JobEntry::JobEntry(Command *cmd, bool stopped) {
    _start = time(nullptr);
    _cmd = cmd;
    _stopped = stopped;
}

Command *JobsList::JobEntry::cmd() {
    return _cmd;
}

bool &JobsList::JobEntry::stopped() {
    return _stopped;
}

/* -------------- JobsList -------------- */

JobsList::JobsList() {
    _next_jid = 1;
}

void JobsList::addJob(Command* cmd, bool stopped) {
    JobEntry *job = new JobEntry(cmd, stopped);
    job->_jid = _next_jid++;
    _jobs.push_back(job);
}

void JobsList::printJobsList() {
    for (const JobEntry *job : _jobs) {
        cout << "[" << job->_jid << "] " << job->_cmd->cmd_line();
        cout << " : " << job->_cmd->pid() << " ";
        cout << difftime(time(nullptr), job->_start) << " secs";
        if (job->_stopped) {
            cout << " (stopped)";
        }
        cout << endl;
    }
}

JobsList::JobEntry *JobsList::getJobById(int jid) {
    FUNC_ENTRY()
    for (JobEntry *job : _jobs) {
        if (job->_jid == jid) {
            return job;
        }
    }

        throw Command::CommandError("something");
}

bool JobsList::isStopped(int jobId){
	    for (JobEntry *job : _jobs) {
			if (job->_jid == jobId) {
				if (job->_stopped){
					return true;
				}
			}
		}
		return false;
}
	

JobsList::JobEntry *JobsList::getLastJob(int* lastJobId) {
    if (_jobs.empty()) {
        throw Command::CommandError("something2");
    }
    JobEntry *ret = _jobs.back();
    _jobs.pop_back();
    if (lastJobId) {
        *lastJobId = ret->_jid;
    }
    return ret;
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int* lastJobId) {
    FUNC_ENTRY()
    if (_jobs.empty()) {
        throw Command::CommandError("something3");
    }
    for (auto it = _jobs.rbegin(); it != _jobs.rend(); ++it) {
        if ((*it)->_stopped) {
            JobEntry *ret = *it;
            if (lastJobId) {
                *lastJobId = ret->_jid;
            }
            return ret;
        }
    }
    return nullptr;
}

void JobsList::removeJobById(int jid) {
    for (JobEntry *job : _jobs) {
        if (job->_jid == jid) {
            _jobs.remove(job);
            return;
        }
    }
}

void JobsList::killAllJobs() {
    cout << "smash: sending SIGKILL signal to " << _jobs.size() << " jobs:" << endl;
    for (const JobEntry *job : _jobs) {
        cout << job->_cmd->pid() << ": " << job->_cmd->cmd_line() << endl;
        kill(job->_cmd->pid(), SIGKILL);
    }
}

void JobsList::removeFinishedJobs() {
    FUNC_ENTRY()
    // FIXME
    list<JobEntry *>::iterator it = _jobs.begin();
    while (it != _jobs.end()) {
        if (waitpid(_jobs.back()->cmd()->pid(), nullptr, WNOHANG)) {
            _jobs.erase(it++);  // alternatively, i = items.erase(i);
        }
    }
}

/* -------------- JobsCommand -------------- */

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs):
    BuiltInCommand(cmd_line) {
    _jobs = jobs;
}

void JobsCommand::execute() {
    _jobs->printJobsList();
}

/* -------------- ForegroundCommand -------------- */

ForegroundCommand::ForegroundCommand(const char* cmd_line, char* args[], JobsList* jobs):
    BuiltInCommand(cmd_line) {
    JobsList::JobEntry *job;
    // todo: check cmd_line

	
    int jid;
	if (!args[1]){
		try{
		 job = jobs->getLastJob(&jid);
		}
		catch (...){
			throw Command::CommandError("fg: jobs list is empty");

	}
	}
    else if (args[1]) {
		if (args[2]){
				throw Command::CommandError("fg: invalid arguments");
			}
		try{
			jid = stoi(args[1]);
		}
		catch (...){
			throw Command::CommandError("fg: invalid arguments");
		}
		try{
		job = jobs->getJobById(jid);
		}
		catch (...){
			std::string first = "fg: job-id ";
			std::string middle = std::to_string(jid);
			std::string last = " does not exist";
			std::string msg = first + middle + last;
			throw Command::CommandError(msg);

    } 
	
    jobs->removeJobById(jid);
    _cmd = job->cmd();
	}
}

void ForegroundCommand::execute() {
    cout << _cmd->cmd_line() << " : " << _cmd->pid() << endl;
    kill(_cmd->pid(), SIGCONT);
    smash_running_cmd() = _cmd;
    waitpid(_cmd->pid(), nullptr, WUNTRACED);
}

/* -------------- BackgroundCommand -------------- */

BackgroundCommand::BackgroundCommand(const char* cmd_line, char* args[], JobsList* jobs):
    BuiltInCommand(cmd_line) {
    FUNC_ENTRY()

    JobsList::JobEntry *job;
    // todo: check cmd_line
    int jid;
     if (args[1]) {
		if (args[2]){
				throw Command::CommandError("bg: invalid arguments");
			}
		try{
			jid = stoi(args[1]);
		}
		catch (...){
			throw Command::CommandError("bg: invalid arguments");
		}
		try{
		job = jobs->getJobById(jid);
		}
		catch (...){
			std::string first = "bg: job-id ";
			std::string middle = std::to_string(jid);
			std::string last = " does not exist";
			std::string msg = first + middle + last;
			throw Command::CommandError(msg);
		}
		if (!jobs->isStopped(jid)){
			std::string first = "bg: job-id ";
			std::string middle = std::to_string(jid);
			std::string last = " is already running in the background";
			std::string msg = first + middle + last;
			throw Command::CommandError(msg);
    } 
    } else {
		try{
        job = jobs->getLastStoppedJob(&jid);
		}
		catch (...){
			throw Command::CommandError("bg: there is no stopped jobs to resume");

    }
	}
    job->stopped() = false;
    _cmd = job->cmd();

}

void BackgroundCommand::execute() {
    cout << _cmd->cmd_line() << " : " << _cmd->pid() << endl;
    kill(_cmd->pid(), SIGCONT);
}

/* -------------- QuitCommand -------------- */

QuitCommand::QuitCommand(const char* cmd_line, char* args[], JobsList* jobs):
    BuiltInCommand(cmd_line) {
    _kill = false;
    _jobs = jobs;
    if (args[1]) {
        if (strcmp(args[1], "kill")) {
            throw CommandError("todo");
        }
        _kill = true;
    }
}

void QuitCommand::execute() {
    if (_kill) {
        _jobs->killAllJobs();
    }
}