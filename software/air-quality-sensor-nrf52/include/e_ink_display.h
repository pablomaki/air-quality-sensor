#ifndef E_INK_DISPLAY_H
#define E_INK_DISPLAY_H

/**
 * @brief Initialize e-ink display
 * 
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_e_ink_display(void);

/**
 * @brief Update data displayed
 * 
 * @return int, 0 if ok, non-zero if an error occured
 */
int update_e_ink_display(void);

#endif // E_INK_DISPLAY_H