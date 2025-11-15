#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(application, LOG_LEVEL_INF);

void application_task(void)
{
    LOG_INF("Application task placeholder");
}
