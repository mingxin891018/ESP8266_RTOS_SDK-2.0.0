#ifndef __FOTA_CRC32_H__
#define __FOTA_CRC32_H__

bool ICACHE_FLASH_ATTR
upgrade_crc_check(uint16 fw_bin_sec ,unsigned int sumlength);

#endif //__FOTA_CRC32_H__