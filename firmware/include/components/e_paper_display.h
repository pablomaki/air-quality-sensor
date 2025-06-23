#ifndef E_PAPER_DISPLAY_H
#define E_PAPER_DISPLAY_H

#include <zephyr/device.h>

/**
 * @brief Initialize e-paper display
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_e_paper_display(void);

/**
 * @brief Update data displayed
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int update_e_paper_display(void);

/**
 * @brief Suspend e-paper display
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int suspend_epd(void);

/**
 * @brief Activate e-paper display
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int activate_epd(void);

#endif // E_PAPER_DISPLAY_H