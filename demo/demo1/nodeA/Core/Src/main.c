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
#include "fs_port.h"
#include "schedule.h"
#include "sem.h"
#include "ccnet.h"
#include "common.h"
#include "shell.h"
#include "comm.h"
#include "scp.h"
#include "timer.h"
#include "rpc.h"
#include "rpc_gen.h"
#include "lexer.h"
#include "parser.h"
#include "heap.h"
#include "ir_lowering.h"
#include "common.h"
#include "ccbpf.h"
#include <stdio.h>
#include <memory.h>

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

#define NODE_ID_A   1
#define NODE_ID_B   2
#define NODE_COUNT  3

struct rpc_transport_class *g_rpc_transport = NULL;

static size_t rpc_scp_send(void *user, const uint8_t *buf, size_t len)
{
    (void)user;
    return (size_t)scp_send(NODE_ID_A, (void *)buf, (int)len);
}

static size_t rpc_scp_recv(void *user, uint8_t *buf, size_t maxlen)
{
    (void)user;
    (void)buf;
    (void)maxlen;
    return 0;
}

static void rpc_scp_close(void *user)
{
    (void)user;
}

static int pc_provider(void *ctx, void *data, size_t len)
{
    (void)ctx;
    HAL_UART_Transmit(&huart1, data, len, HAL_MAX_DELAY);
    return 0;
}


uint8_t send_buf[256];
uint8_t rcv_buf[200];
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
            rpc_on_data(g_rpc_transport, appbuf, (size_t)rn);
            printf("%s\r\n", appbuf);
        }
    }
}

static int nodeB_provider(void *ctx, void *data, size_t len)
{
    (void)ctx;
    memset(send_buf, 0, sizeof(send_buf));

    uint32_t *p = (uint32_t *)send_buf;
    *p = START;
    memcpy(send_buf + 4, data, len);
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

int scp_ccnet_send(void *user, const void *buf, size_t len)
{
    struct ccnet_send_parameter csp = {
            .dst = 2,
            .ttl = CCNET_TTL_DEFAULT,
            .type = 1,
    };
    return ccnet_output(&csp, (void *)buf, (int)len);
}

void timer_excu()
{
    scp_timer_process();
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
    timer_create(timer_excu, 50, run);

    rpc_init(4, 4, 4);
    rpc_register_all();
    g_rpc_transport =
            rpc_trans_class_create((void *)rpc_scp_send,
                                   (void *)rpc_scp_recv,
                                   (void *)rpc_scp_close,
                                   NULL);

    rpc_bind_transport("fs.operation", g_rpc_transport);
    rpc_bind_transport("bpf.load_and_attach", g_rpc_transport);

    HAL_Delay(100);
    task_create(process_rcv, 256, NULL, 1, 12, 0, &t_process);
    task_create(task_shell, 800, NULL, 4, 10, 0, &t_shell);
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

/*
int main(void)
{
    const char *src =
            "struct udp_hdr {\n"
            "    unsigned short sport;\n"
            "    unsigned short dport;\n"
            "};\n"
            "\n"
            "int hook(void *ctx)\n"
            "{\n"
            "    unsigned int x;\n"
            "    unsigned int y;\n"
            "    struct udp_hdr *uh;\n"
            "\n"
            "    uh = (struct udp_hdr *)&ctx[0];\n"
            "    x = ntohs(uh->sport);\n"
            "    print(x);\n"
            "    y = ntohs(uh->dport);\n"
            "    print(y);\n"
            "    return x + y;\n"
            "}\n";

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();

    struct superblock sb;
    fs_port_init();

    if (fs_port_mount(&sb) != 0) {
        printf("FS mount after format failed!\r\n");
    }

    printf("FS mounted OK!\r\n");
    printf("Starting shell...\r\n");

    cmd_mem();

    compiler_init(16, (4*1024), (2*1024));
    lexer_set_input_buffer(src, strlen(src));

    struct lexer lex;
    lexer_init(&lex);

    struct Parser *p = parser_new(&lex);

    parser_program(p);

    cmd_mem();
    heap_debug_dump_leaks();

    frontend_destroy(&lex);

    cmd_mem();
    heap_debug_dump_leaks();

    mg_region_print_pools(frontend_region);
    mg_region_print_pools(longterm_region);
    mg_region_print_pools(ir_region);

    struct bpf_builder b;
    bpf_builder_init(&b, (3*1024));

    struct ir_mes im;
    ir_mes_get(&im);

    ir_lower_program(im.ir_head, im.label_count, &b);

    struct bpf_insn *prog = bpf_builder_data(&b);
    int prog_len = bpf_builder_count(&b);

    size_t image_len = 0;
    uint8_t *image = ccbpf_pack_memory(prog, (size_t)prog_len, &image_len);

    printf("=== CCBPF IMAGE READY ===\n");
    printf("Image at %p, size = %u bytes\n", image, (unsigned)image_len);

    struct inode *ino;
    if (fs_open("/prog.ccbpf", O_CREAT | O_RDWR, &ino) != 0) {
        printf("fs_open for write failed\n");
        return 0;
    }

    int w = fs_write(ino, 0, image, (uint32_t)image_len);
    printf("fs_write wrote %d bytes\n", w);
    fs_close(ino);
    fs_sync();
    mg_region_print_pools(backend_region);
    bpf_builder_free(&b);

    cmd_mem();

    return 0;
}

struct udp_hdr {
    uint16_t sport;
    uint16_t dport;
    uint16_t len;
    uint16_t checksum;
};

uint32_t udp_input(uint8_t *frame, size_t frame_size)
{
    uint32_t ret = hook_run("hook_udp_input", frame, frame_size);

    struct udp_hdr *uh = (struct udp_hdr *)frame;
    uint16_t sport = ntohs(uh->sport);
    uint16_t dport = ntohs(uh->dport);

    printf("udp_input: sport=%u dport=%u hook_ret=%u\n",
           sport, dport, (unsigned)ret);

    return ret;
}

int main()
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    struct superblock sb;
    fs_port_init();
    if (fs_port_mount(&sb) != 0) {
        printf("FS mount failed\n");
        return 0;
    }

    struct inode *ino;
    if (fs_open("/prog.ccbpf", O_RDONLY, &ino) != 0) {
        printf("fs_open /prog.ccbpf failed\n");
        return 0;
    }

    uint32_t fsize = fs_get_size(ino);
    uint8_t *image = heap_malloc(fsize);
    if (!image) {
        printf("heap_malloc(%u) failed\n", (unsigned)fsize);
        fs_close(ino);
        return 0;
    }

    int r = fs_read(ino, 0, image, fsize);
    fs_close(ino);
    if (r != (int)fsize) {
        printf("fs_read size mismatch: %d vs %u\n", r, (unsigned)fsize);
        heap_free(image);
        return 0;
    }

    if (hook_attach("hook_udp_input", image, fsize) != 0) {
        printf("hook_attach failed\n");
        heap_free(image);
        return 0;
    }
    heap_free(image);

    printf("hook_udp_input attached from /prog.ccbpf\n");

    uint8_t pkt[64];
    struct udp_hdr *uh = (struct udp_hdr *)pkt;

    uh->sport = htons(10000);
    uh->dport = htons(20000);
    uh->len   = htons(20);
    uh->checksum = 0;

    uint32_t ret = udp_input(pkt, 20);
    printf("udp_input() returned %u\n", (unsigned)ret);

    while (1) {
    }
}
*/
/*
int main()
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    struct superblock sb;
    fs_port_init();
    if (fs_port_mount(&sb) != 0) {
        printf("FS mount failed\n");
        return 0;
    }

    struct inode *ino;
    if (fs_open("/prog.ccbpf", O_RDONLY, &ino) != 0) {
        printf("fs_open /prog.ccbpf failed\n");
        return 0;
    }

    uint32_t fsize = fs_get_size(ino);
    printf("File size = %u bytes\n", (unsigned)fsize);

    uint8_t *image = heap_malloc(fsize);
    if (!image) {
        printf("heap_malloc(%u) failed\n", (unsigned)fsize);
        fs_close(ino);
        return 0;
    }

    int r = fs_read(ino, 0, image, fsize);
    fs_close(ino);
    if (r != (int)fsize) {
        printf("fs_read size mismatch: %d vs %u\n", r, (unsigned)fsize);
        heap_free(image);
        return 0;
    }

    printf("=== FILE CONTENT (HEX) ===\n");

    for (uint32_t i = 0; i < fsize; i++) {
        printf("%02X ", image[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    if (fsize % 16 != 0)
        printf("\n");

    printf("=== END OF FILE ===\n");

    heap_free(image);

    while (1) {}
}
*/
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
