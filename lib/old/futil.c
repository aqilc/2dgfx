
#include "stdbool.h"

#ifdef _WIN32
#include <io.h>
#define access _access_s
#else
#include <unistd.h>
#endif

bool exists(char *file) {
  return access(file, 0) == 0;
}