#include "print.h"

#include <stdarg.h>
#include <stdio.h>

void print_err(const char* str, ...)
{
	va_list args;
	va_start(args, str);

	vfprintf(stderr, str, args);

	va_end(args);

	fprintf(stderr, EOL);
}

void print_dbg(const char* str, ...)
{
#if DEBUG
	va_list args;
	va_start(args, str);

	vfprintf(stderr, str, args);

	va_end(args);

	fprintf(stderr, EOL);
#else
	(void)str;
#endif
}
