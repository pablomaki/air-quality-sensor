#include <air_quality_monitor.h>
#include <components/matter_handler.h>
#include <components/sensors.h>
#include <utils/variable_buffer.h>
#include <components/event_handler.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(air_quality_monitor);

#define SCHEDULE_SUCCESS 0
#define SCHEDULE_ALREADY_QUEUED 1

/** @brief Delay before the first sample, to let Matter and Thread settle */
#define STARTUP_DELAY_MS 10000

/**
 * @brief Lower priority work queue for handling Periodic task progress
 *
 */
#define PERIODIC_TASK_THREAD_STACK_SIZE 4096
#define PERIODIC_TASK_THREAD_PRIORITY K_PRIO_PREEMPT(0)
K_THREAD_STACK_DEFINE(periodic_task_stack, PERIODIC_TASK_THREAD_STACK_SIZE);
static struct k_work_q periodic_task_work_q;


/**
 * @brief Work items for the two independent clocks
 *
 * Sampling and reporting are scheduled separately: the sampling rate belongs to
 * the sensors, the reporting rate to the application. Both run on the same work
 * queue, so they are serialised against each other and the value buffers need no
 * further locking.
 */
static struct k_work_delayable sample_work;
static struct k_work_delayable report_work;

/**
 * @brief Reschedule a work item, compensating for how long the work itself took
 *
 * @param work Work item to reschedule
 * @param period_ms Nominal period of the work item
 * @param start_time_ms Uptime at which this run started
 * @return int, 0 if ok, non-zero if an error occured
 */
static int reschedule(struct k_work_delayable *work, int64_t period_ms, int64_t start_time_ms)
{
    int64_t delay = period_ms - (k_uptime_get() - start_time_ms);

    if (delay < 0)
    {
        LOG_ERR("Missed deadline by %lld ms, scheduling immediately.", -delay);
        delay = 0;
    }

    int rc = k_work_schedule_for_queue(&periodic_task_work_q, work, K_MSEC(delay));
    if (rc != SCHEDULE_SUCCESS && rc != SCHEDULE_ALREADY_QUEUED)
    {
        LOG_ERR("Error scheduling a task (err %d).", rc);
        return rc;
    }
    return 0;
}

/**
 * @brief Read every enabled sensor into the value buffers
 *
 * @param work Address of work item.
 */
static void sample_task(struct k_work *work)
{
    int64_t start_time_ms = k_uptime_get();
    bool success = true;

    if (read_sensors() != 0)
    {
        LOG_WRN("Failed to read some sensor data.");
        success = false;
        dispatch_event(PERIODIC_TASK_WARNING);
    }

    if (reschedule(&sample_work, CONFIG_SAMPLE_INTERVAL_MS, start_time_ms) != 0)
    {
        dispatch_event(PERIODIC_TASK_ERROR);
    }
    else if (success)
    {
        dispatch_event(PERIODIC_TASK_SUCCESS);
    }
}

/**
 * @brief Publish the buffered values to the Matter data model
 *
 * @param work Address of work item.
 */
static void report_task(struct k_work *work)
{
    int64_t start_time_ms = k_uptime_get();

    if (update_cluster_states() != 0)
    {
        dispatch_event(PERIODIC_TASK_WARNING);
    }

    if (reschedule(&report_work, CONFIG_REPORT_INTERVAL_MS, start_time_ms) != 0)
    {
        dispatch_event(PERIODIC_TASK_ERROR);
    }
}

int init_air_quality_monitor(void)
{
    int rc = 0;
    int status = 0;

    // Initialize LED controller
    LOG_INF("Initializing event handler.");
    rc = init_event_handler();
    if (rc != 0)
    {
        LOG_ERR("Error while initializing event handler (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        status = rc;
    }
    else
    {
        LOG_INF("Event handler initialized succesfully.");
    }

    // Initialize matter
    LOG_INF("Initializing matter.");
    rc = init_matter();
    if (rc != 0)
    {
        LOG_ERR("Error while initializing matter (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        status = rc;
    }

    // Initialize sensors
    LOG_INF("Initializing the sensors.");
    rc = init_sensors();
    if (rc != 0)
    {
        LOG_ERR("Error while initializing sensors (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        status = rc;
    }
    else
    {
        LOG_INF("Sensors initialized succesfully.");
    }

    if (status == 0)
    {
        dispatch_event(INITIALIZATION_SUCCESS);
    }

    return status;
}

int start_air_quality_monitor(void)
{
    int rc = 0;

    // Start periodic task work queue
    k_work_queue_start(&periodic_task_work_q, periodic_task_stack,
                       PERIODIC_TASK_THREAD_STACK_SIZE, PERIODIC_TASK_THREAD_PRIORITY, NULL);

    LOG_INF("Setting up the sampling and reporting tasks.");
    k_work_init_delayable(&sample_work, sample_task);
    k_work_init_delayable(&report_work, report_task);

    int64_t now = k_uptime_get();

    rc = reschedule(&sample_work, STARTUP_DELAY_MS, now);
    if (rc == 0)
    {
        // Offset the first report so it publishes samples that already exist
        rc = reschedule(&report_work, STARTUP_DELAY_MS + CONFIG_SAMPLE_INTERVAL_MS, now);
    }

    if (rc != 0)
    {
        // Fall through to the dispatch loop so the node stays addressable
        LOG_ERR("Failed to schedule the periodic tasks (err %d).", rc);
        dispatch_event(STARTUP_ERROR);
    }
    else
    {
        LOG_INF("Sampling and reporting tasks started succesfully.");
        dispatch_event(STARTUP_SUCCESS);
    }

    matter_dispatch_tasks(); // Never returns

    return rc;
}