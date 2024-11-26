/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define KEY_2_Pin GPIO_PIN_2
#define KEY_2_GPIO_Port GPIOE
#define KEY_1_Pin GPIO_PIN_3
#define KEY_1_GPIO_Port GPIOE
#define KEY_0_Pin GPIO_PIN_4
#define KEY_0_GPIO_Port GPIOE
#define LED_1_Pin GPIO_PIN_9
#define LED_1_GPIO_Port GPIOF
#define LED_2_Pin GPIO_PIN_10
#define LED_2_GPIO_Port GPIOF
#define ADS1292R_CS_Pin GPIO_PIN_4
#define ADS1292R_CS_GPIO_Port GPIOC
#define ADS1292R_ADC_RDY_Pin GPIO_PIN_0
#define ADS1292R_ADC_RDY_GPIO_Port GPIOB
#define ADS1292R_ADC_RDY_EXTI_IRQn EXTI0_IRQn
#define ADS1292R_START_Pin GPIO_PIN_11
#define ADS1292R_START_GPIO_Port GPIOF
#define ADS1292R_RES_Pin GPIO_PIN_14
#define ADS1292R_RES_GPIO_Port GPIOF
#define LCD_BL_Pin GPIO_PIN_15
#define LCD_BL_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
