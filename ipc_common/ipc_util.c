
#include "ipc_util.h"
#include "ipc_debug.h"

static int util_time_clock(clockid_t clk_id, struct timeval* pTime)
{
    struct timespec stTime;
    memset(&stTime, 0, sizeof(struct timespec));
    memset(pTime, 0, sizeof(struct timeval));
    if (!clock_gettime(clk_id, &stTime))
    {
        pTime->tv_sec = stTime.tv_sec;
        pTime->tv_usec = stTime.tv_nsec/1000;
        return 0;
    }

    return -1;
}

int util_time_abs(struct timeval* pTime)
{
    CHECK(NULL != pTime, -1, "invalid parameter.\n");

    return util_time_clock(CLOCK_MONOTONIC, pTime);
}

int util_time_local(struct timeval* pTime)
{
    CHECK(NULL != pTime, -1, "invalid parameter.\n");

    return util_time_clock(CLOCK_REALTIME, pTime);
}

//µ¥Î»ºÁÃë
int util_time_sub(struct timeval* pStart, struct timeval* pEnd)
{
    CHECK(NULL != pStart, -1, "invalid parameter.\n");

    int IntervalTime = (pEnd->tv_sec -pStart->tv_sec)*1000 + (pEnd->tv_usec - pStart->tv_usec)/1000;

    return IntervalTime;
}

//µ¥Î»ºÁÃë
int util_time_pass(struct timeval* previous)
{
    CHECK(NULL != previous, -1, "invalid parameter.\n");

    struct timeval cur = {0, 0};
    util_time_abs(&cur);

    return util_time_sub(previous, &cur);
}

/*char* util_time_string(void)
{
    static char time[2][32];
    memset(time, 0, sizeof(time));
    struct timeval tv = {0, 0};
    util_time_local(&tv);
    strftime(time[0], sizeof(time[0]), "%F %H:%M:%S", localtime(&tv.tv_sec));
    snprintf(time[1], sizeof(time[1]), ".%ld", tv.tv_usec);
    strcat(time[0], time[1]);

    return time[0];
}*/

int util_file_size(char* path)
{
    CHECK(path, -1, "invalid parameter with: %#x\n", path);

    struct stat stStat;
    int ret = stat(path, &stStat);
    CHECK(ret == 0, -1, "error with %#x: %s\n", ret, strerror(errno));

    return stStat.st_size;
}

//¶ÁÈ¡Ö¸¶¨´óÐ¡
int util_file_read(char* path, char* buf, int len)
{
    CHECK(path, -1, "invalid parameter with: %#x\n", path);

    FILE* fp = fopen(path, "r");
    CHECK(fp, -1, "error with %#x: %s\n", fp, strerror(errno));

    int read_len = 0;
    while (read_len != len && !feof(fp))
    {
        int tmp = fread(buf + read_len, 1, len-read_len, fp);
        CHECK(tmp >= 0, -1, "error with %#x: %s\n", tmp, strerror(errno));
        read_len += tmp;
    }

    fclose(fp);
    return 0;
}

#define _CMD_LEN    (256)
static void _close_all_fds (void)
{
    int i;
    for (i = 0; i < sysconf(_SC_OPEN_MAX); i++) {
      if (i != STDIN_FILENO && i != STDOUT_FILENO && i != STDERR_FILENO)
        close(i);
    }
}
extern int __libc_fork (void);
static int _system (char *command)
{
    int pid = 0;
    int status = 0;
    char *argv[4];
    extern char **environ;

    if (NULL == command) {
        return -1;
    }

    pid = __libc_fork();        /* vfork() also works */
    if (pid < 0) {
        return -1;
    }
    if (0 == pid) {             /* child process */
        _close_all_fds();       /* è¿™æ˜¯æˆ‘è‡ªå·±å†™çš„ä¸€ä¸ªå‡½æ•°ï¼Œç”¨æ¥å…³é—­æ‰€æœ‰ç»§æ‰¿çš„æ–‡ä»¶æè¿°ç¬¦ã€‚å¯ä¸ç”¨ */
        argv[0] = "sh";
        argv[1] = "-c";
        argv[2] = command;
        argv[3] = NULL;

        execve ("/bin/sh", argv, environ);    /* execve() also an implementation of exec() */
        _exit (127);
    }

    // else
    /* wait for child process to start */
    while (waitpid(pid, &status, 0) < 0)
        if (errno != EINTR) {
            status = -1; /* error other than EINTR from waitpid() */
            break;
        }

    return (status);
}

int util_system(const char *format, ...)
{
    char cmdBuff[_CMD_LEN];
    va_list vaList;
	int i_ret = 0;
	int i_dbg = 0;
    va_start (vaList, format);
    vsnprintf ((char *)cmdBuff, sizeof(cmdBuff), format, vaList);
    va_end (vaList);
	
    i_ret = _system ((char *)cmdBuff);
    if ((i_ret) && (i_dbg == 1))
        DBG("err %s, cmd: %s\n",strerror(errno), cmdBuff);
	
    return i_ret;
}

