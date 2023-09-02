/**
  ******************************************************************************
  * @file           : enc28j60.c
  * @brief          : ENC28J60 driver
  * @author         : MicroTechnics (microtechnics.ru)
  ******************************************************************************
  */



/* Includes ------------------------------------------------------------------*/

#include "enc28j60.h"



/* Declarations and definitions ----------------------------------------------*/

uint8_t macAddr[MAC_ADDRESS_BYTES_NUM] = {0x00, 0x17, 0x22, 0xED, 0xA5, 0x01};

static uint8_t commandOpCodes[COMMANDS_NUM] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x07};
static ENC28J60_RegBank curBank = BANK_0;

extern SPI_HandleTypeDef hspi1;



/* Functions -----------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static void SetCS(ENC28J60_CS_State state)
{
  HAL_GPIO_WritePin(ENC28J60_CS_PORT, ENC28J60_CS_PIN, (GPIO_PinState)state);
}



/*----------------------------------------------------------------------------*/
static void WriteBytes(uint8_t* data, uint8_t size)
{
  HAL_StatusTypeDef res = HAL_SPI_Transmit(&hspi1, data, size, ENC28J60_SPI_TIMEOUT); 
}



/*----------------------------------------------------------------------------*/
static void WriteByte(uint8_t data)
{
  HAL_StatusTypeDef res = HAL_SPI_Transmit(&hspi1, &data, 1, ENC28J60_SPI_TIMEOUT);
}



/*----------------------------------------------------------------------------*/
static uint8_t ReadByte()
{
  uint8_t txData = 0x00;
  uint8_t rxData = 0x00;
  HAL_StatusTypeDef res = HAL_SPI_TransmitReceive(&hspi1, &txData, &rxData, 1, ENC28J60_SPI_TIMEOUT);
  return rxData;
}



/*----------------------------------------------------------------------------*/
static ENC28J60_RegType getRegType(uint8_t reg)
{
  ENC28J60_RegType type = (ENC28J60_RegType)((reg & ENC28J60_REG_TYPE_MASK) >> ENC28J60_REG_TYPE_OFFSET);
  return type;
}



/*----------------------------------------------------------------------------*/
static ENC28J60_RegBank getRegBank(uint8_t reg)
{
  ENC28J60_RegBank bank = (ENC28J60_RegBank)((reg & ENC28J60_REG_BANK_MASK) >> ENC28J60_REG_BANK_OFFSET);
  return bank;
}



/*----------------------------------------------------------------------------*/
static uint8_t getRegAddr(uint8_t reg)
{
  uint8_t addr = (reg & ENC28J60_REG_ADDR_MASK);
  return addr;
}



/*----------------------------------------------------------------------------*/
static void WriteCommand(ENC28J60_Command command, uint8_t argData)
{
  uint8_t data = 0;
  data = (commandOpCodes[command] << ENC28J60_OP_CODE_OFFSET) | argData;
  WriteByte(data);
}



/*----------------------------------------------------------------------------*/
static void CheckBank(uint8_t reg)
{
  uint8_t regAddr = getRegAddr(reg);  
  if (regAddr < ENC28J60_COMMON_REGS_ADDR)
  {
    ENC28J60_RegBank regBank = getRegBank(reg);    
    if (curBank != regBank)
    {
      uint8_t econ1Addr = getRegAddr(ECON1);
      
      // Clear bank bits
      SetCS(CS_LOW); 
      WriteCommand(BIT_FIELD_CLEAR, econ1Addr);
      WriteByte(ECON1_BSEL1_BIT | ECON1_BSEL0_BIT);
      SetCS(CS_HIGH);
      
      // Set bank bits
      SetCS(CS_LOW); 
      WriteCommand(BIT_FIELD_SET, econ1Addr);
      WriteByte(regBank);
      SetCS(CS_HIGH);
      
      curBank = regBank;
    }
  }
}



/*----------------------------------------------------------------------------*/
static void BitFieldSet(uint8_t reg, uint8_t regData)
{
  uint8_t regAddr = getRegAddr(reg);
  CheckBank(reg);
  
  SetCS(CS_LOW); 
  WriteCommand(BIT_FIELD_SET, regAddr);
  WriteByte(regData);
  SetCS(CS_HIGH);
}



/*----------------------------------------------------------------------------*/
static void BitFieldClear(uint8_t reg, uint8_t regData)
{
  uint8_t regAddr = getRegAddr(reg);
  CheckBank(reg);
  
  SetCS(CS_LOW); 
  WriteCommand(BIT_FIELD_CLEAR, regAddr);
  WriteByte(regData);
  SetCS(CS_HIGH);
}



/*----------------------------------------------------------------------------*/
static uint8_t ReadControlReg(uint8_t reg)
{
  uint8_t data = 0;
  ENC28J60_RegType regType = getRegType(reg);
  uint8_t regAddr = getRegAddr(reg);
  CheckBank(reg);
  
  SetCS(CS_LOW);
  WriteCommand(READ_CONTROL_REG, regAddr);

  if (regType == MAC_MII_REG)
  {
    ReadByte();
  }
  data = ReadByte();
  
  SetCS(CS_HIGH);
  return data;
}



/*----------------------------------------------------------------------------*/
static void WriteControlReg(uint8_t reg, uint8_t regData)
{
  uint8_t regAddr = getRegAddr(reg);
  CheckBank(reg);
  
  SetCS(CS_LOW); 
  WriteCommand(WRITE_CONTROL_REG, regAddr);
  WriteByte(regData);
  SetCS(CS_HIGH);
}



/*----------------------------------------------------------------------------*/
static void WriteControlRegPair(uint8_t reg, uint16_t regData)
{
  WriteControlReg(reg, (uint8_t)regData);
  WriteControlReg(reg + 1, (uint8_t)(regData >> 8));
}



/*----------------------------------------------------------------------------*/
static uint16_t ReadControlRegPair(uint8_t reg)
{
  uint16_t data = 0;
  data = (uint16_t)ReadControlReg(reg) | ((uint16_t)ReadControlReg(reg + 1) << 8);
  return data;
}



/*----------------------------------------------------------------------------*/
static void WriteBufferMem(uint8_t *data, uint16_t size)
{
  SetCS(CS_LOW); 
  WriteCommand(WRITE_BUFFER_MEM, ENC28J60_BUF_COMMAND_ARG);
  WriteBytes(data, size);
  SetCS(CS_HIGH);
}



/*----------------------------------------------------------------------------*/
static void ReadBufferMem(uint8_t *data, uint16_t size)
{
  SetCS(CS_LOW); 
  WriteCommand(READ_BUFFER_MEM, ENC28J60_BUF_COMMAND_ARG);
  
  for (uint16_t i = 0; i < size; i++)
  {
    *data = ReadByte();
    data++;
  }
  
  SetCS(CS_HIGH);
}



/*----------------------------------------------------------------------------*/
static void SystemReset()
{
  SetCS(CS_LOW);
  WriteCommand(SYSTEM_RESET, ENC28J60_RESET_COMMAND_ARG);
  SetCS(CS_HIGH);
  
  curBank = BANK_0;
  HAL_Delay(100);
}



/*----------------------------------------------------------------------------*/
static uint16_t ReadPhyReg(uint8_t reg)
{
  uint16_t data = 0;
  uint8_t regAddr = getRegAddr(reg);
  
  WriteControlReg(MIREGADR, regAddr);
  BitFieldSet(MICMD, MICMD_MIIRD_BIT);
  
  while((ReadControlReg(MISTAT) & MISTAT_BUSY_BIT) != 0);
  
  BitFieldClear(MICMD, MICMD_MIIRD_BIT);
  data = ReadControlRegPair(MIRDL);
  
  return data;
}



/*----------------------------------------------------------------------------*/
static void WritePhyReg(uint8_t reg, uint16_t regData)
{
  uint8_t regAddr = getRegAddr(reg);
  
  WriteControlReg(MIREGADR, regAddr);
  WriteControlRegPair(MIWRL, regData);
  
  while((ReadControlReg(MISTAT) & MISTAT_BUSY_BIT) != 0);
}



/*----------------------------------------------------------------------------*/
void ENC28J60_Init()
{
  HAL_GPIO_WritePin(ENC28J60_RESET_PORT, ENC28J60_RESET_PIN, GPIO_PIN_RESET);
  HAL_Delay(50);
  HAL_GPIO_WritePin(ENC28J60_RESET_PORT, ENC28J60_RESET_PIN, GPIO_PIN_SET);
  HAL_Delay(50);
  
  SystemReset();

  // Rx/Tx buffers
  WriteControlRegPair(ERXSTL, ENC28J60_RX_BUF_START);
  WriteControlRegPair(ERXNDL, ENC28J60_RX_BUF_END);
  
  WriteControlRegPair(ERDPTL, ENC28J60_RX_BUF_START);
  
  // MAC address
  WriteControlReg(MAADR1, macAddr[0]);
  WriteControlReg(MAADR2, macAddr[1]);
  WriteControlReg(MAADR3, macAddr[2]);
  WriteControlReg(MAADR4, macAddr[3]);
  WriteControlReg(MAADR5, macAddr[4]);
  WriteControlReg(MAADR6, macAddr[5]);
  
  WriteControlReg(MACON1, MACON1_TXPAUS_BIT | MACON1_RXPAUS_BIT | MACON1_MARXEN_BIT);
  WriteControlReg(MACON3, MACON3_PADCFG0_BIT | MACON3_TXCRCEN_BIT | MACON3_FRMLNEN_BIT);
    
  WriteControlRegPair(MAIPGL, ENC28J60_NBB_PACKET_GAP);
  WriteControlReg(MABBIPG, ENC28J60_BB_PACKET_GAP);
  
  WriteControlRegPair(MAMXFLL, ENC28J60_FRAME_DATA_MAX);
  
  // PHY resisters
  WritePhyReg(PHCON2, PHCON2_HDLDIS_BIT);
  
  ENC28J60_StartReceiving();
}



/*----------------------------------------------------------------------------*/
void ENC28J60_StartReceiving()
{
  BitFieldSet(ECON1, ECON1_RXEN_BIT);
}



/*----------------------------------------------------------------------------*/
