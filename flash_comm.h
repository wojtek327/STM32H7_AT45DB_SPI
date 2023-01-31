#ifndef INC_FLASH_FLASH_COMM_H_
#define INC_FLASH_FLASH_COMM_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct AT45DB_Size_TypedefStruct{
	uint8_t FlashNumber;
	uint8_t	FlashSize;
	uint16_t PageSize;
	uint16_t Pages;
	uint8_t	 Shift;
	uint16_t CS_GPIO_Pin;
}AT45DB_Size_typedef;

extern AT45DB_Size_typedef AT45DB_Size_t[2];

void AT45DB_InitStructureData(AT45DB_Size_typedef *ptr, uint8_t flashNumber);
uint8_t AT45DB_Init(AT45DB_Size_typedef *ptr);
uint32_t AT45DB_ReadManufactureID(uint16_t CSPin);
void AT45DB_PowerDown(uint16_t CSPin);
void AT45DB_EraseChip(uint16_t CSPin);
void AT45DB_SectorErase(uint8_t sector, uint16_t CSPin);
void AT45DB_ErasePage(AT45DB_Size_typedef *ptr, uint16_t page);
uint8_t AT45DB_WritePage(AT45DB_Size_typedef *ptr, uint8_t *dataToSave, uint16_t dataToSaveLength, uint16_t	pageNumber, uint8_t bufferNumber);
uint8_t AT45DB_ReadPage(AT45DB_Size_typedef *ptr, uint8_t* dataToRead, uint16_t dataToReadLength, uint16_t pageNumber);

#endif /* INC_FLASH_FLASH_COMM_H_ */
