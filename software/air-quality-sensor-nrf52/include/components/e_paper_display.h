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

#endif // E_PAPER_DISPLAY_H