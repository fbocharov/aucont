#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

#include <unistd.h>

static bool file_exists(char const * name)
{
	return 0 == access(name, F_OK);
}


#endif // UTIL_H
