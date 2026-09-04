#include <air_quality_monitor.h>
#include <components/matter_handler.h>
#include <components/sensors.h>
#include <utils/variable_buffer.h>
#include <components/event_handler.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(air_quality_monitor);

#define SCHEDULE_SUCCESS 0
#define SCHEDULE_ALREADY_QUEUED 1

/**
 * @brief Lower priority work queue for handling Periodic task progress
 *
 */
#define PERIODIC_TASK_THREAD_STACK_SIZE 4096
#define PERIODIC_TASK_THREAD_PRIORITY K_PRIO_PREEMPT(0)
K_THREAD_STACK_DEFINE(periodic_task_stack, PERIODIC_TASK_THREAD_STACK_SIZE);
static struct k_work_q periodic_task_work_q;


/**
 * @brief Work item for periodic task that reads sensor data and advertises it
 *
 */
static struct k_work_delayable periodic_work;

/**
 * @brief Schedule the next work task
 *
 * @param delay Delay for launching the task
 * @return int, 0 if ok, non-zero if an error occured
 */
static int schedule_work_task(int64_t delay)
{
    int rc = 0;
    rc = k_work_schedule_for_queue(&periodic_task_work_q, &periodic_work, K_MSEC(delay));
    if (rc != SCHEDULE_SUCCESS && rc != SCHEDULE_ALREADY_QUEUED)
    {
        LOG_ERR("Error scheduling a task (err %d).", rc);
        return rc;
    }
    return 0;
}

/**
 * @brief Calculate the delay for the next task
 *
 * @param start_time_ms Start time of the task
 * @return int64_t Delay in milliseconds
 */
static int64_t calculate_task_delay(int64_t start_time_ms)
{
    int64_t delay = CONFIG_ADVERTISEMENT_INTERVAL / CONFIG_MEASUREMENTS_PER_INTERVAL - (k_uptime_get() - start_time_ms);
    if (delay < 0)
    {
        LOG_ERR("Missed deadline, scheduling immediately. Delay was %lld ms.", delay);
        delay = 0; // Prevent negative delay
    }
    return delay;
}

/**
 * @brief Periodic task that takes care of reading sensor data and update matter cluster states
 *
 * @param work Address of work item.
 */
static void periodic_task(struct k_work *work)
{
    static uint8_t measurement_counter = 0;
    bool success = true;
    int rc = 0;

    LOG_INF("Periodic task begin.");

    // Log time for calculating correct time to sleep
    int64_t start_time_ms = k_uptime_get();

    LOG_INF("Reading sensors.");
    rc = read_sensors();
    if (rc != 0)
    {
        LOG_WRN("Failed to read sensor data (err %d).", rc);
        success = false;
        dispatch_event(PERIODIC_TASK_WARNING);
    }

    // Increment measurement counter and print progress in log
    measurement_counter++;
    LOG_INF("Periodic measurement %d/%d done.", measurement_counter, CONFIG_MEASUREMENTS_PER_INTERVAL);

    // Stop the periodic task in short in case of not enough measurements made yet
    if (measurement_counter < CONFIG_MEASUREMENTS_PER_INTERVAL)
    {
        LOG_INF("Periodic task done, scheduling a new task.");
        int64_t delay = calculate_task_delay(start_time_ms);
        rc = schedule_work_task(delay);
        if (rc != 0)
        {
            dispatch_event(PERIODIC_TASK_ERROR);
        }
        LOG_INF("Task scheduled, entering idle state.");
        if (success)
        {
            dispatch_event(PERIODIC_TASK_SUCCESS);
        }

        return;
    }

    // Reset measurement counter
    measurement_counter = 0;

    LOG_INF("Advertising data.");
    rc = update_cluster_states();
    if (rc != 0)
    {
        dispatch_event(PERIODIC_TASK_WARNING);
    }

    LOG_INF("Periodic task done, scheduling a new task.");
    int64_t delay = calculate_task_delay(start_time_ms);
    rc = schedule_work_task(delay);
    if (rc != 0)
    {
        dispatch_event(PERIODIC_TASK_ERROR);
    }

    LOG_INF("Next task scheduled.");
}

int init_air_quality_monitor(void)
{
    int rc = 0;

    // Initialize LED controller
    LOG_INF("Initializing event handler.");
    rc = init_event_handler();
    if (rc != 0)
    {
        LOG_ERR("Error while initializing event handler (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        return rc;
    }
    LOG_INF("Event handler initialized succesfully.");

    // Initialize bluetooth
    LOG_INF("Initializing matter.");
    rc = init_matter();
    if (rc != 0)
    {
        LOG_ERR("Error while initializing matter (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        return rc;
    }

    // Initialize sensors
    LOG_INF("Initializing the sensors.");
    rc = init_sensors();
    if (rc != 0)
    {
        LOG_ERR("Error while initializing sensors (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        return rc;
    }
    LOG_INF("Sensors initialized succesfully.");
    dispatch_event(INITIALIZATION_SUCCESS);

    return 0;
}

int start_air_quality_monitor(void)
{
    int rc = 0;

    // Start periodic task work queue
    k_work_queue_start(&periodic_task_work_q, periodic_task_stack,
                       PERIODIC_TASK_THREAD_STACK_SIZE, PERIODIC_TASK_THREAD_PRIORITY, NULL);

    // Initialize periodic task and time the first task in 10 seconds
    LOG_INF("Setting up the periodic task for measuring and advertising data.");
    k_work_init_delayable(&periodic_work, periodic_task);
    rc = schedule_work_task(10000); // Start the first task in 10 seconds, some fuckery with timing and priorities here...
    if (rc != 0)
    {
        dispatch_event(STARTUP_ERROR);
        return rc;
    }
    LOG_INF("Periodic task started succesfully.");
    dispatch_event(STARTUP_SUCCESS);

    matter_dispatch_tasks();

    return 0;
}