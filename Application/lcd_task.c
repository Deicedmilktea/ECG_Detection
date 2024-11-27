#include "cmsis_os.h"
#include "lcd.h"
#include "dac.h"
#include "adc.h"
#include "tim.h"
#include "math.h"
#include "stdio.h"
#include "arm_math.h"

/* 定义 DAC 输出的数据缓冲区 */
#define DAC_BUFFER_SIZE 1024
#define ADC_BUFFER_SIZE 1024
#define LCD_WIDTH 320
#define LCD_HEIGHT 480
#define ADC_MAX_VALUE 4095
#define PI 3.1415926f
uint16_t dac_buffer[DAC_BUFFER_SIZE];
uint16_t adc_buffer[ADC_BUFFER_SIZE];

#define FFT_X_START 0
#define FFT_Y_START 300
#define FFT_WIDTH 300
#define FFT_HEIGHT 120
#define FFT_LENGTH 1024

arm_cfft_radix4_instance_f32 scfft;
float FFT_InputBuf[FFT_LENGTH * 2]; // FFT 输入缓冲区
float FFT_OutputBuf[FFT_LENGTH];    // FFT 输出缓冲区

extern uint8_t key_mode;

void Choose_wave_generate();
void GenerateSineWave(void);
void GenerateSquareWave(void);
void GenerateTriangleWave(void);
void Draw_ADC_Line(void);
void FFT_Process();
void DrawFFT(void);

void LCDTask(void *argument)
{
    LCD_Clear(GBLUE);

    /* 启动 DAC 的 DMA 传输 */
    HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)dac_buffer, DAC_BUFFER_SIZE, DAC_ALIGN_12B_R);

    /* 启动 TIM6 */
    HAL_TIM_Base_Start(&htim6);

    // 启动 ADC 的 DMA 传输
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, ADC_BUFFER_SIZE);

    arm_cfft_radix4_init_f32(&scfft, FFT_LENGTH, 0, 1);

    for (;;)
    {
        Choose_wave_generate();

        Draw_ADC_Line();

        FFT_Process();

        DrawFFT();

        osDelay(500);
        LCD_Clear(GBLUE);
    }
}

void Choose_wave_generate()
{
    if (key_mode == 0)
        GenerateSineWave();
    else if (key_mode == 1)
        GenerateSquareWave();
    else if (key_mode == 2)
        GenerateTriangleWave();
}

void FFT_Process()
{
    // Copy ADC data to FFT input buffer
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        FFT_InputBuf[2 * i] = (float)adc_buffer[i];
        FFT_InputBuf[2 * i + 1] = 0.0f; // Imaginary part
    }

    // Perform FFT
    arm_cfft_radix4_f32(&scfft, FFT_InputBuf);
    arm_cmplx_mag_f32(FFT_InputBuf, FFT_OutputBuf, FFT_LENGTH);

    // Normalize and limit FFT values for display
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        FFT_OutputBuf[i] *= 0.03f;
        if (FFT_OutputBuf[i] > 500.0f)
            FFT_OutputBuf[i] = 500.0f;
    }
}

/**
 * @brief 生成正弦波形数据
 */
void GenerateSineWave(void)
{
    for (int i = 0; i < DAC_BUFFER_SIZE; i++)
    {
        // 12位DAC，范围0-4095
        dac_buffer[i] = 2048 + sin(2 * PI * 40 * i / DAC_BUFFER_SIZE) * 2047;
    }
}

/**
 * @brief 生成方波数据
 */
void GenerateSquareWave(void)
{
    for (int i = 0; i < DAC_BUFFER_SIZE; i++)
    {
        if ((i % (DAC_BUFFER_SIZE / (2 * 40))) < (DAC_BUFFER_SIZE / (4 * 40)))
        {
            dac_buffer[i] = 4095; // 高电平
        }
        else
        {
            dac_buffer[i] = 0; // 低电平
        }
    }
}

/**
 * @brief 生成40Hz三角波数据
 */
void GenerateTriangleWave(void)
{
    int period_samples = DAC_BUFFER_SIZE / 40; // 每个周期的样本数
    for (int i = 0; i < DAC_BUFFER_SIZE; i++)
    {
        int sample_index = i % period_samples;
        float phase = (float)sample_index / period_samples;
        if (phase < 0.5f)
        {
            // 上升沿
            dac_buffer[i] = (uint16_t)(phase * 2 * 4095);
        }
        else
        {
            // 下降沿
            dac_buffer[i] = (uint16_t)((1.0f - (phase - 0.5f) * 2) * 4095);
        }
    }
}

/**
 * @brief 绘制 ADC 数据波形
 */
void Draw_ADC_Line()
{
    uint16_t x1, y1, x2, y2;

    // 计算缩放比例
    float x_scale = (float)(LCD_WIDTH - 20) / ADC_BUFFER_SIZE;
    float y_scale = (float)(LCD_HEIGHT / 3) / ADC_MAX_VALUE;

    // 初始点坐标
    x1 = 0;
    y1 = 0;

    for (uint16_t i = 1; i < ADC_BUFFER_SIZE; i++)
    {
        x2 = (uint16_t)(i * x_scale);
        y2 = (uint16_t)(adc_buffer[i] * y_scale);

        // 绘制线段
        LCD_DrawLine(x1, y1, x2, y2);

        x1 = x2;
        y1 = y2;
    }
}

/* Draw FFT spectrum */
void DrawFFT(void)
{
    int prev_x = FFT_X_START;
    int prev_y = FFT_Y_START + FFT_HEIGHT / 2;

    for (int i = 1; i < FFT_LENGTH / 2; i += 5)
    {
        int curr_x = FFT_X_START + (i * FFT_WIDTH) / (FFT_LENGTH / 2);
        int curr_y = FFT_Y_START + FFT_HEIGHT / 2 - (int)(FFT_OutputBuf[i] * FFT_HEIGHT / 500.0f);

        LCD_DrawLine(prev_x, prev_y, curr_x, curr_y);
        prev_x = curr_x;
        prev_y = curr_y;
    }
}