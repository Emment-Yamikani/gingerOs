/*
 * @Author: Banda Emment Yamikani 
 * @Date: 2021-12-19 17:11:35 
 * @Last Modified by: Banda Emment Yamikani
 * @Last Modified time: 2021-12-19 18:56:28
 */
#ifndef BIOS_H
#define BIOS_H

//bios data area
#define BDA     0x400
//extended bios data area
#define EBDA    (*((uint16_t *)(BDA + 0xe)) << 4)
//bios rom below 1M
#define BIOSROM 0xe0000

#endif //BIOS_H