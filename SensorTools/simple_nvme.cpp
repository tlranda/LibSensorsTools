#include "sensor_tool.h"

int main(void) {
    args.debug = 0;
    args.format = 2;
    args.nvme = true;

    cache_nvme();
    update_nvme();

    return 0;
}
