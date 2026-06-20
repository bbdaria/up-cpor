// In-process wrapper around the vendored Metric-FF (v2.3).
//
// Metric-FF keeps hundreds of mutable globals and calls exit() on errors, so it
// is not reentrant and cannot safely be called repeatedly in one address space.
// Since SDR replans many times per problem, each solve runs in a forked child:
// FF is still compiled into this engine (no external binary, no stdout regex),
// but the child's globals and any exit() stay contained. The child sets
// gplan_capture_fd so FF's print_plan() writes the plan as structured lines
// ("OP arg arg\n") to a pipe; the parent reads them back.

#include "ff_wrapper.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" {
int ff_main(int argc, char* argv[]);
extern int gplan_capture_fd;
// When non-NULL, the FF scanners read the PDDL from these in-memory streams
// instead of fopen()ing the -o/-f paths (see load_ops_file / load_fct_file).
FILE* gff_ops_override = nullptr;
FILE* gff_fct_override = nullptr;
}

// Read the plan the child wrote to `read_fd` and split it into operator lines.
static std::vector<std::string> drain_plan(int read_fd, pid_t pid) {
    std::string buffer;
    char chunk[4096];
    ssize_t n;
    while ((n = read(read_fd, chunk, sizeof(chunk))) > 0) {
        buffer.append(chunk, static_cast<size_t>(n));
    }
    close(read_fd);
    waitpid(pid, nullptr, 0);

    std::vector<std::string> plan;
    size_t start = 0;
    while (start < buffer.size()) {
        size_t end = buffer.find('\n', start);
        if (end == std::string::npos) end = buffer.size();
        if (end > start) {
            plan.emplace_back(buffer.substr(start, end - start));
        }
        start = end + 1;
    }
    return plan;
}

// Redirect the child's stdout/stderr to /dev/null and route the plan to write_fd.
static void prepare_child(int write_fd) {
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) {
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO) close(devnull);
    }
    gplan_capture_fd = write_fd;
}

std::vector<std::string> ff_solve(const std::string& domain_path,
                                  const std::string& problem_path) {
    std::vector<std::string> plan;

    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) {
        return plan;  // empty => treated as "no plan"
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return plan;
    }

    if (pid == 0) {
        // ---- child ----
        close(pipe_fds[0]);  // close read end
        prepare_child(pipe_fds[1]);

        // Build argv: ff -o <domain> -f <problem> -i 0
        std::string dom = domain_path;
        std::string prob = problem_path;
        char arg0[] = "ff";
        char opt_o[] = "-o";
        char opt_f[] = "-f";
        char opt_i[] = "-i";
        char val_i[] = "0";
        char* argv[] = {arg0, opt_o, &dom[0], opt_f, &prob[0], opt_i, val_i, nullptr};

        ff_main(7, argv);   // FF exits internally on success (output_planner_info)
        _exit(0);           // safety net if it ever returns
    }

    // ---- parent ----
    close(pipe_fds[1]);  // close write end
    return drain_plan(pipe_fds[0], pid);
}

std::vector<std::string> ff_solve_strings(const std::string& domain_str,
                                          const std::string& problem_str) {
    std::vector<std::string> plan;

    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) {
        return plan;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return plan;
    }

    if (pid == 0) {
        // ---- child ----
        close(pipe_fds[0]);
        prepare_child(pipe_fds[1]);

        // Point FF's scanners at the PDDL held in memory (no disk I/O). These
        // buffers live in the child until ff_main exits it, so they stay valid
        // for the whole parse. The dummy -o/-f names are never fopen()ed.
        std::string dom = domain_str;
        std::string prob = problem_str;
        gff_ops_override = fmemopen(const_cast<char*>(dom.data()), dom.size(), "r");
        gff_fct_override = fmemopen(const_cast<char*>(prob.data()), prob.size(), "r");

        char arg0[] = "ff";
        char opt_o[] = "-o";
        char opt_f[] = "-f";
        char opt_i[] = "-i";
        char val_i[] = "0";
        char dom_name[] = "mem_ops";
        char prob_name[] = "mem_fct";
        char* argv[] = {arg0, opt_o, dom_name, opt_f, prob_name, opt_i, val_i, nullptr};

        if (gff_ops_override && gff_fct_override) {
            ff_main(7, argv);
        }
        _exit(0);
    }

    // ---- parent ----
    close(pipe_fds[1]);
    return drain_plan(pipe_fds[0], pid);
}
