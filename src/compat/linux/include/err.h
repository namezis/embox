/**
 * @file err.h
 * @brief Linux-compatible formatted error messages
 * @author Denis Deryugin <deryugin.denis@gmail.com>
 * @version
 * @date 29.11.2017
 */

#ifndef LINUX_ERR_H_
#define LINUX_ERR_H_

#include <stdlib.h>

/* TODO output format is not the same as "man err" requiers */

static inline void err(int eval, const char *fmt, ...) {
	/*
	va_list args;

	va_start(args, fmt);
	log_error(fmt, args);
	va_end(args);

	exit(eval); */
}

static inline void errx(int eval, const char *fmt, ...) {
	/*
	va_list args;

	va_start(args, fmt);
	log_error(fmt, args);
	va_end(args);

	exit(eval); */
}

static inline void warn(const char *fmt, ...) {
	/*
	va_list args;

	va_start(args, fmt);
	log_warning(fmt, args);
	va_end(args); */
}

static inline void warnx(const char *fmt, ...) {
/*	va_list args;

	va_start(args, fmt);
	log_warning(fmt, args);
	va_end(args); */
}

#endif /* LINUX_ERR_H_ */
