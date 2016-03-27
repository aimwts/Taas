//-------------------------------------------------------------------------------------------------
// Title:       MIPI_RFFE_demo.cpp
// 
// Description: This demonstration/starting application utilizes one DPS12 board
//              to simulate a 4 site program with a certain percentage of failures
//              over 4 test functions.
// 
// Copyright (c) 2011 Test Evolution Inc.
// All Rights Reserved
//-------------------------------------------------------------------------------------------------
#ifndef __MIPI_RFFE_demo_H__
#define __MIPI_RFFE_demo_H__

#include <stdio.h>

#ifndef _DEBUG
#define _DEBUG 0
#endif

// Patterns get loaded in ApplicationLoad()
// They will get loaded in the order below. 
// Once loaded into DD48 memory they are referenced by th e index number (zero based).
typedef enum pHandleIdx {
//      enum name                  idx  Name-----------			Used in:
		MIPI = 0,				//= 0,	MIPI = 0,				MIPIWaveformsDD48()
		SPI_25LC256,			//= 1,	SPI_25LC256				SPIWaveformsDD48()
		I2C_24LC256,			//= 2,	I2C_24LC256				I2CWaveformsDD48()
		SPI_25LC256_LFE,		//= 3,	SPI_25LC256_LFE			SPIWaveformsDD48()
		I2C_24LC256_LFE,		//= 4,	I2C_24LC256_LFE			I2CWaveformsDD48()
		NUM_OF_PATTERNS			//= 5		
	} pHandleIdx;

// Keep array of pattern filenames in same order as above.
static char*	patterns[]	={  
		"MIPI",					//= 0,	MIPI = 0,			
		"SPI_25LC256",			//= 1,	SPI_25LC256				SPIWaveformsDD48()
		"I2C_24LC256",			//= 2,	I2C_24LC256				I2CWaveformsDD48()
		"SPI_25LC256_LFE",		//= 3,	SPI_25LC256_LFE			SPIWaveformsDD48()
		"I2C_24LC256_LFE"		//= 4,	I2C_24LC256_LFE			I2CWaveformsDD48()
		};																				

extern int AxiSystemLoad();
extern int AxiDutStart();
extern int AxiDutEnd();
extern int AxiSystemUnload();

static BOOL	gPatternsLoaded;

#endif