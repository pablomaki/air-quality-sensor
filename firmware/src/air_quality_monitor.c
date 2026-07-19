#include <air_quality_monitor.h>
#include <components/matter_handler.h>
#include <components/sensors.h>
#include <utils/variable_buffer.h>
#include <components/event_handler.h>
#include <components/state_manager.h>
#include <components/flash_manager.h>

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
 * @brief Semaphore to synchronize pairing completion
 *
 */
static struct k_sem pairing_sem;

/**
 * @brief Semaphore to synchronize advertising completion
 *
 */
static struct k_sem advertising_sem;

/**
 * @brief Work item for periodic task that reads sensor data and advertises it
 *
 */
static struct k_work_delayable periodic_work;

/**
 * @brief Update data to matter service and start data advertisement
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
static int advertise_data(void)
{
    int rc = 0;
    // Update and advertise data
    LOG_INF("Update cluster states.");
    rc = update_cluster_states();
    return rc;
}

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

    // Set state to measuring and idle idle some time for sensor warmup
    set_state(MEASURING);
#ifdef CONFIG_ENABLE_SGP40
    LOG_INF("Sensor warming up started, waiting for %d ms.", CONFIG_SENSOR_WARMUP_TIME_MS);
    k_sleep(K_MSEC(CONFIG_SENSOR_WARMUP_TIME_MS));
    LOG_INF("Sensor warm up complete.");
#endif

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
            set_state(ERROR);
        }
        LOG_INF("Task scheduled, entering idle state.");
        if (success)
        {
            dispatch_event(PERIODIC_TASK_SUCCESS);
        }

        // Enter idle state
        set_state(IDLE);
        return;
    }

    // Reset measurement counter
    measurement_counter = 0;

    LOG_INF("Periodic task done, scheduling a new task.");
    int64_t delay = calculate_task_delay(start_time_ms);
    rc = schedule_work_task(delay);
    if (rc != 0)
    {
        dispatch_event(PERIODIC_TASK_ERROR);
        set_state(ERROR);
    }

    LOG_INF("Task scheduled, going idle.");
    set_state(IDLE);
}

/**
 * @brief Callback for pairing result
 *
 * @param success True if a device was successfully paired, false if not
 */
static void pairing_result_callback(bool success)
{
    // Check pairing results and decide next action
    if (success)
    {
        LOG_INF("Device successfully paired.");
        dispatch_event(PAIRING_SUCCESS);
    }
    else
    {
        LOG_INF("Device pairing failed.");
        dispatch_event(PAIRING_FAILURE);
    }
}

int init_air_quality_monitor(void)
{
    int rc = 0;

    // Set state correctly
    set_state(INITIALIZING);

    // Initialize LED controller
    LOG_INF("Initializing event handler.");
    rc = init_event_handler();
    if (rc != 0)
    {
        LOG_ERR("Error while initializing event handler (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
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
        set_state(ERROR);
        return rc;
    }

    // Initialize sensors
    LOG_INF("Initializing the sensors.");
    rc = init_sensors();
    if (rc != 0)
    {
        LOG_ERR("Error while initializing sensors (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return rc;
    }
    LOG_INF("Sensors initialized succesfully.");

    LOG_INF("Initializing flash manager.");
    rc = init_flash_manager();
    if (rc != 0)
    {
        LOG_ERR("Error while initializing flash manager (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return rc;
    }
    LOG_INF("Flash manager initialized succesfully.");
    dispatch_event(INITIALIZATION_SUCCESS);

    // Enter idle state
    set_state(IDLE);

    return 0;
}

int start_air_quality_monitor(void)
{
    int rc = 0;

    // Enter idle state
    set_state(STARTUP);

    // Initialize pairing and advertisement semaphores
    k_sem_init(&pairing_sem, 0, 1);

    // Start pairing mode
    LOG_INF("Starting pairing, waiting for %d ms.", CONFIG_PAIRING_TIMEOUT);
    display_notification("Pairing");

    rc = start_pairing();
    if (rc != 0)
    {
        LOG_ERR("Error while starting pairing mode (err %d).", rc);
        dispatch_event(STARTUP_ERROR);
        set_state(ERROR);
        return rc;
    }

    // Wait for pairing to complete or timeout
    rc = k_sem_take(&pairing_sem, K_MSEC(CONFIG_PAIRING_TIMEOUT + 1000)); // Add 1s buffer
    if (rc != 0)
    {
        LOG_ERR("Pairing semaphore timeout (err %d).", rc);
        dispatch_event(STARTUP_ERROR);
        set_state(ERROR);
        return rc;
    }
    display_notification("");

    if (!has_bonded_devices())
    {
        LOG_ERR("No pairing occurred and no bonded devices exist.");
        dispatch_event(STARTUP_ERROR);
        set_state(ERROR);
        return rc;
    }

    rc = setup_data_advertisement();
    if (rc != 0)
    {
        LOG_ERR("Error while setting up normal advertisement (err %d).", rc);
        dispatch_event(STARTUP_ERROR);
        set_state(ERROR);
        return rc;
    }

    // Start periodic task work queue
    k_work_queue_start(&periodic_task_work_q, periodic_task_stack,
                       PERIODIC_TASK_THREAD_STACK_SIZE, PERIODIC_TASK_THREAD_PRIORITY, NULL);
    k_sem_init(&advertising_sem, 0, 1);

    // Initialize periodic task and time the first task in 10 seconds
    LOG_INF("Setting up the periodic task for measuring and advertising data.");
    k_work_init_delayable(&periodic_work, periodic_task);
    rc = schedule_work_task(10000); // Start the first task in 10 seconds, some fuckery with timing and priorities here...
    if (rc != 0)
    {
        dispatch_event(STARTUP_ERROR);
        set_state(ERROR);
        return rc;
    }
    LOG_INF("Periodic task started succesfully.");
    dispatch_event(STARTUP_SUCCESS);

    // Enter idle state
    set_state(IDLE);

    return 0;
}