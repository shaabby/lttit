/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
#include "stm32f1xx_hal.h"
#include "schedule.h"
#include "ccnet.h"
#include "common.h"
#include <stdio.h>
#include <memory.h>

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

#define NODE_ID_A   1
#define NODE_ID_B   2
#define NODE_COUNT  3

static uint8_t uart1_rx;
static uint8_t uart2_rx;

static int pc_provider(void *ctx, void *data, size_t len)
{
    (void)ctx;
    HAL_UART_Transmit(&huart1, data, len, HAL_MAX_DELAY);
    return 0;
}


#define START 0xA55A
#define CLOSE 0xDEAD
static uint8_t tmp_buf[256];
static uint8_t packet[128];
static uint8_t cnt = 0;

void process(void)
{
    int start = 0;
    int end = 0;
    for (uint16_t i = 0; i < 256; i++) {
        uint16_t tmp = *((uint16_t *)&tmp_buf[i]);
        uint16_t magic = (uint16_t)tmp;
        if (magic == START) {
            start = i + 2;
        }
        if (magic == CLOSE && start < i) {
            end = i;
            break;
        }
    }
    if (end > start) {
        int len = end - start;
        void *start_data = &tmp_buf[start];
        memcpy(packet, start_data, len);
        struct ccnet_hdr *ch = (struct ccnet_hdr *) packet;
        int packet_len = htons(ch->len) + sizeof(struct ccnet_hdr);
        ccnet_input(NULL, packet, packet_len);
        memset(packet, 0, sizeof(packet));
    }
}


uint8_t send_buf[256];
#define START 0xA55A
#define CLOSE 0xDEAD
static int nodeB_provider(void *ctx, void *data, size_t len)
{
    (void)ctx;
    memset(send_buf, 0, sizeof(send_buf));
    uint16_t *p = (uint16_t *)send_buf;
    *p = START;
    p = (uint16_t *)&send_buf[len + 2];
    *p = CLOSE;
    memcpy(send_buf + 2, data, len);
    for (int i = 0; i < 256; i++) {
        HAL_UART_Transmit(&huart1, (void *)&send_buf[i], 1, HAL_MAX_DELAY);
    }
    return 0;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        tmp_buf[cnt++] = uart2_rx;
        if (cnt == 255) {
            cnt = 0;
            process();
        }

        HAL_UART_Receive_IT(&huart2, &uart2_rx, 1);
    }
}

void rec_pc_input(void *ctx)
{
    while (1) {
        char a = 0;
        HAL_UART_Receive(&huart1, &a, 1, HAL_MAX_DELAY);
        if (a != 0) {
            struct ccnet_send_parameter csp;
            csp.dst = NODE_ID_B;
            csp.ttl = CCNET_TTL_DEFAULT;
            csp.type = CCNET_TYPE_DATA;

            ccnet_output(&csp, &a, 1);
        }

        TaskDelay(10);
    }
}

TaskHandle_t t2;
void APP(void)
{
    ccnet_init(NODE_ID_A, NODE_COUNT);

    ccnet_register_node_link(NODE_ID_A, pc_provider);
    ccnet_register_node_link(NODE_ID_B, nodeB_provider);

    ccnet_graph_set_edge(NODE_ID_A, NODE_ID_B, 1);
    ccnet_graph_set_edge(NODE_ID_B, NODE_ID_A, 1);

    ccnet_build_routing_table();

    HAL_UART_Receive_IT(&huart2, &uart2_rx, 1);
    TaskCreate(rec_pc_input, 64, NULL, 0, 10, 1000, &t2);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    SchedulerInit();
    APP();
    SchedulerStart();

    while (1) {}
}


/* USER CODE END 0 */

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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

#ifdef  USE_FULL_ASSERT
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
