#include "err.h"

#include <stdarg.h>
#include <stdio.h>

void error_init(struct error* err, const int val, const char* msg, ...)
{
	if (!err)
		return;

	err->val = val;

	va_list args;
	va_start(args, msg);

	vsnprintf(err->msg, ERRMSG_LEN_MAX, msg, args);

	va_end(args);
}
