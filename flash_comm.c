#include "flash/flash_comm.h"
#include "flash/flash_commands_list.h"
#include "spi.h"

//4096 Stron, po 528 bajtów

#define AT45DB021 	0x03
#define AT45DB041 	0x04
#define AT45DB081 	0x05
#define AT45DB161	0x06
#define AT45DB321	0x07
#define AT45DB641	0x08

#define AT45DB_FLASH_CS_HI_DEF_(CS_CONTROL_PIN)		HAL_GPIO_WritePin(AT45DB_CS_PORT, CS_CONTROL_PIN, GPIO_PIN_SET)
#define AT45DB_FLASH_CS_LO_SEND_(CS_CONTROL_PIN)	HAL_GPIO_WritePin(AT45DB_CS_PORT, CS_CONTROL_PIN, GPIO_PIN_RESET)

AT45DB_Size_typedef	AT45DB_Size_t[2];
//-------------------------------------------------------------------------------------
static uint8_t at45db_spi_send_receive(uint8_t data, uint8_t *returnValue);
static uint8_t at45db_spi_send(uint8_t data);
static uint8_t at45db_spi_send_getdata(uint8_t data);

static uint8_t at45db_readstatus(uint16_t CSPin);
static void at45db_waitbusy(uint16_t CSPin);
static void at45db_resume(uint16_t CSPin);
static uint8_t at45db_get_size(uint8_t subcode, uint8_t status, AT45DB_Size_typedef **ptr);
//-------------------------------------------------------------------------------------
void AT45DB_InitStructureData(AT45DB_Size_typedef *ptr, uint8_t flashNumber)
{
	if(flashNumber == 1)
	{
		ptr->FlashNumber = 1;
		ptr->CS_GPIO_Pin = AT45DB_CS_PIN_1;
	}
	else if(flashNumber == 2)
	{
		ptr->FlashNumber = 2;
		ptr->CS_GPIO_Pin = AT45DB_CS_PIN_2;
	}

	ptr->FlashSize = 0;
	ptr->PageSize = 0;
	ptr->Pages = 0;
	ptr->Shift = 0;
}

void AT45DB_PowerDown(uint16_t CSPin)
{
    AT45DB_FLASH_CS_LO_SEND_(CSPin);
	at45db_spi_send(AT45DB_PWRDOWN);
    AT45DB_FLASH_CS_HI_DEF_(CSPin);
}

uint32_t AT45DB_ReadManufactureID(uint16_t CSPin)
{
    uint32_t id_from_device = 0;

    AT45DB_FLASH_CS_HI_DEF_(CSPin);
    HAL_Delay(4);
    AT45DB_FLASH_CS_LO_SEND_(CSPin);

    at45db_spi_send(0x9F);

    id_from_device |= at45db_spi_send_getdata(0x00) << 24;    //Revision
    id_from_device |= at45db_spi_send_getdata(0x00) << 16;    //Subcode
    id_from_device |= at45db_spi_send_getdata(0x00) << 8;     //Family
    id_from_device |= at45db_spi_send_getdata(0x00);          //Manufacturer

    AT45DB_FLASH_CS_HI_DEF_(CSPin);

    return id_from_device;
}

uint8_t AT45DB_Init(AT45DB_Size_typedef *ptr) {
	uint8_t revision = 0;
	uint8_t subcode = 0;
	uint8_t status = 0;

	uint32_t manufactureID = AT45DB_ReadManufactureID(ptr->CS_GPIO_Pin);

	revision = (manufactureID & 0xFF000000) >> 24;
	subcode = ((manufactureID & 0x00FF0000) >> 16) & 0x1F;
	status = at45db_readstatus(ptr->CS_GPIO_Pin) & 0x01;

	if(revision != 0x1F) {
		return 1;
	}

	at45db_get_size(subcode, status, (AT45DB_Size_typedef **)&ptr);

	return 0;
}

void AT45DB_EraseChip(uint16_t CSPin)
{
	at45db_resume(CSPin);
	at45db_waitbusy(CSPin);

	AT45DB_FLASH_CS_LO_SEND_(CSPin);

	at45db_spi_send(AT45DB_CHIPERASE1);
	at45db_spi_send(AT45DB_CHIPERASE2);
	at45db_spi_send(AT45DB_CHIPERASE3);
	at45db_spi_send(AT45DB_CHIPERASE4);

	AT45DB_FLASH_CS_HI_DEF_(CSPin);

	at45db_waitbusy(CSPin);
}

void AT45DB_SectorErase(uint8_t sector, uint16_t CSPin)
{
	AT45DB_FLASH_CS_LO_SEND_(CSPin);

	at45db_spi_send(AT45DB_SECTERASE);

    if((sector == 0x0a) || (sector == 0x0b))
    {
    	at45db_spi_send(0x00);
        at45db_spi_send(((sector & 0x01) << 4));
        at45db_spi_send(0x00);
    }
    else
    {
    	at45db_spi_send(sector << 1);
    	at45db_spi_send(0x00);
    	at45db_spi_send(0x00);
    }

	AT45DB_FLASH_CS_HI_DEF_(CSPin);
	at45db_waitbusy(CSPin);
}

void AT45DB_ErasePage(AT45DB_Size_typedef *ptr, uint16_t page)
{
	page = page << ptr->Shift;

	at45db_resume(ptr->CS_GPIO_Pin);
	at45db_waitbusy(ptr->CS_GPIO_Pin);

    AT45DB_FLASH_CS_LO_SEND_(ptr->CS_GPIO_Pin);

	at45db_spi_send(AT45DB_PGERASE);
	at45db_spi_send((page >> 16) & 0xff);
	at45db_spi_send((page >> 8) & 0xff);
	at45db_spi_send(page & 0xff);

    AT45DB_FLASH_CS_HI_DEF_(ptr->CS_GPIO_Pin);

	at45db_waitbusy(ptr->CS_GPIO_Pin);
}

uint8_t AT45DB_WritePage(AT45DB_Size_typedef *ptr, uint8_t *dataToSave, uint16_t dataToSaveLength, uint16_t	pageNumber, uint8_t bufferNumber)
{
	pageNumber = pageNumber << ptr->Shift;
	at45db_resume(ptr->CS_GPIO_Pin);
	at45db_waitbusy(ptr->CS_GPIO_Pin);

	AT45DB_FLASH_CS_LO_SEND_(ptr->CS_GPIO_Pin);

	if(bufferNumber == 1) 		{ at45db_spi_send(AT45DB_MNTHRUBF1); }
	else if(bufferNumber == 2) 	{ at45db_spi_send(AT45DB_MNTHRUBF2); }

	at45db_spi_send((pageNumber >> 16) & 0xff);
	at45db_spi_send((pageNumber >> 8) & 0xff);
	at45db_spi_send(pageNumber & 0xff);

	HAL_StatusTypeDef opStatus = HAL_SPI_Transmit(&hspi1, dataToSave, dataToSaveLength, 100);

	AT45DB_FLASH_CS_HI_DEF_(ptr->CS_GPIO_Pin);

	at45db_waitbusy(ptr->CS_GPIO_Pin);

	return opStatus;
}

uint8_t AT45DB_ReadPage(AT45DB_Size_typedef *ptr, uint8_t* dataToRead, uint16_t dataToReadLength, uint16_t pageNumber)
{
	pageNumber = pageNumber << ptr->Shift;
	if(dataToReadLength > ptr->PageSize) {
		dataToReadLength = ptr->PageSize;
	}

	at45db_resume(ptr->CS_GPIO_Pin);
	at45db_waitbusy(ptr->CS_GPIO_Pin);

	AT45DB_FLASH_CS_LO_SEND_(ptr->CS_GPIO_Pin);

	at45db_spi_send(AT45DB_RDARRAYHF);
	at45db_spi_send((pageNumber >> 16) & 0xff);
	at45db_spi_send((pageNumber >> 8) & 0xff);
	at45db_spi_send(pageNumber & 0xff);
	at45db_spi_send(0);

	HAL_StatusTypeDef opStatus = HAL_SPI_Receive(&hspi1, dataToRead, dataToReadLength, 100);

	AT45DB_FLASH_CS_HI_DEF_(ptr->CS_GPIO_Pin);

	return opStatus;
}
//-----------------------------------------------------------------------------------------------------------------
//--------------------------------------------STATIC FUNCTIONS-----------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------
static uint8_t at45db_readstatus(uint16_t CSPin) {
	uint8_t	status = 0;

	AT45DB_FLASH_CS_LO_SEND_(CSPin);
	at45db_spi_send(AT45DB_RDSR);
	at45db_spi_send_receive(0x00, &status);
	AT45DB_FLASH_CS_HI_DEF_(CSPin);

	return status;
}

static void at45db_waitbusy(uint16_t CSPin)
{
	uint8_t	status = 0x00;

	AT45DB_FLASH_CS_LO_SEND_(CSPin);
	at45db_spi_send(AT45DB_RDSR);
	at45db_spi_send_receive(0x00, &status);
	AT45DB_FLASH_CS_HI_DEF_(CSPin);

	while((status & 0x80) == 0) {
		HAL_Delay(1);
		AT45DB_FLASH_CS_LO_SEND_(CSPin);
	  	at45db_spi_send(AT45DB_RDSR);
		at45db_spi_send_receive(0x00, &status);
		AT45DB_FLASH_CS_HI_DEF_(CSPin);
	}
}

static uint8_t at45db_get_size(uint8_t subcode, uint8_t status, AT45DB_Size_typedef **ptr)
{
	if(subcode == AT45DB021){
		(*ptr)->FlashSize = 2;
		(*ptr)->Pages = 1024;
		if(status) {
			(*ptr)->Shift = 0;
			(*ptr)->PageSize = 256;
		}
		else{
			(*ptr)->Shift = 9;
			(*ptr)->PageSize = 264;
		}
	}
	else if (subcode == AT45DB041) {
		(*ptr)->FlashSize = 4;
		(*ptr)->Pages = 2048;
		if(status) {
			(*ptr)->Shift = 0;
			(*ptr)->PageSize = 256;
		}
		else {
			(*ptr)->Shift = 9;
			(*ptr)->PageSize = 264;
		}
	}
	else if (subcode == AT45DB081) {
		(*ptr)->FlashSize = 8;
		(*ptr)->Pages = 4096;
		if(status) {
			(*ptr)->Shift = 0;
			(*ptr)->PageSize = 256;
		}
		else {
			(*ptr)->Shift = 9;
			(*ptr)->PageSize = 264;
		}
	}
	else if (subcode == AT45DB161) {
		(*ptr)->FlashSize = 16;
		(*ptr)->Pages = 4096;
		if(status) {
			(*ptr)->Shift = 0;
			(*ptr)->PageSize = 512;
		}
		else {
			(*ptr)->Shift = 10;
			(*ptr)->PageSize = 528;
		}
	}
	else if (subcode == AT45DB321) {
		(*ptr)->FlashSize = 32;
		(*ptr)->Pages = 8192;
		if(status)
		{
			(*ptr)->Shift = 0;
			(*ptr)->PageSize = 512;
		}
		else {
			(*ptr)->Shift = 10;
			(*ptr)->PageSize = 528;
		}
	}
	else if (subcode == AT45DB641) {
		(*ptr)->FlashSize = 64;
		(*ptr)->Pages = 8192;
		if(status)
		{
			(*ptr)->Shift = 0;
			(*ptr)->PageSize = 1024;
		}
		else {
			(*ptr)->Shift = 11;
			(*ptr)->PageSize = 1056;
		}
	}
	else{
		return 1;
	}

	return 0;
}

static void at45db_resume(uint16_t CSPin)
{
	AT45DB_FLASH_CS_LO_SEND_(CSPin);
	at45db_spi_send(AT45DB_RESUME);
    AT45DB_FLASH_CS_HI_DEF_(CSPin);
}

static uint8_t at45db_spi_send_receive(uint8_t data, uint8_t *returnValue) {
	uint8_t readVal = 0;
	HAL_StatusTypeDef opStatus = HAL_SPI_TransmitReceive(&hspi1,&data,&readVal,1,1000);

	*(returnValue + 0) = readVal;

	return (uint8_t)opStatus;
}

static uint8_t at45db_spi_send(uint8_t data) {
	uint8_t readDummy = 0;
	HAL_StatusTypeDef opStatus = HAL_SPI_TransmitReceive(&hspi1,&data,&readDummy,1,1000);
	return (uint8_t)opStatus;
}

static uint8_t at45db_spi_send_getdata(uint8_t data) {
	uint8_t readedData = 0;
	HAL_SPI_TransmitReceive(&hspi1,&data,&readedData,1,1000);
	return readedData;
}
