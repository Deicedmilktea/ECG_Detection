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
#define ECG_Y_START 210
#define ECG_WIDTH (LCD_WIDTH - ECG_X_START)
#define ECG_HEIGHT 120
#define FFT_X_START 50
#define FFT_Y_START 390
#define FFT_WIDTH (LCD_WIDTH - FFT_X_START)
#define FFT_HEIGHT 120

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

static uint16_t current_index = ECG_X_START;
static uint16_t last_ecg_y = ECG_Y_START - ECG_HEIGHT / 2;
static uint8_t read_count = 0; // 十次循环执行一次FFT

extern uint8_t ads1292_flag;
extern uint8_t device_ID;
extern uint8_t key_mode;

static void ecg_data_process(void);
void update_ecg_buffer(int16_t new_ecg_data);
void FFT_Process(void);
void CalculateSignalFrequency(void);
void Draw_ECG(void);
void Draw_FFT(void);
void Draw_ECG_UI(void);
void Draw_FFT_UI(void);

void ECGTask(void *argument)
{
    LCD_Clear(GBLUE);
    arm_rfft_fast_init_f32(&fft_instance, FFT_LENGTH);
    Draw_ECG_UI();
    Draw_FFT_UI();

    for (;;)
    {
        if (ads1292_flag == 1)
        {
            ADS1292R_ReadData(ads1292_raw_data);
            ads1292_flag = 0;
            ecg_data_process();

            // FIR 滤波
            FIR_filtered_data = FIR_filter(ads1292_ecg_data[0]);
            printf("{FIR_filtered_data}");
            printf("%d\n", FIR_filtered_data);
            update_ecg_buffer(FIR_filtered_data);
            Draw_ECG();
            // 十次循环读取一次
            if (read_count > 9)
            {
                read_count = 0;
                FFT_Process();
                Draw_FFT();
                CalculateSignalFrequency();
            }
            read_count++;

            // // 通过 UART 发送字符串
            // char buffer[5] = "abcde";
            // HAL_UART_Transmit(&huart1, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
        }
        osDelay(1);
        LCD_Fill(FFT_X_START + 1, FFT_Y_START - FFT_HEIGHT, FFT_X_START + FFT_WIDTH, FFT_Y_START - 1, GBLUE);
    }
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

    if (ads1292_ecg_data[0] > 50)
    {
        ads1292_ecg_data[0] = -10;
    }
    else if (ads1292_ecg_data[0] < -50)
    {
        ads1292_ecg_data[0] = -10;
    }

    printf("{ecg_channel_1}");
    printf("%d\n", ads1292_ecg_data[0]);
    // printf("{ecg_channel_2}");
    // printf("%d\n", ads1292_ecg_data[1]);
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
void CalculateSignalFrequency(void)
{
    float maxValue = 0.0f;
    int maxIndex = 0;

    for (int i = 25; i < FFT_LENGTH / 2; i++)
    {
        if (FFT_OutputBuf[i] > maxValue)
        {
            maxValue = FFT_OutputBuf[i];
            maxIndex = i;
        }
    }

    float frequency = (1000 * maxIndex) / (float)FFT_LENGTH;

    // 在 ecg 数据中找到最大值和最小值
    uint16_t ecgMax = 0;
    uint16_t ecgMin = 50;

    for (int i = 0; i < FFT_LENGTH; i++)
    {
        if (ecg_buffer[i] > ecg_max)
        {
            ecgMax = ecg_buffer[i];
        }
        if (ecg_buffer[i] < ecg_min)
        {
            ecgMin = ecg_buffer[i];
        }
    }

    // 计算峰峰值
    uint16_t ecgPeakToPeak = ecgMax - ecgMin;

    // // 将 ADC 值转换为电压值，假设 ADC 参考电压为 3.3V
    // float voltage = (float)ecgPeakToPeak * 2.42f / 32767.0f;

    LCD_ShowString(90, 25, 200, 24, 24, (uint8_t *)"Frequency:");
    LCD_ShowNum(210, 25, (uint32_t)frequency, 4, 24);

    LCD_ShowString(90, 55, 200, 24, 24, (uint8_t *)"PeakToPeak:");
    LCD_ShowNum(225, 55, (uint32_t)ecgPeakToPeak, 4, 24);
}

void Draw_ECG_UI()
{
    LCD_DrawLine(ECG_X_START, ECG_Y_START, ECG_X_START, ECG_Y_START - ECG_HEIGHT); // 竖线
    LCD_DrawLine(ECG_X_START, ECG_Y_START, ECG_X_START + ECG_WIDTH, ECG_Y_START);  // 横线

    LCD_ShowNum(30, ECG_Y_START, 0, 1, 16);
    LCD_ShowNum(10, ECG_Y_START - ECG_HEIGHT / 2, 2048, 4, 16);
    LCD_ShowNum(10, ECG_Y_START - ECG_HEIGHT, 4095, 4, 16);

    LCD_ShowString(ECG_X_START + 80, ECG_Y_START + 10, 200, 24, 24, (uint8_t *)"ECG Line");
}

void Draw_FFT_UI()
{
    LCD_DrawLine(FFT_X_START, FFT_Y_START - FFT_HEIGHT, FFT_X_START, FFT_Y_START); // 竖线
    LCD_DrawLine(FFT_X_START, FFT_Y_START, FFT_X_START + FFT_WIDTH, FFT_Y_START);  // 横线

    LCD_ShowNum(35, FFT_Y_START, 0, 1, 16);
    LCD_ShowNum(35, FFT_Y_START - FFT_HEIGHT, 1, 1, 16);

    LCD_ShowString(FFT_X_START + 80, FFT_Y_START + 10, 200, 24, 24, (uint8_t *)"FFT Line");
    LCD_ShowString(FFT_X_START + 30, FFT_Y_START + 50, 300, 16, 16, (uint8_t *)"CopyRight@Deicedmilktea"); // 嘿嘿 调皮一下hhhhh
}

/**
 * @brief 绘制ECG波形
 */
void Draw_ECG()
{
    // 计算当前点的y坐标，基于FIR_filtered_data（后期换）
    uint16_t current_y = ECG_Y_START - ECG_HEIGHT / 2 - FIR_filtered_data; // 需要根据实际情况处理 FIR_filtered_data 映射

    // 限制y坐标范围在ECG_Y_START-ECG_HEIGHT到ECG_Y_START之间
    if (current_y < ECG_Y_START - ECG_HEIGHT)
    {
        current_y = ECG_Y_START - ECG_HEIGHT;
    }
    else if (current_y > ECG_Y_START)
    {
        current_y = ECG_Y_START;
    }

    // 计算当前点的x坐标
    uint16_t current_x = current_index;

    // 绘制当前点与上一个点之间的线
    if (current_index > 0)
    {
        LCD_DrawLine(current_x - 1, last_ecg_y, current_x, current_y);
    }

    // 更新上一点的y坐标
    last_ecg_y = current_y;

    // 累计索引，若达到最大值，则从头开始
    current_index++;
    if (current_index > LCD_WIDTH)
    {
        current_index = ECG_X_START;
        LCD_Fill(ECG_X_START + 1, ECG_Y_START - ECG_HEIGHT, ECG_X_START + ECG_WIDTH, ECG_Y_START - 1, GBLUE);
    }
}

void Draw_FFT()
{
    float x, y;
    float x_scale = (float)FFT_WIDTH / (FFT_LENGTH / 2); // X轴缩放比例
    float y_scale;
    float max_value = 0.0f;

    // 寻找幅值最大值，用于Y轴缩放
    for (int i = 0; i < FFT_LENGTH / 2; i++)
    {
        if (FFT_OutputBuf[i] > max_value)
        {
            max_value = FFT_OutputBuf[i];
        }
    }

    // 防止除以零
    if (max_value == 0)
    {
        max_value = 1.0f;
    }

    y_scale = (float)FFT_HEIGHT / max_value;

    // 绘制频谱
    for (int i = 0; i < FFT_LENGTH / 2; i++)
    {
        x = FFT_X_START + i * x_scale;
        y = FFT_Y_START - FFT_OutputBuf[i] * y_scale;

        // 将浮点坐标转换为整数坐标进行绘制
        LCD_DrawLine((uint16_t)x, FFT_Y_START, (uint16_t)x, (uint16_t)y);
    }
}