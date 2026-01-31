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
#include "sem.h"
#include "ccnet.h"
#include "common.h"
#include "shell.h"
#include "fs_port.h"
#include "comm.h"
#include "scp.h"
#include "timer.h"
#include <stdio.h>
#include <memory.h>

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

#define NODE_ID_A   1
#define NODE_ID_B   2
#define NODE_COUNT  3

static int pc_provider(void *ctx, void *data, size_t len)
{
    (void)ctx;
    HAL_UART_Transmit(&huart1, data, len, HAL_MAX_DELAY);
    return 0;
}


uint8_t send_buf[256];
uint8_t rcv_buf[256];
uint8_t packet[256];
semaphore_handle sem_process;
#define START 0xA55AA55A
#define CLOSE 0xDEAD5A5A

uint8_t appbuf[256];
void process(void)
{
    int start = 0;
    int end = 0;
    for (uint16_t i = 0; i < 256; i++) {
        uint32_t tmp = *((uint32_t *)&rcv_buf[i]);
        uint32_t magic = (uint32_t)tmp;
        if (magic == START) {
            start = i + 4;
        }
        if (magic == CLOSE && start < i) {
            end = i;
            break;
        }
    }
    if (end > start) {
        int len = end - start;
        void *start_data = &rcv_buf[start];
        memset(packet, 0, sizeof(packet));
        memcpy(packet, start_data, len);
        struct ccnet_hdr *ch = (struct ccnet_hdr *) packet;
        uint16_t packet_len = ntohs(ch->len) + sizeof(struct ccnet_hdr);

        ccnet_input(NULL, packet, packet_len);
        memset(appbuf, 0, sizeof(appbuf));
        int rn = scp_recv(1, appbuf, sizeof(appbuf));
        if (rn > 0) {
            printf("NodeA recv from SCP: %s\r\n", appbuf);
        }
    }
}
/* -------------------- NODE B PROVIDER -------------------- */
static int nodeB_provider(void *ctx, void *data, size_t len)
{
    (void)ctx;
    memset(send_buf, 0, sizeof(send_buf));

    /* START magic */
    uint32_t *p = (uint32_t *)send_buf;
    *p = START;

    /* payload */
    memcpy(send_buf + 4, data, len);

    /* CLOSE magic */
    p = (uint32_t *)&send_buf[len + 4];
    *p = CLOSE;

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

    HAL_UART_Transmit(&huart2, send_buf, sizeof(send_buf), HAL_MAX_DELAY);

    return 0;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        semaphore_release(sem_process);
        __HAL_UART_CLEAR_OREFLAG(&huart2);
        HAL_UART_Receive_IT(&huart2, rcv_buf, 256);
    }
}

void process_rcv(void *ctx)
{
    while (1) {
        task_enter();
        if (semaphore_take(sem_process, 1000) == true) {
            process();
        }
        task_exit();
    }
}


void task_shell(void *ctx)
{
    while (1) {
        task_enter();
        shell_main();
        task_exit();
    }
}

TaskHandle_t t_process;
TaskHandle_t t_shell;
TaskHandle_t t_timer;

int scp_ccnet_send(void *user, const void *buf, size_t len)
{
    struct ccnet_send_parameter csp = {
            .dst = 2,
            .ttl = CCNET_TTL_DEFAULT,
            .type = 1,
    };
    return ccnet_output(&csp, (void *)buf, (int)len);
}

uint32_t timer_count = 0;
void timer_excu()
{
    if (timer_count++ % 5) {
        scp_timer_process();
    }

}

#define scp_fd_AtoB 1
#define scp_fd_BtoA 1
struct scp_transport_class scp_trans = {
        .send  = scp_ccnet_send,
        .recv  = NULL,
        .close = NULL,
        .user  = NULL,
};
void APP(void)
{
    ccnet_init(NODE_ID_A, NODE_COUNT);

    ccnet_register_node_link(NODE_ID_A, scp_input);
    ccnet_register_node_link(NODE_ID_B, nodeB_provider);

    ccnet_graph_set_edge(NODE_ID_A, NODE_ID_B, 1);
    ccnet_graph_set_edge(NODE_ID_B, NODE_ID_A, 1);

    ccnet_build_routing_table();
    sem_process = semaphore_create(0);

    __HAL_UART_CLEAR_OREFLAG(&huart2);
    HAL_UART_Receive_IT(&huart2, rcv_buf, 256);

    comm_init_uart(&huart1);
    struct superblock sb;
    fs_port_init();

    if (fs_port_mount(&sb) != 0) {
        printf("FS mount after format failed!\r\n");
    }

    printf("FS mounted OK!\r\n");
    printf("Starting shell...\r\n");

    scp_init(4);
    scp_stream_alloc(&scp_trans, scp_fd_AtoB, scp_fd_AtoB);
    t_timer = timer_init(256, 10, 0, 10);
    timer_create(timer_excu, 1, run);
    HAL_Delay(100);
    task_create(process_rcv, 256, NULL, 0, 10, 0, &t_process);
    task_create(task_shell, 1024, NULL, 1, 10, 0, &t_shell);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    scheduler_init();
    APP();
    scheduler_start();

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
