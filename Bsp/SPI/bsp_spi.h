#ifndef __BSP_SPI_H
#define __BSP_SPI_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"
#include "stdint.h"

    // 如果是 CubeMX 生成的工程，一般会生成 extern SPI_HandleTypeDef hspi1;
    // 你也可以手动声明
    extern SPI_HandleTypeDef hspi3;

    // SPI1 单字节发送并接收
    uint8_t SPI3_ReadWriteByte(uint8_t txData);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SPI_H */
