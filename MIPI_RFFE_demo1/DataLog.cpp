//==============================================================================
//
// Title:       DataLog.cpp
// Purpose:     This module provides an API interface to Test Evolution's 
//				data services.
//
// Copyright (c) 2012 Test Evolution Corp.
// All Rights Reserved
// Eng	Rev	Date______  Description_____________________________________________
// MRL	 1  09/24/2012  Merged Datalog & DatalogSTDF into 1
// MRL	 2  01/04/2013  Added Fastbinning and 1 of N filtering support
// MRL	 3  01/10/2013  Bug Fixes
// MRL	 3  02/05/2013  New Lot Dut # reset & dut prompt
// MRL	 4  10/24/2013  Added MLog_LogSiteState ()
// MRL	 5  07/01/2014  Added MLog_SetNumberOfBins () and updated MLog_SetupBin 
//						to support 256 soft bins
// MRL	 6  07/23/2014  Added MLog_Footer()
// MRL	 7  08/24/2014  Added Direct Binning mode
// MRL   8  05/19/2015  AppTemplate fix for DeActivate Site Dlog bug
//=================================================================================
// For each active site (1 based): Datalog the channel(s) result.

/*
// Gets the number of sites associated with a test program.
// Returns the largest site number loaded into the MVP list by TevTester_AddInstrumentToMVP(), 
// or the number of sites specified by the pin map file referenced by TevTester_LoadPinMap().
UINT        TevTester_NumberOfSites         (void);

// Inform the tester that no further testing should occur at the specified site 
// for the remainder of the present execution of the test program.
// A disabled site(s) is automatically re-enabled at the start of the next test program execution.
// Note: A DISABLED site is still ACTIVE
TEV_STATUS  TevTester_DisableSite (UINT siteNumber);

// Setting a site's active state (activate, deactivate) occurs at the test exec level. 
// This state persists between runs of a test program – once set, 
// it remains in that state until changed.
BOOL	TevTester_IsSiteActive    (UINT siteNumber);

// A site's enable state (enable, disable) is controlled by the test program. 
// It persists for the duration of the present execution of the program.
// If a site is deactivated (by the test exec), then it is also considered to be disabled.
BOOL	TevTester_IsSiteEnabled   (UINT siteNumber);


int     TevTester_GetChannelSite  (const char* mvp, UINT instrIndex);

mvpResultData[] format (this is the default format):
[0]  pin 1  site 1
[1]  pin 1  site 2
[2]  pin 2  site 1
[3]  pin 2  site 2
[4]  pin 3  site 1
[5]  pin 3  site 2
[6]  pin 4  site 1
[7]  pin 4  site 2
*/

#include "stdafx.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "AXIeSys.h"
#include "ACS_TEV_DataServices.h"
#include "DataLog.h"
#include "AXIGo.h"

#define MAX_STDF_SOFT_BINS 32

static BOOL IMMED_MODE = 1;
static BOOL	STDF = 1;
static BOOL APPEND_WITH_UNDERSCORE = 0;
static BOOL FORCE_IMMED_MODE_DLOG = 1;

// User Functions ==============================================================
UINT gDataOrder = 0;			// 0:SITE_MAJOR(default), 1:PIN_MAJOR
UINT gNumSites  = 0;
static BOOL g1ofNFilter				= FALSE;
static BOOL gFastBinning			= FALSE;
static BOOL gStopOnFirstFailure		= FALSE;
static MLOG_BIN_MODE gBinningMode	= MLOG_DISQ_BINNING;

static char gApplicationName [64] = {'\0'};
UINT gSite1Count = 1;
UINT g1ofN = 0;
UINT gSoftBin[MAX_STDF_SOFT_BINS];
UINT gDisableThisSite[MAX_STDF_SOFT_BINS];
UINT gSubTestNumber = 1;


void MLog_DisableSites(void);

static UINT gSiteTestFailedMask   = 0;
UINT   gDutSerialNumbers[MAX_STDF_SOFT_BINS];
char mvpSiteStr[32];

const char* MLog_MakeSiteMVP(char *mvp, UINT siteNumber)
{
	char str[5];

	strcpy_s  (mvpSiteStr, 32, mvp);
	strcat_s  (mvpSiteStr, 32, "@");					// not used as an escape char
	sprintf_s (str, 5, "%d", siteNumber);		// site is ONE based for MVPs
	strcat_s  (mvpSiteStr, 32, str);

	return mvpSiteStr;

}
void BuildTestNameString(const char* mvpName, const char* testName, char* testStr, size_t testStrSize, int pin)
{
	const char *pin_Name = {0};
	UINT	numPinsPerSite = TevTester_NumberOfPins(mvpName);	// Pins per site
	// Build testName string
	strcpy_s(testStr, testStrSize, testName);
	if (numPinsPerSite ==0)
	{
		if (strnlen(mvpName, 1024)>0)
		{
			if (APPEND_WITH_UNDERSCORE)
			{
				strcat_s(testStr, testStrSize, "_"    );
				strcat_s(testStr, testStrSize, mvpName );
			}
			else
			{
				strcat_s(testStr, testStrSize, " ("    );
				strcat_s(testStr, testStrSize, mvpName );
				strcat_s(testStr, testStrSize, ")"     );
			}
		}
	}
	else
	{
		if (numPinsPerSite > 1)
		{	// Add name of *this* pin
			pin_Name = TevTester_GetPinName(mvpName, pin);
			if (APPEND_WITH_UNDERSCORE)
			{
				strcat_s(testStr, testStrSize, "_"    );
				strcat_s(testStr, testStrSize, pin_Name );
			}
			else
			{
				strcat_s(testStr, testStrSize, " ("    );
				strcat_s(testStr, testStrSize, pin_Name );
				strcat_s(testStr, testStrSize, ")"     );
			}
		}
		else
		{	// Use MVPName passed in
			if (APPEND_WITH_UNDERSCORE)
			{
				strcat_s(testStr, 256, "_"    );
				strcat_s(testStr, 256, mvpName );
			}
			else
			{
				strcat_s(testStr, testStrSize, " ("    );
				strcat_s(testStr, testStrSize, mvpName );
				strcat_s(testStr,testStrSize, ")"     );
			}
		}
	}
}

void MLog_SetNumberOfBins(UINT numberOfBins)
{
	TevData_SetNumberOfBins(numberOfBins);
	TevTester_SetNumberOfTestBins (numberOfBins);
}

void MLog_SetupBin(UINT softBin, UINT hardBin, const char *binName, MLOG_BINTYPE binType)
{
	int status;

	status = TevTester_SetTestBinName (softBin, binName);
	TevData_SetBinName(softBin-1, binName);
    status = TevTester_SetTestBinHardBin (softBin, hardBin);
	TevData_SetHardBin(softBin-1, hardBin);
    status = TevTester_SetTestBinType(softBin,binType);
	TevData_SetBinGood(softBin-1, binType);
	
	if (binType == MLOG_BIN_PASS)
		status = Dlog_RegisterBin( softBin,  hardBin, binName, BINTYPE_PASS);
	else
		status = Dlog_RegisterBin( softBin,  hardBin, binName, BINTYPE_FAIL);

}

BOOL MLog_AnyEnabledSites(void)
{	
	for (UINT siteIndex = 0; siteIndex < TevTester_NumberOfSites(); siteIndex++) 
	{
		UINT siteNumber = siteIndex+1;
		if (TevTester_IsSiteActive(siteNumber) && TevTester_IsSiteEnabled(siteNumber)) 
		{
			return true;
		}
	}

	return false;
}

BOOL MLog_IsSiteEnabled(UINT siteIndex) // zero Based
{	
	UINT siteNumber = siteIndex+1;
	if (TevTester_IsSiteActive(siteNumber) && TevTester_IsSiteEnabled(siteNumber)) 
	{
		return true;	
	}

	return false;
}



void MVP_SetDataOrder(UINT order)
{
	if (order == SITE_MAJOR)	// This is the default
	{
		TevExec_SetPinGroupDataOrder    ("SiteMajor");
	}
	else if (order == PIN_MAJOR)
	{
		TevExec_SetPinGroupDataOrder    ("PinMajor");
	}
	
	gDataOrder = order;

}

void MVP_GetArray(const char mvpName[], double resultData[], UINT numberOfPoints, UINT siteIndex, UINT channelIndex, double *resultArray)
{
	UINT dataIndex;

	UINT numChanPerSite, numSites, channels;
	
	numSites = TevTester_NumberOfSites();
	channels = TevTester_NumberOfChannels(mvpName);
	numChanPerSite = channels / numSites;
		
	if (gDataOrder == SITE_MAJOR)
	{
		//resultData[] format (this is the default format):
		// Quad Site - 3 Pin Group
		//[0]  pin A  site 1
		//[1]  pin B  site 1
		//[2]  pin C  site 1
		//[3]  pin A  site 2
		//[4]  pin B  site 2
		//[5]  pin C  site 2
		//[6]  pin A  site 3
		//[7]  pin B  site 3
		//[8]  pin C  site 3
		//[9]  pin A  site 4
		//[10] pin B  site 4
		//[11] pin C  site 4
	
		dataIndex = channelIndex*numberOfPoints + (siteIndex*numChanPerSite*numberOfPoints);
		memcpy(resultArray, &resultData[dataIndex], sizeof(double)*numberOfPoints);
	}
	else if (gDataOrder == PIN_MAJOR)
	{
		//resultData[] format (this is the default format):
		// Quad Site - 3 Pin Group
		//[0]  pin A  site 1
		//[1]  pin A  site 2
		//[2]  pin A  site 3
		//[3]  pin A  site 4
		//[4]  pin B  site 1
		//[5]  pin B  site 2
		//[6]  pin B  site 3
		//[7]  pin B  site 4
		//[0]  pin C  site 1
		//[1]  pin C  site 2
		//[2]  pin C  site 3
		//[3]  pin C  site 4
		//[4]  pin D  site 1
		//[5]  pin D  site 2
		//[6]  pin D  site 3
		//[7]  pin D  site 4
	
		dataIndex = channelIndex*numberOfPoints*numSites + siteIndex*numberOfPoints;
		
		memcpy(resultArray, &resultData[dataIndex], sizeof(double)*numberOfPoints);
	}

}


double MVP_GetResult(const char mvpName[], double resultData[], UINT siteIndex, UINT channelIndex)
{
	UINT dataIndex;
	double result;
	UINT numChanPerSite, numSites, channels;
	
	numSites = TevTester_NumberOfSites();
	channels = TevTester_NumberOfChannels(mvpName);
	numChanPerSite = channels / numSites;
		
	if (gDataOrder == SITE_MAJOR)
	{
		//resultData[] format (this is the default format):
		// Quad Site - 3 Pin Group
		//[0]  pin A  site 1
		//[1]  pin B  site 1
		//[2]  pin C  site 1
		//[3]  pin A  site 2
		//[4]  pin B  site 2
		//[5]  pin C  site 2
		//[6]  pin A  site 3
		//[7]  pin B  site 3
		//[8]  pin C  site 3
		//[9]  pin A  site 4
		//[10] pin B  site 4
		//[11] pin C  site 4
	
		dataIndex = channelIndex + (siteIndex*numChanPerSite);
		result = resultData[dataIndex];
	
	}
	else if (gDataOrder == PIN_MAJOR)
	{
		//resultData[] format (this is the default format):
		// Quad Site - 3 Pin Group
		//[0]  pin A  site 1
		//[1]  pin A  site 2
		//[2]  pin A  site 3
		//[3]  pin A  site 4
		//[4]  pin B  site 1
		//[5]  pin B  site 2
		//[6]  pin B  site 3
		//[7]  pin B  site 4
		//[0]  pin C  site 1
		//[1]  pin C  site 2
		//[2]  pin C  site 3
		//[3]  pin C  site 4
		//[4]  pin D  site 1
		//[5]  pin D  site 2
		//[6]  pin D  site 3
		//[7]  pin D  site 4
	
		dataIndex = siteIndex + channelIndex*numSites;
		result = resultData[dataIndex];
	}

	return result;
}

static int DisqualifyBinName (const char* binName)
{
	UINT bin;
	if(IMMED_MODE)
	{
		bin = TevExec_TestBinNumber (binName);
		if (bin)
			TevData_DisqualifyBin (bin - 1);
	}
	return bin;
}

void MLog_SetFastBinning (BOOL fastBinning)
{
    gFastBinning = fastBinning;
}

BOOL MLog_GetFastBinning (void)
{
    return gFastBinning;
}
void MLog_SetBinningMode (MLOG_BIN_MODE mode)
{
    gBinningMode = mode;
	TevData_SetBinningMode(mode);
}
MLOG_BIN_MODE MLog_GetBinningMode (void)
{
	int mode = TevData_GetBinningMode();
    return gBinningMode;
}

void MLog_SetStopOnFirstFail (BOOL stopFirstFail)
{
    gStopOnFirstFailure = stopFirstFail;
}

BOOL MLog_GetStopOnFirstFail (void)
{
    return gStopOnFirstFailure;
}
BOOL MLog_1ofN(UINT siteIndex)
{
	if (g1ofNFilter)
	{
		if ((gSite1Count+siteIndex) % g1ofN)
			return false;
		else
			return true;
	}
	else
		return true;

}

void MLog_Set1ofNFilter(BOOL state, UINT N)
{
	g1ofNFilter = state;
	g1ofN = N;
}

void MLog_Get1ofNFilter(BOOL *state, UINT *N)
{
	*state = g1ofNFilter;
	*N = g1ofN;
}

void MLog_Header (unsigned int testNumber, const char* testName, const char* comment)
{
	gSubTestNumber = 1; // for AutoNumber
	MLOG_ForEachEnabledSite(siteIndex)
	{
		if (MLog_1ofN(siteIndex))
		{
			if (IMMED_MODE)
				TevData_LogHeader (testNumber, testName, comment, siteIndex+1);
			if (STDF)
				Dlog_BeginSequence (siteIndex+1, testName, comment);
		}
	}
		
}

void MLog_Footer (SITE_PASS_MASK passMask, const char* testName, const char* comment)
{
	
	MLOG_ForEachEnabledSite(siteIndex)
	{
		if (MLog_1ofN(siteIndex))
		{
			if (IMMED_MODE)
			{
				if ((passMask & (1 << siteIndex)) == 0)
				{
					TevData_SetFailed(siteIndex+1);
				}
				TevData_LogFooter ( testName, comment, siteIndex+1);
			}
			
		}
	}
		
}


BOOL MLog_CheckLimits (double value, double minVal, double maxVal)
{
    BOOL pass = TRUE;
    double minimum = minVal;
    double maximum = maxVal;

    if (minimum != INVALID_FLOAT)
        if (value < minimum)
            pass = FALSE;

    if (maximum != INVALID_FLOAT)
        if (value > maximum)
            pass = FALSE;

    return pass;
}
void MLog_DisableSites(void)
{
	UINT siteNumber;
	if (gFastBinning & !gStopOnFirstFailure)
	{
		MLOG_ForEachEnabledSite(siteIndex)
		{
			if (gDisableThisSite[siteIndex])
			{
				siteNumber = siteIndex + 1;
				MLog_UserDisableSite(siteNumber);		 // Must be defined in user code
				TevTester_DisableSite (siteNumber);
			}
		}
	}
}


// See these for information on unit strings.
// ~AXITest\Doc\Data Log Wrapper API Spec.doc
//	C:\AXITest\Source\Core\CoreServices\ATEServices\UnitConvert.h
//	C:\AXITest\Source\Core\CoreServices\ATEServices\UnitStrings.cpp
// No need to Multiply min and max values to scale for display

SITE_PASS_MASK MLog_Datalog  (const char* mvpName, 
                              unsigned int testNumberIn,				// testNumber.subTestNumber 
                              const char* testName, 
                              double mvpResultData[],					// array with all the data for each pin of each site
                              char *units, 
                              double minLimit, double maxLimit, 
                              const char* comment,						// For STDF: not used
                              const char* binName)						// For STDF only.
{
	char    testStr[256]={0};
	int     passMask   = 0;	// bit mask for each site, default is all sites fail
	int     sitePass   = 1;	// 1 = pass 0 = fail
	int     siteFail   = 1;	// 1 = fail 0 = pass
	UINT    siteIndex  = 0;	// 0 based
	int	    siteNumber = 1;	// 1 based 

	UINT	numPinsPerSite = TevTester_NumberOfPins(mvpName);	// Pins per site
	UINT    numSites = TevTester_NumberOfSites();
	UINT	testNumber;
	const char *mvp_Name = {0};
	const char *pin_Name = {0};
	double	result = 0.0;

	UINT	pin = 0;
    UINT    dlogFlags = 0;
	
	const char* groupOrder = TevExec_GetPinGroupDataOrder();

//	unsigned int testNumber = MLog_GetTestNumber();	// Coming soon !  04/05/12 JC // gets major testNumber that was set with MLog_Header()

	
	if (testNumberIn == AUTONUM)
	{
		testNumberIn = gSubTestNumber;
		gSubTestNumber++;		
	}

	if (minLimit != INVALID_FLOAT)
		dlogFlags |= DLOG_PARM_MVAL_EQ_LOLIM_PASS;
	if (maxLimit != INVALID_FLOAT)
		dlogFlags |= DLOG_PARM_MVAL_EQ_HILIM_PASS;
	if (minLimit == INVALID_FLOAT)
		minLimit = -1.7976931348623158e+308;
	if (maxLimit == INVALID_FLOAT)
		maxLimit =  1.7976931348623158e+308;

		
	if (numPinsPerSite == 0)	// Datalogging single result not associated with a Pin or Group.
	{
		// Build testName string
		BuildTestNameString(mvpName, testName, testStr,sizeof(testStr), 0);
		
		// Send the Datalog
		MLOG_ForEachEnabledSite(siteIndex)
		{
			if (MLog_1ofN(siteIndex))
			{
				siteNumber = siteIndex + 1;
				result = MVP_GetResult(mvpName, mvpResultData, siteIndex, pin);
				if (IMMED_MODE)
				{
					UINT bin = TevExec_TestBinNumber (binName); // Use Bin Name to Disqualify bin int IMMED_MODE
					if (gBinningMode == MLOG_DIRECT_BINNING)
					{
						UINT currentBin = TevData_GetBin(siteIndex);
						if (currentBin == 1)
							TevData_SetBinResult(bin);  // only bin 1st failure
					}
					else
					{
						TevData_DisqualifyBin (bin - 1); 
					}

					sitePass = TevData_LogLimits  (testNumberIn, testStr, result, minLimit, maxLimit, units, comment, siteNumber);	// Tev
					
					if (gFastBinning && !sitePass) // Failure
					{	// only disable if fail bin
						BOOL type;
						TevExec_TestBinType(bin, &type);
						if (!type) // failBin
							gDisableThisSite[siteIndex] = 1;							
					}
					if (sitePass) // Pass
						passMask |= 1 << siteIndex;
				}
				if (STDF)
				{
					siteFail = Dlog_Test (siteNumber, testNumberIn, testStr,  result, minLimit, maxLimit, units, "%.3f", dlogFlags);
					if (siteFail == DLOG_FAILED)
					{
						MLog_SetBinResult (siteIndex, binName);
					}
					else
						passMask |= 1 << siteIndex;

					if (gFastBinning && siteFail == DLOG_FAILED)
					{   // only disable if fail bin
						BOOL type;
						UINT bin = TevExec_TestBinNumber (binName);
						TevExec_TestBinType(bin, &type);
						if (!type) // failBin
							gDisableThisSite[siteIndex] =  1;
						//TevTester_DisableSite (siteNumber);
					}
					
				}
			}
			else
			{  //1ofN
				MLOG_ForEachEnabledSite(siteIndex)
				{
					result = MVP_GetResult(mvpName, mvpResultData, siteIndex, pin);
					sitePass = MLog_CheckLimits (result, minLimit, maxLimit);
					if (gFastBinning && !sitePass)  // Failure
					{	// only disable if fail bin
						UINT bin = TevExec_TestBinNumber (binName);
						BOOL type;
						TevExec_TestBinType(bin, &type);
						if (!type) // failBin
							gDisableThisSite[siteIndex] = 1;
					}
					if (!sitePass) // Failure
					{ // Bin Device of Failure
						UINT bin = TevExec_TestBinNumber (binName);
						if (bin > gSoftBin[siteIndex])
							gSoftBin[siteIndex] = bin;
					}
				}
			}
		}
	}
	else
	{
		for (pin = 0; pin < numPinsPerSite; pin++)
		{
			// Build testName string
			BuildTestNameString(mvpName, testName, testStr, sizeof(testStr), pin);
			
			testNumber = testNumberIn + pin; 
			
			// Send the Datalog
			MLOG_ForEachEnabledSite(siteIndex)
			{
				if (MLog_1ofN(siteIndex))
				{	
					siteNumber = siteIndex + 1;
				
					result = MVP_GetResult(mvpName, mvpResultData, siteIndex, pin);
					if (IMMED_MODE)
					{
						UINT bin = TevExec_TestBinNumber (binName); // Use Bin Name to Disqualify bin int IMMED_MODE
						if (gBinningMode == MLOG_DIRECT_BINNING)
						{
							UINT currentBin = TevData_GetBin(siteIndex);
							if (currentBin == 1)
								TevData_SetBinResult(bin);  // only bin 1st failure
						}
						else
						{
							TevData_DisqualifyBin (bin - 1); 
						}
						sitePass = TevData_LogLimits  (testNumber, testStr, result, minLimit, maxLimit, units, comment, siteNumber);	// Tev
						BOOL type;
						
						if (gFastBinning && !sitePass)  // Failure
						{   // only disable if fail bin
							UINT bin = TevExec_TestBinNumber (binName);
							TevExec_TestBinType(bin, &type);
							if (!type) // failBin
								gDisableThisSite[siteIndex] = 1; 							
						}
						if (sitePass) // Pass
							passMask |= 1 << siteIndex;
					}
					if (STDF)
					{
						siteFail = Dlog_Test (siteNumber, testNumber, testStr,  result, minLimit, maxLimit, units, "%.3f", dlogFlags);
						if (siteFail == DLOG_FAILED)
						{
							MLog_SetBinResult (siteNumber, binName);
						}
						else
							passMask |= 1 << siteIndex;
						if (gFastBinning && siteFail == DLOG_FAILED)
						{   // only disable if fail bin
							BOOL type;
							UINT bin = TevExec_TestBinNumber (binName);
							bin = TevExec_TestBinNumber (binName);
							TevExec_TestBinType(bin, &type);
							if (!type) // failBin
								gDisableThisSite[siteIndex] = 1;
						}

					}
				}
				else
				{	//1ofN
					MLOG_ForEachEnabledSite(siteIndex)
					{
						result = MVP_GetResult(mvpName, mvpResultData, siteIndex, pin);
						sitePass = MLog_CheckLimits (result, minLimit, maxLimit);
						if (gFastBinning && !sitePass)  //Failure
						{	// only disable if fail bin
							UINT bin = TevExec_TestBinNumber (binName);
							BOOL type;
							TevExec_TestBinType(bin, &type);
							if (!type) // failBin
								gDisableThisSite[siteIndex] = 1;							
						}

						if (!sitePass) // Failure
						{ // Bin Device if Failure
							UINT bin = TevExec_TestBinNumber (binName);
							if (bin > gSoftBin[siteIndex])
								gSoftBin[siteIndex] = bin;
						}
					}
				}
			}
		}
	}
	if (gStopOnFirstFailure)
	{
		MLOG_ForEachEnabledSite(siteIndex)
		{
			if (gDisableThisSite[siteIndex])
			{
				siteNumber = siteIndex + 1;
				MLog_UserDisableSite(siteNumber);		 // Must be defined in user code
				TevTester_DisableSite (siteNumber);
			}
		}
	}
					
	return passMask;
}

BOOL MLog_GetTestFailed(UINT siteNumber)
{
	UINT siteIndex = siteNumber - 1;

	if (gSiteTestFailedMask & (1 << siteIndex))
		return 1;

	return 0;
}

void MLog_ClearTestFailed(void)
{
	gSiteTestFailedMask =0;
}

void MLog_SetTestFailed(UINT siteNumber)
{
	if (IMMED_MODE)
		TevData_SetFailed (siteNumber);
	
	UINT siteIndex = siteNumber - 1;
	gSiteTestFailedMask |= 1 << siteIndex;	
}

SITE_PASS_MASK  MLog_LogState (const char *mvpName, unsigned int testNumberIn, const char* testName, double mvpResultData[],
                               char *units, const char* comment, const char* binName)
{
	char    testStr[256]={0};
	int     passMask   = 0;	// bit mask, one bit per site
	int     sitePass   = 1;	// 1 = pass  0 = fail
	int		siteFail   = 0;	// 1 = fail  0 = pass
	UINT    siteIndex  = 0;	// 0 based
	int	    siteNumber = 1;	// 1 based 

	UINT    numSites = TevTester_NumberOfSites();
	UINT	numPinsPerSite = TevTester_NumberOfPins(mvpName);	// Pins per site
	UINT	testNumber;
	const char *mvp_Name = {0};
	const char *pin_Name = {0};
	double	result = 0.0;
	UINT	channel = 0;
	UINT	pin = 0;
    UINT    dlogFlags = 0;
	
    double  NoMinLimit = -1.7976931348623158e+308;
    double  NoMaxLimit =  1.7976931348623158e+308;

   //	unsigned int testNumber = MLog_GetTestNumber();	// Comming soon !  04/05/12 JC // gets major testNumber that was set with MLog_Header()

	if (testNumberIn == AUTONUM)
	{
		testNumberIn = gSubTestNumber;
		gSubTestNumber++;		
	}

	if (numPinsPerSite == 0)	// Datalogging single result not associated with a Pin or Group.
	{
		siteNumber = siteIndex + 1;

		// Build testName string
		BuildTestNameString(mvpName, testName, testStr, sizeof(testStr), 0);
	
		// Send the Datalog
		MLOG_ForEachEnabledSite(siteIndex)
		{
			if (MLog_1ofN(siteIndex))
			{
				siteNumber = siteIndex + 1;
			
				if (MLog_GetTestFailed(siteNumber))
				{
					dlogFlags = DLOG_TEST_FAILED;
				}
				else
				{
					dlogFlags = DLOG_TEST_PASSED;
				}

				result = MVP_GetResult(mvpName, mvpResultData, siteIndex, channel);
				if (IMMED_MODE)
				{
					UINT bin = TevExec_TestBinNumber (binName); // Use Bin Name to Disqualify bin int IMMED_MODE
					if (gBinningMode == MLOG_DIRECT_BINNING)
					{
						UINT currentBin = TevData_GetBin(siteIndex);
						if (currentBin == 1)
							TevData_SetBinResult(bin);  // only bin 1st failure
					}
					else
					{
						TevData_DisqualifyBin (bin - 1);
					}
					sitePass = TevData_LogState (testNumberIn, testStr, result, units, comment, siteNumber);	// Tev

					if (gFastBinning && !sitePass) // Failure
					{	// only disable if fail bin
						BOOL type;
						UINT bin = TevExec_TestBinNumber (binName);
						TevExec_TestBinType(bin, &type);
						if (!type) // failBin
							gDisableThisSite[siteIndex] = 1;
					}
					if (sitePass) // Pass
						passMask |= 1 << siteIndex;
				}
				if (STDF)
				{
					siteFail = Dlog_Test (siteNumber, testNumberIn, testStr,  result, NoMinLimit, NoMaxLimit, units, "%.3f", dlogFlags);
					if (siteFail == DLOG_FAILED)
					{
						MLog_SetBinResult (siteIndex, binName);
					}
					else
						passMask |= 1 << siteIndex;

					if (gFastBinning && sitePass == DLOG_FAILED)
					{   // only disable if fail bin
						BOOL type;
						UINT bin = TevExec_TestBinNumber (binName);
						TevExec_TestBinType(bin, &type);
						if (!type) // failBin
							gDisableThisSite[siteIndex] = 1;
					}

				}
			}
			else
			{  //1ofN
				MLOG_ForEachEnabledSite(siteIndex)
				{
					sitePass = MLog_GetTestFailed(siteNumber);
					if (gFastBinning && !sitePass) // Failure
					{	// only disable if fail bin
						UINT bin = TevExec_TestBinNumber (binName);
						BOOL type;
						TevExec_TestBinType(bin, &type);
						if (!type) // failBin
							gDisableThisSite[siteIndex] = 1;
					}
					if (!sitePass) // Failure
					{// Bin Device if Failure
						UINT bin = TevExec_TestBinNumber (binName);
						if (bin > gSoftBin[siteIndex])
							gSoftBin[siteIndex] = bin;
					}
				}
			}
		}
	}
	else
	{
		for (pin = 0; pin < numPinsPerSite; pin++)
		{
			// Build testName string
			BuildTestNameString(mvpName, testName, testStr, sizeof(testStr), pin);
 			
			testNumber = testNumberIn + pin;

			// Send the Datalog
			MLOG_ForEachEnabledSite(siteIndex)
			{
				if (MLog_1ofN(siteIndex))
				{	
					siteNumber = siteIndex + 1;
				
					if (MLog_GetTestFailed(siteNumber))
					{
						dlogFlags = DLOG_TEST_FAILED;
					}
					else
						dlogFlags = DLOG_TEST_PASSED;

					result = MVP_GetResult(mvpName, mvpResultData, siteIndex, channel);
					if(IMMED_MODE)
					{
						UINT bin = TevExec_TestBinNumber (binName); // Use Bin Name to Disqualify bin int IMMED_MODE
						if (gBinningMode == MLOG_DIRECT_BINNING)
						{
							UINT currentBin = TevData_GetBin(siteIndex);
							if (currentBin == 1)
								TevData_SetBinResult(bin);  // only bin 1st failure
						}
						else
						{
							TevData_DisqualifyBin (bin - 1);
						}
						sitePass = TevData_LogState (testNumber, testStr, result, units, comment, siteNumber);	// Tev

						if (gFastBinning && !sitePass) // Failure
						{   // only disable if fail bin
							BOOL type;
							UINT bin = TevExec_TestBinNumber (binName);
							TevExec_TestBinType(bin, &type);
							if (!type) // failBin
								gDisableThisSite[siteIndex] = 1;	
						}
						if (sitePass) // Pass
							passMask |= 1 << siteIndex;
					}
					if (STDF)
					{
						siteFail = Dlog_Test (siteNumber, testNumber, testStr,  result, NoMinLimit, NoMaxLimit, units, "%.3f", dlogFlags);  // Alternative
						if (siteFail == DLOG_FAILED || dlogFlags == DLOG_TEST_FAILED)
						{
							MLog_SetBinResult (siteNumber, binName);
						}
						else
							passMask |= 1 << siteIndex;

						if (gFastBinning && siteFail == DLOG_FAILED)
						{   // only disable if fail bin
							BOOL type;
							UINT bin = TevExec_TestBinNumber (binName);
							bin = TevExec_TestBinNumber (binName);
							TevExec_TestBinType(bin, &type);
							if (!type) // failBin
								gDisableThisSite[siteIndex] = 1;
						}

					}
				}
				else
				{	//1ofN
					MLOG_ForEachEnabledSite(siteIndex)
					{
						siteFail = MLog_GetTestFailed(siteNumber);
						if (gFastBinning && siteFail)
						{	// only disable if fail bin
							UINT bin = TevExec_TestBinNumber (binName);
							BOOL type;
							TevExec_TestBinType(bin, &type);
							if (!type) // failBin
								gDisableThisSite[siteIndex] = 1;
						}

						if (siteFail) // Failure
						{ // Bin Device if Failure
							UINT bin = TevExec_TestBinNumber (binName);
							if (bin > gSoftBin[siteIndex])
								gSoftBin[siteIndex] = bin;
						}
					}
				}
			}
		}
	}
	MLog_ClearTestFailed();  // Clear Test Fail Flag

	if (gStopOnFirstFailure)
	{
		MLOG_ForEachEnabledSite(siteIndex)
		{
			if (gDisableThisSite[siteIndex])
			{
				siteNumber = siteIndex + 1;
				MLog_UserDisableSite(siteNumber);		 // Must be defined in user code
				TevTester_DisableSite (siteNumber);
			}
		}
	}
	
    return passMask;
}


int  MLog_LogSiteState (const char *mvpName, unsigned int testNumberIn, const char* testName, double result,
                               char *units, const char* comment, const char* binName, int siteNumber)
{
	char    testStr[256]={0};
	int     sitePass   = 1;	// 1 = pass  0 = fail
	int		siteFail   = 0;	// 1 = fail  0 = pass
	int		siteIndex;
	const char *mvp_Name = {0};
	const char *pin_Name = {0};
	
	UINT	channel = 0;
	UINT	pin = 0;
	int		numberMode = 0;	// 0: normal, -2: AUTONUM
    UINT    dlogFlags = 0;
	
    double  NoMinLimit = -1.7976931348623158e+308;
    double  NoMaxLimit =  1.7976931348623158e+308;

   //	unsigned int testNumber = MLog_GetTestNumber();	// Comming soon !  04/05/12 JC // gets major testNumber that was set with MLog_Header()

	if (testNumberIn== AUTONUM)		// AUTONUM
		numberMode = AUTONUM;


	// Build testName string
	BuildTestNameString(mvpName, testName, testStr, sizeof(testStr), 0);
	
	siteIndex = siteNumber-1;
	// Send the Datalog		
	if (MLog_1ofN(siteIndex))
	{	
		if (MLog_GetTestFailed(siteNumber))
		{
			dlogFlags = DLOG_TEST_FAILED;
		}
		else
		{
			dlogFlags = DLOG_TEST_PASSED;
		}
				
		if (IMMED_MODE)
		{
			UINT bin = TevExec_TestBinNumber (binName); // Use Bin Name to Disqualify bin int IMMED_MODE
			if (gBinningMode == MLOG_DIRECT_BINNING)
			{
				UINT currentBin = TevData_GetBin(siteIndex);
				if (currentBin == 1)
					TevData_SetBinResult(bin);  // only bin 1st failure
			}
			else
			{
				TevData_DisqualifyBin (bin - 1);
			}
					
			sitePass = TevData_LogState (testNumberIn, testStr, result, units, comment, siteNumber);	// Tev

			if (gFastBinning && !sitePass) // Failure
			{	// only disable if fail bin
				BOOL type;
				UINT bin = TevExec_TestBinNumber (binName);
				TevExec_TestBinType(bin, &type);
				if (!type) // failBin
					gDisableThisSite[siteIndex] = 1;
			}
			
		}
		if (STDF)
		{
			siteFail = Dlog_Test (siteNumber, testNumberIn, testStr,  result, NoMinLimit, NoMaxLimit, units, "%.3f", dlogFlags);
			if (siteFail == DLOG_FAILED)
			{
				sitePass = 0;
				MLog_SetBinResult (siteIndex, binName);
			}
			
			if (gFastBinning && siteFail == DLOG_FAILED)
			{   // only disable if fail bin
				BOOL type;
				UINT bin = TevExec_TestBinNumber (binName);
				TevExec_TestBinType(bin, &type);
				if (!type) // failBin
					gDisableThisSite[siteIndex] = 1;
			}

		}
	}
	else
	{  //1ofN
		MLOG_ForEachEnabledSite(siteIndex)
		{
			sitePass = MLog_GetTestFailed(siteNumber);
			if (gFastBinning && !sitePass) // Failure
			{	// only disable if fail bin
				UINT bin = TevExec_TestBinNumber (binName);
				BOOL type;
				TevExec_TestBinType(bin, &type);
				if (!type) // failBin
					gDisableThisSite[siteIndex] = 1;
			}
			if (!sitePass) // Failure
			{// Bin Device if Failure
				UINT bin = TevExec_TestBinNumber (binName);
				if (bin > gSoftBin[siteIndex])
					gSoftBin[siteIndex] = bin;
			}
		}
	}
	
	

	MLog_ClearTestFailed();  // Clear Test Fail Flag

	if (gStopOnFirstFailure)
	{
		MLOG_ForEachEnabledSite(siteIndex)
		{
			if (gDisableThisSite[siteIndex])
			{
				siteNumber = siteIndex + 1;
				MLog_UserDisableSite(siteNumber); // Must be defined in user code		
				TevTester_DisableSite (siteNumber);
			}
		}
	}
	
   UINT passMask = sitePass << siteIndex;
   return passMask;
}


void MLog_CreateNewLot (char* lotId)
{
	char	dlogFileName[256]={0};
	DWORD   stdfStreamHandle;

	SYSTEMTIME	sysTime;
	char	path[256]={0};
	char timeStamp[64];

	const char *dlogPath;

	const char *loginMode = TevTester_GetRunInfoString("Login Mode");
	if (strcmp(loginMode,"Operator")== 0)
	{ /// Use open exec dlog path
		dlogPath = TevExec_GetDlogPath();
		strcpy_s (path, sizeof (path), dlogPath);
	}
	else
	{  // put into Test Program Directory
		if (GetTestFunctionDllPath(path, sizeof (path)))
			strcat_s (path, sizeof (path), "\\Dlog");
		else
			strcpy_s (path, sizeof (path), "Dlog");
		if (! FileExists(path)) // Directory
			CreateDirectory (path, NULL);
	}
	GetLocalTime(&sysTime);
	sprintf_s(timeStamp, sizeof (timeStamp), 
		"%0.2u-%0.2u-%0.4u_%0.2u%0.2u%0.2u",
		sysTime.wMonth, sysTime.wDay, sysTime.wYear,\
		sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
	TevTester_SetRunInfoString("LotTimestamp", timeStamp);
	gSite1Count = 1;	
	if (IMMED_MODE)
	{
		sprintf_s(dlogFileName, sizeof (dlogFileName), "%s\\%s_%s_%s.dlg", path, gApplicationName, lotId, timeStamp);

		TevData_SetFileName(dlogFileName);
		TevData_CreateNewLot(lotId);
	}
	
	if(STDF)
	{
		BOOL initRequired = (Dlog_GetInitStatus(DLOG_INIT_QUIET) != DLOG_SUCCESS);
		BOOL openStdf = TRUE;
		BOOL openDat  = TRUE;

		if (initRequired)
		{   // First run
			openStdf = TRUE;
			openDat  = TRUE;
			Dlog_Initialize (gNumSites, DLOG_DEFAULT_OPTIONS, NULL);
		}
		else if (Dlog_IsLotStarted())
		{   // Change Lot
			Dlog_EndLot (NULL);
			//openStdf = Dlog_IsStdfFileStreamOpen();
			//openDat  = Dlog_IsTextFileStreamOpen();
			Dlog_CloseAllFileStreams();
			//openStdf = Dlog_IsStdfFileStreamOpen();
			//openDat  = Dlog_IsTextFileStreamOpen();
		
		}

		Dlog_SetLotId(lotId);

		//DWORD txtStreamHandle;
		if (openStdf)
		{
			sprintf_s(dlogFileName, sizeof (dlogFileName), "%s\\%s_%s_%s.stdf", path, gApplicationName, lotId, timeStamp);
			Dlog_OpenStdfFileStream(&stdfStreamHandle, dlogFileName, 0, NULL);
		//	sprintf_s(dlogFileName, sizeof (dlogFileName), "%s\\%s_%s_%s.txt", path, gApplicationName, lotId, timeStamp);
		//	Dlog_OpenTextFileStream(&txtStreamHandle, dlogFileName, 0, NULL);
			Dlog_SetBaseSiteIndex(1);
		}
		//Sleep (2000);

		UINT tmpTime;
		Dlog_GetTime        (&tmpTime);
		Dlog_SetSetupTime   (tmpTime);
		Dlog_SetStartTime   (tmpTime);
		Dlog_SetStationNum  (1);
		Dlog_SetLotId       (lotId);
		Dlog_SetPartType    (gApplicationName);
		Dlog_SetJobName     (gApplicationName);
		Dlog_SetJobRev      ("0.1");
		char nodeName[MAX_COMPUTERNAME_LENGTH + 1];
		DWORD characterCount = MAX_COMPUTERNAME_LENGTH + 1;
		if (GetComputerName (nodeName, &characterCount))
			Dlog_SetNodeName (nodeName);   // Computer name
		Dlog_SetOperatorName (TevTester_GetRunInfoString("Operator"));
		Dlog_SetExecutiveType("OpenExec");
		Dlog_SetExecutiveVersion (TevTester_GetMVPEnvironmentVersion());
		#ifdef _DEBUG
			Dlog_SetModeCode    ('D');
		#else
			Dlog_SetModeCode    ('P');
		#endif
		Dlog_BeginLot(NULL);
	}
}

void MLog_OnStartDut ()
{
	UINT dlogSites;
	UINT dutNumbers[32];

	IMMED_MODE = TevTester_GetRunInfoBoolean("Immediate Mode Datalog");	
    STDF = TevTester_GetRunInfoBoolean("STDF Mode Datalog");

	if (!IMMED_MODE && !STDF)
	{
		if (FORCE_IMMED_MODE_DLOG)
		{
			IMMED_MODE = 1;
			TevTester_SetRunInfoBoolean("Immediate Mode Datalog", IMMED_MODE);
		}
		else
		{
			STDF = 1;
			TevTester_SetRunInfoBoolean("STDF Mode Datalog", STDF);
		}
	}
	
  	gSiteTestFailedMask = 0;
	dlogSites = 0;
	int inc = 0;
	MLOG_ForEachSite(siteIndex)
	{
		if (TevTester_IsSiteActive(siteIndex+1))
		{
			if(MLog_1ofN(siteIndex))
			{
				dlogSites |= 1 << siteIndex;
				dutNumbers[siteIndex] = gSite1Count+inc;
				inc++;
			}
		}
	}

	if(IMMED_MODE)
	{
		if (dlogSites)
		{
			if (TevTester_GetRunInfoBoolean("Prompt_SN"))
			{
				TevData_StartDuts(gDutSerialNumbers, dlogSites);
				gSite1Count = gDutSerialNumbers[0];
			}
			else
			{
				TevData_StartDuts(dutNumbers, dlogSites);
			}
		}
		
	}
		
	if (STDF)
	{
		if (dlogSites)
		{
			Dlog_BeginBatch();
			
			MLOG_ForEachSite(siteIndex)
			{
				UINT siteNumber = siteIndex + 1;
				int inc = 0;
				if (TevTester_IsSiteActive(siteNumber))
				{	
					if(MLog_1ofN(siteIndex))
					{
						if (TevTester_GetRunInfoBoolean("Prompt_SN"))
						{
							if (gDutSerialNumbers[siteIndex] == AUTONUM)
								Dlog_BeginFlow(siteNumber, gSite1Count+siteIndex, NULL);	// Site, SerialNumber, TevData_StartDut
							else
								Dlog_BeginFlow(siteNumber, gDutSerialNumbers[siteIndex], NULL);	// Site, SerialNumber, TevData_StartDut
						}
						else
						{
							Dlog_BeginFlow(siteNumber, gSite1Count+inc, NULL);	// Site, SerialNumber, TevData_StartDut
							inc++;
						}
					}
				}
			}
		}
	} 
	
	// for 1ofN filtering
	MLOG_ForEachEnabledSite(siteIndex)
	{
		gSoftBin[siteIndex] = 1;
		gDisableThisSite[siteIndex] = 0;

	}
			
}

void MLog_OnEndDut ()
{
	static int count = 1;
	BOOL endBatch = false, endDut = false;	
	
	if(IMMED_MODE)
	{
		MLOG_ForEachSite(siteIndex)
	 	{
			int siteNumber = siteIndex + 1;
			if (TevTester_IsSiteActive(siteNumber))
			{
				if(MLog_1ofN(siteIndex))
				{
					endDut = true;
				}
			}
		}
		if (endDut)
			TevData_EndDut();
	}

	if (STDF)
	{
		MLOG_ForEachSite(siteIndex)
		{
			char bin1Name[32];
			int softBin;
			int siteNumber = siteIndex + 1;
			if (TevTester_IsSiteActive(siteNumber))
			{
				if(MLog_1ofN(siteIndex))
				{
					Dlog_GetSoftBin(siteNumber, &softBin);
					if (softBin == 0)
					{
						TevExec_TestBinName(1, bin1Name, sizeof(bin1Name));					
						Dlog_SetBinResult(siteNumber, bin1Name);
					}
															
					Dlog_EndFlow(siteNumber, NULL); 
					endBatch = true; 
				}
			}
		}
		if (endBatch)
			Dlog_EndBatch();

	}
	// for 1ofN filtering
	MLOG_ForEachSite(siteIndex)
	{
		if (TevTester_IsSiteActive(siteIndex+1))
		{
			gSite1Count++;
		}
	}
	
}


UINT MLog_GetBin (UINT siteNumber)
{
	int softBin = 0;
	int softBin2 = 0;
	UINT siteIndex = siteNumber - 1;
	if(MLog_1ofN(siteIndex))
	{
		if(IMMED_MODE)
		{
			softBin = TevData_GetBin(siteIndex);
		}
		else if (STDF)
		{
			Dlog_GetSoftBin(siteNumber, &softBin);
			if (softBin == 0)
				softBin = 1;
		}
	}
	else
	{
		softBin = gSoftBin[siteIndex];
	}
	 return softBin;
}

void MLog_OnProgramLoad (const char* name)
{
	
	gNumSites = TevTester_NumberOfSites();
	strcpy_s (gApplicationName, sizeof (gApplicationName), name);

	MLog_Set1ofNFilter(false, 0);

	if(IMMED_MODE)
	{
		TevData_Initialize(gNumSites); 
		TevData_SetMode("Immediate",0xFFFF);
	}
	if (STDF)
	{
		BOOL initRequired = (Dlog_GetInitStatus(DLOG_INIT_QUIET) != DLOG_SUCCESS);
		BOOL openStdf = TRUE;
		BOOL openDat  = TRUE;

		if (initRequired)
		{   // First run
			openStdf = TRUE;
			openDat  = TRUE;
			Dlog_Initialize (gNumSites, DLOG_DEFAULT_OPTIONS, NULL);
		}
	}
	TevTester_RunInfoAdd       ("Immediate Mode Datalog","Boolean", NULL);
    TevTester_SetRunInfoBoolean("Immediate Mode Datalog", TRUE);	
 	TevTester_RunInfoAdd       ("STDF Mode Datalog","Boolean", NULL);
    TevTester_SetRunInfoBoolean("STDF Mode Datalog", TRUE);	
	TevTester_RunInfoAddHidden ("Login Mode","String", "");
    TevTester_SetRunInfoString ("Login Mode", "Development");	 

}

void MLog_OnProgramUnload ()
{
	*gApplicationName = '\0';
	gNumSites = 0;

	if(IMMED_MODE)
	{
		
	}
	if (STDF)
	{
		if (Dlog_GetInitStatus(DLOG_INIT_QUIET) == DLOG_SUCCESS)
		{
			DLOG_STATUS rc;
			if (Dlog_IsLotStarted())
			{
				// End the lot
				rc = Dlog_EndLot(NULL);
				Dlog_CloseAllFileStreams();
			}

			// Shutdown datalogger
			rc = Dlog_Shutdown(DLOG_DEFAULT_OPTIONS, NULL);
		}
	}	
}

void MLog_SetBinResult (int siteNumber, const char* binName)
{
	int currentBin, thisBin, sts;
	
	thisBin = TevExec_TestBinNumber (binName);
	
	if (STDF)
	{
		 sts = Dlog_GetSoftBin(siteNumber, &currentBin);
		 if (gBinningMode == MLOG_DISQ_BINNING)
		 {
			 if (thisBin > currentBin)
			 { 
				sts |= Dlog_SetBinResult(siteNumber, binName);
			
			 }
		 }
		 else
		 {
			 if (currentBin == 0)
				sts |= Dlog_SetBinResult(siteNumber, binName);  // only bin 1st failure	
		 }
	}
    // performed directly with TevData_LogLimits or LogState.
}


/*
char* unitStrings [UNIT_LIST_SIZE] = 
{
" ",
"V",
"TV",
"GV",
"MV",
"kV",
"mV",
"uV",
"nV",
"pV",
"fV",
"Vrms",
"TVrms",
"GVrms",
"MVrms",
"kVrms",
"mVrms",
"uVrms",
"nVrms",
"pVrms",
"fVrms",
"Vpp",
"TVpp",
"GVpp",
"MVpp",
"kVpp",
"mVpp",
"uVpp",
"nVpp",
"pVpp",
"fVpp",
"Vpk",
"TVpk",
"GVpk",
"MVpk",
"kVpk",
"mVpk",
"uVpk",
"nVpk",
"pVpk",
"fVpk",
"A",
"TA",
"GA",
"MA",
"kA",
"mA",
"uA",
"nA",
"pA",
"fA",
"Arms",
"TArms",
"GArms",
"MArms",
"kArms",
"mArms",
"uArms",
"nArms",
"pArms",
"fArms",
"App",
"TApp",
"GApp",
"MApp",
"kApp",
"mApp",
"uApp",
"nApp",
"pApp",
"fApp",
"Apk",
"TApk",
"GApk",
"MApk",
"kApk",
"mApk",
"uApk",
"nApk",
"pApk",
"fApk",
"W",
"TW",
"GW",
"MW",
"kW",
"mW",
"uW",
"nW",
"pW",
"fW",
"Hz",
"THz",
"GHz",
"MHz",
"kHz",
"mHz",
"uHz",
"nHz",
"pHz",
"fHz",
"Ohms",
"TOhms",
"GOhms",
"MOhms",
"kOhms",
"mOhms",
"uOhms",
"nOhms",
"pOhms",
"fOhms",
"S",            // Siemens, Unit of Conductance in SI
"F",
"TF",
"GF",
"MF",
"kF",
"mF",
"uF",
"nF",
"pF",
"fF",
"H",
"TH",
"GH",
"MH",
"kH",
"mH",
"uH",
"nH",
"pH",
"fH",
"deg C",
"deg F",
"rad",
"deg",
"rad/s",
"deg/s",
"M",
"mM",
"cM",
"in",
"ft",
"M/s",
"ft/s",
"s",
"ms",
"us",
"ns",
"ps",
"fs",
"min",
"cy",
"cy",
"psi",
"bar",
"%",
"ppm",
"dB",
"dBm",
"dBm/Hz",
"dBw",
"dBc",
"dBc/Hz",
"mdeg",
"udeg",
"mrad",
"urad",
"INT",
"HEX16",
"HEX32",
"BOOL",
"OC",
"CO",
"SEPARATOR",
"REV16",
"V/us",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
""
};

*/
