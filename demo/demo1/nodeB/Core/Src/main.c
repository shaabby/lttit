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
#include "fs.h"
#include "fs_port.h"
#include "shell.h"
#include "sem.h"
#include "comm.h"
#include "ccnet.h"
#include "common.h"
#include "scp.h"
#include "timer.h"
#include <memory.h>
#include <stdio.h>

extern UART_HandleTypeDef huart1;

#define NODE_ID_A 1
#define NODE_ID_B 2
#define NODE_COUNT 3

semaphore_handle sem;
uint8_t send_buf[256];
uint8_t rcv_buf[256];
uint8_t packet[256];

#define START 0xA55AA55A
#define CLOSE 0xDEAD5A5A

/* ---------- SCP transport & stream ---------- */

#define SCP_FD_A2B 1
#define SCP_FD_B2A 1

static int scp_ccnet_send(void *user, const void *buf, size_t len)
{
    (void)user;
    struct ccnet_send_parameter p = {
            .dst  = NODE_ID_A,
            .ttl  = CCNET_TTL_DEFAULT,
            .type = CCNET_TYPE_DATA,
    };
    return ccnet_output(&p, (void *)buf, (int)len);
}

static struct scp_transport_class scp_trans = {
        .send  = scp_ccnet_send,
        .recv  = NULL,
        .close = NULL,
        .user  = NULL,
};

/* ---------- timer task for SCP ---------- */

TaskHandle_t t_timer;
static void timer_excu(void *ctx)
{
    (void)ctx;
    scp_timer_process();
}

/* ---------- UART framing (START/CLOSE) ---------- */

int buf_init(void)
{
    memset(send_buf, 0, sizeof(send_buf));
    memset(rcv_buf, 0, sizeof(rcv_buf));
    memset(packet, 0, sizeof(packet));
    return 0;
}

static int nodeA_provider(void *ctx, void *data, size_t len)
{
    (void)ctx;
    memset(send_buf, 0, sizeof(send_buf));

    uint32_t *p = (uint32_t *)send_buf;
    *p = START;

    memcpy(send_buf + 4, data, len);

    p = (uint32_t *)&send_buf[len + 4];
    *p = CLOSE;

    HAL_UART_Transmit(&huart1, (void *)send_buf, sizeof(send_buf), HAL_MAX_DELAY);

    return 0;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        semaphore_release(sem);
        __HAL_UART_CLEAR_OREFLAG(&huart1);

        __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
        HAL_NVIC_EnableIRQ(USART1_IRQn);

        HAL_UART_Receive_IT(&huart1, rcv_buf, 256);
    }
}

/* ---------- shell over SCP (application) ---------- */

int shell_process_mes(void *ctx, void *data, size_t len)
{
    (void)ctx;
    shell_on_message(data, len);
    return 0;
}

TaskHandle_t t_shell;

/* Parse UART frame, feed into ccnet (which feeds SCP) */
void shell_process_remote(void)
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

        struct ccnet_hdr *ch = (struct ccnet_hdr *)packet;
        uint16_t packet_len = ntohs(ch->len) + sizeof(struct ccnet_hdr);

        /* Feed raw ccnet packet into stack (upper layer is SCP now) */
        ccnet_input(NULL, (void *)packet, packet_len);
    }
}

/* Task: wait for UART frame, push into ccnet/SCP */
void task_shell_rx(void *ctx)
{
    (void)ctx;
    while (1) {
        if (semaphore_take(sem, 10) == true) {
            shell_process_remote();
        }
    }
}

/* Task: read from SCP stream and feed shell */
TaskHandle_t t_scp_shell;

static uint8_t buf[256];
static void task_scp_shell(void *ctx)
{
    (void)ctx;

    while (1) {
        int rn = scp_recv(SCP_FD_B2A, buf, sizeof(buf));
        if (rn > 0) {
            shell_on_message(buf, (size_t)rn);
        }
    }
}

void send(void *ctx, void *data, int len)
{
    (void)ctx;
    scp_send(1, data, len);
}

void fs_init_hello(void)
{
    struct inode *ino = NULL;
    const char *msg = "hello world\n";
    if (fs_open("/hello.c", O_CREAT, &ino) < 0) {
        return;
    }
    if (fs_write(ino, 0, msg, (uint32_t)strlen(msg)) < 0) {
        fs_close(ino);
        return;
    }
    fs_close(ino);
    fs_sync();
}

void APP(void)
{
    buf_init();
    /* CCNet init as node B */
    ccnet_init(NODE_ID_B, NODE_COUNT);

    /* Node B upper layer is SCP now */
    ccnet_register_node_link(NODE_ID_B, scp_input);
    /* Node A link provider: wrap SCP/CCNet payload into UART frame */
    ccnet_register_node_link(NODE_ID_A, nodeA_provider);

    ccnet_graph_set_edge(NODE_ID_A, NODE_ID_B, 1);
    ccnet_graph_set_edge(NODE_ID_B, NODE_ID_A, 1);

    ccnet_build_routing_table();

    /* SCP init and stream allocation */
    scp_init(4);
    scp_stream_alloc(&scp_trans, SCP_FD_B2A, SCP_FD_B2A);

    struct superblock sb;
    fs_port_init();
    if (fs_port_mount(&sb) != 0)
        printf("FS mount failed!\r\n");
    else
        printf("FS mounted OK!\r\n");

    //fs_init_hello();

    sem = semaphore_create(0);
    __HAL_UART_CLEAR_OREFLAG(&huart1);

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_UART_Receive_IT(&huart1, rcv_buf, 256);

    timer_create(timer_excu, 1, run);

    /* UART¡úccnet/SCP feeder (BE) */
    task_create(task_shell_rx, 300, NULL, 0, 10, 0, &t_shell);

    /* SCP¡úshell bridge (BE) */
    task_create(task_scp_shell, 800, NULL, 0, 10, 0, &t_scp_shell);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();

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
