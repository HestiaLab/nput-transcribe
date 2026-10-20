#include "cli_args.h"

#include <errno.h>
#include <stdlib.h>

int cli_parse_int(const char *text, int32_t *out_value) {
    if (text == NULL || out_value == NULL) {
        return 0;
    }
    char *end = NULL;
    errno = 0;
    const long long value = strtoll(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE || value < 1 || value > INT32_MAX) {
        return 0;
    }
    *out_value = (int32_t)value;
    return 1;
}
