//==============================================================================
//
// Title:       DataLogACS.cpp
// Purpose:     This module provides an API interface to ACS - TEV STDF data services.
//              Both fast binning and disqualify bin logic is implemented here.
//
// Copyright (c) 2011 Test Evolution Corp.
// All Rights Reserved
// Eng	Rev	Date______  Description_____________________________________________
//  KS	 1	02/28/2012	Initial issue 
//  JC   2  04/06/2012  Cleanup
//  JC   3  05/29/2012  Fix with testNum = subTestNumberIn + channel;
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
// This state persists between runs of a test program � once set, 
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
#include "AXIData.h"


#define MAX_STDF_SOFT_BINS 32

// User Functions ==============================================================
UINT gDataOrder = 0;			// 0:SITE_MAJOR(default), 1:PIN_MAJOR
static int  gNumSites             = 0;
static BOOL gFastBinning          = FALSE;
static BOOL gUseBinDisqualification = TRUE;		// STDF needs TRUE, TEV needs FALSE
static char gApplicationName [64] = {'\0'};
static UINT gSiteFailedMask       = 0;
static UINT gPresetBin            = 0;
static UINT gSiteCurrentBins [MAX_STDF_SOFT_BINS];
static void InitializeSiteBins (UINT startBin)
{
    for (int i = 0 ; i < MAX_STDF_SOFT_BINS ; i++)
        gSiteCurrentBins[i] = startBin;
}

static void DisqualifyPresetBin (UINT siteNumber, BOOL pass)
{
    if (! pass && gPresetBin > 0)
    {
        UINT siteIndex = siteNumber - 1;
        if (siteIndex < MAX_STDF_SOFT_BINS)
        {
            UINT currentBin = gSiteCurrentBins[siteIndex];
            if (gPresetBin >= currentBin)
                gSiteCurrentBins[siteIndex] = gPresetBin;
        }
    }
}

static const char* GetDisqualifiedBinName (UINT siteNumber, char* name, size_t nameSize)
{
    *name = '\0';
    UINT siteIndex = siteNumber - 1;
    if (siteIndex < MAX_STDF_SOFT_BINS)
        TevExec_TestBinName (gSiteCurrentBins[siteIndex], name , nameSize);
    return name;
}

static void DisqualifyBinName (const char* binName)
{
    UINT bin = TevExec_TestBinNumber (binName);
    if (bin)
        gPresetBin = bin;
}

void MLog_SetFastBinning (BOOL fastBinning)
{
    gFastBinning = fastBinning;
}

void MLog_SetBinDisqualification (BOOL binDisqualification)
{
    gUseBinDisqualification = binDisqualification;
}

void MVP_SetFastBinning(UINT state)
{
	gFastBinning = gFastBinning;
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
//	return resultData[channel + numChanPerSite*(site-1)];
}

void MLog_Header (unsigned int testNumber, const char* testName, const char* comment)
{
    for (int siteNumber = 1 ; siteNumber <= gNumSites ; siteNumber++)
    {
        if(TevTester_IsSiteActive (siteNumber) && TevTester_IsSiteEnabled (siteNumber))
        {
            Dlog_BeginSequence (siteNumber, testName, comment);
        }
    }
}



// See these for information on unit strings.
// ~AXITest\Doc\Data Log Wrapper API Spec.doc
//	C:\AXITest\Source\Core\CoreServices\ATEServices\UnitConvert.h
//	C:\AXITest\Source\Core\CoreServices\ATEServices\UnitStrings.cpp
// No need to Multiply min and max values to scale for display

SITE_PASS_MASK MLog_Datalog  (const char *mvpName, 
                              unsigned int subTestNumberIn, 
                              const char* testName, 
                              double mvpResultData[],					// array with all the data for each pin of each site
                              char *units, 
                              double minLimit, double maxLimit, 
                              const char* comment,
                              const char* binName)
{
	char    testStr[256]={0};
	int     passMask   = 0;	// bit mask, one bit per site
	int     sitePass   = 1;	// 0=pass,  1=fail
	int	    siteIndex  = 0;	// 0 based
	int	    siteNumber = 1;	// 1 based 
	unsigned int testNum =  subTestNumberIn; 
	UINT	channels = TevTester_NumberOfChannels(mvpName);
	UINT    numSites = TevTester_NumberOfSites();
	UINT	numPinPerSite = TevTester_NumberOfPins(mvpName);	// Pins per site
	UINT	subTestNumber;
	const char *mvp_Name = {0};
	double	result = 0.0;
	UINT	channel = 0;
	int		numberMode = 0;	// 0: normal, -2: AUTONUM
    UINT    dlogFlags = 0;

	const char* groupOrder = TevExec_GetPinGroupDataOrder();

//	unsigned int testNumber = MLog_GetTestNumber();	// Comming soon !  04/05/12 JC // gets major testNumber that was set with MLog_Header()

	if (subTestNumberIn == AUTONUM)		// AUTONUM
		numberMode = AUTONUM;

    if (minLimit != INVALID_FLOAT)
        dlogFlags |= DLOG_PARM_MVAL_EQ_LOLIM_PASS;
    if (maxLimit != INVALID_FLOAT)
        dlogFlags |= DLOG_PARM_MVAL_EQ_HILIM_PASS;
    if (minLimit == INVALID_FLOAT)
        minLimit = -1.7976931348623158e+308;
    if (maxLimit == INVALID_FLOAT)
        maxLimit =  1.7976931348623158e+308;
  
	if (gUseBinDisqualification)
        DisqualifyBinName (binName);

	for (UINT siteIndex = 0; siteIndex < numSites; siteIndex++)
	{
		if (channels == 0)	// Datalogging single result not associated with a Pin or Group.
		{
			siteNumber = siteIndex + 1;

			// Build testName string
			strcpy_s(testStr, sizeof(testStr), testName);
			if (strnlen(mvpName, 1024)>0)
			{
				strcat_s(testStr, sizeof(testStr), " ("    );
				strcat_s(testStr, sizeof(testStr), mvpName );
				strcat_s(testStr, sizeof(testStr), ")"     );
			}

			// Send the Datalog
			if(TevTester_IsSiteActive(siteNumber) && TevTester_IsSiteEnabled(siteNumber))
			{
				result = MVP_GetResult(mvpName, mvpResultData, siteIndex, channel);
				sitePass = Dlog_Test (siteNumber, subTestNumberIn, testStr,  result, minLimit, maxLimit, units, "%.3f", dlogFlags);
				if (sitePass == DLOG_FAILED)
				{
					if (gUseBinDisqualification)
					{
						char disqualifiedBinName [32];
						DisqualifyPresetBin    (siteNumber, FALSE);
						GetDisqualifiedBinName (siteNumber, disqualifiedBinName, sizeof (disqualifiedBinName));
						Dlog_SetBinResult      (siteNumber, disqualifiedBinName);
					}
					else
						Dlog_SetBinResult (siteIndex, binName);
				}
				else
					passMask |= 1 << siteIndex;
				if (gFastBinning && sitePass == DLOG_FAILED)
					TevTester_DisableSite (siteNumber);
			}
		}
		else
		{
			for (channel = 0; channel < numPinPerSite; channel++)
			{
				//	siteIndex  = TevTester_GetChannelSite(mvpName, channel);
				siteNumber = siteIndex + 1;

				// Build testName string
				strcpy_s(testStr, sizeof(testStr), testName);
				strcat_s(testStr, sizeof(testStr), " ("    );
				if (numPinPerSite > 1)
				{	// Add name of *this* pin
				//	mvp_Name = TevTester_GetChannelPinName(mvpName, channel);	// All pins all sites
					mvp_Name = TevTester_GetPinName       (mvpName, channel);	// Per site
					strcat_s(testStr, sizeof(testStr), mvp_Name );
				}
				else
				{	// Use MVPName passed in
					strcat_s(testStr, sizeof(testStr), mvpName );
				}
				strcat_s(testStr, sizeof(testStr), ")"     );

				if (numberMode == AUTONUM)			// AUTONUM
					testNum = channel+1;
				else
					testNum = subTestNumberIn + channel;


				// Send the Datalog
				if(TevTester_IsSiteActive(siteNumber) && TevTester_IsSiteEnabled(siteNumber))
				{
					result = MVP_GetResult(mvpName, mvpResultData, siteIndex, channel);
					sitePass = Dlog_Test (siteNumber, testNum, testStr,  result, minLimit, maxLimit, units, "%.3f", dlogFlags);
					if (sitePass == DLOG_FAILED)
					{
						if (gUseBinDisqualification)
						{
							char disqualifiedBinName [32];
							DisqualifyPresetBin    (siteNumber, FALSE);
							GetDisqualifiedBinName (siteNumber, disqualifiedBinName, sizeof (disqualifiedBinName));
							Dlog_SetBinResult      (siteNumber, disqualifiedBinName);
						}
						else
							Dlog_SetBinResult (siteNumber, binName);
					}
					else
						passMask |= 1 << siteIndex;
					if (gFastBinning && sitePass == DLOG_FAILED)
						TevTester_DisableSite (siteNumber);
				}
			}
		}
	}
    return passMask;
}

void MLog_SetFailed(UINT siteNumber)
{
    UINT siteIndex = siteNumber - 1;
    gSiteFailedMask |= 1 << siteIndex;
}

// The log state function should work by setting the value and limits to invalid, and setting the
// test flags to 0x02 for pass, and 0x82 for fail.  For some reason, Galaxy did not interpret 0x82 as a fail.
// So....
// I implemented the pass fail flag as the value as an alternative algorithm.
// mvpResultData and units are ignored for now.

SITE_PASS_MASK  MLog_LogState (const char *mvpName, unsigned int subTestNumber, const char* testName, double mvpResultData[],
                               char *units, const char* binName)
{
	char    testStr[256]={0};
	int     passMask   = 0;	// bit mask, one bit per site
	int     sitePass   = 1;	// 0=pass,  1=fail
	int	    siteIndex  = 0;	// 0 based
	int	    siteNumber = 1;	// 1 based 
	unsigned int testNum =  subTestNumber; 
	UINT	channels = TevTester_NumberOfChannels(mvpName);
	UINT    numSites = TevTester_NumberOfSites();
	UINT	numPinPerSite = TevTester_NumberOfPins(mvpName);	// Pins per site
	const char *mvp_Name = {0};
	double	result = 0.0;
	UINT	channel = 0;
	int		numberMode = 0;	// 0: normal, -2: AUTONUM
    UINT    dlogFlags = 0;

    double  minLimit = -1.7976931348623158e+308;
    double  maxLimit =  1.7976931348623158e+308;
    if (gUseBinDisqualification)
        DisqualifyBinName (binName);
    dlogFlags |= DLOG_PARM_MVAL_EQ_LOLIM_PASS;  // Alternative
    dlogFlags |= DLOG_PARM_MVAL_EQ_HILIM_PASS;  // Alternative

	for (UINT siteIndex = 0; siteIndex < numSites; siteIndex++)
	{
		if (channels == 0)	// Datalogging single result not associated with a Pin or Group.
		{
			siteNumber = siteIndex + 1;

		// Build testName string
			strcpy_s(testStr, sizeof(testStr), testName);
			if (strnlen(mvpName, 1024)>0)
			{
				strcat_s(testStr, sizeof(testStr), " ("    );
				strcat_s(testStr, sizeof(testStr), mvpName );
				strcat_s(testStr, sizeof(testStr), ")"     );
			}

			if (numberMode == AUTONUM)			// AUTONUM	// yeah, just to keep consistency
				testNum = subTestNumber + channel+1;
			else
				testNum = subTestNumber + channel;

		// Send the Datalog
			if(TevTester_IsSiteActive(siteNumber) && TevTester_IsSiteEnabled(siteNumber))
			{
				result = MVP_GetResult(mvpName, mvpResultData, siteIndex, channel);
				sitePass = Dlog_Test (siteNumber, testNum, testStr,  result, minLimit, maxLimit, units, "%.3f", dlogFlags);
				if (sitePass == DLOG_FAILED)
				{
					if (gUseBinDisqualification)
					{
						char disqualifiedBinName [32];
						DisqualifyPresetBin    (siteNumber, FALSE);
						GetDisqualifiedBinName (siteNumber, disqualifiedBinName, sizeof (disqualifiedBinName));
						Dlog_SetBinResult      (siteNumber, disqualifiedBinName);
					}
					else
						Dlog_SetBinResult (siteIndex, binName);
				}
				else
					passMask |= 1 << siteIndex;
				if (gFastBinning && sitePass == DLOG_FAILED)
					TevTester_DisableSite (siteNumber);
			}
		}
		else
		{
			for (channel = 0; channel < numPinPerSite; channel++)
			{
			//	siteIndex  = TevTester_GetChannelSite(mvpName, channel);
				siteNumber = siteIndex + 1;

			// Build testName string
				strcpy_s(testStr, sizeof(testStr), testName);
				strcat_s(testStr, sizeof(testStr), " ("    );
				if (numPinPerSite > 1)
				{	// Add name of *this* pin
				//	mvp_Name = TevTester_GetChannelPinName(mvpName, channel);	// All pins all sites
					mvp_Name = TevTester_GetPinName       (mvpName, channel);	// Per site
					strcat_s(testStr, sizeof(testStr), mvp_Name );
				}
				else
				{	// Use MVPName passed in
					strcat_s(testStr, sizeof(testStr), mvpName );
				}
				strcat_s(testStr, sizeof(testStr), ")"     );

				if (numberMode == AUTONUM)			// AUTONUM
					testNum = subTestNumber + channel+1;
				else
					testNum = subTestNumber + channel;

			// Send the Datalog
				if(TevTester_IsSiteActive(siteNumber) && TevTester_IsSiteEnabled(siteNumber))
				{
					//	result = mvpResultData[channel];
					//  dlogFlags &= ~ DLOG_TEST_FAILED;
					//  dlogFlags |= DLOG_TEST_RESULT_INVALID;
					//  if (gSiteFailedMask & 1 << siteIndex)
					//      dlogFlags |= DLOG_TEST_FAILED;
					//  sitePass = Dlog_Test (siteNumber, testNumber, testStr,  result, minLimit, maxLimit, units, "%.3f", dlogFlags);
					BOOL pass = ((gSiteFailedMask & 1 << siteIndex) == 0);                                           // Alternative
					sitePass = Dlog_Test (siteNumber, testNum, testStr,  pass, 1.0, 1.0, "", "%.0f", dlogFlags);  // Alternative
					if (sitePass == DLOG_FAILED)
					{
						if (gUseBinDisqualification)
						{
							char disqualifiedBinName [32];
							DisqualifyPresetBin (siteNumber, FALSE);
							GetDisqualifiedBinName (siteNumber, disqualifiedBinName, sizeof (disqualifiedBinName));
							Dlog_SetBinResult (siteNumber, disqualifiedBinName);
						}
						else
							Dlog_SetBinResult (siteNumber, binName);
					}
					else
						passMask |= 1 << siteIndex;
					if (gFastBinning && sitePass == DLOG_FAILED)
						TevTester_DisableSite (siteNumber);
				}
			}
		}
	}
    gSiteFailedMask = 0;
    return passMask;
}

void MLog_CreateNewLot (char* lotId)
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
        openStdf = Dlog_IsStdfFileStreamOpen();
        openDat  = Dlog_IsTextFileStreamOpen();
        Dlog_CloseAllFileStreams();
    }

    char binName[64];
    int hardBin;
    BOOL binType;
    for (int i = 0 ; i < 5 ; i++)
    {
        UINT binNumber = i + 1;
        TevExec_TestBinName    (binNumber, binName, sizeof (binName));
        TevExec_TestBinHardBin (binNumber, &hardBin);
        TevExec_TestBinType    (binNumber, &binType);
        DLOG_BINTYPE stdfBinType = (DLOG_BINTYPE) binType;
        if (stdfBinType == BINTYPE_UNDEF)
            stdfBinType = BINTYPE_FAIL;
        Dlog_RegisterBin(binNumber, hardBin, binName, stdfBinType);
    }

    Dlog_SetLotId(lotId);

    DWORD datStreamHandle;
    if (openStdf)
    {
        SYSTEMTIME	sysTime;
        char	dlogFileName[256]={0};
        char	path[256]={0};
        char timeStamp[64];
        if (GetTestFunctionDllPath(path, sizeof (path)))
        {
            Dlog_SetProgramPath (path);
            strcat_s (path, sizeof (path), "\\Dlog");
        }
        else
            strcpy_s (path, sizeof (path), "Dlog");
        if (! FileExists(path))     // Directory
            CreateDirectory (path, NULL);
        GetLocalTime(&sysTime);
        sprintf_s(timeStamp, 64, 
            "%0.2u-%0.2u-%0.4u_%0.2u%0.2u%0.2u", \
            sysTime.wMonth, sysTime.wDay, sysTime.wYear,\
            sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
        TevTester_SetRunInfoString("LotTimestamp", timeStamp);
        sprintf_s(dlogFileName, sizeof (dlogFileName), "%s\\%s_%s_%s.stdf", path, gApplicationName, lotId, timeStamp);
        Dlog_OpenStdfFileStream(&datStreamHandle, dlogFileName, 0, NULL);
        Dlog_SetBaseSiteIndex(1);
    }
    Sleep (2000);

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

void MLog_OnStartDut ()
{
    Dlog_BeginBatch();
    for (int si = 0 ; si < gNumSites ; si++)
    {
        int siteNumber = si + 1;
        if (TevTester_IsSiteActive(siteNumber))
        {
            Dlog_BeginFlow(siteNumber, 2, NULL);
//          Creates 12 DTR recorders per DUT.
//            Dlog_WriteHeader(siteNumber, DLOG_HEADER_STANDARD, NULL);
        }
    }
    gPresetBin = 0;
    InitializeSiteBins (1);
}

void MLog_OnEndDut ()
{
    for (int si = 0; si < gNumSites; si++)
    {
        int siteNumber = si + 1;
        if (TevTester_IsSiteActive(siteNumber))
            int bin = Dlog_EndFlow(siteNumber, NULL); 
    }
    Dlog_EndBatch();
}

UINT MLog_GetBin (UINT siteNumber)
{
    int softBin;
    Dlog_GetSoftBin(siteNumber, &softBin);
    return softBin;
}

void MLog_OnProgramLoad (const char* name)
{
    gNumSites = TevTester_NumberOfSites();
    strcpy_s (gApplicationName, sizeof (gApplicationName), name);
}

void MLog_OnProgramUnload ()
{
    if (Dlog_GetInitStatus(DLOG_INIT_QUIET) == DLOG_SUCCESS)
    {
        DLOG_STATUS rc;
        if (Dlog_IsLotStarted())
        {
            // End the lot
            rc = Dlog_EndLot(NULL);
        }

        // Shutdown datalogger
        rc = Dlog_Shutdown(DLOG_DEFAULT_OPTIONS, NULL);
    }
    *gApplicationName = '\0';
    gNumSites = 0;
}

void MLog_SetApplicationName (const char* name)
{
    strcpy_s (gApplicationName, sizeof (gApplicationName), name);
}

void MLog_SetBinResult (int siteNumber, const char* binName)
{
    Dlog_SetBinResult(siteNumber, binName);
}