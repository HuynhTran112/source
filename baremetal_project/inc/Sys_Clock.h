#ifndef SYSTEM_CLOCK_H
#define SYSTEM_CLOCK_H

#include "Reg.h"
#include <stdint.h>


typedef enum {
  RESET_REASON_UNKNOWN = 0,
  RESET_REASON_POR,       /**< Power-on / Power-down Reset (Cắm nguồn) */
  RESET_REASON_PIN,       /**< External Pin Reset (Bấm nút B1 NRST) */
  RESET_REASON_SOFTWARE,  /**< Software Reset (NVIC_SystemReset) */
  RESET_REASON_IWDG,      /**< Independent Watchdog Reset */
  RESET_REASON_WWDG,      /**< Window Watchdog Reset */
  RESET_REASON_LOW_POWER, /**< Low-Power Management Reset */
  RESET_REASON_BOR        /**< Brown-out Reset (Sụt nguồn) */
} SystemResetReason_t;

/* Khai báo nguyên mẫu hàm (Prototypes) */
void SystemClock_Config_216MHz(void);
SystemResetReason_t System_GetResetReason(void);
const char *System_GetResetReasonString(SystemResetReason_t reason);
void System_ClearResetFlags(void);

#endif /* SYSTEM_CLOCK_H */