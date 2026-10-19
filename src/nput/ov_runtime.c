#include "nput/asr.h"

#include <openvino/c/openvino.h>

#include <stdlib.h>
#include <string.h>

nput_status nput_asr_devices(char **out_csv) {
    if (out_csv == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_csv = NULL;

    ov_core_t *core = NULL;
    if (ov_core_create(&core) != OK) {
        return NPUT_ERR_RUNTIME;
    }
    ov_available_devices_t devices;
    memset(&devices, 0, sizeof(devices));
    if (ov_core_get_available_devices(core, &devices) != OK) {
        ov_core_free(core);
        return NPUT_ERR_RUNTIME;
    }

    size_t length = 1;
    for (size_t i = 0; i < devices.size; ++i) {
        length += strlen(devices.devices[i]) + 2;
    }
    char *csv = (char *)malloc(length);
    if (csv == NULL) {
        ov_available_devices_free(&devices);
        ov_core_free(core);
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    csv[0] = '\0';
    for (size_t i = 0; i < devices.size; ++i) {
        if (i > 0) {
            strcat(csv, ",");
        }
        strcat(csv, devices.devices[i]);
    }

    ov_available_devices_free(&devices);
    ov_core_free(core);
    *out_csv = csv;
    return NPUT_OK;
}
