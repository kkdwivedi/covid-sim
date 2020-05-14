#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

enum log_level {
	LOG_INFO,
	LOG_WARN,
	LOG_FAIL,
	_LOG_LEVEL_MAX,
};

//#define LOG_DEBUG
#define LOG_BUF_SIZE 8192U

extern char log_buf[];

static char* lev2str[_LOG_LEVEL_MAX] = {
	[LOG_INFO] = "INFO",
	[LOG_WARN] = "WARN",
	[LOG_FAIL] = "FAIL",
};

#ifdef LOG_DEBUG
#define log_more(lev) do {						\
		fprintf(stderr, "(%s) [%s::%s at %d] ",			\
			lev2str[lev], __FILE__, __func__, __LINE__);	\
	} while (0)
#else
#define log_more(x)
#endif

/* Use token pasting GNU extension */
#define log_internal(lev, str, ...) do {				\
		log_more(lev);						\
		fprintf(stderr, str "\n", ##__VA_ARGS__);		\
	} while (0)

#define log_info(...) log_internal(LOG_INFO, __VA_ARGS__)
#define log_warn(...) log_internal(LOG_WARN, __VA_ARGS__)
#define log_error(...) do { log_internal(LOG_FAIL, __VA_ARGS__); fflush(stderr); } while (0)
#define log_oom() log_error("Memory allocation failed!")

#endif
