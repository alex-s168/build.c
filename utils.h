// FROM https://raw.githubusercontent.com/alex-s168/allib/refs/heads/main/utils.h

#ifndef UTILS_H
#define UTILS_H

#include <string.h>

#define SPLITERATE(str,split,p) for (const char *p = strtok(str, split); p != NULL; p = strtok(NULL, split))

#endif //UTILS_H
