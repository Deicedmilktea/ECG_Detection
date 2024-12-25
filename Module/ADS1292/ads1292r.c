/**
 ******************************************************************************
 * @file    bsp_ads1292_hal.c
 * @author
 * @version V1.0
 * @date
 * @brief   ADS1292 ECG 模块驱动 - HAL 源文件
 ******************************************************************************
 */

#include "ads1292r.h"
#include "bsp_spi.h" // 确保你有一个基于 HAL 的 SPI 驱动
#include "bsp_dwt.h" // 如果你有自己的微秒级延时，否则可用 HAL_Delay()

//========================== 全局变量 =========================//

// 寄存器数组
uint8_t ADS1292_REG[12];

// 各种配置结构体，根据你的需求初始化
ADS1292_CONFIG1 Ads1292_Config1 = {DATA_RATE_250SPS};
ADS1292_CONFIG2 Ads1292_Config2 = {PDB_LOFF_COMP_ON, PDB_REFBUF_ON, VREF_242V, CLK_EN_OFF, INT_TEST_OFF};
ADS1292_CHSET Ads1292_Ch1set = {PD_ON, GAIN_6, MUX_Normal_input};
ADS1292_CHSET Ads1292_Ch2set = {PD_ON, GAIN_6, MUX_Normal_input};
ADS1292_RLD_SENS Ads1292_Rld_Sens = {PDB_RLD_ON, RLD_LOFF_SENSE_ON, RLD_CANNLE_ON, RLD_CANNLE_ON, RLD_CANNLE_OFF, RLD_CANNLE_OFF};
ADS1292_LOFF_SENS Ads1292_Loff_Sens = {FLIP2_OFF, FLIP1_OFF, LOFF_CANNLE_ON, LOFF_CANNLE_ON, LOFF_CANNLE_ON, LOFF_CANNLE_ON};
ADS1292_RESP1 Ads1292_Resp1 = {RESP_DEMOD_ON, RESP_MOD_ON, 0x0D, RESP_CTRL_CLOCK_INTERNAL};
ADS1292_RESP2 Ads1292_Resp2 = {CALIB_OFF, FREQ_32K, RLDREF_INT_INTERNALLY};

//========================== 函数实现 =========================//

/**
 * @brief  ADS1292 相关 GPIO、SPI 以及内部上电复位流程
 * @note   在此函数中初始化 SPI, 并做最初步的复位配置
 */
void ADS1292_Init(void)
{
    // 1) 你需要先初始化 SPI 硬件（根据项目情况而定）。
    // MX_SPI1_Init();

    // 2) 其余 GPIO（CS、RESET、START、CLKSEL、DRDY 中断等），
    //    可在 CubeMX 里生成或手动在某处统一初始化。
    //    这里假设已经完成了 GPIO 配置 (输入/输出、EXTI等)，
    //    只做最小化操作。

    // 3) 拉高 CS，避免干扰
    ADS_CS_HIGH();

    // 4) 上电复位
    ADS1292_PowerOnInit();

    while (Set_ADS1292_Collect(0))
    {
    }
}

/**
 * @brief  上电复位初始化：内部时钟、复位时序、写寄存器初始值等
 */
void ADS1292_PowerOnInit(void)
{
    uint8_t vref = 0xA0; // CONFIG2 的一个示例值(内部参考)

    // 启用内部时钟
    ADS_CLKSEL_HIGH();
    // 停止数据输出
    ADS_START_LOW();
    // 拉低复位
    ADS_RESET_LOW();
    // 等待 1s
    DWT_Delay_ms(1000);
    // 结束复位
    ADS_RESET_HIGH();
    DWT_Delay_ms(100);

    // 停止连续读取数据
    ADS1292_Send_CMD(SDATAC);
    DWT_Delay_ms(100);

    // 配置 CONFIG2 使能内部参考电压(示例)
    ADS1292_WR_REGS(WREG | CONFIG2, 1, &vref);

    // 启动转换（如果你想让它先处于启动状态）
    ADS1292_Send_CMD(START);

    // 如果需要可以进入连续读取
    ADS1292_Send_CMD(RDATAC);

    // 再次发送 SDATAC 停止连续输出，好让后续能读写寄存器
    ADS1292_Send_CMD(SDATAC);
    DWT_Delay_ms(100);
}

/**
 * @brief  SPI 读写一个字节，并返回从 ADS1292 读到的字节
 */
uint8_t ADS1292_SPI(uint8_t com)
{
    // 这里的 SPI1_ReadWriteByte() 由你自己的 bsp_spi.c 提供
    // 内部调用 HAL_SPI_TransmitReceive(&hspi1, ...) 等
    return SPI3_ReadWriteByte(com);
}

/**
 * @brief  发送一条命令字节
 */
void ADS1292_Send_CMD(uint8_t cmd)
{
    ADS_CS_LOW();
    // 多字节命令之间需要一定延时(>~8us)，这里预留
    DWT_Delay_us(100);
    ADS1292_SPI(cmd);
    DWT_Delay_us(100);
    ADS_CS_HIGH();
}

/**
 * @brief  读写多个寄存器
 * @param  reg: RREG/WREG + 起始寄存器地址
 * @param  len: 读或写的寄存器个数
 * @param  data: 数据指针
 */
void ADS1292_WR_REGS(uint8_t reg, uint8_t len, uint8_t *data)
{
    uint8_t i;
    ADS_CS_LOW();
    DWT_Delay_us(100);

    ADS1292_SPI(reg);
    DWT_Delay_us(100);

    // length - 1
    ADS1292_SPI(len - 1);

    // 写寄存器
    if (reg & 0x40)
    {
        for (i = 0; i < len; i++)
        {
            DWT_Delay_us(100);
            ADS1292_SPI(*data++);
        }
    }
    // 读寄存器
    else
    {
        for (i = 0; i < len; i++)
        {
            DWT_Delay_us(100);
            *data++ = ADS1292_SPI(0xFF);
        }
    }

    DWT_Delay_us(100);
    ADS_CS_HIGH();
}

/**
 * @brief  读取 9 个字节(状态 + CH1 + CH2)
 * @param  data: 数据保存缓冲区 (至少 9 字节)
 */
uint8_t ADS1292_Read_Data(uint8_t *data)
{
    uint8_t i;

    ADS_CS_LOW();
    DWT_Delay_us(10);

    for (i = 0; i < 9; i++)
    {
        *data++ = ADS1292_SPI(0x00);
    }

    DWT_Delay_us(10);
    ADS_CS_HIGH();

    return 0;
}

/**
 * @brief  开启 DRDY 中断（或恢复中断）
 */
void ADS1292_Recv_Start(void)
{
    // 如果使用 HAL 库中断，可以启用 NVIC
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/**
 * @brief  设置全局寄存器数组
 */
void ADS1292_SET_REGBUFF(void)
{
    // 1) ID只读，此处无须设置
    ADS1292_REG[ID] = DEVICE_ID_ADS1292R; // ID 读到的值

    // 2) CONFIG1
    ADS1292_REG[CONFIG1] = 0x00;
    ADS1292_REG[CONFIG1] |= Ads1292_Config1.Data_Rate; // [2:0]

    // 3) CONFIG2
    ADS1292_REG[CONFIG2] = 0x00;
    ADS1292_REG[CONFIG2] |= (Ads1292_Config2.Pdb_Loff_Comp << 6);
    ADS1292_REG[CONFIG2] |= (Ads1292_Config2.Pdb_Refbuf << 5);
    ADS1292_REG[CONFIG2] |= (Ads1292_Config2.Vref << 4);
    ADS1292_REG[CONFIG2] |= (Ads1292_Config2.Clk_EN << 3);
    ADS1292_REG[CONFIG2] |= (Ads1292_Config2.Int_Test << 1);
    // 默认位：bit7=1, bit0=1 => 0x81
    ADS1292_REG[CONFIG2] |= 0x81;

    // 4) LOFF (本例固定设置成 0xF0，若需修改可更改)
    ADS1292_REG[LOFF] = 0xF0;

    // 5) CH1SET
    ADS1292_REG[CH1SET] = 0x00;
    ADS1292_REG[CH1SET] |= (Ads1292_Ch1set.PD << 7);
    ADS1292_REG[CH1SET] |= (Ads1292_Ch1set.GAIN << 4);
    ADS1292_REG[CH1SET] |= Ads1292_Ch1set.MUX;

    // 6) CH2SET
    ADS1292_REG[CH2SET] = 0x00;
    ADS1292_REG[CH2SET] |= (Ads1292_Ch2set.PD << 7);
    ADS1292_REG[CH2SET] |= (Ads1292_Ch2set.GAIN << 4);
    ADS1292_REG[CH2SET] |= Ads1292_Ch2set.MUX;

    // 7) RLD_SENS
    ADS1292_REG[RLD_SENS] = 0x00;
    // 默认 bit7:6 = 11 => 0xC0
    ADS1292_REG[RLD_SENS] |= 0xC0;
    ADS1292_REG[RLD_SENS] |= (Ads1292_Rld_Sens.Pdb_Rld << 5);
    ADS1292_REG[RLD_SENS] |= (Ads1292_Rld_Sens.Rld_Loff_Sense << 4);
    ADS1292_REG[RLD_SENS] |= (Ads1292_Rld_Sens.Rld2N << 3);
    ADS1292_REG[RLD_SENS] |= (Ads1292_Rld_Sens.Rld2P << 2);
    ADS1292_REG[RLD_SENS] |= (Ads1292_Rld_Sens.Rld1N << 1);
    ADS1292_REG[RLD_SENS] |= Ads1292_Rld_Sens.Rld1P;

    // 8) LOFF_SENS
    ADS1292_REG[LOFF_SENS] = 0x00;
    ADS1292_REG[LOFF_SENS] |= (Ads1292_Loff_Sens.Flip2 << 5);
    ADS1292_REG[LOFF_SENS] |= (Ads1292_Loff_Sens.Flip1 << 4);
    ADS1292_REG[LOFF_SENS] |= (Ads1292_Loff_Sens.Loff2N << 3);
    ADS1292_REG[LOFF_SENS] |= (Ads1292_Loff_Sens.Loff2P << 2);
    ADS1292_REG[LOFF_SENS] |= (Ads1292_Loff_Sens.Loff1N << 1);
    ADS1292_REG[LOFF_SENS] |= Ads1292_Loff_Sens.Loff1P;

    // 9) LOFF_STAT 只读，置 0
    ADS1292_REG[LOFF_STAT] = 0x00;

    // 10) RESP1
    ADS1292_REG[RESP1] = 0x00;
    ADS1292_REG[RESP1] |= (Ads1292_Resp1.RESP_DemodEN << 7);
    ADS1292_REG[RESP1] |= (Ads1292_Resp1.RESP_modEN << 6);
    // RESP_ph[5:2]
    ADS1292_REG[RESP1] |= (Ads1292_Resp1.RESP_ph << 2);
    ADS1292_REG[RESP1] |= Ads1292_Resp1.RESP_Ctrl;
    // bit1 = 1 => 0x02
    ADS1292_REG[RESP1] |= 0x02;

    // 11) RESP2
    ADS1292_REG[RESP2] = 0x00;
    ADS1292_REG[RESP2] |= (Ads1292_Resp2.Calib << 7);
    ADS1292_REG[RESP2] |= (Ads1292_Resp2.freq << 2);
    ADS1292_REG[RESP2] |= (Ads1292_Resp2.Rldref_Int << 1);
    // bit0=1 => 0x01
    ADS1292_REG[RESP2] |= 0x01;

    // 12) GPIO (最后一个寄存器)
    // 示例 0x0C => GPIO3:2 为输入
    ADS1292_REG[GPIO] = 0x0C;
}

/**
 * @brief  将寄存器数组写入芯片，再读回校验
 * @retval 0 表示成功，1 表示失败
 */
uint8_t ADS1292_WRITE_REGBUFF(void)
{
    uint8_t i, res = 0;
    uint8_t REG_Cache[12];

    ADS1292_SET_REGBUFF(); // 设置本地寄存器数组

    // 写入除 ID(0x00) 以外的 11 个寄存器
    ADS1292_WR_REGS(WREG | CONFIG1, 11, ADS1292_REG + 1);
    DWT_Delay_ms(10);

    // 读回 12 个寄存器
    ADS1292_WR_REGS(RREG | ID, 12, REG_Cache);
    DWT_Delay_ms(10);

    // 校验
    for (i = 0; i < 12; i++)
    {
        if (ADS1292_REG[i] != REG_Cache[i])
        {
            // 0(ID)、8(LOFF_STAT)、11(GPIO) 可能与写入不一致，不做严格校验
            if (i != 0 && i != 8 && i != 11)
            {
                res = 1;
            }
        }
    }

    return res;
}

/**
 * @brief  设置通道 1/2 为内部测试信号输入
 */
uint8_t ADS1292_Single_Test(void)
{
    uint8_t res = 0;
    Ads1292_Config2.Int_Test = INT_TEST_ON;
    Ads1292_Ch1set.MUX = MUX_Test_signal;
    Ads1292_Ch2set.MUX = MUX_Test_signal;

    if (ADS1292_WRITE_REGBUFF())
        res = 1;

    DWT_Delay_ms(10);
    return res;
}

/**
 * @brief  设置内部噪声测试
 */
uint8_t ADS1292_Noise_Test(void)
{
    uint8_t res = 0;
    Ads1292_Config2.Int_Test = INT_TEST_OFF;
    Ads1292_Ch1set.MUX = MUX_input_shorted;
    Ads1292_Ch2set.MUX = MUX_input_shorted;

    if (ADS1292_WRITE_REGBUFF())
        res = 1;

    DWT_Delay_ms(10);
    return res;
}

/**
 * @brief  设置正常信号采集模式
 */
uint8_t ADS1292_Single_Read(void)
{
    uint8_t res = 0;
    Ads1292_Config2.Int_Test = INT_TEST_OFF;
    Ads1292_Ch1set.MUX = MUX_Normal_input;
    Ads1292_Ch2set.MUX = MUX_Normal_input;

    if (ADS1292_WRITE_REGBUFF())
        res = 1;

    DWT_Delay_ms(10);
    return res;
}

/**
 * @brief  配置 ADS1292 的采集方式 (0=正常，1=测试，2=噪声)
 */
uint8_t Set_ADS1292_Collect(uint8_t mode)
{
    uint8_t res = 0;

    DWT_Delay_ms(10);
    switch (mode)
    {
    case 0:
        res = ADS1292_Single_Read();
        break;
    case 1:
        res = ADS1292_Single_Test();
        break;
    case 2:
        res = ADS1292_Noise_Test();
        break;
    default:
        res = 1;
        break;
    }

    if (res)
        return 1; // 配置寄存器失败

    // 开始转换
    ADS1292_Send_CMD(START);
    DWT_Delay_ms(10);

    // 连续读取模式
    ADS1292_Send_CMD(RDATAC);
    DWT_Delay_ms(10);

    return 0;
}

/**
 * @brief  读取设备 ID (寄存器 0x00)
 * @note   需要先发送 SDATAC 停止连续输出
 * @retval 读到的设备 ID
 */
uint8_t ADS1292_ReadDeviceID(void)
{
    uint8_t device_id;

    // 1) 停止连续输出
    ADS1292_Send_CMD(SDATAC);
    DWT_Delay_ms(2);

    // 2) 拉低 CS，发送 RREG 命令
    ADS_CS_LOW();
    DWT_Delay_us(10);

    ADS1292_SPI(RREG | 0x00); // RREG + 地址=0
    ADS1292_SPI(0x00);        // 读取 1 个寄存器 => length - 1 = 0

    // 3) 读取
    device_id = ADS1292_SPI(0xFF);
    DWT_Delay_us(10);

    ADS_CS_HIGH();
    DWT_Delay_ms(2);

    return device_id;
}
