#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <string>
#include <vector>
#include <list>

#define COMMAND_ARGS_MAX_LENGTH (80)
#define COMMAND_MAX_ARGS (20)

class SmallShell;
class Command {
public:
    Command(const char* cmd_line);
    virtual ~Command() {}
    virtual void execute() = 0;
    int pid();
    const char *cmd_line();

    class CommandError;
private:
    char* _cmd_line;
protected:
    SmallShell *_smash;
    int _pid;
};

class Command::CommandError {
private:
    std::string _message;
public:
    CommandError(const std::string& message);
    const std::string& what() const;
};

#define DECLARE_SMALL_SHELL()                       \
    /* todo: please declare it after JobList */     \
class SmallShell {                                  \
private:                                            \
    SmallShell();                                   \
    friend class BuiltInCommand;                    \
    friend class ExternalCommand;                   \
                                                    \
    std::string _name;                              \
    char *_cwd;                                     \
    bool _cd_called;                                \
    JobsList _job_list;                             \
                                                    \
    Command* _running_cmd;                          \
                                                    \
public:                                             \
    static SmallShell& getInstance();               \
    SmallShell(SmallShell const&)      = delete;    \
    void operator=(SmallShell const&)  = delete;    \
    ~SmallShell() {}                                \
                                                    \
    Command *CreateCommand(const char* cmd_line);   \
    bool executeCommand(const char* cmd_line);      \
    const std::string& name() const;                \
    void handle_ctrl_z(int sig_num);                \
    void handle_sigchld(int sig_num);               \
};


class BuiltInCommand : public Command {
protected:
    std::string& smash_name();
    char *smash_cwd();
    bool &smash_cd_called();
    Command* &smash_running_cmd();
public:
    BuiltInCommand(const char* cmd_line);
    virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char* cmd_line);
    virtual ~ExternalCommand() {}
    void execute() override;
};

class ChpromptCommand : public BuiltInCommand {
private:
    std::string _new_name;
public:
    ChpromptCommand(const char* cmd_line, char* args[]);
    virtual ~ChpromptCommand() {}
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char* cmd_line, char* args[]);
    virtual ~ShowPidCommand() {}
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char* cmd_line, char* args[]);
    virtual ~GetCurrDirCommand() {}
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
private:
    char *_new_dir;
public:
    ChangeDirCommand(const char* cmd_line, char* args[]);
    virtual ~ChangeDirCommand() {}
    void execute() override;
};

class JobsList {
public:
    class JobEntry;
    JobsList();
    ~JobsList() {}
    void addJob(Command* cmd, bool stopped = false);
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    JobEntry * getJobById(int jobId);
    void removeJobById(int jobId);
    JobEntry * getLastJob(int* lastJobId); // add support when it's nullptr
    JobEntry *getLastStoppedJob(int *jobId);
	bool isStopped(int jobId);
//   TODO: Add extra methods or modify exisitng ones as needed
private:
    std::list<JobEntry *> _jobs;
    int _next_jid;
};

class JobsList::JobEntry {
public:
    JobEntry(Command *cmd, bool stopped);
    JobEntry(const JobEntry&) = delete;
    void operator=(const JobEntry&) = delete;
    ~JobEntry() {}
    Command *cmd();
    bool &stopped();

private:
    int _jid;
    bool _stopped;
    time_t _start;
    Command *_cmd;

    friend JobsList;
};

DECLARE_SMALL_SHELL()

class JobsCommand : public BuiltInCommand {
    JobsList *_jobs;
public:
    JobsCommand(const char* cmd_line, JobsList* jobs);
    virtual ~JobsCommand() {}
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    Command *_cmd;
public:
    ForegroundCommand(const char* cmd_line, char* args[], JobsList* jobs);
    virtual ~ForegroundCommand() {}
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
    Command *_cmd;
public:
    BackgroundCommand(const char* cmd_line, char* args[], JobsList* jobs);
    virtual ~BackgroundCommand() {}
    void execute() override;
};

class QuitCommand : public BuiltInCommand {
    bool _kill;
    JobsList* _jobs;
public:
    QuitCommand(const char* cmd_line, char* args[], JobsList* jobs);
    virtual ~QuitCommand() {}
    void execute() override;
};




#endif //SMASH_COMMAND_H_
