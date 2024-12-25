#include "cmsis_os.h"
#include "ads1292r.h"
#include "usart.h"
#include "stdio.h"
#include "string.h"
#include "FIR.h"
#include "arm_math.h"
#include "lcd.h"

#define FFT_LENGTH 1024 // 设定FFT的点数，根据你的需求调整
#define LCD_WIDTH 320
#define LCD_HEIGHT 480
#define ECG_X_START 50
#define ECG_X_END 300
#define ECG_Y_START 210
#define ECG_Y_END 390

static int16_t ecg_buffer[FFT_LENGTH];          // 环形缓冲区，用于存储ECG数据
static int16_t ecg_max = 0;                     // 最大值
static int16_t ecg_min = 0;                     // 最小值
static uint16_t ecg_diff = 0;                   // 最大最小值之差
static uint16_t buffer_index = 0;               // 缓冲区索引，用于跟踪最新数据位置
static float FFT_InputBuf[FFT_LENGTH];          // 用于存储输入信号（复数形式）
static float FFT_OutputBuf[FFT_LENGTH];         // 用于存储FFT输出（复数形式）
static arm_rfft_fast_instance_f32 fft_instance; // FFT实例

static uint8_t ads1292_raw_data[9];
static int16_t ads1292_ecg_data[2];
static int32_t ads1292_ecg_data_temp[2];
static int16_t FIR_filtered_data = 0;

static float ecg_frequency = 0.0f;
static float ecg_vol = 0.0f;

extern uint8_t ads1292_flag;
extern uint8_t device_ID;

static void ecg_data_process(void);
static int32_t get_volt(uint32_t num);
void update_ecg_buffer(int16_t new_ecg_data);
void FFT_Process(void);
float CalculateSignalFrequency(void);
void Draw_ECG(void);

void ECGTask(void *argument)
{
    LCD_Clear(GBLUE);
    arm_rfft_fast_init_f32(&fft_instance, FFT_LENGTH);

    for (;;)
    {
        if (ads1292_flag == 1)
        {
            ADS1292R_ReadData(ads1292_raw_data);
            ads1292_flag = 0;
            ecg_data_process();

            // FIR 滤波
            FIR_filtered_data = FIR_filter(ads1292_ecg_data[0]);
            update_ecg_buffer(FIR_filtered_data);
            Draw_ECG();
            FFT_Process();
            ecg_frequency = CalculateSignalFrequency();

            // printf("%d\n", ads1292_ecg_data[0]);

            // // 通过 UART 发送字符串
            // char buffer[5] = "abcde";
            // HAL_UART_Transmit(&huart1, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
        }
        osDelay(1);
        LCD_Clear(GBLUE);
    }
}

int32_t get_volt(uint32_t num)
{
    uint32_t temp;
    temp = num;
    temp <<= 8;
    temp >>= 8;
    return temp;
}

/**
 * @brief ECG 数据处理，单次采集的CH1和CH2数据拼接并裁剪为16位数据
 */
static void ecg_data_process(void)
{
    for (uint8_t n = 0; n < 2; n++) // 单次采集的CH1和CH2数据存入ADS1292R_ECG_BUFFER
    {
        ads1292_ecg_data_temp[n] = ads1292_raw_data[3 + 3 * n];
        ads1292_ecg_data_temp[n] = ads1292_ecg_data_temp[n] << 8;
        ads1292_ecg_data_temp[n] |= ads1292_raw_data[3 + 3 * n + 1];
        ads1292_ecg_data_temp[n] = ads1292_ecg_data_temp[n] << 8;
        ads1292_ecg_data_temp[n] |= ads1292_raw_data[3 + 3 * n + 2];
    }

    ads1292_ecg_data_temp[0] = ads1292_ecg_data_temp[0] >> 8; // 舍去低8位
    ads1292_ecg_data_temp[1] = ads1292_ecg_data_temp[1] >> 8;

    ads1292_ecg_data_temp[0] &= 0xffff; // 消除补码算数移位对数据影响
    ads1292_ecg_data_temp[1] &= 0xffff;

    ads1292_ecg_data[0] = (int16_t)ads1292_ecg_data_temp[0];
    ads1292_ecg_data[1] = (int16_t)ads1292_ecg_data_temp[1];

    ecg_vol = ads1292_ecg_data[0] * 2.42f / 32767.0f;

    printf("{ecg}");
    printf("%d\n", ads1292_ecg_data[0]);
    // HAL_UART_Transmit(&huart1, (uint8_t *)ads1292_ecg_data, sizeof(ads1292_ecg_data), HAL_MAX_DELAY);
    // printf("{ecg_vol}");
    // printf("%f\n", ecg_vol);
}

/**
 * @brief 将ECG数据存入环形缓冲区
 */
void update_ecg_buffer(int16_t new_ecg_data)
{
    // 将新的ECG数据存入缓冲区
    ecg_buffer[buffer_index] = new_ecg_data;

    // 更新缓冲区索引（循环）
    buffer_index = (buffer_index + 1) % FFT_LENGTH;

    // 计算ecg_buffer的最大最小之差
    if (new_ecg_data > ecg_max)
    {
        ecg_max = new_ecg_data;
    }
    if (new_ecg_data < ecg_min)
    {
        ecg_min = new_ecg_data;
    }
    ecg_diff = ecg_max - ecg_min;
}

/**
 * @brief FFT变换
 */
void FFT_Process(void)
{
    // 读取ecg_buffer到FFT_InputBuf
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        FFT_InputBuf[i] = (float)ecg_buffer[(buffer_index + i) % FFT_LENGTH];
    }

    /* 执行 FFT 计算 */
    arm_rfft_fast_f32(&fft_instance, FFT_InputBuf, FFT_OutputBuf, 0);

    /* 计算幅值谱 */
    FFT_OutputBuf[0] = fabsf(FFT_OutputBuf[0]); // 直流分量，实数

    for (int i = 1; i < FFT_LENGTH / 2; i++)
    {
        float real = FFT_OutputBuf[2 * i];                   // 实部
        float imag = FFT_OutputBuf[2 * i + 1];               // 虚部
        FFT_OutputBuf[i] = sqrtf(real * real + imag * imag); // 计算幅值
    }
}

/**
 * @brief 计算频率
 */
float CalculateSignalFrequency(void)
{
    float maxValue = 0.0f;
    int maxIndex = 0;

    for (int i = 1; i < FFT_LENGTH / 2; i++)
    {
        if (FFT_OutputBuf[i] > maxValue)
        {
            maxValue = FFT_OutputBuf[i];
            maxIndex = i;
        }
    }

    float frequency = (2500 * maxIndex) / (float)FFT_LENGTH;
    return frequency;
}

/**
 * @brief 绘制ECG波形
 */
void Draw_ECG()
{
    uint16_t x1, y1, x2, y2;

    // 计算缩放比例
    float x_scale = (float)(ECG_X_END - ECG_X_START) / FFT_LENGTH;
    float y_scale = (float)(ECG_Y_END - ECG_Y_START) / 10000;

    // 初始点坐标
    x1 = ECG_X_START;
    y1 = (ECG_Y_END + ECG_Y_START) / 2;

    for (uint16_t i = 1; i < FFT_LENGTH; i++)
    {
        x2 = ECG_X_START + (uint16_t)(i * x_scale);
        y2 = (ECG_Y_END + ECG_Y_START) / 2 - (int16_t)(ecg_buffer[i] * y_scale); // 从中间开始画

        // 绘制线段
        LCD_DrawLine(x1, y1, x2, y2);

        x1 = x2;
        y1 = y2;
    }
}