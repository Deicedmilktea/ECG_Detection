#include "cmsis_os.h"
#include "lcd.h"
#include "dac.h"
#include "adc.h"
#include "tim.h"
#include "math.h"
#include "stdio.h"
#include "arm_math.h"

/* 定义 DAC 输出的数据缓冲区 */
#define SAMPLING_RATE 100000
#define DAC_BUFFER_SIZE 10240
#define ADC_BUFFER_SIZE 10240
#define LCD_WIDTH 320
#define LCD_HEIGHT 480
#define ADC_MAX_VALUE 4095
#define PI 3.1415926f
uint16_t dac_buffer[DAC_BUFFER_SIZE];
uint16_t adc_buffer[ADC_BUFFER_SIZE];

#define ADC_X_START 50
#define ADC_Y_START 210
#define ADC_WIDTH (LCD_WIDTH - ADC_X_START)
#define ADC_HEIGHT 120
#define FFT_X_START 50
#define FFT_Y_START 390
#define FFT_WIDTH (LCD_WIDTH - FFT_X_START)
#define FFT_HEIGHT 120
#define FFT_LENGTH 1024
#define FREQRESOLUTION ((float)(SAMPLING_RATE / 10) / (FFT_LENGTH)) // 十采一

arm_rfft_fast_instance_f32 fft_instance;
float FFT_InputBuf[FFT_LENGTH];  // FFT 输入缓冲区
float FFT_OutputBuf[FFT_LENGTH]; // FFT 输出缓冲区

extern uint8_t key_mode;

void Choose_wave_generate();
void GenerateSineWave();
void GenerateSquareWave();
void GenerateTriangleWave();
void Draw_ADC_UI();
void Draw_ADC_Line();
void Draw_FFT_UI();
void FFT_Process();
void DrawFFT();
void CalculateSignalFrequency();

void LCDTask(void *argument)
{
    LCD_Clear(GBLUE);

    /* 启动 DAC 的 DMA 传输 */
    HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)dac_buffer, DAC_BUFFER_SIZE, DAC_ALIGN_12B_R);

    /* 启动 TIM6 */
    HAL_TIM_Base_Start(&htim6);
    HAL_TIM_Base_Start(&htim2);

    // 启动 ADC 的 DMA 传输
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, ADC_BUFFER_SIZE);

    arm_rfft_fast_init_f32(&fft_instance, FFT_LENGTH);

    Draw_ADC_UI();
    Draw_FFT_UI();

    for (;;)
    {
        Choose_wave_generate();

        Draw_ADC_Line();

        FFT_Process();

        DrawFFT();

        CalculateSignalFrequency();

        osDelay(500);
        // LCD_Clear(GBLUE);
        LCD_Fill(ADC_X_START + 1, ADC_Y_START - ADC_HEIGHT, ADC_X_START + ADC_WIDTH, ADC_Y_START - 1, GBLUE);
        LCD_Fill(FFT_X_START + 1, FFT_Y_START - FFT_HEIGHT, FFT_X_START + FFT_WIDTH, FFT_Y_START - 1, GBLUE);
    }
}

void Draw_ADC_UI()
{
    LCD_DrawLine(ADC_X_START, ADC_Y_START, ADC_X_START, ADC_Y_START - ADC_HEIGHT); // 竖线
    LCD_DrawLine(ADC_X_START, ADC_Y_START, ADC_X_START + ADC_WIDTH, ADC_Y_START);  // 横线

    LCD_ShowNum(30, ADC_Y_START, 0, 1, 16);
    LCD_ShowNum(10, ADC_Y_START - ADC_HEIGHT / 2, 2048, 4, 16);
    LCD_ShowNum(10, ADC_Y_START - ADC_HEIGHT, 4095, 4, 16);

    LCD_ShowString(ADC_X_START + 80, ADC_Y_START + 10, 200, 24, 24, (uint8_t *)"ADC Line");
}

void Draw_FFT_UI()
{
    LCD_DrawLine(FFT_X_START, FFT_Y_START - FFT_HEIGHT, FFT_X_START, FFT_Y_START); // 竖线
    LCD_DrawLine(FFT_X_START, FFT_Y_START, FFT_X_START + FFT_WIDTH, FFT_Y_START);  // 横线

    LCD_ShowNum(35, FFT_Y_START, 0, 1, 16);
    // LCD_ShowNum(20, FFT_Y_START - FFT_HEIGHT / 2, 250, 3, 16);
    LCD_ShowNum(35, FFT_Y_START - FFT_HEIGHT, 1, 1, 16);

    LCD_ShowString(FFT_X_START + 80, FFT_Y_START + 10, 200, 24, 24, (uint8_t *)"FFT Line");
    LCD_ShowString(FFT_X_START + 30, FFT_Y_START + 50, 300, 16, 16, (uint8_t *)"CopyRight@Deicedmilktea"); // 嘿嘿 调皮一下hhhhh
}

/**
 * @brief 选择波形生成
 */
void Choose_wave_generate()
{
    if (key_mode == 0)
        GenerateSineWave();
    else if (key_mode == 1)
        GenerateSquareWave();
    else if (key_mode == 2)
        GenerateTriangleWave();
}

/**
 * @brief 生成正弦波形数据
 */
void GenerateSineWave(void)
{
    for (int i = 0; i < DAC_BUFFER_SIZE; i++)
    {
        // 12位DAC，范围0-4095
        dac_buffer[i] = 2048 + sin(2 * PI * 66 * i / DAC_BUFFER_SIZE) * 2047;
    }
}

/**
 * @brief 生成50Hz方波数据
 */
void GenerateSquareWave(void)
{
    int period_samples = DAC_BUFFER_SIZE / 66; // 每个周期的样本数
    for (int i = 0; i < DAC_BUFFER_SIZE; i++)
    {
        if ((i % period_samples) < (period_samples / 2))
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
 * @brief 生成50Hz三角波数据
 */
void GenerateTriangleWave(void)
{
    int period_samples = DAC_BUFFER_SIZE / 66; // 每个周期的样本数
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
    float x_scale = (float)(LCD_WIDTH - ADC_X_START) / (ADC_BUFFER_SIZE / 10);
    float y_scale = (float)(ADC_HEIGHT) / ADC_MAX_VALUE;

    // 初始点坐标
    x1 = ADC_X_START;
    y1 = ADC_Y_START;

    for (uint16_t i = 1; i < ADC_BUFFER_SIZE / 10; i++)
    {
        x2 = ADC_X_START + (uint16_t)(i * x_scale);
        y2 = ADC_Y_START - (uint16_t)(adc_buffer[i] * y_scale);

        // 绘制线段
        LCD_DrawLine(x1, y1, x2, y2);

        x1 = x2;
        y1 = y2;
    }
}

/**
 * @brief FFT 处理
 */
void FFT_Process()
{
    // 每隔10个点抽样adc的数据，然后进行fft
    for (int i = 0; i < FFT_LENGTH; i++)
    {
        FFT_InputBuf[i] = (float)adc_buffer[i * 10] / ADC_MAX_VALUE;
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

    // 计算对应 30Hz 的索引
    uint16_t index_30Hz = (uint16_t)(30.0f / FREQRESOLUTION);

    // 确保索引不超过 FFT 长度
    if (index_30Hz >= FFT_LENGTH / 2)
    {
        index_30Hz = FFT_LENGTH / 2 - 1;
    }

    // 滤除 30Hz 以下的频率分量
    for (uint16_t i = 0; i <= index_30Hz; i++)
    {
        FFT_OutputBuf[i] = 0.0f;
    }
}

/* Draw FFT spectrum */
void DrawFFT(void)
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

    // 清除绘图区
    // LCD_ClearArea(FFT_X_START, FFT_Y_START, FFT_WIDTH, FFT_HEIGHT);

    // 绘制频谱
    for (int i = 0; i < FFT_LENGTH / 2; i++)
    {
        x = FFT_X_START + i * x_scale;
        y = FFT_Y_START - FFT_OutputBuf[i] * y_scale;

        // 将浮点坐标转换为整数坐标进行绘制
        LCD_DrawLine((uint16_t)x, FFT_Y_START, (uint16_t)x, (uint16_t)y);
    }
}

/**
 * @brief 计算ADC信号频率
 */
void CalculateSignalFrequency()
{
    int peakIndex = 0;
    float maxValue = 0.0f;

    // 寻找幅值谱中的最大值（忽略直流分量）
    for (int i = 1; i < FFT_LENGTH / 2; i++)
    {
        if (FFT_OutputBuf[i] > maxValue)
        {
            maxValue = FFT_OutputBuf[i];
            peakIndex = i;
        }
    }

    // 计算信号频率
    float signalFrequency = peakIndex * FREQRESOLUTION;

    // 在 ADC 数据中找到最大值和最小值
    uint16_t adcMax = 0;
    uint16_t adcMin = ADC_MAX_VALUE;

    for (int i = 0; i < ADC_BUFFER_SIZE; i++)
    {
        if (adc_buffer[i] > adcMax)
        {
            adcMax = adc_buffer[i];
        }
        if (adc_buffer[i] < adcMin)
        {
            adcMin = adc_buffer[i];
        }
    }

    // 计算峰峰值
    uint16_t adcPeakToPeak = adcMax - adcMin;

    // 将 ADC 值转换为电压值，假设 ADC 参考电压为 3.3V
    float voltage = ((float)adcPeakToPeak / ADC_MAX_VALUE) * 3.3f;

    LCD_ShowString(90, 25, 200, 24, 24, (uint8_t *)"Frequency:");
    LCD_ShowNum(210, 25, (uint32_t)signalFrequency, 4, 24);

    LCD_ShowString(90, 55, 200, 24, 24, (uint8_t *)"Voltage:");
    LCD_ShowFloat(185, 55, voltage, 2, 24); // 显示小数点后2位
}