#ifndef REG_H
#define REG_H
#include <stdint.h>

#define BASE (0x40000000UL)
#define APB1 (BASE)
#define APB2 (BASE + 0x10000UL)
#define AHB1 (BASE + 0x20000UL)
#define AHB2 (BASE + 0x10000000UL)
#define AHB3 (BASE + 0x20000000UL)

#define PWR_BASE (APB1 + 0x7000UL)
#define RCC_BASE (AHB1 + 0X3800UL)
#define FLASH_READ_BASE (AHB1 + 0x3C00UL)

typedef struct {
  volatile uint32_t CR1;
  volatile uint32_t CSR1;
  volatile uint32_t CR2;
  volatile uint32_t CSR2;
} PWR_Reg_t;
#define PWR ((PWR_Reg_t *)(PWR_BASE))

typedef struct {
  volatile uint32_t CR;        /**< Offset: 0x00 */
  volatile uint32_t PLLCFGR;   /**< Offset: 0x04 */
  volatile uint32_t CFGR;      /**< Offset: 0x08 */
  volatile uint32_t CIR;       /**< Offset: 0x0C */
  volatile uint32_t AHB1RSTR;  /**< Offset: 0x10 */
  volatile uint32_t AHB2RSTR;  /**< Offset: 0x14 */
  volatile uint32_t AHB3RSTR;  /**< Offset: 0x18 */
  uint32_t RESERVED0;          /**< Padding 0x1C (4 bytes) */
  volatile uint32_t APB1RSTR;  /**< Offset: 0x20 */
  volatile uint32_t APB2RSTR;  /**< Offset: 0x24 */
  uint32_t RESERVED1[2];       /**< Padding 0x28-0x2C (8 bytes) */
  volatile uint32_t AHB1ENR;   /**< Offset: 0x30 */
  volatile uint32_t AHB2ENR;   /**< Offset: 0x34 */
  volatile uint32_t AHB3ENR;   /**< Offset: 0x38 */
  uint32_t RESERVED2;          /**< Padding 0x3C (4 bytes) */
  volatile uint32_t APB1ENR;   /**< Offset: 0x40 */
  volatile uint32_t APB2ENR;   /**< Offset: 0x44 */
  uint32_t RESERVED3[2];       /**< Padding 0x48-0x4C (8 bytes) */
  volatile uint32_t AHB1LPENR; /**< Offset: 0x50 */
  volatile uint32_t AHB2LPENR; /**< Offset: 0x54 */
  volatile uint32_t AHB3LPENR; /**< Offset: 0x58 */
  uint32_t RESERVED4;          /**< Padding 0x5C (4 bytes) */
  volatile uint32_t APB1LPENR; /**< Offset: 0x60 */
  volatile uint32_t APB2LPENR; /**< Offset: 0x64 */
  uint32_t RESERVED5[2];       /**< Padding 0x68-0x6C (8 bytes) */
  volatile uint32_t BDCR;      /**< Offset: 0x70 */
  volatile uint32_t CSR;       /**< Offset: 0x74 (Hộp đen Reset) */
} RCC_Reg_t;
#define RCC ((RCC_Reg_t *)(RCC_BASE))

typedef struct {
  volatile uint32_t ACR;     /**< Offset: 0x00 */
  volatile uint32_t KEYR;    /**< Offset: 0x04 */
  volatile uint32_t OPTKEYR; /**< Offset: 0x08 */
  volatile uint32_t SR;      /**< Offset: 0x0C */
  volatile uint32_t CR;      /**< Offset: 0x10 */
  volatile uint32_t OPTCR;   /**< Offset: 0x14 */
  volatile uint32_t OPTCR1;  /**< Offset: 0x18 */
} FLASH_Reg_t;
#define FLASH ((FLASH_Reg_t *)(FLASH_READ_BASE))

#define RCC_APB1ENR_PWREN (1U << 28U)
#define PWR_CR1_VOS_Pos (14U)
#define PWR_CR1_VOS_Msk (0x3U << PWR_CR1_VOS_Pos)
#define PWR_CR1_VOS_SCALE1 (3u << PWR_CR1_VOS_Pos)
// Over-drive mode
#define PWR_CR1_ODEN (1u << 16u)
#define PWR_CSR1_ODRDY (1u << 16u)
#define PWR_CR1_ODSWEN (1u << 17u)
#define PWR_CSR1_ODSWRDY (1u << 17u)

/* --- RCC_CR Bits (Bật HSE & PLL) --- */
#define RCC_CR_HSEON_Pos (16U)
#define RCC_CR_HSEON (0x1U << RCC_CR_HSEON_Pos)
#define RCC_CR_HSERDY_Pos (17U)
#define RCC_CR_HSERDY (0x1U << RCC_CR_HSERDY_Pos)
#define RCC_CR_PLLON_Pos (24U)
#define RCC_CR_PLLON (0x1U << RCC_CR_PLLON_Pos)
#define RCC_CR_PLLRDY_Pos (25U)
#define RCC_CR_PLLRDY (0x1U << RCC_CR_PLLRDY_Pos)

/* --- RCC_PLLCFGR Bits (Tham số PLL) --- */
#define RCC_PLLCFGR_PLLM_Pos (0U)
#define RCC_PLLCFGR_PLLM_Msk (0x3FU << RCC_PLLCFGR_PLLM_Pos)
#define RCC_PLLCFGR_PLLN_Pos (6U)
#define RCC_PLLCFGR_PLLN_Msk (0x1FFU << RCC_PLLCFGR_PLLN_Pos)
#define RCC_PLLCFGR_PLLP_Pos (16U)
#define RCC_PLLCFGR_PLLP_Msk (0x3U << RCC_PLLCFGR_PLLP_Pos)
#define RCC_PLLCFGR_PLLP_DIV2 (0x0U << RCC_PLLCFGR_PLLP_Pos) /* 00b = /2 */
#define RCC_PLLCFGR_PLLSRC_Pos (22U)
#define RCC_PLLCFGR_PLLSRC_Msk (0x1U << RCC_PLLCFGR_PLLSRC_Pos)
#define RCC_PLLCFGR_PLLSRC_HSE (0x1U << RCC_PLLCFGR_PLLSRC_Pos)
#define RCC_PLLCFGR_PLLQ_Pos (24U)
#define RCC_PLLCFGR_PLLQ_Msk (0xFU << RCC_PLLCFGR_PLLQ_Pos)

/* --- RCC_CFGR Bits (Bộ chia Bus & Switch nguồn clock) --- */
#define RCC_CFGR_SW_Pos (0U)
#define RCC_CFGR_SW_Msk (0x3U << RCC_CFGR_SW_Pos)
#define RCC_CFGR_SW_PLL (0x2U << RCC_CFGR_SW_Pos) /* 10b: PLL làm SYSCLK */
#define RCC_CFGR_SWS_Pos (2U)
#define RCC_CFGR_SWS_Msk (0x3U << RCC_CFGR_SWS_Pos)
#define RCC_CFGR_SWS_PLL (0x2U << RCC_CFGR_SWS_Pos) /* 10b: Đang dùng PLL */
#define RCC_CFGR_HPRE_Pos (4U)
#define RCC_CFGR_HPRE_Msk (0xFU << RCC_CFGR_HPRE_Pos)
#define RCC_CFGR_HPRE_DIV1 (0x0U << RCC_CFGR_HPRE_Pos) /* AHB div 1 */
#define RCC_CFGR_PPRE1_Pos (10U)
#define RCC_CFGR_PPRE1_Msk (0x7U << RCC_CFGR_PPRE1_Pos)
#define RCC_CFGR_PPRE1_DIV4 (0x5U << RCC_CFGR_PPRE1_Pos) /* 101b: APB1 div 4 */
#define RCC_CFGR_PPRE2_Pos (13U)
#define RCC_CFGR_PPRE2_Msk (0x7U << RCC_CFGR_PPRE2_Pos)
#define RCC_CFGR_PPRE2_DIV2 (0x4U << RCC_CFGR_PPRE2_Pos) /* 100b: APB2 div 2 */

/* --- FLASH_ACR --- */
#define FLASH_ACR_LATENCY_Pos (0U)
#define FLASH_ACR_LATENCY_Msk (0xFU << FLASH_ACR_LATENCY_Pos)
#define FLASH_ACR_LATENCY_6WS (0x6U << FLASH_ACR_LATENCY_Pos)
#define FLASH_ACR_PRFTEN (0x1U << 8U)
#define FLASH_ACR_ARTEN (0x1U << 9U)

/* --- RCC Peripheral Clock Enables --- */
#define RCC_AHB1ENR_GPIOAEN (1U << 0U)
#define RCC_AHB1ENR_GPIOBEN (1U << 1U)
#define RCC_AHB1ENR_GPIOCEN (1U << 2U)
#define RCC_AHB1ENR_GPIODEN (1U << 3U)
#define RCC_AHB1ENR_GPIOIEN (1U << 8U)
#define RCC_AHB1ENR_DMA1EN  (1U << 21U)
#define RCC_AHB1ENR_DMA2EN  (1U << 22U)

#define RCC_APB1ENR_CAN1EN  (1U << 25U)
#define RCC_APB2ENR_USART1EN (1U << 4U)
#define RCC_APB2ENR_SDMMC1EN (1U << 11U)

/* --- GPIO Peripheral --- */
#define GPIOA_BASE (AHB1 + 0x0000UL)
#define GPIOB_BASE (AHB1 + 0x0400UL)
#define GPIOC_BASE (AHB1 + 0x0800UL)
#define GPIOD_BASE (AHB1 + 0x0C00UL)
#define GPIOI_BASE (AHB1 + 0x2000UL)

typedef struct {
  volatile uint32_t MODER;   /**< Offset: 0x00 */
  volatile uint32_t OTYPER;  /**< Offset: 0x04 */
  volatile uint32_t OSPEEDR; /**< Offset: 0x08 */
  volatile uint32_t PUPDR;   /**< Offset: 0x0C */
  volatile uint32_t IDR;     /**< Offset: 0x10 */
  volatile uint32_t ODR;     /**< Offset: 0x14 */
  volatile uint32_t BSRR;    /**< Offset: 0x18 */
  volatile uint32_t LCKR;    /**< Offset: 0x1C */
  volatile uint32_t AFR[2];  /**< Offset: 0x20, 0x24 */
} GPIO_Reg_t;

#define GPIOA ((GPIO_Reg_t *)(GPIOA_BASE))
#define GPIOB ((GPIO_Reg_t *)(GPIOB_BASE))
#define GPIOC ((GPIO_Reg_t *)(GPIOC_BASE))
#define GPIOD ((GPIO_Reg_t *)(GPIOD_BASE))
#define GPIOI ((GPIO_Reg_t *)(GPIOI_BASE))

/* --- USART Peripheral (STM32F7 USART v2 with ISR / ICR) --- */
#define USART1_BASE (APB2 + 0x1000UL)

typedef struct {
  volatile uint32_t CR1;      /**< Offset: 0x00 */
  volatile uint32_t CR2;      /**< Offset: 0x04 */
  volatile uint32_t CR3;      /**< Offset: 0x08 */
  volatile uint32_t BRR;      /**< Offset: 0x0C */
  volatile uint32_t GTPR;     /**< Offset: 0x10 */
  volatile uint32_t RTOR;     /**< Offset: 0x14 */
  volatile uint32_t RQR;      /**< Offset: 0x18 */
  volatile uint32_t ISR;      /**< Offset: 0x1C Status Register */
  volatile uint32_t ICR;      /**< Offset: 0x20 Interrupt Clear (W1C) */
  volatile uint32_t RDR;      /**< Offset: 0x24 */
  volatile uint32_t TDR;      /**< Offset: 0x28 */
} USART_Reg_t;

#define USART1 ((USART_Reg_t *)(USART1_BASE))

#define USART_ISR_RXNE   (1U << 5U)
#define USART_ISR_TC     (1U << 6U)
#define USART_ISR_TXE    (1U << 7U)
#define USART_ISR_IDLE   (1U << 4U)
#define USART_ISR_ORE    (1U << 3U)
#define USART_ISR_FE     (1U << 1U)

#define USART_ICR_IDLECF (1U << 4U)
#define USART_ICR_ORECF  (1U << 3U)
#define USART_ICR_TCCF   (1U << 6U)

/* --- DMA Controller (DMA2 for USART1 RX Stream 2 / 5 Channel 4) --- */
#define DMA2_BASE (AHB1 + 0x6400UL)

typedef struct {
  volatile uint32_t CR;     /**< Offset: 0x00 */
  volatile uint32_t NDTR;   /**< Offset: 0x04 */
  volatile uint32_t PAR;    /**< Offset: 0x08 */
  volatile uint32_t M0AR;   /**< Offset: 0x0C */
  volatile uint32_t M1AR;   /**< Offset: 0x10 */
  volatile uint32_t FCR;    /**< Offset: 0x14 */
} DMA_Stream_Reg_t;

typedef struct {
  volatile uint32_t LISR;   /**< Offset: 0x00 */
  volatile uint32_t HISR;   /**< Offset: 0x04 */
  volatile uint32_t LIFCR;  /**< Offset: 0x08 */
  volatile uint32_t HIFCR;  /**< Offset: 0x0C */
  DMA_Stream_Reg_t S[8];    /**< Offset: 0x10..0xD0 */
} DMA_Reg_t;

#define DMA2 ((DMA_Reg_t *)(DMA2_BASE))

/* --- bxCAN Controller (CAN1) --- */
#define CAN1_BASE (APB1 + 0x6400UL)

typedef struct {
  volatile uint32_t TIR;    /**< Offset: 0x00 */
  volatile uint32_t TDTR;   /**< Offset: 0x04 */
  volatile uint32_t TDLR;   /**< Offset: 0x08 */
  volatile uint32_t TDHR;   /**< Offset: 0x0C */
} CAN_TxMailBox_Reg_t;

typedef struct {
  volatile uint32_t RIR;    /**< Offset: 0x00 */
  volatile uint32_t RDTR;   /**< Offset: 0x04 */
  volatile uint32_t RDLR;   /**< Offset: 0x08 */
  volatile uint32_t RDHR;   /**< Offset: 0x0C */
} CAN_FIFOMailBox_Reg_t;

typedef struct {
  volatile uint32_t FR1;    /**< Filter Bank Register 1 */
  volatile uint32_t FR2;    /**< Filter Bank Register 2 */
} CAN_FilterRegister_Reg_t;

typedef struct {
  volatile uint32_t MCR;    /**< Offset: 0x00 Master Control */
  volatile uint32_t MSR;    /**< Offset: 0x04 Master Status */
  volatile uint32_t TSR;    /**< Offset: 0x08 Transmit Status */
  volatile uint32_t RF0R;   /**< Offset: 0x0C Receive FIFO 0 */
  volatile uint32_t RF1R;   /**< Offset: 0x10 Receive FIFO 1 */
  volatile uint32_t IER;    /**< Offset: 0x14 Interrupt Enable */
  volatile uint32_t ESR;    /**< Offset: 0x18 Error Status */
  volatile uint32_t BTR;    /**< Offset: 0x1C Bit Timing */
  uint32_t RESERVED0[88];
  CAN_TxMailBox_Reg_t sTxMailBox[3];
  CAN_FIFOMailBox_Reg_t sFIFOMailBox[2];
  uint32_t RESERVED1[12];
  volatile uint32_t FMR;    /**< Offset: 0x200 Filter Master */
  volatile uint32_t FM1R;   /**< Offset: 0x204 Filter Mode */
  uint32_t RESERVED2;
  volatile uint32_t FS1R;   /**< Offset: 0x20C Filter Scale */
  uint32_t RESERVED3;
  volatile uint32_t FFA1R;  /**< Offset: 0x214 Filter FIFO Assignment */
  uint32_t RESERVED4;
  volatile uint32_t FA1R;   /**< Offset: 0x21C Filter Activation */
  uint32_t RESERVED5[8];
  CAN_FilterRegister_Reg_t sFilterRegister[28];
} CAN_Reg_t;

#define CAN1 ((CAN_Reg_t *)(CAN1_BASE))

#endif /* REG_H */