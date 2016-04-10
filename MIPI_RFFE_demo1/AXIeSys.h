//==============================================================================
//
// Title:       AXIeSys.cpp
// Purpose:     This module provides test executive interface functions for 
//				Loading, Starting, Ending, and Unloading test applications.
//              Some basic utility functions are also provided.
//
// Rev Eng	Date		Description
//  2	JC	05/09/12	Add AXIe_I2C functions
//
// Copyright (c) 2011,2012 Test Evolution Corp.
// All Rights Reserved
//=================================================================================
// Function Called directly from TestStand
// 1.   AxiSystemLoad called from PreUUT when running standalone TestStand.
// 2.   AxiDutStart called from MainSequence Setup.
// 3.   AxiDutEnd called from MainSequence cleanup.
// 4.   AxiSystemUnload called from PostUUT when running standalone TestStand.
//==============================================================================

#ifndef __AXIeSys_H__
#define __AXIeSys_H__
//==============================================================================
// Utility functions
int UINTArray2float(UINT *UINTArray, float *numberOUT);
int float2UINTArray(UINT* UINTArray, float numberIN);
int WriteUserEEPROM(UINT slaveAddr, UINT cmdRegAddr, UINT slaveData,  UINT numDataBytes);
int WriteUserEEPROM(UINT slaveAddr, UINT cmdRegAddr, char *slaveData, UINT numDataBytes);
int WriteUserEEPROM(UINT slaveAddr, UINT cmdRegAddr, float number_f,  UINT numData);
int WriteUserEEPROM(UINT slaveAddr, UINT cmdRegAddr, double number_d, UINT numData);
int ReadUserEEPROM (UINT slaveAddr, UINT cmdRegAddr, UINT *slaveData, UINT numDataBytes);
int ReadUserEEPROM (UINT slaveAddr, UINT cmdRegAddr, char *slaveData, UINT numDataBytes);
int ReadUserEEPROM (UINT slaveAddr, UINT cmdRegAddr, float *numberOUT_f,  UINT numDataBytes);
int ReadUserEEPROM (UINT slaveAddr, UINT cmdRegAddr, double *numberOUT_d, UINT numDataBytes);


extern bool FileExists              (const char * filename);
extern bool GetWorkingPath          (char *pathName, size_t pathSize);
extern bool GetTestFunctionDllPath  (char *pathName, size_t pathSize);

typedef enum
{
	WRITE_0_CMD = 0,	// write without a device target address
	WRITE_1_CMD,		// write with one byte (Target Address7:0)
	WRITE_2_CMD,		// write with two bytes (Target Address15:0)
	READ_0_CMD = 4,		// read without a device target address
	READ_1_CMD,			// read with one byte (Target Address7:0)
	READ_2_CMD,			// read with two bytes (Target Address15:0)
} I2C_TRANSACTION;



int	AXIe_EnableI2C ( UINT chassisNumber, UINT slotNumber );
int AXIe_I2C_Engine( UINT chassisNumber, UINT slotNumber, UINT transactionType, UINT cmdRegAddr, UINT slaveAddr, UINT numDataBytes, UINT data[]);

//==============================================================================
// Global functions

#endif  /* ndef __AXIeSys_H__ */
