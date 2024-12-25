#include "ads1292r.h"
#include "main.h"
#include <stdio.h>
#include "spi.h"
#include "lcd.h"
#include "usart.h"

uint8_t device_id = 0;                     // 芯片ID
uint8_t ads1292r_data_buff[9] = {0};       // 数据缓存区
int16_t ECGRawData[2] = {0};               // 16位CH1和CH2通道数据
int32_t ADS1292R_ECG_BUFFER[2] = {0};      // 32位CH1和CH2通道数据
volatile uint8_t ads1292r_recive_flag = 0; // ADS1292R接收完成标志
uint8_t ads1292r_id_flag = 0;              // ADS1292R ID读取完成标志
uint8_t ads_id[15];                        // ADS1292R ID

float ecg_vol;

void ADS1292R_Init(void)
{
    HAL_NVIC_DisableIRQ(EXTI4_IRQn);

    ADS1292R_CS_H;
    ADS1292R_START_H;
    ADS1292R_PWDN_L;

    ADS1292R_PowerOnInit();
}

uint8_t ADS1292R_SPI_RW(uint8_t data)
{
    uint8_t TxData = data;
    uint8_t RxData = 0;
    HAL_SPI_TransmitReceive(&hspi3, &TxData, &RxData, 1, 10); //	HAL_SPI_TransmitReceive(&SPI3_Handler, &data, &rx_data, 1, 10);
    return RxData;
}

/**ADS1292R上电复位 **/
void ADS1292R_PowerOnInit(void)
{
    ADS1292R_START_L;
    ADS1292R_CS_H;
    ADS1292R_PWDN_L; // 进入掉电模式
    ADS1292R_PWDN_H;
    HAL_Delay(1000);               // 等待稳定
    ADS1292R_CMD(ADS1292R_SDATAC); // 发送停止连续读取数据命令
    HAL_Delay(1000);
    ADS1292R_CMD(ADS1292R_RESET); // 发送复位命令
    HAL_Delay(1000);
    ADS1292R_CMD(ADS1292R_SDATAC); // 发送停止连续读取数据命令
    HAL_Delay(1000);
    //    ADS1292R_START_H;
    //		ADS1292R_CS_H;
    //		ADS1292R_PWDN_L;//进入掉电模式
    //		delay_ms(100);
    //		ADS1292R_PWDN_H;//退出掉电模式
    //		delay_ms(100);//等待稳定
    //		ADS1292R_PWDN_L;//发出复位脉冲
    //		DWT_Delay_us(100);
    //		ADS1292R_PWDN_H;
    //		delay_ms(1000);//等待稳定，可以开始使用ADS1292R
    //		ADS1292R_START_L;
    //	  ADS1292R_CMD(ADS1292R_ADSRESET);//发送复位命令
    //		ADS1292R_CMD(ADS1292R_SDATAC);//发送停止连续读取数据命令
    //		delay_ms(1);
    while (device_id != 0x73 && device_id != 0x53) // 识别芯片型号，1292为0x53，也就是这里的十进制数83  1292r为0x73，即115
    {
        device_id = ADS1292R_REG(ADS1292R_RREG | ADS1292R_ID, 0x00);

        HAL_Delay(1000);
    }
    ads1292r_id_flag = 1;
    // printf("正确ID:%d\n",device_id);

    ADS1292R_REG(ADS1292R_WREG | ADS1292R_CONFIG2, 0xa0); // 使用内部参考电压
    // ADS1292R_REG(ADS1292R_WREG|ADS1292R_CONFIG2,    0xa3);//使用测试信号
    HAL_Delay(10);                                        // 等待内部参考电压稳定
    ADS1292R_REG(ADS1292R_WREG | ADS1292R_CONFIG1, 0x02); // 设置转换速率为500SPS
    ADS1292R_REG(ADS1292R_WREG | ADS1292R_CH1SET, 0x00);
    // ADS1292R_REG(ADS1292R_WREG|ADS1292R_CH1SET,     0x05);//采集测试信号（方波）
    ADS1292R_REG(ADS1292R_WREG | ADS1292R_CH2SET, 0x00);
    ADS1292R_REG(ADS1292R_WREG | ADS1292R_RLD_SENS, 0x2c); // green 0x2c
    ADS1292R_REG(ADS1292R_WREG | ADS1292R_RESP1, 0x02);    // 0xea
    ADS1292R_REG(ADS1292R_WREG | ADS1292R_RESP2, 0x03);    // 0x03

    //	#if 0
    //	ADS1292R_REG(ADS1292R_WREG|ADS1292R_CONFIG2,    0XE0);	//使用内部参考电压：2.42V或4.033V。bit4：0->2.42V，1->4.033V
    //	HAL_Delay(10);//等待内部参考电压稳定
    //
    //	ADS1292R_REG(ADS1292R_WREG|ADS1292R_CONFIG1,    0X02);	//设置转换速率为500SPS
    //	//ADS1292R_REG(ADS1292R_WREG|ADS1292R_CONFIG1,    0X03);	//设置转换速率为1kSPS
    //	ADS1292R_REG(ADS1292R_WREG|ADS1292R_LOFF,       0XF0);
    //	ADS1292R_REG(ADS1292R_WREG|ADS1292R_CH1SET,     0X00);
    //	ADS1292R_REG(ADS1292R_WREG|ADS1292R_CH2SET,     0x00);
    //	ADS1292R_REG(ADS1292R_WREG|ADS1292R_RLD_SENS,   0x30);
    //	//	ADS1292R_REG(ADS1292R_WREG|ADS1292R_RLD_SENS,   0x3C);	//使用通道2提取共模电压
    //	ADS1292R_REG(ADS1292R_WREG|ADS1292R_LOFF_SENS,  0x3F);
    //	//  ADS1292R_REG(ADS1292R_WREG|LOFF_STAT,  0X00);
    //	ADS1292R_REG(ADS1292R_WREG|ADS1292R_RESP1,      0xDE);
    //	ADS1292R_REG(ADS1292R_WREG|ADS1292R_RESP2,      0x07);
    //	ADS1292R_REG(ADS1292R_WREG|ADS1292R_GPIO,       0x0C);
    //  #endif
}

//** 配置外部中断
void ADS1292R_Work(void)
{
    //	lcd_show_string(70, 250, 200, 50, 24, "ADS1292R Working", RED);
    ADS1292R_CMD(ADS1292R_RDATAC); // 回到连续读取数据模式，检测噪声数据
    ADS1292R_START_H;              // 启动转换
    // 中断线1-PB1
    HAL_NVIC_EnableIRQ(EXTI0_IRQn); // 使能中断线
}

void ADS1292R_Halt(void)
{
    // OLED_PrintfString(0,2,"ADS1292R Halt");
    ADS1292R_START_L;                // 启动转换
    ADS1292R_CMD(ADS1292R_SDATAC);   // 发送停止连续读取数据命令
    HAL_NVIC_DisableIRQ(EXTI0_IRQn); // 关闭中断线1
}

// 对ADS1292R写入指令
void ADS1292R_CMD(uint8_t cmd)
{
    ADS1292R_CS_L;
    DWT_Delay_us(100);
    ADS1292R_SPI_RW(cmd);
    DWT_Delay_us(100);
    ADS1292R_CS_H;
}

// 对ADS1292R内部寄存器进行操作
uint8_t ADS1292R_REG(uint8_t cmd, uint8_t data) // 只读一个数据
{
    ADS1292R_CS_L;
    DWT_Delay_us(100);
    ADS1292R_SPI_RW(cmd);             // 读写指令
    ADS1292R_SPI_RW(0X00);            // 需要写几个数据（n+1个）
    if ((cmd & 0x20) == 0x20)         // 判断是否为读寄存器指令
        return ADS1292R_SPI_RW(0X00); // 返回寄存器值
    else
        ADS1292R_SPI_RW(data); // 写入数值
    DWT_Delay_us(100);
    ADS1292R_CS_H;

    return 0;
}

// static uint8_t temp1292r[9] = {0};	//暂时无用
// 读取72位的数据1100+LOFF_STAT[4:0]+GPIO[1:0]+13个0+2CHx24位，共9字节
//	1100	LOFF_STAT[4			3			2			1			0	]	//导联脱落相关的信息在LOFF_STAT寄存器里
//									RLD		1N2N	1N2P	1N1N	1N1P
//	例	C0 00 00 FF E1 1A FF E1 52
void ADS1292R_ReadData(uint8_t *data)
{
    int i, k;
    ADS1292R_CS_L;
    for (k = 0; k < 10; k++)
    {
        ;
    }
    for (i = 0; i < 9; i++)
    {
        *data++ = ADS1292R_SPI_RW(0Xff);
    }
    for (k = 0; k < 10; k++)
    {
        ;
    }
    ADS1292R_CS_H;
}

// // float max_vol, min_vol = 2.42f, delt_vol; // 177 -> 0.00436v
// // 单次采集的CH1和CH2数据拼接并裁剪为16位数据
// void data_trans(void)
// {
//     for (uint8_t n = 0; n < 2; n++) // 单次采集的CH1和CH2数据存入ADS1292R_ECG_BUFFER
//     {
//         ADS1292R_ECG_BUFFER[n] = ads1292r_data_buff[3 + 3 * n];
//         ADS1292R_ECG_BUFFER[n] = ADS1292R_ECG_BUFFER[n] << 8;
//         ADS1292R_ECG_BUFFER[n] |= ads1292r_data_buff[3 + 3 * n + 1];
//         ADS1292R_ECG_BUFFER[n] = ADS1292R_ECG_BUFFER[n] << 8;
//         ADS1292R_ECG_BUFFER[n] |= ads1292r_data_buff[3 + 3 * n + 2];
//     }

//     ADS1292R_ECG_BUFFER[0] = ADS1292R_ECG_BUFFER[0] >> 8; // 舍去低8位
//     ADS1292R_ECG_BUFFER[1] = ADS1292R_ECG_BUFFER[1] >> 8;

//     ADS1292R_ECG_BUFFER[0] &= 0xffff; // 消除补码算数移位对数据影响
//     ADS1292R_ECG_BUFFER[1] &= 0xffff;

//     ECGRawData[0] = (int16_t)ADS1292R_ECG_BUFFER[0];
//     ECGRawData[1] = (int16_t)ADS1292R_ECG_BUFFER[1];

//     ecg_vol = ECGRawData[0] * 2.42f / 32767.0f;
// }

// int32_t get_volt(uint32_t num)
//{
//			int32_t temp;
//			temp = num;
//			temp <<= 8;
//			temp >>= 8;
//
//			return temp;
// }
