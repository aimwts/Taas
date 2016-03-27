//==============================================================================
//
// Title:       AXIeSys.cpp
// Purpose:     This module provides test executive interface functions for 
//				Loading, Starting, Ending, and Unloading test applications.
//              Some basic utility functions are also provided.
//
// Eng	Rev	Date______  Description_____________________________________________
//	JC	 2	05/09/12	Add AXIe_I2C functions
//	JC	 3	05/26/15	Add call to generic function updateNewLotID() in <app>.cpp from AxiDutStart().
//=================================================================================
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

#include "stdafx.h"
#include "AXIAPI.h"
#include "MLog.h"

#include <stdlib.h>
#include <stdio.h>
#include <direct.h>		// _getcwd()
#include "AXIeSys.h"

#define AXI_SYS __declspec(dllexport)

// Constants
#define GLOBAL_I2C_CMD		0x0002
#define GLOBAL_I2C_WDATA	0x0003
#define GLOBAL_I2C_RDATA	0x0004
#define RELAYON  0;



static char gCurrentLotNumber[64];

extern void MLog_CreateNewLot   (char* lotId);
extern void MLog_OnStartDut     (void);
extern void MLog_OnEndDut       (void);
extern UINT MLog_GetBin         (UINT siteNumber);

extern void updateNewLotID   (char* myLotNumber);


// AXI Tester Functions ========================================================
/**********************************************************************************
* AxiSystemLoad()                                                                 *
*   When you first [Run] a TestStand Sequence it will call the                    *
*   "PreUUTLoop" sequence once then it will prompt with:                          *
*             Enter UUT Serial Number: [OK] [Stop].  							  *
*                                                                                 *
*   PreUUTLoop calls AxiSystemLoad() where                                  *
*	the Tester is initialized													  *
***********************************************************************************/
AXI_SYS int AxiSystemLoad()
{
	
    TEV_STATUS rc;
	rc = TevExec_SwRestart  (TRUE);	// TRUE=I am the Test Executive. 

	if (rc !=0)
	{
        char str[1024];
		if (rc == FATAL_InvalidEnumeration) // -5
		{
			sprintf_s(str, 1024,
						"Software Restart Error: Code= %d\n\n \
                         REBOOT this computer after toggling testhead power!!!!\n \
                         PCI Express enumeration is not valid,\n \
                         or there is a non-functioning PCIe endpoint.\n\n \
                         Use TraceTool for more information.", rc);
			MessageBox (NULL, (LPCSTR)str, (LPCSTR)"Tev System Error:", MB_ICONQUESTION + MB_OKCANCEL + MB_SYSTEMMODAL + MB_TOPMOST);
			return -1;	// need to abort, return code should be ?....
		}

		else if (rc == WARNING_NoTesterOnPCIe) //  3
		{
			sprintf_s(str, 1024,
						"Software Resart Error: Code= %d\n\n \
                         WARNING_NoTesterOnPCIe\n \
                         Running in simulation mode.\n \
                         Use TraceTool for more information.", rc);
			MessageBox (NULL, (LPCSTR)str, (LPCSTR)"Tev System Error:", MB_ICONQUESTION + MB_OKCANCEL + MB_SYSTEMMODAL + MB_TOPMOST);
		}
		else
		{
			sprintf_s(str, 1024, "Software Resart Error: Code= %d\nUse TraceTool for more information.", rc);
			MessageBox (NULL, (LPCSTR)str, (LPCSTR)"Tev System Error:", MB_ICONQUESTION + MB_OKCANCEL + MB_SYSTEMMODAL + MB_TOPMOST);
		}
	}

	return 0;
}

/**********************************************************************************
* AxiSystemUnload()                                                               *
*                                                                                 *
*   PostUUTLoop calls AxiSystemUnload() after [Stop] has been selected.           *
*	-DUT power is properly shutdown.      										  *
*	-Instruments placed in *safe* state. 										  *
***********************************************************************************/
AXI_SYS int AxiSystemUnload()
{
    TEV_STATUS rc;
	
	//Unload MVP File
  	rc = TevExec_UnloadPinMap ();
	// Release Tester
    rc = TevExec_SwShutdown   ();

	strcpy_s(gCurrentLotNumber, sizeof (gCurrentLotNumber), "");

    return 0;
}

/**********************************************************************************
* AxiDutStart()                                                                   *
*   Gets called at the start of the MainSequence.                                 *
*	-Lots are evaluated and created when necessary                                *
*	-TIP light is turned on.                                                      *
***********************************************************************************/
AXI_SYS int AxiDutStart()
{
	UINT	passMask= 0xFFFF;		// bit mask for each site, default is all sites pass // 0:TestStand displays as failed Test Step.
	char	myLotNumber[256] = {};
	double	results[4] = {0.0, 0.0, 0.0, 0.0};

	const char* myString= {"1234567890123456789012345678901234567890"};
	const char* lotNumber = TevTester_GetRunInfoString ("LotId");	// Case sensitive here!

	strcpy_s(myLotNumber, 256, lotNumber);					// use char* type

// Set LotNumber for DLOG and Summary files
	if (strlen (myLotNumber) == 0)							// Check if empty
		strcpy_s(myLotNumber, "DefaultLot");

	if (strcmp(gCurrentLotNumber, myLotNumber) != 0)		// New Lot ?
	{
		updateNewLotID   (myLotNumber);						// User customize of Datalog & Summay filenames after ApplicationLoad()
		MLog_CreateNewLot((char*) myLotNumber);
		strcpy_s(gCurrentLotNumber, sizeof (gCurrentLotNumber), myLotNumber);	// gApplicationName = '\0'; in MLog_OnProgramUnload()
	}



// Check DUT Power, Turns on TIP light, Re-enables all sites.
	TevExec_StartDutTest ();


	char promptStr[100];
 	char dutStr[10];
    if (TevTester_GetRunInfoBoolean("Prompt_SN"))
	{ 
		MLOG_ForEachEnabledSite(site)
		{
			sprintf_s(promptStr,"Enter Site %d Serial Number", site+1);
			TevTester_CreateAppPrompt (promptStr, dutStr, sizeof (dutStr));
			if (strlen(promptStr) == 0)
				gDutSerialNumbers[site] = AUTONUM;
			else
				gDutSerialNumbers[site] = atoi(dutStr);			
		}
	}
	

	return 0;
}

static int BinDevices(void)
{
	UINT bin = 0;
    for (UINT siteNumber = 1 ; siteNumber <= TevTester_NumberOfSites() ; siteNumber++)
    {
        bin = MLog_GetBin(siteNumber); 
        TevExec_SetTestBin (siteNumber, bin);
    }

    return 0;

}

/**********************************************************************************
* AxiDutEnd()                                                                     *
*   Gets called at the end of the MainSequence.                                   *
*	-DUT power is properly shutdown      										  *
*	-TIP light is turned off.                                                     *
*	-All Duts are binned                                                          *
***********************************************************************************/
AXI_SYS int AxiDutEnd()
{

	TevExec_EndDutTest ();
	BinDevices();
	// Finalize Dlog Device 
	MLog_OnEndDut();
	
    return 0;
}

// Utility Functions ===============================================================
bool FileExists (const char * filename)
{	// Check and see if a file exists
    bool exists = true;
    if (GetFileAttributes(filename) == 0xFFFFFFFF)
        exists = false;;
    return exists;
}

bool GetWorkingPath (char *pathName, size_t pathSize)
{	   // Get the current working directory: 
    char* buffer;
    *pathName = '\0';
    bool success = false;

    if( (buffer = _getcwd( NULL, (int) pathSize )) != NULL )
    {
        strcpy_s(pathName, pathSize, buffer);
        success = true;
    }
    free(buffer);
    return success;
}

EXTERN_C IMAGE_DOS_HEADER __ImageBase;  // Goes with GetTestFunctionDLL()

bool GetTestFunctionDllPath(char *pathname, size_t pathSize)
{	  

    LPTSTR  strDLLPath = new TCHAR[_MAX_PATH];
    ::GetModuleFileName((HINSTANCE)&__ImageBase, strDLLPath, _MAX_PATH);
    int len = (int)strlen(strDLLPath);
    for (int c = len-1; len > 0; c--)
    {
        if (strDLLPath[c] == '\\')
        {
            strDLLPath[c+1] = 0;
            break;
        }
    }
    sprintf_s(pathname, pathSize, strDLLPath);

    free(strDLLPath);
    return true;
}


//********************************************************************************************
//*** Functions for converting data formats to use with EEPROM *******************************
//********************************************************************************************
#if 0
// Converting numbers to char array and back.
	size_t sz;
	sz = sizeof(char);			// 1 bytes
	sz = sizeof(short int);		// 2
	sz = sizeof(int);			// 4
	sz = sizeof(UINT);			// 4
	sz = sizeof(long int);		// 4
	sz = sizeof(float);			// 4
	sz = sizeof(double);		// 8
	sz = sizeof(long double);	// 8
#endif

int float2UINTArray(UINT* UINTArray, float numberIN)
{// inverse: float f = *(float*)(charArray)

	char charArray[4];
	char *p = (char*)&numberIN;
 
	for (int i = 0; i < sizeof(numberIN); i++) 
		charArray[i] =  p[i];

	UINTArray[0] = UINT(charArray[0] << 24) + UINT(charArray[1] << 16) + UINT(charArray[2] << 8) + UINT(charArray[3]);

	return 0;
}

int UINTArray2float(UINT *UINTArray, float *numberOUT)
{// inverse: float f = *(float*)(charArray)

	char charArray[4];
	char *p = (char*)&numberOUT;
 
	for (int i = 0; i < sizeof(numberOUT); i++) 
		charArray[i] =  p[i];

	charArray[0] = (UINTArray[0] >> 24) & 0x00ff;
	charArray[1] = (UINTArray[0] >> 16) & 0x00ff;
	charArray[2] = (UINTArray[0] >>  8) & 0x00ff;
	charArray[3] =  UINTArray[0]        & 0x00ff;

	*numberOUT = *(float*)(charArray);

	return 0;
}


int double2UINTArray(UINT* UINTArray, double numberIN)
{// inverse: float f = *(float*)(charArray)

	char charArray[8];
	char *p = (char*)&numberIN;
 
	for (int i = 0; i < sizeof(numberIN); i++) 
		charArray[i] =  p[i];

	UINTArray[0] = UINT(charArray[0] << 24) + UINT(charArray[1] << 16) + UINT(charArray[2] << 8) + UINT(charArray[3]); // upper
	UINTArray[1] = UINT(charArray[4] << 24) + UINT(charArray[5] << 16) + UINT(charArray[6] << 8) + UINT(charArray[7]); // lower

	return 0;
}

int UINTArray2double(UINT *UINTArray, double *numberOUT)
{// inverse: float f = *(float*)(charArray)

	char charArray[8];
	char *p = (char*)&numberOUT;
 
	for (int i = 0; i < sizeof(numberOUT); i++) 
		charArray[i] =  p[i];

// upper
	charArray[0] = (UINTArray[0] >> 24) & 0x00ff;
	charArray[1] = (UINTArray[0] >> 16) & 0x00ff;
	charArray[2] = (UINTArray[0] >>  8) & 0x00ff;
	charArray[3] =  UINTArray[0]        & 0x00ff;
// lower
	charArray[4] = (UINTArray[1] >> 24) & 0x00ff;
	charArray[5] = (UINTArray[1] >> 16) & 0x00ff;
	charArray[6] = (UINTArray[1] >>  8) & 0x00ff;
	charArray[7] =  UINTArray[1]        & 0x00ff;

	*numberOUT = *(double*)(charArray);

	return 0;
}

int WriteUserEEPROM(UINT slaveAddr, UINT cmdRegAddr, UINT slaveData, UINT numDataBytes)
{	// UINT
//	UINT chassis = 1;					// AX500
//	UINT CIFslot = 1;					// CIF
	UINT chassis = (UINT)TevTester_GetVariableInt16 ("UserI2CEEPROM_chassis");
	UINT CIFslot = (UINT)TevTester_GetVariableInt16 ("UserI2CEEPROM_slot");
	double Twc   =       TevTester_GetVariableDouble("Twc");
	UINT transactionType = WRITE_2_CMD;	// Write with 2 address bytes (16 bits)
	int  I2C_status=0;

	I2C_status = AXIe_I2C_Engine (chassis, CIFslot, transactionType, cmdRegAddr, slaveAddr, numDataBytes, &slaveData);
	TevWait_Now( Twc );	// Wait for Twc Write Cycle time 5mS.

	return I2C_status;
}

int WriteUserEEPROM(UINT slaveAddr, UINT cmdRegAddr, char *slaveData, UINT numDataBytes)
{	// char
	UINT data2Write = 0;
	unsigned short data[4]={0,0,0,0};
	int  I2C_status=0;

// Arrange bytes
	data[0] = slaveData[0];
	data[1] = slaveData[1];
	data[2] = slaveData[2];
	data[3] = slaveData[3];

	data2Write = UINT(data[3] << 24) + UINT(data[2] << 16) + UINT(data[1] << 8) + UINT(data[0]); 
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr,  data2Write, 4);

	return I2C_status;
}

int WriteUserEEPROM(UINT slaveAddr, UINT cmdRegAddr, float number_f, UINT numData)
{	// float
	int  I2C_status=0;
	unsigned int numberUINT = 0;
	UINT UINTArray_f[2];
	float numberOUT_f;

	float2UINTArray (UINTArray_f, number_f);
	UINTArray2float (UINTArray_f, &numberOUT_f);	// Check
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr,  UINTArray_f[0], 4);

	return I2C_status;
}



int WriteUserEEPROM(UINT slaveAddr, UINT cmdRegAddr, double number_d, UINT numData)
{	// double
	int  I2C_status=0;
	unsigned int numberUINT = 0;
	UINT UINTArray_d[2];
	double numberOUT_d;

	double2UINTArray (UINTArray_d, number_d);
	UINTArray2double (UINTArray_d, &numberOUT_d);	// Check
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr,  UINTArray_d[0], 4);
	cmdRegAddr += 4;
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr,  UINTArray_d[1], 4);

	return I2C_status;
}


int ReadUserEEPROM(UINT slaveAddr, UINT cmdRegAddr, UINT *slaveData, UINT numDataBytes)
{	// UINT
	UINT chassis = 1;					// AX500
	UINT CIFslot = 1;					// CIF
	UINT transactionType = READ_2_CMD;	// Write with 2 address bytes (16 bits)
	return AXIe_I2C_Engine (chassis, CIFslot, transactionType, cmdRegAddr, slaveAddr, numDataBytes, slaveData);
}


int ReadUserEEPROM(UINT slaveAddr, UINT cmdRegAddr, char *slaveData, UINT numDataBytes)
{	// char
	UINT chassis = 1;					// AX500
	UINT CIFslot = 1;					// CIF
	UINT transactionType = READ_2_CMD;	// Write with 2 address bytes (16 bits)
	UINT data2Read = 0;
	char *ptr = (char*)&data2Read;
	int i = 0;

	AXIe_I2C_Engine (chassis, CIFslot, transactionType, cmdRegAddr, slaveAddr, numDataBytes, &data2Read);
	
	for (i = 0; i < int(numDataBytes); i++) 
		slaveData[i] =  ptr[i];
	slaveData[i] =  0;		// Terminate

	return 0;
}

int ReadUserEEPROM(UINT slaveAddr, UINT cmdRegAddr, float *numberOUT_f, UINT numDataBytes)
{	// float
	UINT UINTArray_f[2];
	int	I2C_status=0;

	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr, &UINTArray_f[0], 4);
	UINTArray2float (UINTArray_f, numberOUT_f);

	return I2C_status;
}

int ReadUserEEPROM(UINT slaveAddr, UINT cmdRegAddr, double *numberOUT_d, UINT numDataBytes)
{	// double
	UINT UINTArray_d[2];
	int	I2C_status=0;

	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr, &UINTArray_d[0], 4);
	cmdRegAddr += 4;
	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr, &UINTArray_d[1], 4);
	UINTArray2double (UINTArray_d, numberOUT_d);

	return I2C_status;
}

int WaitForI2C ( UINT chassisNumber, UINT slotNumber)
{
	if (!TevTester_IsPresent()) 
		return 0;

	int  i;
	UINT I2C_data;
	i = 0;
	do {
		TevDiag_ReadRegister ( chassisNumber, slotNumber, GLOBAL_I2C_CMD, &I2C_data);
		i+=1;
	} while ((((I2C_data >> 30) & 0x1 ) != 1) && ( i < 5000 ));
    return 0;
}


int I2C_WriteReg ( UINT chassisNumber, UINT slotNumber, UINT I2C_cmd, UINT data)
{
	if (!TevTester_IsPresent()) 
		return 0;

	TEV_STATUS apiErr = AXI_SUCCESS;

	UINT I2C_readcmd;
	WaitForI2C ( chassisNumber, slotNumber);
	apiErr |= TevDiag_WriteRegister ( chassisNumber, slotNumber, GLOBAL_I2C_WDATA, data);
	apiErr |= TevDiag_WriteRegister ( chassisNumber, slotNumber, GLOBAL_I2C_CMD, I2C_cmd);
	WaitForI2C ( chassisNumber, slotNumber);
	apiErr |= TevDiag_ReadRegister ( chassisNumber, slotNumber, GLOBAL_I2C_CMD, &I2C_readcmd);
	I2C_readcmd &=0x70000000; 
	I2C_readcmd = I2C_readcmd >> 27;	// I2C bus EN, I2C Interface Ready, I2C No-ACK, I2C Bus Ready
 
    return I2C_readcmd;
}

int AXIe_I2C_Engine ( UINT chassisNumber, UINT slotNumber, UINT transactionType, UINT cmdRegAddr, UINT slaveAddr, UINT numDataBytes, UINT data[])
{
/*
 This function will use the AXIe common registers for I2C (0x2 cmd, 0x3 write, 0x4 read) to read and write data on the 
 AXIe_User_I2C bus. It does not provide access to the AXIe_DUT_ID_I2C (FRU) bus.
 The I2C engine is in the instrument FPGA and can write or read upto 4 bytes at a time and is the Master.
 ACK is checked after each byte is sent. The transaction will stop immediately if ACK is not recieved from the slave.

 VHDM pinout SDA:A10, SCK:B10, +5V:C10,D10
 CIF,DPS12,DD48 have internal 5k pull-ups to 5V that is always on. 
 May be a good idea to use ADuM1250 isolator.

 For further info refer to:
 AXIe Instrument Standard Common Register Definition, 
 AXIe Instrument Standard User I2C Interface Guide.
*/
	if (!TevTester_IsPresent()) 
		return 0;

	TEV_STATUS apiErr = AXI_SUCCESS;

	UINT BUS_ENABLE_BIT	= 0x80000000;
//	UINT slaveAddr = 0x40;				// PCA9535D Driver I2C device slave ADDR (type + A2,A1,A0)
//	UINT slaveData = 0x0;				// Upto 4 bytes, LSbyte first
//	UINT cmdRegAddr  = 0x0;				// (target Address [15:0]) Slave I2C Command or Special Addr Register (15:8, 7:0); shift bits by 12
//	UINT transactionType = WRITE_1_CMD;	// W:0,1,2, or R:4,5,6 number of Target Addr (Command) bytes; shift bits by 7
//	UINT numDataBytes = 1;				// 1 to 4 bytes, data is stored in AXIe I2C Write Data Register (0x3); shift bits bt 10
	UINT I2C_readcmd, I2C_cmd;

	I2C_cmd = (BUS_ENABLE_BIT + ( (numDataBytes - 1) << 10 ) + ( cmdRegAddr << 12 ) + (transactionType << 7) + slaveAddr);

	WaitForI2C ( chassisNumber, slotNumber);
	apiErr |= TevDiag_WriteRegister ( chassisNumber, slotNumber, GLOBAL_I2C_WDATA, data[0]);
	apiErr |= TevDiag_WriteRegister ( chassisNumber, slotNumber, GLOBAL_I2C_CMD, I2C_cmd);
	WaitForI2C ( chassisNumber, slotNumber);
	apiErr |= TevDiag_ReadRegister ( chassisNumber, slotNumber, GLOBAL_I2C_CMD, &I2C_readcmd);
	I2C_readcmd &=0x70000000; 
	I2C_readcmd = I2C_readcmd >> 28;	// I2C Interface Ready, I2C No-ACK, I2C Bus Ready, (expect 0x5)
 
	if (transactionType==READ_0_CMD || transactionType==READ_1_CMD || transactionType==READ_2_CMD)
	{
		apiErr |= TevDiag_ReadRegister ( chassisNumber, slotNumber,	GLOBAL_I2C_RDATA, data);
	}
    return I2C_readcmd;
}

int	AXIe_EnableI2C ( unsigned int chassisNumber, unsigned int slotNumber )
{
  unsigned int	cmd_I2C		= 0x80000180;
  unsigned int	data_I2C	= 0x55;
  int			err			= AXI_SUCCESS;

	// Enable CIF USER I2C bus driver with no transaction (need to call only once)
	err = I2C_WriteReg ( chassisNumber, slotNumber, cmd_I2C, data_I2C); // ABUS_1FS

	return err;
}

