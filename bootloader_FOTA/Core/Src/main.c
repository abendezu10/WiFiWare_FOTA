/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "crc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "bootloader.h"
#include "st7789.h"
#include "st7789_bootloader.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */


/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int __io_putchar(int ch)
{
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart2, &c, 1, HAL_MAX_DELAY);
    return ch;
}



/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_CRC_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  HAL_GPIO_WritePin(TFT_BCKLIGHT_GPIO_Port, TFT_BCKLIGHT_Pin, GPIO_PIN_SET);

  ST7789_Init();

  setvbuf(stdout, NULL, _IONBF, 0);


  fw_chunk_t fw_chunk = {0};


  HAL_Delay(10);
  uint32_t btn = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);
  printf("PC13=%lu\n\r", btn);


  const bcb_t *bcb = (bcb_t*)BCB_SEC;
  printf("before if current slot: %d\n\r", bcb->active_slot);
  uint32_t active_addr = (bcb->active_slot) ? SLOT_A_SEC : SLOT_B_SEC;
  uint8_t update_complete = 0;
  //printf("active_addr: 0x%08lX\n\r", active_addr);

  if (btn == GPIO_PIN_SET) {
      if (IMG_valid(active_addr)) {

          printf("Jumping to active image... Address: 0x%08lx\r\n", active_addr);
          HAL_Delay(10);

          jmp_to_IMG(active_addr);
      } else {
          printf("No valid image. Staying in bootloader... Address: 0x%08lx\r\n", active_addr);

          while(1) { HAL_Delay(100); }
      }
  } else {
	  UI_Bootloader_ShowStart();
      printf("Staying in bootloader.\r\n");
      uint8_t recieved_signal = 2;


      HAL_UART_Transmit(&huart1, (uint8_t*)&recieved_signal, sizeof(recieved_signal), HAL_MAX_DELAY );
      uint32_t addr;
      uint8_t fw_chunk_signal = 1;

	  if(bcb->active_slot == SLOT_A){
		  printf("Writing to SLOT_B\n\r");

		  addr = SLOT_B_SEC;

		  HAL_FLASH_Unlock();
		  FLASH_Erase_Sector(FLASH_SECTOR_6, FLASH_VOLTAGE_RANGE_3);
		  FLASH_Erase_Sector(FLASH_SECTOR_7, FLASH_VOLTAGE_RANGE_3);
		  HAL_FLASH_Lock();

		  bcb_slotswitch(SLOT_B);
	  } else if(bcb->active_slot == SLOT_B){
		  printf("Writing to SLOT_A\n\r");
		  addr = SLOT_A_SEC;


		  HAL_FLASH_Unlock();
		  FLASH_Erase_Sector(FLASH_SECTOR_3, FLASH_VOLTAGE_RANGE_3);
		  FLASH_Erase_Sector(FLASH_SECTOR_4, FLASH_VOLTAGE_RANGE_3);
		  FLASH_Erase_Sector(FLASH_SECTOR_5, FLASH_VOLTAGE_RANGE_3);
	      HAL_FLASH_Lock();

	      bcb_slotswitch(SLOT_A);
	  }else{
    	  printf("ERROR HAS OCCURED! BCB IS NOT 1 OR 0");
    	  update_complete = 0;
      }

	  do{
		  fw_chunk_t fw_chunk = {0};

	      HAL_UART_Receive(&huart1, (uint8_t*)&fw_chunk, sizeof(fw_chunk), HAL_MAX_DELAY);

	      if (fw_chunk.length > 0 && fw_chunk.length <= FW_IMG_SIZE && fw_chunk.last_chunk <= 1) {
	    	  if (fw_chunk.length >= 4) {
	    		  uint32_t first_word;
	    		  memcpy(&first_word, &fw_chunk.fw_img[0], sizeof(first_word));

	    		  uint32_t last_word;
	    		  memcpy(&last_word, &fw_chunk.fw_img[fw_chunk.length - 4], sizeof(last_word));

	    		  printf("len=%d last_chunk=%d first_word=%08X last_word=%08X and last byte=%02x\r\n",
	    			          			             fw_chunk.length, fw_chunk.last_chunk, first_word, last_word,fw_chunk.fw_img[fw_chunk.length - 1]);
	    	  } else if (fw_chunk.length > 0) {
	    		  printf("len=%d last_chunk=%d first_byte=%02X last_byte=%02X\r\n",
	    			             fw_chunk.length, fw_chunk.last_chunk,
	    			             fw_chunk.fw_img[0],
	    			             fw_chunk.fw_img[fw_chunk.length - 1]);
	    	  }

	    	  if (write_to_flash(&fw_chunk, addr)) {

	    		  printf("success ...\n\r");
	    	  }
	    	  addr += fw_chunk.length;
	    	  printf("address = 0x%08lX\r\n", (uint32_t)addr);

	    	  if (HAL_UART_Transmit(&huart1, &fw_chunk_signal, 1, HAL_MAX_DELAY) != HAL_OK) {
	    		  printf("transmit error\n");
	    	  }

	    	  if (fw_chunk.last_chunk == 1) {
	    		  printf("Last chunk, breaking.\n");
	    		  break;
	    	  }
	      }

	  }while(1);

	  printf("after if current slot: %d\n\r", bcb->active_slot);
	  update_complete = 1;

      if(update_complete)
    	  UI_Bootloader_ShowDone();
      else
    	  UI_Bootloader_ShowFailed();


      printf("Update finished! Hold Button during Reset for the new image!");

  }


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
