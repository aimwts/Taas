//==============================================================================
// Title:       MIPI_RFFE_demo.cpp
// 
// Description: This demonstration/starting application utilizes one DPS12 board
//              to simulate a 4 site program with a certain percentage of failures
//              over 4 test functions.
// 
// Copyright (c) 2011 Test Evolution Inc.
// All Rights Reserved
//  KS	 1	01/28/2012	Initial issue 
//  JC   2  05/29/2012  Add comment for STDF datalogging
//  JC   3  01/15/2014  Add DD48 Examples for SPI, MIPI interfaces.
//  JC   4  01/20/2014  Add DD48 Examples for I2C interface.
//  JC   5  01/23/2014  Added User_I2C UINT, char, float, double examples for 24LC256 EEPROM.
//  JC   6  05/07/2014  Added comments for Binning
//  JC   7  06/16/2014  Added pltData() for example.
//  JC	 8	07/28/2014	Add MLog_Footer(), setup bins.
//==============================================================================
//	JC	 1	06/01/2015	Add generic updateNewLotID() and call from AXIeSys.cpp AxiDutStart()
//                      along with generic support of Configuration files. 
//                      See #define useUserVariables 1.
//==============================================================================

// Project -> Properties -> Configuration Properties -> Debugging:
// Command: $(TestStand)\bin\seqedit.exe
// Command Arguments: MIPI_RFFE_demo.seq
// Debugger Type: Native only

// Project -> Properties -> Configuration Properties -> Debugging:
// Command: $(AXILIBDIR)\OpenExec.exe
// Command Arguments: 
// Debugger Type: Native only

#include "stdafx.h"
#include "MIPI_RFFE_demo.h"
#include "MLog.h"

#include "ddcapi.h"		// for DD48
#include <stdlib.h>     // malloc, free, rand
#include <sys/stat.h>	// _stat()
#include <direct.h>		// _getcwd()
#include <math.h>		// fabs()

#include <string.h>
#include <stdio.h>
#include "AXIeSys.h"
#include "AXIAPI.h"
#include "TevPlot.h"

// Global Defines ==============================================================
#define MAX_PATHNAME_LEN 260
#define TEST_FUNCT __declspec(dllexport)
#define MAX_SITES 4		// PCA0075 R0 on AxRF520 system (Aeroflex/Tev)
#define MAX_STDF_SOFT_BINS 32
#define MAX_HEADER_SIZE 32
#define UNCONDIIONAL_STOP_ON_FAIL 0
#define useUserVariables 1

#define APPLICATION_NAME "MIPI_RFFE_demo" 

// Global Variables ============================================================
static double   settlingTime = 0.05;    // Mode variable set by test executive.

// TEV Variables *************************************************************
#define MAX_PATHNAME_LEN 260
char	gDirPath[MAX_PATHNAME_LEN];
//const char*	gDirPath;		// OpenExec
//UINT	gNumSites = 1;
unsigned long	gPatternHandle;
PatternHandle	*pPHandles;
//UINT	gFAST_BINNING;		// 0:off 1:0n
//UINT	gDataOrder;			// 0:SITE_MAJOR, 1:PIN_MAJOR
BOOL	g_status = TRUE;	// global datalog Pass=1, Fail=0
//BOOL	gResetRelayDD48Shadow = false;
//static BOOL	gPatternsLoaded =	false;
//BOOL	active_site[MAX_SITES];			// Zero based
//BOOL	gShowPinFailures = false;
double	gTimer0Val=0.0, gTimer1Val=0.0, gTimer2Val=0.0;	// UINT timer, 0, 1, 2 are the 3 timers
BOOL	gShowElapsedTime = true;
BOOL	gShowElapsedTime0 = true;
BOOL	gShowElapsedTime1 = true;
BOOL	gShowElapsedTime2 = true;
unsigned short gRcode=0;
//BOOL	gRFSourceChanListStored=false;
static BOOL gUseBinDisqualification = TRUE;
static char gApplicationName [64] = {'\0'};
static UINT gSiteFailedMask       = 0;
static UINT gPresetBin            = 0;
static UINT gSiteCurrentBins [MAX_STDF_SOFT_BINS];
UINT	gTraceMode = MESSAGE_TRACE_NORMAL;
unsigned short	capturedData [MAX_SITES][100];
BOOL gPatternModified_I2C = FALSE;		// Example to modify "lcnt" in pattern.
BOOL gPatternModified_SPI = FALSE;		// Example to modify "lcnt" in pattern.



void MLog_UserDisableSite(UINT siteNumber) // Must be defined in user code
{	// User should disable hardware for site here ( 1 based)
	// When a site is Disabled the DPS12 channels are gated off automatically. See VISiteDisableCallback()
	// User must ensure DD48 channels are disconnected.
	TevWait_Now ( 1 ms);
}

static void OnAPIOrSystemError (int errorNumber, const char* message)
{   // Break here to stop on API error.
}

static void VISiteDisableCallback (const char* mvp)
{	// mvp (pin) for disabled site is passed in. Example "VBAT@2"
    TevVI_Gate (mvp, "Off");
}


// *************************************** User customization ***************************************
// Must be defined in user code.
// This function is called by AxiDutStart() in AXIeSys.cpp and is normally empty.
// AXIeDutStart() is called after ApplicationLoad().
// *************************************** User customization ***************************************
void updateNewLotID (char* myLotNumber)
{
#if useUserVariables
// EXAMPLE: Check to see if we are using a Configuration File that was read after ApplicationLoad().
// If required, modify "myLotNumber" so that Datalog and Summary filenames include user info.
// <applicationName>_<CustLotId>_<SubLotId>_<TestType>_<Page>_Date_TimeStamp>

	if ( !strstr(TevTester_GetRunInfoString("CustLotId"), "default1") )
	{
		strcpy_s(myLotNumber, 256, TevTester_GetRunInfoString("CustLotId") );	// "default1" from ApplicationLoad()
		strcat_s(myLotNumber, 256, "_");
		strcat_s(myLotNumber, 256, TevTester_GetRunInfoString("SubLotId")  );	// "default2" from ApplicationLoad()
		strcat_s(myLotNumber, 256, "_");
		strcat_s(myLotNumber, 256, TevTester_GetRunInfoString("TestType") );	// "default3" from ApplicationLoad()
		strcat_s(myLotNumber, 256, "_");
		strcat_s(myLotNumber, 256, TevTester_GetRunInfoString("Page")      );	// "default4" from ApplicationLoad()

		TevTester_SetRunInfoString ("LotId", myLotNumber);		// Summary filename also...
	}
#endif

	return;
}

bool MLog_CustomizeDlogFileName (char*	dlogFileName,  char* ext) // Must be defined in user code
{
	// User callback from AxiDutStart to set a customized dlog file name
	// pass in the file extenstion for the filename. (examples: "dlg", "stdf", "txt")
	// should be modified for each customer
	SYSTEMTIME	sysTime;
	char timeStamp[64];
	const char *dlogPath;

	if (0)
	{
		
		dlogPath = TevExec_GetDlogPath();
	
		GetLocalTime(&sysTime);
		sprintf_s(timeStamp, sizeof (timeStamp), 
			"%0.2u-%0.2u-%0.4u_%0.2u%0.2u%0.2u",
			sysTime.wMonth, sysTime.wDay, sysTime.wYear,\
			sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
		TevTester_SetRunInfoString("LotTimestamp", timeStamp);

		const char* lotId = TevTester_GetRunInfoString  ("LotId");
		const char* testerID = TevTester_GetRunInfoString  ("Tester ID");
		const char* subLotNo = TevTester_GetRunInfoString  ("SublotNo");		
		const char* testCode = TevTester_GetRunInfoString  ("Test Code");
	
		sprintf_s(dlogFileName,  sizeof (char)*256, "%s%s_%s_%s_%s_%s_%s.%s", dlogPath, testerID, APPLICATION_NAME, lotId,subLotNo,testCode, timeStamp, ext);
		return true;
	}
	return false;
}


void MLog_SetUserProductionVariables(void) // Must be defined in user code
{	
	// User callback from AxiDutStart to set User Production Variables
		
}


// User Generic Functions =========================================================
void UnloadPatterns(UINT startIndex)
{// Make room more patterns by unloading some.
	TevUtil_TraceMessage("    Unloading Patterns.....");
	TevUtil_SetTraceMode ("IMMEDIATE", MESSAGE_TRACE_NOTHING);	// "IMMEDIATE"|"QUEUED"|"OFF", <see AXIErroCodes.h>
	UINT i = NUM_OF_PATTERNS+1;
	while (i > startIndex)
	{
		pPHandles[i-1] = -1;
		TevDD_PatternUnload (i-1);
		i--;
	}
	TevUtil_SetTraceMode ("IMMEDIATE", MESSAGE_TRACE_DEBUG);	// "IMMEDIATE"|"QUEUED"|"OFF", <see AXIErroCodes.h>
	TevError_ClearAccumulatedErrors();	// gErrors[site]
	return;
}
/*
UINT get_active_site( void)
{	// Stuff global array.
	// active_site is Zero based.
	UINT goodSites = 0x0;
	for (UINT site = 0; site < MAX_SITES; site++) 
	{
		if (TevTester_IsSiteActive(site+1) && TevTester_IsSiteEnabled(site+1)) 
		{
			active_site[site] = true;
			goodSites |=  1 << (site); 
		}
		else
		{
			active_site[site] = false;
		}
	}
	return goodSites;
}
*/


bool file_exists(const char * filename)
{	// Check and see if a file exists (fopen_s doesn't always work)
	struct _stat info;
	int ret = -1;			//get the file attributes  
	ret = _stat(filename, &info);  
	if (ret==0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool get_working_path(char *pathname)
{	   // Get the current working directory: 
 char* buffer;

   if( (buffer = _getcwd( NULL, 0 )) == NULL )
	{
		sprintf_s(pathname, 128, "notfound");
		delete(buffer);
		return 1;
	}
   else
   {
      sprintf_s(pathname, 128, buffer);
   }
   delete(buffer);
   return 0;
}



int datalogTimer(char* testName, int subTestNum, UINT timer)
{	
	UINT	passMask= 0xFFFF;		// bit mask for each site, default is all sites pass // 0:TestStand displays as failed Test Step.
	
	if (!gShowElapsedTime) return 1;
	if (timer==0 && !gShowElapsedTime0) return 1;
	if (timer==1 && !gShowElapsedTime1) return 1;
	if (timer==2 && !gShowElapsedTime2) return 1;

	int pass = 1;
	double elapsedTime;
	double timeIt[MAX_SITES];
	elapsedTime = TevDiag_GetXTime(timer);	// UINT timer, 0, 1, 2 are the 3 timers

	MLOG_ForEachEnabledSite(siteIndex)
		timeIt [siteIndex] = elapsedTime;
	
    passMask &= MLog_Datalog("", subTestNum, testName, timeIt, "ms", 0.0, 60.0, "", "Bin25");
	return passMask;	// Pass/Fail  0:TestStand displays as failed Test Step.
}


int datalogTimer(char* testName, int subTestNum, UINT timer,  const char* comment)
{	
	UINT	passMask= 0xFFFF;		// bit mask for each site, default is all sites pass // 0:TestStand displays as failed Test Step.
	
	if (!gShowElapsedTime) return 1;
	if (timer==0 && !gShowElapsedTime0) return 1;
	if (timer==1 && !gShowElapsedTime1) return 1;
	if (timer==2 && !gShowElapsedTime2) return 1;

	int pass = 1;
	double elapsedTime;
	double timeIt[MAX_SITES];
	elapsedTime = TevDiag_GetXTime(timer);	// UINT timer, 0, 1, 2 are the 3 timers

	MLOG_ForEachEnabledSite(siteIndex)
		timeIt [siteIndex] = elapsedTime;
	
    passMask &= MLog_Datalog("", subTestNum, testName, timeIt, "ms", 0.0, 10.0, comment, "Bin25");
	return passMask;	// Pass/Fail  0:TestStand displays as failed Test Step.
}


//*********************************************************
//	Function: Check each site to see if pattern timed out.
//		      If it did then stop SEQuencers for all sites.
//*********************************************************
TEV_STATUS checkForPatternTimedOut(double	timeout, UINT handle)
{
	unsigned short	rcode = 0;
	char			siteStr[128], traceMsg[128], filename[256]={"jim"};
	
	TEV_STATUS apiStatus = AXI_SUCCESS, timeoutStatus = AXI_SUCCESS;

	MLOG_ForEachEnabledSite(siteIndex)
	{	// Expect rcode=99 for each site for normal end of pattern.
			sprintf_s(siteStr, 3, "%d", siteIndex);	// Zero based !!
			TevDD_SetExecMode ("CurSite", siteStr);	// Set Current Site so we can read stuff from it.

			apiStatus = TevDD_PatternWait(timeout);
			if (apiStatus > 0)
			{
				TevDD_GetReadCode (&rcode);			// [0]site0, [1]site1,....
				sprintf_s(traceMsg, " Pattern '%s' Failed to stop on site %d rcode %d", patterns[handle],  siteIndex+1, rcode);
				TevUtil_TraceMessage(traceMsg);
				timeoutStatus++;
			}
	}

	if (timeoutStatus)
	{// At least one site's SEQuencer didn't finish so stop all SEQuencers.
		TevDD_PatternStop();				// Stop ALL SEQuencers
	}

	return (timeoutStatus);
}


// RECEIVE memory => global array capturedData[site].
void readCaptureDataDD48(unsigned long numberElements, unsigned long offset)
{ // From Section 14.8 of DD48 User Manual:
// Since digital waveform memory is separate from the normal pattern memory, 
// a test program can write data to and read data from waveform segments while a pattern is running. 
// Although it is possible to write/read a waveform segments contents while it is active/running, 
// this is not recommended because it may create unpredictable results (do to data buffering).
	char errormsg[1000];
	int	 msgboxReply= 0;
	char siteStr[3];
	TEV_STATUS	apiStatus = AXI_SUCCESS;

// Read values from DD48 Capture memory
	for (UINT site = 0; site < gNumSites; site++)				
	{
		if(TevTester_IsSiteActive(site+1) && TevTester_IsSiteEnabled(site+1))
		{
			// Accessing data from multiple sites for those APIs that return data for the current site 
			// requires an API call to explicitly set the current site:
			sprintf_s(siteStr, 3, "%d", site);					// Zero based !!
			apiStatus = TevDD_SetExecMode ("CurSite", siteStr);	// Set Current Site (0 based)
			apiStatus = TevDD_WaveRead ("CaptureData",			// const char *waveName,
										capturedData[site],		// unsigned short *dataArray,
										numberElements,			// unsigned long  dataArraySize,
										offset,					// unsigned long  offset, 
										numberElements);		// unsigned long  numberElements); // For current site

			TevDD_GetReadCode (&gRcode);	// for debug

			if (apiStatus != 0)
			{
				sprintf_s(errormsg, 999, "readCaptureDataDD48: TevDD_WaveRead error. %d", apiStatus);
				TevUtil_TraceMessage(errormsg);
			}
		}
	}
	return;
}



// System Generic Functions ======================================================

// ApplicationLoad 
// This routine gets called from Application Load sequence step
TEST_FUNCT void ApplicationLoad(void)
{
	int rc, i;
	char	str[1024], str2[1024];
	BOOL	loadForEdit = false;

// Pattern stuff
	BOOL	patternState[NUM_OF_PATTERNS] = {FALSE};
	UINT	svmSize=0, lvmSize=0, svmSizeTotal=0, lvmSizeTotal=0;			// Pattern memory allocation sizes

	TEV_STATUS	status = AXI_SUCCESS, err, apiStatus = AXI_SUCCESS;

    if (! TevTester_IsMVPDataLoaded())
    {   // When not using OpenExec (TestStand stand alone).
        char str[260];
        GetTestFunctionDllPath (str, sizeof (str));
        strcat_s (str, sizeof (str), "MIPI_RFFE_demo.mvp");
        rc = TevExec_LoadPinMap (str);    // Default MVP for direct TestStand usage
        if (rc < 0)
        {
            sprintf_s(str,"MVP File: %s not found!", "MIPI_RFFE_demo.mvp");
            TevUtil_TraceMessage(str);
        }
    }

// **************************************************************************************
// Use the following to enable Trace messages from the APIs
//		TevDD_SetExecMode    ("SetTraceLevel", "0");		// mask: 0x1 enable reg write, 0x2 reg read
//		TevUtil_SetTraceMode ("IMMEDIATE", MESSAGE_TRACE_NOTHING);	// "IMMEDIATE"|"QUEUED"|"OFF", <see AXIErroCodes.h>
		TevUtil_SetTraceMode ("IMMEDIATE", MESSAGE_TRACE_DEBUG);	// "IMMEDIATE"|"QUEUED"|"OFF", <see AXIErroCodes.h>
		gTraceMode = MESSAGE_TRACE_DEBUG;
// **************************************************************************************

	TevUtil_TraceMessage ("    Loading Program...");

	GetTestFunctionDllPath (gDirPath, sizeof (gDirPath));	// gets dir of dll  "C:\AXITestPrograms\myApp\"

	gNumSites = TevTester_NumberOfSites();		// Returns the largest site number loaded into the MVP list

	// Setup Datalogging Streams

    TevTester_RunInfoAdd       ("Immediate Mode Datalog", "Boolean", NULL);
    TevTester_RunInfoAdd       ("STDF Mode Datalog", "Boolean", NULL);
    TevTester_RunInfoAdd       ("TEXT Mode Datalog", "Boolean", NULL);
 
    TevTester_SetRunInfoBoolean("Immediate Mode Datalog", TRUE);  
    TevTester_SetRunInfoBoolean("STDF Mode Datalog", TRUE);       
    TevTester_SetRunInfoBoolean("TEXT Mode Datalog", TRUE);       

    // Initialized Datalog Engine
    MLog_OnProgramLoad(APPLICATION_NAME); 
    rc = TevError_NotificationCallback (OnAPIOrSystemError);

// Load patterns.
// Allocate pattern handles array as needed - delete on program exit
// Load all PATTERN files needed for application execution
// Patterns will be activated by appropriate tests, using pattern handles
	pPHandles = (PatternHandle *) malloc(NUM_OF_PATTERNS * sizeof(PatternHandle));
	char patternPath[256];
	char m_PatternFileName[512];

	UnloadPatterns(0);	// Unload all patterns to make room.

	TevUtil_TraceMessage("    Loading digital patterns....");

// Set value = TRUE to load a pattern.
// Comment the line to not load.   idx  Name-----------			Used in:
	patternState [0] = TRUE;	//= 0,	MIPI = 0,				MIPIWaveformsDD48()
	patternState [1] = FALSE;	//= 1,	SPI_25LC256,			SPIWaveformsDD48()
	patternState [2] = FALSE;	//= 2,	I2C_24LC256,			I2CWaveformsDD48()
	patternState [3] = FALSE;	//= 1,	SPI_25LC256_LFE,		SPIWaveformsDD48()
	patternState [4] = FALSE;	//= 2,	I2C_24LC256_LFE,		I2CWaveformsDD48()

// Build the pathname
	strcpy_s (patternPath, 256, gDirPath);			
	strcat_s (patternPath, 256, "Patterns\\");	// OpenExec
	status = AXI_SUCCESS;

// Load the patterns
	for (i = 0; i < NUM_OF_PATTERNS; i++)
	{	
		if (patternState[i])
		{
		// Build pattern path and name
			strcpy_s (m_PatternFileName, sizeof (m_PatternFileName), patternPath);
			strcat_s (m_PatternFileName, patterns[i]);
			strcat_s (m_PatternFileName, ".do");

		// Temporarily set as a failure to start with.
			rc = ERROR_FileNotFound;
			status = 1;	

			if (file_exists(m_PatternFileName))
			{
				if ( strstr(m_PatternFileName, "_LFE.do"))		{loadForEdit = true;}

				if (loadForEdit)
				{	// Keep a copy in CPU for run time pattern modification.
					status = TevDD_SetExecMode("LoadForEdit", "1");
					status = TevDD_SetExecMode("ReadHW", "1");
				}

				if (_DEBUG)	
				{
					sprintf_s (str, 1024, "user_init: Loading Pattern '%s'......\n", patterns[i]);
					OutputDebugString(str);	// VC2005 Output -> Debug window
				}


				status = TevDD_PatternLoad(m_PatternFileName, &pPHandles[i]);
				if (_DEBUG)	
				{// Note: Returned pPHandles are INT and are in seqential order. No skipping!!
					err = TevDD_GetSvmLvmSize (pPHandles[i], &svmSize, &lvmSize);
					svmSizeTotal += svmSize;
					lvmSizeTotal += lvmSize;
					sprintf_s (str, 1024, "\t\t...Done. PatIndx: %2d  Hndl: %2d  svm:%4d lvm:%4d TOTALS: svm:%4d/4096 lvm:%f/32MB\n", i, pPHandles[i],svmSize,lvmSize,svmSizeTotal, float(lvmSizeTotal)/float(1000000));
					OutputDebugString(str); 
				}
				rc = AXI_SUCCESS;
				*str2 = 0;

				if(loadForEdit)
				{//	Back to default pattern load mode
					status = TevDD_SetExecMode("LoadForEdit", "0");
					status = TevDD_SetExecMode("ReadHW", "0");
					loadForEdit = false;
				}

			}	// if (file_exists(m_PatternFileName))


			if (status != AXI_SUCCESS)	//	return ERROR_FileNotFound;
			{
				if (rc == ERROR_FileNotFound)  strcpy_s(str2, 1024, " File Not Found\n");
				sprintf_s(str, 1024, "TevDD_PatternLoad() Error on File: %s.\n  Code= %d %s", m_PatternFileName, status, str2);
				TevUtil_TraceMessage(str); 
				strcat_s (str, 1024, "\nUse TraceTool for more information.");
				MessageBox (NULL, (LPCSTR)str, (LPCSTR)"TestProgramLoad:", MB_ICONQUESTION + MB_OKCANCEL + MB_SYSTEMMODAL + MB_TOPMOST);
				*str2 = 0;
			}
		}	// if (patternState[i])
		else	
		{
			if (_DEBUG)
			{
				sprintf_s (str, 1024, "user_init: *Skipping Pattern '%s' \t PatIndx: %d\n", patterns[i],i);
				OutputDebugString(str); 
			}
		}
	}	// for (int i = 0; i < NUM_OF_PATTERNS; i++)

	TevUtil_TraceMessage("    Done Loading digital patterns....");
	if (_DEBUG)
	{
		sprintf_s (str, 1024, "user_init: ___Done loading patterns___\n");
		OutputDebugString(str); 
	}
	gPatternsLoaded = true;	// Keep track if patterns are loaded



// Check if DUT Site Power is on. Prompt user if is not.
	TevDiag_DiagnosticStatement (0, "Checking for DUT Site Power");
    int dutPower = 0, waitForDutPower = 1;
	while(!dutPower && waitForDutPower)
    {
        dutPower = TevTester_IsDutSitePoweredOn ();
        if (! dutPower)
		{
            waitForDutPower = MessageBox (NULL, "Turn On DUT Site Power", APPLICATION_NAME, MB_ICONQUESTION + MB_OKCANCEL + MB_SYSTEMMODAL + MB_TOPMOST);
			waitForDutPower -= 2;
		}
	}
    
	if (!TevTester_IsDutSitePoweredOn())	// abort!
	{
		rc = ERROR_DUTPowerNotEnabled;
		// What should we do here to ensure run will be stopped? - JC
		return;
	}

// Check to see if Variables have already been defined.
	UINT traceFilter = TevUtil_GetTraceFilter();
	TevUtil_SetTraceMode ("IMMEDIATE", MESSAGE_TRACE_NOTHING);	// "IMMEDIATE"|"QUEUED"|"OFF", <see AXIErroCodes.h>
    UINT FBindx = TevExec_GetVariableIndex  ("FastBinning");
	TevUtil_SetTraceMode ("IMMEDIATE", traceFilter);			// "IMMEDIATE"|"QUEUED"|"OFF", <see AXIErroCodes.h>

	if (FBindx !=0)	// Expect zero if has already been defined as the first Variable. 
	{
	// *********************************************************************
	// "Variables" tab in OpenExec. These are disabled for "Operator" mode.
	// Must be one of the following:
	//	"Enum", " Double ", "Single", "Int32", "Int16", "Boolean", "Char", "String"
	// *********************************************************************
		// Always declare these
		TevTester_VariableAdd       ("FastBinning", "Boolean", NULL);	// Make this the first one (index==0)
		TevTester_SetVariableBoolean("FastBinning", FALSE);
		TevTester_VariableAdd       ("StopOnFirstFail", "Boolean", NULL);
		TevTester_SetVariableBoolean("StopOnFirstFail", FALSE);

		TevTester_VariableAdd       ("1 of N Filter", "Boolean", NULL);
		TevTester_SetVariableBoolean("1 of N Filter", FALSE);
		TevTester_VariableAdd       ("1 of N 'N'", "Int16", NULL);
		TevTester_SetVariableInt16	("1 of N 'N'", 3);

		// Declare additional variables
		TevTester_VariableAdd       ("DutMaxLimit", "Double", "V");
		TevTester_SetVariableDouble ("DutMaxLimit", 0.005);
		TevTester_VariableAdd       ("DutMinLimit", "Double", "V");
		TevTester_SetVariableDouble ("DutMinLimit", -0.005);
		TevTester_VariableAdd       ("Consistency", "Double", "V");
		TevTester_SetVariableDouble ("Consistency", 0.02);
		TevTester_VariableAdd       ("SettlingTime", "Double", "s");
		TevTester_SetVariableDouble ("SettlingTime", 0.002);
		// UserI2CEEPROM
		TevTester_VariableAdd       ("UserI2CEEPROM_chassis", "Int16", "");
		TevTester_VariableAdd       ("UserI2CEEPROM_slot",    "Int16", "");
		TevTester_SetVariableInt16  ("UserI2CEEPROM_chassis", 1);
		TevTester_SetVariableInt16  ("UserI2CEEPROM_slot", 1);
		TevTester_VariableAdd       ("Twc", "Double", "s");
		TevTester_SetVariableDouble ("Twc", 0.005);


	// *********************************
	// "Production" tab in OpenExec
	// *********************************
	// Runtime information Test Program; GUI <=> test program
	// Lot ID runtime  (required by Open Exec for Lot management)
		TevTester_RunInfoAdd        ("LotId",           "String", "");
		TevTester_RunInfoAddHidden  ("Operator",        "String", "");
		TevTester_RunInfoAdd        ("Prompt_SN", "Boolean", NULL);
		TevTester_SetRunInfoBoolean ("Prompt_SN", FALSE);
		TevTester_RunInfoAddHidden  ("LotTimestamp",    "String", "");

	// To avoid Prober/Handler "Variable Name 'X', not found."
	// To avoid Prober/Handler "Variable Name 'Y', not found."
		TevTester_RunInfoAdd        ("X",                 "Int32",  "");
		TevTester_RunInfoAdd        ("Y",                 "Int32",  "");
		TevTester_SetRunInfoInt32   ("X", 1);
		TevTester_SetRunInfoInt32   ("X", 2);

#if useUserVariables
	//	EXAMPLE: Additional customer variables used in AxiDutStart(), updateNewLotID(), LotID()
		TevTester_RunInfoAdd        ("CustLotId",         "String", "");
		TevTester_RunInfoAdd        ("SubLotId",          "String", "");
		TevTester_RunInfoAdd        ("TestType",          "String", "");
		TevTester_RunInfoAdd        ("Page",              "String", "");

		TevTester_SetRunInfoString  ("CustLotId",         "default1");
		TevTester_SetRunInfoString  ("SubLotId",          "default2");
		TevTester_SetRunInfoString  ("TestType",          "default3");
		TevTester_SetRunInfoString  ("Page",              "default4");

/* EXAMPLE: AutoConfig.cfg
TEST PROGRAMS DIRECTORY = C:\AXITestPrograms\
TEST SEQUENCE DIRECTORY = MIPI_RFFE_demo
MVP FILE = MIPI_RFFE_demo.mvp
HANDLER DRIVERS DIRECTORY = C:\Program Files\TEV\AXI\Handlers\
HANDLER DRIVER = Sim100Iterations.dll
HANDLER GPIB ADDR = 1
PRODUCTION DATALOG DIRECTORY = C:\AxiTestPrograms\Data\
UNLOAD BATCH FILE =
HANDLER TRACE = TRUE
LOTID = JimDefaultLot5
[ProductionTab]
Immediate Mode Datalog = TRUE
STDF Mode Datalog = TRUE
LotId = JimDefaultLot5
Prompt_SN = FALSE
X = 0
Y = 0
CustLotId = SigP226
SubLotId = 9mm
TestType = FT
Page = FT3
[VariablesTab]
FastBinning = FALSE
StopOnFirstFail = FALSE
1 of N Filter = FALSE
1 of N 'N' = 1
*/
#endif
	}


// Binning
	TevTester_SetNumberOfTestBins (32);				// 2-256 bins

	//			soft, hard, bin
	//			 bin, bin,  name,  bin_type
	MLog_SetupBin( 1,  1, "Bin1",  MLOG_BIN_PASS);	// PASS
	MLog_SetupBin( 2,  5, "Bin2",  MLOG_BIN_FAIL);	// 
	MLog_SetupBin( 3,  5, "Bin3",  MLOG_BIN_FAIL);	// 
	MLog_SetupBin( 4,  5, "Bin4",  MLOG_BIN_FAIL);	// 
	MLog_SetupBin( 5,  5, "Bin5",  MLOG_BIN_FAIL);	// 
	MLog_SetupBin( 6,  5, "Bin6",  MLOG_BIN_FAIL);	// 
	MLog_SetupBin( 7,  5, "Bin7",  MLOG_BIN_FAIL);	// 
	MLog_SetupBin( 8,  5, "Bin8",  MLOG_BIN_FAIL);	// 
	MLog_SetupBin( 9,  5, "Bin9",  MLOG_BIN_FAIL);	// 
	MLog_SetupBin(10,  5, "Bin10", MLOG_BIN_FAIL);	// 
	MLog_SetupBin(11,  5, "Bin11", MLOG_BIN_FAIL);	// 
	MLog_SetupBin(12,  5, "Bin12", MLOG_BIN_FAIL);	// 
	MLog_SetupBin(13,  5, "Bin13", MLOG_BIN_FAIL);	// 
	MLog_SetupBin(14,  5, "Bin14", MLOG_BIN_FAIL);	// 
	MLog_SetupBin(15,  5, "Bin15", MLOG_BIN_FAIL);	// 
	MLog_SetupBin(16,  5, "Bin16", MLOG_BIN_FAIL);	// USer I2C
	MLog_SetupBin(17,  5, "Bin17", MLOG_BIN_FAIL);	// MIPI WaveformsDD48
	MLog_SetupBin(18,  5, "Bin18", MLOG_BIN_FAIL);	// I2C WaveformsDD48
	MLog_SetupBin(19,  5, "Bin19", MLOG_BIN_FAIL);	// SPI WaveformsDD48
	MLog_SetupBin(20,  5, "Bin20", MLOG_BIN_FAIL);	// Consistency
	MLog_SetupBin(21,  5, "Bin21", MLOG_BIN_FAIL);	// UnderVoltage
	MLog_SetupBin(22,  5, "Bin22", MLOG_BIN_FAIL);	// OverVoltage
	MLog_SetupBin(23,  5, "Bin23", MLOG_BIN_FAIL);	// ContinuityDD48
	MLog_SetupBin(24,  5, "Bin24", MLOG_BIN_FAIL);	// Continuity
	MLog_SetupBin(25,  5, "Bin25", MLOG_BIN_FAIL);	// Timeout

	MLog_SetupBin(26,  5, "Bin26", MLOG_BIN_FAIL);	// 
	MLog_SetupBin(27,  5, "Bin27", MLOG_BIN_FAIL);	// 
	MLog_SetupBin(28,  5, "Bin28", MLOG_BIN_FAIL);	// 
	MLog_SetupBin(29,  5, "Bin29", MLOG_BIN_FAIL);	// 
	MLog_SetupBin(30,  5, "Bin30", MLOG_BIN_FAIL);	// 
	MLog_SetupBin(31,  5, "Bin31", MLOG_BIN_FAIL);	// 
	MLog_SetupBin(32,  5, "Bin32", MLOG_BIN_FAIL);	// LotID

// For Debugging. Will get set by OpenExec each RUN
	TevExec_ActivateSite(1);
	TevExec_ActivateSite(2); 
//	TevExec_DeactivateSite(1);
//	TevExec_DeactivateSite(2);

	MLog_Set1ofNFilter(false , 3);	

	TevVI_SetDisableSiteCallback(VISiteDisableCallback);
}


// ApplicationUnload 
// This routine gets called from Application Unload sequence step
// It is called once when the sequence is first executed
TEST_FUNCT void ApplicationUnload(void)
{
	// Add Test Program Code Here 
    TEV_STATUS apiErr = TevError_NotificationCallback (NULL);
    MLog_OnProgramUnload();
}


// ApplicationStart 
// This routine gets called from Application Start sequence step
TEST_FUNCT void ApplicationStart(void)
{
   
	TevUtil_TraceMessage("__ApplicationStart_______");

    settlingTime = TevTester_GetVariableDouble ("SettlingTime");
    TevVI_Gate ("ALL_DPS", "On");
    for (UINT sn = 1 ; sn <= TevTester_NumberOfSites() ; sn++)
        MLog_SetBinResult (sn, "Bin1");
    TevWait_Now (settlingTime);
    BOOL fastBinning = TevTester_GetVariableBoolean("FastBinning");
    MLog_SetFastBinning (fastBinning);

	BOOL stopFirstFail = TevTester_GetVariableBoolean("StopOnFirstFail");
    MLog_SetStopOnFirstFail (stopFirstFail);
  
	BOOL filter = TevTester_GetVariableBoolean("1 of N Filter");
	UINT N = TevTester_GetVariableInt16("1 of N 'N'");
	MLog_Set1ofNFilter(filter , N);
	
// Use PIN_MAJOR so DD48 and DPS12 use same order.
	MVP_SetDataOrder(PIN_MAJOR);	// Order of data returned in measurement arrays for MVP pins and groups.

// Start Dlog Device 
	MLog_OnStartDut();

}

// ApplicationEnd 
// This routine gets called from Application End sequence step
TEST_FUNCT void ApplicationEnd(void)
{
	TevUtil_TraceMessage("__ApplicationStop________");

	TevVI_ForceVoltage ("ALL_DPS", 0.0, 25.0);
    TevVI_Gate ("ALL_DPS", "Off");
}


void formatHeadername(const char * testName, char * headerName, size_t maxSize)
{	// "-- testName -------------------", MAX_HEADER_SIZE = 32

	char   *str1 = new char[maxSize];
	char   *str2 = new char[maxSize];

	memset (str2, '_', maxSize-1);
	str2[maxSize-1] = 0;	// NULL
	str2[0] = 32;			// space

	strcpy_s(str1, maxSize-1, "__ ");

	size_t sz = strlen(testName);
	if (sz > maxSize-2) sz = maxSize-2;
	strncpy_s((str1+3), maxSize-3, testName, sz);

	sz = strlen(str1);
	strncpy_s((str1+sz), maxSize-sz, str2, maxSize-1-sz);

	strncpy_s(headerName, maxSize, str1, maxSize);

	//	sz = strlen(str1);
	//	printf("\nHeader name: '%s'   length: %d  sizeof: %d\n", headerName, sz, maxSize);

	delete(str1);
	delete(str2);

	return;
}

void plotData (UINT testNumber)
	{
	//	sprintf_s(plotTitle, 256, "Captured Raw IQ Data");
	//	int plotHandle= TevPlot_NewPlot(NoSamples, iDataWLAN,  VAL_FLOAT, VAL_RED, plotTitle, "iData", "Data Samples", "Volts", 0);
	//	TevPlot_AddPlot(plotHandle, NoSamples, qDataWLAN,  VAL_FLOAT, VAL_BLUE, "qData", 1);
	}


// User Test Functions =========================================================
TEST_FUNCT int LotID(UINT testNumber)
{
#if useUserVariables
/*************************************************************************************
	Datalog "Production" and [run]"Variables" values.
	These get declared and set in ApplicationLoad().
	If a Configuration file is used it will be read immediately after ApplicationLoad().
	The values in the Configuration file will overwrite the values set in ApplicationLoad().
**************************************************************************************/

   	MLOG_CheckFastBinning   // Return if Fastbinning is On and No Enabled Sites
	
	const char* testName = "Lot ID";
	char  headerName[MAX_HEADER_SIZE] = {""};
	formatHeadername(testName, headerName, sizeof (headerName));

	testNumber= 1;					// testNumber is set in MLog_Header(). 
// *********************************************************************
// Note:  STDF requires unique testNumber and subTestNum
// *********************************************************************

	TEV_STATUS		apiStatus = AXI_SUCCESS;
//	UINT	testNumber= 1;			// testNumber is set in MLog_Header(). 
//	int		subTestNum = 1;			// subTestNum is set in MLog_Datalog(); 
	int		subTestNum = testNumber + 1; // STDF requires unique testNumber and subTestNum 
	int		fail_bin = 32;
	char	fBinName[10];			// Fail bin name
	UINT	passMask= 0xFFFF;		// bit mask for each site, default is all sites pass // 0:TestStand displays as failed Test Step.
	UINT	site=0;

	double results[MAX_SITES] = {0.0, 0.0, 0.0, 0.0};
	BOOL	NFilter= FALSE;
	short	NFltrValue = 0;
	char	msg[32]={""};

	BOOL	ImmedMode= FALSE, STDFMode = FALSE;
	const char* myString= {"1234567890123456789012345678901234567890"};

//	For debugging we set values here
	//TevTester_SetRunInfoString ("RunType",           "FT");		// FT RT
	//TevTester_SetRunInfoString ("Page",              "FT1");	// FT1, FT2, RT1, RT2
	//TevTester_SetRunInfoString ("CustLotNo",         "Sig229");
	//TevTester_SetRunInfoString ("SubLot",            "EE2");

// Set datalog Header, testNumber, and fBinName
	sprintf_s(fBinName, 10, "%d", int(fail_bin));		// if gUseBinDisqualification
	formatHeadername(testName, headerName, sizeof (headerName));
    MLog_Header         (testNumber, headerName, fBinName);

	sprintf_s(fBinName, 10, "Bin%d", int(fail_bin));		// if gUseBinDisqualification

	MLOG_ForEachEnabledSite(siteIndex)
	{
		results[siteIndex] = siteIndex+1;
	}

// Openexec:[ProductionData]    Configuration File: [ProductionTab]
	myString = TevTester_GetRunInfoString      ("CustLotId");
	passMask &= MLog_Datalog("", 1, "CustLotNo", results, "", 0.0,  4.0, myString, fBinName);

	myString = TevTester_GetRunInfoString      ("SubLotId");
	passMask &= MLog_Datalog("", 2, "SubLot",  results, "", 0.0,  4.0, myString, fBinName);

	myString = TevTester_GetRunInfoString      ("TestType");
	passMask &= MLog_Datalog("", 3, "RunType", results, "", 0.0,  4.0, myString, fBinName);

	myString = TevTester_GetRunInfoString      ("Page");
	passMask &= MLog_Datalog("", 4, "Page",    results, "", 0.0,  4.0, myString, fBinName);



// Openexec:[Variables]		Configuration File: [VariablesTab]
	NFilter = TevTester_GetVariableBoolean("1 of N Filter");
	MLOG_ForEachEnabledSite(siteIndex)
	{
		if (NFilter) 
		{
			results[siteIndex] = 1.0;
			sprintf_s(msg, 32, "True");
		}
		else
		{
			results[siteIndex] = 0.0;
			sprintf_s(msg, 32, "False");	
		}
	}
	passMask &= MLog_Datalog("", 5, "1ofN_Fltr",  results, "", 0.0,  4.0, msg, fBinName);


	NFltrValue = TevTester_GetVariableInt16	("1 of N 'N'");
	MLOG_ForEachEnabledSite(siteIndex)
	{
		results[siteIndex] = double(NFltrValue);
	}
	passMask &= MLog_Datalog("", 6, "1ofN_val" ,  results, "", 0.0,  4.0, "1ofN", fBinName);


	
// Defined in MLog_OnProgramLoad()
	ImmedMode = TevTester_GetRunInfoBoolean("Immediate Mode Datalog");	
	MLOG_ForEachEnabledSite(siteIndex)
	{
		if (ImmedMode) 
		{
			results[siteIndex] = 1.0;
			sprintf_s(msg, 32, "True");
		}
		else
		{
			results[siteIndex] = 0.0;
			sprintf_s(msg, 32, "False");	
		}
	}
	passMask &= MLog_Datalog("", 7, "ImmediateModeDatalog",  results, "", 0.0,  4.0, msg, fBinName);


	STDFMode = TevTester_GetRunInfoBoolean("STDF Mode Datalog");	
	MLOG_ForEachEnabledSite(siteIndex)
	{
		if (ImmedMode) 
		{
			results[siteIndex] = 1.0;
			sprintf_s(msg, 32, "True");
		}
		else
		{
			results[siteIndex] = 0.0;
			sprintf_s(msg, 32, "False");	
		}
	}
	passMask &= MLog_Datalog("", 8, "STDFModeDatalog",  results, "", 0.0,  4.0, msg, fBinName);

    myString = TevTester_GetRunInfoString ("Login Mode");	 
	MLOG_ForEachEnabledSite(siteIndex)
	{
		results[siteIndex] = siteIndex+1;
	}
	passMask &= MLog_Datalog("", 9, "LoginMode", results, "", 0.0,  4.0, myString, fBinName);

#endif

	return 1;	// Pass/Fail  0:TestStand displays as failed Test Step.
}


// Continuity test actually just force and measures DPS channels.
TEST_FUNCT int Continuity(UINT testNumber)
{
   	MLOG_CheckFastBinning   // Return if Fastbinning is On and No Enabled Sites
	
	const char* testName = "Continuity";
	char  headerName[MAX_HEADER_SIZE] = {""};
	formatHeadername(testName, headerName, sizeof (headerName));

	testNumber= 10;					// testNumber is set in MLog_Header(). 
// *********************************************************************
// Note:  STDF requires unique testNumber and subTestNum
// *********************************************************************

	TEV_STATUS		apiStatus = AXI_SUCCESS;
//	UINT	testNumber= 1;			// testNumber is set in MLog_Header(). 
//	int		subTestNum = 1;			// subTestNum is set in MLog_Datalog(); 
	int		subTestNum = testNumber + 1; // STDF requires unique testNumber and subTestNum 
	int		fail_bin = 24;
	char	fBinName[10];			// Fail bin name
	UINT	passMask= 0xFFFF;		// bit mask for each site, default is all sites pass // 0:TestStand displays as failed Test Step.
	UINT	site=0;

// UNCONDIIONAL_STOP_ON_FAIL
	UINT siteBin;	// Zero based
	BOOL binType;	// 0:Fail, 1:Pass

// Set datalog Header, testNumber, and fBinName
	sprintf_s(fBinName, 10, "%d", int(fail_bin));		// if gUseBinDisqualification
	formatHeadername(testName, headerName, sizeof (headerName));
    MLog_Header         (testNumber, headerName, fBinName);

	sprintf_s(fBinName, 10, "Bin%d", int(fail_bin));		// if gUseBinDisqualification

	gTimer0Val = TevDiag_GetXTime(0);	// UINT timer, 0, 1, 2 are the 3 timers
	TevUtil_TraceMessage("Start: Continuity");

	//get_active_site( );		// Update global array


// User code
    double results[8];

	TevVI_ForceVoltage  ("ALL_DPS", 5.0, 25.0);
    TevWait_Now         (settlingTime);
    TevVI_MeasureVoltage("ALL_DPS", results, sizeof (results));

    passMask &= MLog_Datalog("ALL_DPS", subTestNum++, testName, results, "V", 4.0, 6.0, "", fBinName);
	subTestNum++;

// Disable site if (gFastBinning & !gStopOnFirstFailure)
	MLOG_DisableFailedSites		// also calls MLog_UserDisableSite(siteNumber)

// Force site DISABLED if fail above block of tests even if Fast Binning is off.
// Use this when you want to protect DUT and Tester from damage if a DUT has failed a test.
if (UNCONDIIONAL_STOP_ON_FAIL)
{
	for (UINT thisSite=0; thisSite < gNumSites; thisSite++)
	{
		if (TevTester_IsSiteActive(thisSite+1) && TevTester_IsSiteEnabled(thisSite+1)) 
		{
			siteBin = MLog_GetBin (thisSite+1);		// 1 based
			TevExec_TestBinType(siteBin, &binType);	// 0:Fail, 1:Pass
			if (binType==0)
			{
				MLog_UserDisableSite (thisSite+1);	// 1 based. User code for clean shudown
				TevTester_DisableSite(thisSite+1);	// 1 based.
			}
		}
	}
}

// Cleaup
	// cleanup code here

	datalogTimer("Time_Continuity", subTestNum++, 0);	// (UINT timer, 0, 1, 2 are the 3 timers), int testNum 

//	MLog_Footer	(passMask, headerName, "");				// For enhanced Dlogviewer display of a failed site.

	return passMask;	// Pass/Fail  0:TestStand displays as failed Test Step.
}

// Test utilizes variations between force and measure to periodically fail.
TEST_FUNCT int OverVoltage (UINT testNumber)
{
   	MLOG_CheckFastBinning   // Return if Fastbinning is On and No Enabled Sites
	
	const char* testName = "OverVoltage";
	char  headerName[MAX_HEADER_SIZE] = {""};
	formatHeadername(testName, headerName, sizeof (headerName));

	testNumber= 30;					// testNumber is set in MLog_Header(). 

	TEV_STATUS		apiStatus = AXI_SUCCESS;
//	UINT	testNumber= 1;			// testNumber is set in MLog_Header(). 
//	int		subTestNum = 1;			// subTestNum is set in MLog_Datalog(); 
	int		subTestNum = testNumber + 1; // STDF requires unique testNumber and subTestNum 
	int		fail_bin = 23;
	char	fBinName[10];			// Fail bin name
	UINT	passMask= 0xFFFF;		// bit mask for each site, default is all sites pass // 0:TestStand displays as failed Test Step.
	UINT	site=0;

// Set datalog Header, testNumber, and fBinName
	sprintf_s(fBinName, 10, "%d", int(fail_bin));		// if gUseBinDisqualification
	formatHeadername(testName, headerName, sizeof (headerName));
    MLog_Header         (testNumber, headerName, fBinName);

	sprintf_s(fBinName, 10, "Bin%d", int(fail_bin));		// if gUseBinDisqualification

	gTimer0Val = TevDiag_GetXTime(0);	// UINT timer, 0, 1, 2 are the 3 timers
	TevUtil_TraceMessage("Start: OverVoltage");

	//get_active_site( );		// Update global array


// User code
    double results[4];

	TevVI_ForceVoltage  ("DPS_1", 10.0, 25.0);
    TevWait_Now         (settlingTime);
    TevVI_MeasureVoltage("DPS_1", results, sizeof (results));

    double upperLimit = 10.0 + TevTester_GetVariableDouble ("DutMaxLimit");
    passMask &= MLog_Datalog("DPS_1", subTestNum++, testName, results, "V", 9.0, upperLimit, "", fBinName);




// Disable site if (gFastBinning & !gStopOnFirstFailure)
	MLOG_DisableFailedSites		// also calls MLog_UserDisableSite(siteNumber)

	datalogTimer("Time_OverVoltage", subTestNum++, 0);	// (UINT timer, 0, 1, 2 are the 3 timers), int testNum 

//	MLog_Footer	(passMask, headerName, "");				// For enhanced Dlogviewer display of a failed site.

	return passMask;	// Pass/Fail  0:TestStand displays as failed Test Step.
}

// Test utilizes variations between force and measure to periodically fail.
TEST_FUNCT int UnderVoltage (UINT testNumber)
{
   	MLOG_CheckFastBinning   // Return if Fastbinning is On and No Enabled Sites
	
	const char* testName = "UnderVoltage";
	char  headerName[MAX_HEADER_SIZE] = {""};
	formatHeadername(testName, headerName, sizeof (headerName));

	testNumber= 40;					// testNumber is set in MLog_Header(). 

	TEV_STATUS		apiStatus = AXI_SUCCESS;
//	UINT	testNumber= 1;			// testNumber is set in MLog_Header(). 
//	int		subTestNum = 1;			// subTestNum is set in MLog_Datalog(); 
	int		subTestNum = testNumber + 1; // STDF requires unique testNumber and subTestNum 
	int		fail_bin = 21;
	char	fBinName[10];			// Fail bin name
	UINT	passMask= 0xFFFF;		// bit mask for each site, default is all sites pass // 0:TestStand displays as failed Test Step.
	UINT	site=0;

// Set datalog Header, testNumber, and fBinName
	sprintf_s(fBinName, 10, "%d", int(fail_bin));		// if gUseBinDisqualification
	formatHeadername(testName, headerName, sizeof (headerName));
    MLog_Header         (testNumber, headerName, fBinName);

	sprintf_s(fBinName, 10, "Bin%d", int(fail_bin));		// if gUseBinDisqualification

	gTimer0Val = TevDiag_GetXTime(0);	// UINT timer, 0, 1, 2 are the 3 timers
	TevUtil_TraceMessage("Start: UnderVoltage");

//	get_active_site( );		// Update global array


// User code
    double results[4];

	TevVI_ForceVoltage  ("DPS_1", -10.0, 25.0);
    TevWait_Now         (settlingTime);
    TevVI_MeasureVoltage("DPS_1", results, sizeof (results));

    double lowerLimit = -10.0 + TevTester_GetVariableDouble ("DutMinLimit");
    passMask &= MLog_Datalog("DPS_1", subTestNum++, testName, results, "V", lowerLimit, -9.0, "", fBinName);




// Disable site if (gFastBinning & !gStopOnFirstFailure)
	MLOG_DisableFailedSites		// also calls MLog_UserDisableSite(siteNumber)

	datalogTimer("Time_UnderVoltage", subTestNum++, 0);	// (UINT timer, 0, 1, 2 are the 3 timers), int testNum 

//	MLog_Footer	(passMask, headerName, "");				// For enhanced Dlogviewer display of a failed site.

	return passMask;	// Pass/Fail  0:TestStand displays as failed Test Step.
}

// Test utilizes variations between force and measure to periodically fail.
TEST_FUNCT int Consistency (UINT testNumber)
{
   	MLOG_CheckFastBinning   // Return if Fastbinning is On and No Enabled Sites
	
	const char* testName = "Consistency";
	char  headerName[MAX_HEADER_SIZE] = {""};
	formatHeadername(testName, headerName, sizeof (headerName));

	testNumber= 50;					// testNumber is set in MLog_Header(). 

	TEV_STATUS		apiStatus = AXI_SUCCESS;
//	UINT	testNumber= 1;			// testNumber is set in MLog_Header(). 
//	int		subTestNum = 1;			// subTestNum is set in MLog_Datalog(); 
	int		subTestNum = testNumber + 1; // STDF requires unique testNumber and subTestNum 
	int		fail_bin = 20;
	char	fBinName[10];			// Fail bin name
	UINT	passMask= 0xFFFF;		// bit mask for each site, default is all sites pass // 0:TestStand displays as failed Test Step.
	UINT	site=0;

// Set datalog Header, testNumber, and fBinName
	sprintf_s(fBinName, 10, "%d", int(fail_bin));		// if gUseBinDisqualification
	formatHeadername(testName, headerName, sizeof (headerName));
    MLog_Header         (testNumber, headerName, fBinName);

	sprintf_s(fBinName, 10, "Bin%d", int(fail_bin));		// if gUseBinDisqualification

	gTimer0Val = TevDiag_GetXTime(0);	// UINT timer, 0, 1, 2 are the 3 timers
	TevUtil_TraceMessage("Start: Consistency");

//	get_active_site( );		// Update global array


// User code
    double results   [4][100];
    double variation [4];
    double lowVal, highVal;

	TevVI_ForceVoltage  ("DPS_1", 2.0, 25.0);
    TevWait_Now         (settlingTime);
    TevVI_MeasureVoltageArray ("DPS_1", 100, 100 us, (double*) results, sizeof (results[0]), sizeof (results));

    MLOG_ForEachEnabledSite(siteIndex)
	{
        lowVal = highVal = results[siteIndex][0];
        for (int i = 0 ; i < 100 ; i++)
        {
            if (lowVal > results[siteIndex][i])
                lowVal = results[siteIndex][i];
            if (highVal < results[siteIndex][i])
                highVal = results[siteIndex][i];
        }
        variation[siteIndex] = highVal - lowVal;
    }

    double consistencyRange = TevTester_GetVariableDouble ("Consistency");
    passMask &= MLog_Datalog("DPS_1", subTestNum++, testName, variation, "V", 0.0, consistencyRange, "", fBinName);

    // Example of using SetFailed for logical failures.
    MLOG_ForEachEnabledSite(siteIndex)
	{
        if (variation[siteIndex] > consistencyRange)
            MLog_SetTestFailed (siteIndex + 1);
    }

// Datalog w/o limits
    MLog_LogState ("DPS_1", subTestNum++, "ConsistencyState", variation, "V", "", "Bin2");




// Disable site if (gFastBinning & !gStopOnFirstFailure)
	MLOG_DisableFailedSites		// also calls MLog_UserDisableSite(siteNumber)

	datalogTimer("Time_Contsistency", subTestNum++, 0);	// (UINT timer, 0, 1, 2 are the 3 timers), int testNum 

//	MLog_Footer	(passMask, headerName, "");				// For enhanced Dlogviewer display of a failed site.

	return passMask;	// Pass/Fail  0:TestStand displays as failed Test Step.
}


// ********************************************************************************
// Continuity:
// We use DD48 pins in PMU mode. So any pattern with those pins will do.
// Pins must be declared in both the pattern file and the ~.mvp file.
// ********************************************************************************
TEST_FUNCT int ContinuityDD48(UINT testNumber)
{
   	MLOG_CheckFastBinning   // Return if Fastbinning is On and No Enabled Sites
	
    const char* testName = "ContinuityDD48";
	char  headerName[MAX_HEADER_SIZE] = {""};
	formatHeadername(testName, headerName, sizeof (headerName));

	testNumber= 20;					// testNumber is set in MLog_Header(). 

	TEV_STATUS		apiStatus = AXI_SUCCESS;
//	UINT	testNumber= 1;			// testNumber is set in MLog_Header(). 
//	int		subTestNum = 1;			// subTestNum is set in MLog_Datalog(); 
	int		subTestNum = testNumber + 1; // STDF requires unique testNumber and subTestNum 
	int		fail_bin = 23;
	char	fBinName[10];			// Fail bin name
	UINT	passMask= 0xFFFF;		// bit mask for each site, default is all sites pass // 0:TestStand displays as failed Test Step.
	UINT	site=0;

// Set datalog Header, testNumber, and fBinName
	sprintf_s(fBinName, 10, "%d", int(fail_bin));		// if gUseBinDisqualification
	formatHeadername(testName, headerName, sizeof (headerName));
    MLog_Header         (testNumber, headerName, fBinName);

	sprintf_s(fBinName, 10, "Bin%d", int(fail_bin));		// if gUseBinDisqualification

	gTimer0Val = TevDiag_GetXTime(0);	// UINT timer, 0, 1, 2 are the 3 timers
	TevUtil_TraceMessage("Start: ContinuityDD48");

//	get_active_site( );		// Update global array


// User code
	UINT    numSites;	// Total active and inactive
	UINT	numChanPerSite;
	double	pmuVoltageLo = -1.80, pmuVoltageHi =  1.80, VIOforceV=1.8;
	double	negCurrent   = -350.0e-6;
	double	posCurrent   =  350.0e-6;
	int		i = 0;

	double	measPeriod = 5.01 us, settleDelay = 100 us;
	int		numSamples=5;


// Set datalog Header, testNumber, and fBinName
	testNumber = 1;
	TevUtil_TraceMessage("Start: SerialWVFMs");

	apiStatus = TevDD_SetActivePatternState(pPHandles[MIPI], FALSE);			// Must have an active pattern

// Set supply pins to 0 volts i.e. GND
	apiStatus = TevDD_DriverMode ("VIO", "PMU");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)
	apiStatus = TevDD_RelayClose ("VIO", "DUT");	// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_PMUForceV  ("VIO",  pmuVoltageHi, pmuVoltageLo, "32mA", 0.0);	// 2uA, 20uA, 200uA, 2mA, 32mA


// Relays are left open for this app when pattern is loaded.
	TevWait_Now(1 ms);
	apiStatus = TevDD_RelayClose ("patternPins", "DUT");	// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_DriverMode ("patternPins", "PMU");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)

	int channels = TevTester_NumberOfPins("patternPins");	// Per site!
	
	double *mvpResultsNeg = new (double[channels*MAX_SITES]);
	double *mvpResultsPos = new (double[channels*MAX_SITES]);
	int		nSamples = 1;

	numSites = TevTester_NumberOfSites();	// Total active and inactive
	numChanPerSite = channels;

	for (i=0; i<channels*MAX_SITES; i++)	// Reset array
	{
		mvpResultsNeg[i] = 0.0;
		mvpResultsPos[i] = 0.0;
	}
	
// Positive Voltage measurement settings @ 350uA
	apiStatus = TevDD_PMUForceI    ("patternPins", pmuVoltageHi, pmuVoltageLo, "2mA",    posCurrent);	// 2uA, 20uA, 200uA, 2mA, 32mA
	apiStatus = TevDD_PMUSetMeasure("patternPins", "MeasureV", settleDelay, measPeriod, numSamples, "CaptureDisable");
	apiStatus = TevDD_PMUGetMeasure("patternPins", mvpResultsPos, 1, NULL);

// Positive Voltage measurement settings @ 350uA
	apiStatus = TevDD_PMUForceI    ("patternPins", pmuVoltageHi, pmuVoltageLo, "2mA",    negCurrent);	// 2uA, 20uA, 200uA, 2mA, 32mA
	apiStatus = TevDD_PMUSetMeasure("patternPins", "MeasureV", settleDelay, measPeriod, numSamples, "CaptureDisable");
	apiStatus = TevDD_PMUGetMeasure("patternPins", mvpResultsNeg, 1, NULL);

// Datalog the results.
//            MLog_Datalog  (mvpName, testNumberIn, testName, mvpResultData[],	units, minLimit, maxLimit, comment, failbinName)
	subTestNum = 1;
	passMask &= MLog_Datalog("patternPins", subTestNum, "CONT_P", mvpResultsPos, "V", -0.20,  0.50, "", fBinName);
	subTestNum = 5;
	passMask &= MLog_Datalog("patternPins", subTestNum, "CONT_N", mvpResultsNeg, "V", -1.00,  0.40, "", fBinName);

// Cleanup	
	delete( mvpResultsNeg);
	delete( mvpResultsPos);

// Reset all PMUs
	apiStatus = TevDD_PMUForceV ("patternPins", pmuVoltageHi, pmuVoltageLo, "2mA",  0.000);	// 2uA, 20uA, 200uA, 2mA, 32mA
	apiStatus = TevDD_DriverMode("patternPins", "OFF");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)
	apiStatus = TevDD_PMUForceV ("VIO",         pmuVoltageHi, pmuVoltageLo, "2mA",  0.000);	// 2uA, 20uA, 200uA, 2mA, 32mA
	apiStatus = TevDD_DriverMode("VIO",         "OFF");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)

//	datalogTimer("Time_CONT", subTestNum++, 0);	// (UINT timer, 0, 1, 2 are the 3 timers), int testNum 

// Disable site if (gFastBinning & !gStopOnFirstFailure)
	MLOG_DisableFailedSites		// also calls MLog_UserDisableSite(siteNumber)

	datalogTimer("Time_ContinuityDD48", subTestNum++, 0);	// (UINT timer, 0, 1, 2 are the 3 timers), int testNum 

//	MLog_Footer	(passMask, headerName, "");				// For enhanced Dlogviewer display of a failed site.

	return passMask;	// Pass/Fail  0:TestStand displays as failed Test Step.
}


// ********************************************************************************
// SerialWaveformsDD48:
// We use DD48 waveforms to send and receive serial data (MIPI format).
// ********************************************************************************
TEST_FUNCT int MIPIWaveformsDD48(UINT testNumber)
{
   	MLOG_CheckFastBinning   // Return if Fastbinning is On and No Enabled Sites
	
	const char* testName = "MIPIWaveformsDD48";
	char  headerName[MAX_HEADER_SIZE] = {""};
	formatHeadername(testName, headerName, sizeof (headerName));

	testNumber= 2500;				// testNumber is set in MLog_Header(). 

	TEV_STATUS		apiStatus = AXI_SUCCESS;
//	UINT	testNumber= 1;			// testNumber is set in MLog_Header(). 
//	int		subTestNum = 1;			// subTestNum is set in MLog_Datalog(); 
	int		subTestNum = testNumber + 1; // STDF requires unique testNumber and subTestNum 
	int		fail_bin = 17;
	char	fBinName[10];			// Fail bin name
	UINT	passMask= 0xFFFF;		// bit mask for each site, default is all sites pass // 0:TestStand displays as failed Test Step.
	UINT	site=0;

// Set datalog Header, testNumber, and fBinName
	sprintf_s(fBinName, 10, "%d", int(fail_bin));		// if gUseBinDisqualification
	formatHeadername(testName, headerName, sizeof (headerName));
    MLog_Header         (testNumber, headerName, fBinName);

	sprintf_s(fBinName, 10, "Bin%d", int(fail_bin));		// if gUseBinDisqualification

	gTimer0Val = TevDiag_GetXTime(0);	// UINT timer, 0, 1, 2 are the 3 timers
	TevUtil_TraceMessage("Start: MIPIWaveformsDD48");

//	get_active_site( );		// Update global array


// User code
	int		i = 0;

	double	pmuVoltageLo = -1.80, pmuVoltageHi =  1.80, VIOforceV=1.8;
	double	result[200][MAX_SITES] = {0};
	unsigned short	sendData[20*MAX_SITES] = {0};	// site1[0], site2[1]
	UINT	dataLength	= 2;						// Must match Length (for each site) as defined in the pattern.
	UINT	bgBit =0;
	double	measPeriod = 5.01 us, settleDelay = 100 us;
	int		numSamples=5;

	unsigned short slaveID = 0xf & 0xF;		// 4 bits
	unsigned short writeCMD= 0xf & 0x7;		// 3 bits
	unsigned short address = 0xf & 0x1F;	// 5 bits
	unsigned short parity0 = 0xf & 0x1;		// 1 bit
	unsigned short regData = 0xf & 0xFF;	// 8 bits
	unsigned short parity1 = 0xf & 0x1;		// 1 bit
	unsigned long  sendWord = 0xfff;	// 22 bits wide

	unsigned long numberElements, offset;
	UINT	defReg = 0;

	BOOL	scopeTrigger = FALSE;

	apiStatus = TevDD_SetActivePatternState(pPHandles[MIPI], FALSE);			// Must have an active pattern
	
// Set supply pins to 0 volts i.e. GND
	apiStatus = TevDD_DriverMode ("VIO", "PMU");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)
	apiStatus = TevDD_RelayClose ("VIO", "DUT");	// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_PMUForceV  ("VIO",  pmuVoltageHi, pmuVoltageLo, "32mA", 0.0);	// 2uA, 20uA, 200uA, 2mA, 32mA


// Relays are left open for this app when pattern is loaded.
	TevWait_Now(1 ms);
	apiStatus = TevDD_RelayClose ("patternPins", "DUT");	// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_DriverMode ("patternPins", "PMU");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)


if (scopeTrigger)
{//	Setup trigger for scope.
	apiStatus = TevDD_TriggerEnablePattern ("S4_DStar_A");					// SEQuencer => DSTARA slot 4
	apiStatus = TevTrigger_ConnectTerminals("S4_DStar_A", "PFI2", false);	// DSTARA slot 4 => PFI2
}


// Setup VIO: Powerup DUT using DD48 resource
	pmuVoltageHi=2.5, pmuVoltageLo=-1.0, VIOforceV=1.8;
	apiStatus = TevDD_DriverMode ("VIO", "PMU");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)
	apiStatus = TevDD_RelayClose ("VIO", "DUT");	// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_PMUForceV  ("VIO",  pmuVoltageHi, pmuVoltageLo, "2mA", 1.8);	// 2uA, 20uA, 200uA, 2mA, 32mA

// If using DPS12
//	TevVI_Gate ("VIO_DPS", "OFF");
//	TevVI_ConnectDUT("VIO_DPS", "REMOTE");
////	TevVI_ForceVoltage  ("VIO_DPS", 1.8, 25.0);
//	TevVI_SetVoltage    ("VIO_DPS", 1.8, 0.025, 25.0, 0.010, -0.010);
//	TevVI_Gate ("VIO_DPS", "ON");
//    TevWait_Now         (settlingTime);
//    TevVI_MeasureCurrent("VIO_DPS", result[10], sizeof (result[0]));


// Relays are left open for this app when pattern is loaded.
	TevWait_Now(1 ms);
	apiStatus = TevDD_RelayClose ("patternPins",  "DUT");			// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_DriverMode ("patternPins",  "DriveSense");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)

// ********************************************************************************
// DevID:   Now we try to read ID Register 0x1D from DUT (MIPI RFFE Read)
// 1. Load SEND memory.
// 2. Run pattern that writes then reads from DUT => RECEIVE memory.
// 3. Retrieve RECEIVE memory and datalog.
// ********************************************************************************
#if 1
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("Capture_Data", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("send9Data",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("send_Data",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti


// MIPI Read ID Register 0x1D  
//	(SEND    memory with 9 bits SerialMSB mode, Same data for all sites.)
//	(RECEIVE memory with 9 bits SerialMSB mode, 8 bits + Parity)

//	Start Sequence (SSC)
//	slaveID = 0xf & 0xF;	// 4 bits	1111 by default
	writeCMD= 0x3 & 0x7;	// 3 bits   Write:010  Read:011
	address = 0x1D & 0x1F;	// 5 bits   C[4:0] Register address to read
	parity0 = 0x1 & 0x1;	// 1 bit    Make for odd sum readCMD + Address
//	busPark == 0;
//	regData = 0xf & 0xFF;	// 8 bits	D[7:0] driven by slave
//	parity1 = 0xf & 0x1;	// 1 bit
//	busPark == 0;
	sendWord = 0xfff;		// 10 bits wide of SEND memory "W" in pattern

// Now build the entire bit sequence
	sendWord = (parity0) + (address<<1) + (writeCMD<<6);

// Setup up C array and load into SEND waveform. Same data for all sites.
	sendData[0] = unsigned short(sendWord & 0x1FF);				//  9 bits
	dataLength = 1;

//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	apiStatus = TevDD_WaveLoad ("send9Data",   sendData, dataLength,    0,      dataLength);

// Now we are ready to write to DUT using SEND memory and read an 8 bit register using RECEIVE memory.
	apiStatus = TevDD_PatternStart("RFFEWriteSR");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, MIPI);

	numberElements=1, offset=0;		// Per site. read 8 bits + parity
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{
		capturedData[siteIndex][0] = capturedData[siteIndex][0] & 0x1FF;	// Mask unwanted bits
		capturedData[siteIndex][0] = capturedData[siteIndex][0] >> 1;		// Mask Parity bit
		result[9][siteIndex] = (double)capturedData[siteIndex][0];		// Convert for datalog
	}
	

// Datalog the results.
//	subTestNum = testNumber + 1;
	passMask &= MLog_Datalog("", subTestNum++, "DevID", result[9], "HEX16",  0.50,  1.50, "", fBinName);
#endif


// ********************************************************************************
// Idd Pre Active Mode (Normal Operation):   Write 0x00(default) to 0x1C
// 1. Load SEND memory.
// 2. Run pattern that writes then measure VIO current
// ********************************************************************************
#if 1
// MIPI Write Register 0x1C  (SEND memory with 18 bits SerialMSB mode, Same data for all sites.)
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("Capture_Data", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("send18Data",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("send_Data",	"StartI");			// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

//	slaveID = 0xf & 0xF;	// 4 bits	1111 by default so use hard coded in pattern.
	writeCMD= 0x2 & 0x7;	// 3 bits   Write:010  Read:011
	address = 0x1C & 0x1F;	// 5 bits   C[4:0] Register address to read
	parity0 = 0x1 & 0x1;	// 1 bit    Make for odd sum readCMD + Address
//	busPark == 0;			// 1 bit	Hard coded in pattern.
	regData = 0x00 & 0xFF;	// 8 bits	D[7:0]
	parity1 = 0x1 & 0x1;	// 1 bit	Make for odd value.
//	busPark == 0;			// 1 bit	Hard coded in pattern.

	sendWord = parity1 + (regData<<1) + (parity0<<9) + (address<<10) + (writeCMD<<15);
	sendData[1] = unsigned short(sendWord & 0xFFFF);			// 16 LSBs (lower byte)
	sendData[0] = unsigned short(sendWord & 0xFFFF0000 >> 16);	//  2 MSBs (upper byte)

//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	apiStatus = TevDD_WaveLoad ("send18Data",   sendData, 2,    0,      dataLength);

	apiStatus = TevDD_PatternStart("RFFEWriteS");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, MIPI);

// Measure VIO current 2mA range
	apiStatus = TevDD_PMUSetMeasure("VIO", "MeasureI", settleDelay, measPeriod, numSamples, "CaptureDisable");
	TevWait_Now(1 ms);
	apiStatus = TevDD_PMUGetMeasure("VIO", result[111], numSamples, NULL);	// Idd

// Datalog
//	subTestNum = testNumber + 1;
	passMask &= MLog_Datalog("",	subTestNum++, "Idd_Pre_Active", 	result[111]   , "uA"    ,  -0.1 uA,   100.0 uA, "", fBinName);
#endif



// Disable site if (gFastBinning & !gStopOnFirstFailure)
//	MLOG_DisableFailedSites		// also calls MLog_UserDisableSite(siteNumber)

// Cleanup  
	TevDD_PatternStop();		// Just in-case
	apiStatus = TevDD_DriverMode ("patternPins", "OFF");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)
	TevWait_Now (400 us);	
	apiStatus = TevDD_PMUForceV  ("VIO",  pmuVoltageHi, pmuVoltageLo, "32mA", 1.8);	// 2uA, 20uA, 200uA, 2mA, 32mA
	TevWait_Now (4 ms);	
	apiStatus = TevDD_DriverMode ("VIO", "OFF");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)

	if (scopeTrigger)
	{	// **ALWAYS** cleanup DSTAR Triggers
		apiStatus = TevTrigger_DisconnectTerminals	("PFI2");		// CIF: Disconnect PFI2
		apiStatus = TevTrigger_DisconnectTerminals	("S4_DStar_A");	// DD48,CIF: Disconnect DSTAR
		apiStatus = TevDD_TriggerDisable			("S4_DStar_A");	// DD48: Disable DSTAR Trigger
	}

	datalogTimer("Time_MIPI", subTestNum++, 0);	// (UINT timer, 0, 1, 2 are the 3 timers), int testNum 

//	MLog_Footer	(passMask, headerName, "");				// For enhanced Dlogviewer display of a failed site.

	return passMask;	// Pass/Fail  0:TestStand displays as failed Test Step.
}




// ********************************************************************************
// SerialWaveformsDD48:
// We use DD48 waveforms to send and receive serial data (SPI format).
/********************************************************************************************
 25LC256 EEPROM (SPI)  (http://ww1.microchip.com/downloads/en/DeviceDoc/21822E.pdf)
 Reg
 Name    Value         Description
 READ    0000 0011     Read data from memory array beginning at selected address
 WRITE   0000 0010     Write data to memory array beginning at selected address
 WRDI    0000 0100     Reset the write enable latch (disable write operations)
 WREN    0000 0110     Set the write enable latch (enable write operations)
 RDSR    0000 0101     Read STATUS register
 WRSR    0000 0001     Write STATUS register

 Pin   Name  Function
  1    CS    Chip Select Input
  2    SO    Serial Data Output
  3    WP    Write-Protect
  4    VSS   Ground
  5    SI    Serial Data Input
  6    SCK   Serial Clock Input
  7    HOLD  Hold Input
  8    VCC   Supply Voltage
*******************************************************************************************/
TEST_FUNCT int SPIWaveformsDD48(UINT testNumber)
{
   	MLOG_CheckFastBinning   // Return if Fastbinning is On and No Enabled Sites
	
	const char* testName = "SPIWaveformsDD48";
	char  headerName[MAX_HEADER_SIZE] = {""};
	formatHeadername(testName, headerName, sizeof (headerName));

	testNumber= 1000;				// testNumber is set in MLog_Header(). 

	TEV_STATUS		apiStatus = AXI_SUCCESS;
//	UINT	testNumber= 1;			// testNumber is set in MLog_Header(). 
//	int		subTestNum = 1;			// subTestNum is set in MLog_Datalog(); 
	int		subTestNum = testNumber + 1; // STDF requires unique testNumber and subTestNum 
	int		fail_bin = 19;
	char	fBinName[10];			// Fail bin name
	UINT	passMask= 0xFFFF;		// bit mask for each site, default is all sites pass // 0:TestStand displays as failed Test Step.
	UINT	site=0;

// Set datalog Header, testNumber, and fBinName
	sprintf_s(fBinName, 10, "%d", int(fail_bin));		// if gUseBinDisqualification
	formatHeadername(testName, headerName, sizeof (headerName));
    MLog_Header         (testNumber, headerName, fBinName);

	sprintf_s(fBinName, 10, "Bin%d", int(fail_bin));		// if gUseBinDisqualification

	gTimer0Val = TevDiag_GetXTime(0);	// UINT timer, 0, 1, 2 are the 3 timers
	TevUtil_TraceMessage("Start: SPIWaveformsDD48");

//	get_active_site( );		// Update global array


// User code
	int		i = 0, loopCnt=64;
	double	Twc = 5 ms;				// Time write cycle as per datasheet.
	char	testDlogName[80];

	double	pmuVoltageLo = -1.80, pmuVoltageHi =  1.80, VIOforceV=1.8;
	double	result[200][MAX_SITES] = {0};
	unsigned short	sendData[20*MAX_SITES] = {0};	// site1[0], site2[1]
	UINT	dataLength	= 2;						// Must match Length (for each site) as defined in the pattern.
	UINT	bgBit =0;
	double	measPeriod = 5.01 us, settleDelay = 100 us;
	int		numSamples=5;

	unsigned short address = 0x0 & 0xFFFF;	//16 bits
	unsigned long numberElements, offset;
	BOOL	scopeTrigger = FALSE;
	BOOL patternModified = FALSE;		// Example to modify "lcnt" in pattern.

// Must have an active pattern before *any* TevDD_ APIs are used.
	apiStatus = TevDD_SetActivePatternState(pPHandles[SPI_25LC256], FALSE);
	
if (scopeTrigger)
{//	Setup trigger for scope.
	apiStatus = TevDD_TriggerEnablePattern ("S4_DStar_A");					// SEQuencer => DSTARA slot 4
	apiStatus = TevTrigger_ConnectTerminals("S4_DStar_A", "PFI2", false);	// DSTARA slot 4 => PFI2
}


// Setup VIO: Powerup DUT using DD48 resource
	pmuVoltageHi=2.5, pmuVoltageLo=-1.0, VIOforceV=1.8;
	apiStatus = TevDD_DriverMode ("VCC", "PMU");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)
	apiStatus = TevDD_RelayClose ("VCC", "DUT");	// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_PMUForceV  ("VCC",  pmuVoltageHi, pmuVoltageLo, "2mA", 3.3);	// 2uA, 20uA, 200uA, 2mA, 32mA

// Relays are left open for this app when pattern is loaded.
	TevWait_Now(1 ms);
	apiStatus = TevDD_RelayClose ("SPIPins",  "DUT");			// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_DriverMode ("SPIPins",  "DriveSense");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)


	
// Simple Write and Read
	apiStatus = TevDD_PatternStart("WriteMem2");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, SPI_25LC256);
	TevWait_Now( Twc);	// Wait for Write Cycle to complete.

	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
	apiStatus = TevDD_PatternStart("ReadMemR2");					// Start Pattern
	checkForPatternTimedOut(50 ms, SPI_25LC256);

	numberElements=2, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]






// ********************************************************************************
// Read Memory Address 0xFFFF
// 1. Run pattern that writes then reads from DUT => RECEIVE memory.
// 2. Retrieve RECEIVE memory and datalog.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
//	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

// Now we are ready to write to DUT using hard coded command and address.
	apiStatus = TevDD_PatternStart("ReadMemR");					// Start Pattern
	checkForPatternTimedOut(50 ms, SPI_25LC256);

	numberElements=1, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{	
		capturedData[siteIndex][0] = capturedData[siteIndex][0] & 0xFF;	// Mask unwanted bits
		result[2][siteIndex] = (double)capturedData[siteIndex][0];		// Convert for datalog
	}

// Datalog the results.
	subTestNum = testNumber + 1;
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x0", result[2], "HEX16",  double(0xAA),  double(0xAA), "ReadMemR", fBinName);




// ********************************************************************************
// WriteMem:   
// Write 7 bytes starting at address 0x000 using hard coded command and starting address.
// ********************************************************************************
	apiStatus = TevDD_PatternStart("WriteMem");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, SPI_25LC256);
	TevWait_Now( Twc);	// Wait for Write Cycle to complete.


// ********************************************************************************
// ReadMemSR:   Command byte is hard coded.
// Read Memory Address using SEND waveform.
// 1. Run pattern that writes then reads from DUT => RECEIVE memory.
// 2. Retrieve RECEIVE memory and datalog.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

	address = 0x0 & 0xFFFF;		//16 bits   Register address to read

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[0] = unsigned short((address & 0xFF00) >> 8);	//  8 MSBs (upper byte)
	sendData[1] = unsigned short(address & 0x00FF);			//  8 LSBs (lower byte)

	dataLength = 2;	// If numberElements == dataArraySize then write same data for *all* sites.

//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 2,    0,      dataLength);

	apiStatus = TevDD_PatternStart("ReadMemSR");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, SPI_25LC256);

	numberElements=7, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{
		for (i=0; i<int(numberElements); i++)
		{
			capturedData[siteIndex][i] = capturedData[siteIndex][i] & 0xFF;	// Mask unwanted bits
			result[i][siteIndex] = (double)capturedData[siteIndex][i];		// Convert for datalog
		}
	}

// Datalog the results.
	subTestNum = testNumber + 10;
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x0", result[0], "HEX16",  double(0x55),  double(0x55), "ReadMemSR", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x1", result[1], "HEX16",  double(0xAA),  double(0xAA), "ReadMemSR", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x2", result[2], "HEX16",  double(0x02),  double(0x02), "ReadMemSR", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x3", result[3], "HEX16",  double(0x03),  double(0x03), "ReadMemSR", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x4", result[4], "HEX16",  double(0x04),  double(0x04), "ReadMemSR", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x5", result[5], "HEX16",  double(0x05),  double(0x05), "ReadMemSR", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x6", result[6], "HEX16",  double(0x06),  double(0x06), "ReadMemSR", fBinName);








// ********************************************************************************
// Check Status Register 
//	7		6	5	4	3		2		1		0
//	W/R		-	-	-	W/R		W/R		R		R
//	WPEN	x	x	x	BP1		BP0		WEL		WIP
//	W/R = writable/readable. R = read-only.
//	WEL: Write Enable Latch
//	WIP: Write In Progress
//
// WriteMemS:   Command byte is hard coded.
// Write Memory Address using SEND waveform.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

	address = 0x0 & 0xFFFF;		//16 bits   Register address to start at.

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[0] = unsigned short((address & 0xFF00) >> 8);	//  8 MSBs (upper ADDR byte)
	sendData[1] = unsigned short( address & 0x00FF );		//  8 LSBs (lower ADDR byte)
	sendData[2] = unsigned short(0x00 & 0x00FF);			//  8 LSBs (byte 0)
	sendData[3] = unsigned short(0x01 & 0x00FF);			//  8 LSBs (byte 1)
	sendData[4] = unsigned short(0x02 & 0x00FF);			//  8 LSBs (byte 2)
	sendData[5] = unsigned short(0x03 & 0x00FF);			//  8 LSBs (byte 3)
	sendData[6] = unsigned short(0x04 & 0x00FF);			//  8 LSBs (byte 4)
	sendData[7] = unsigned short(0x05 & 0x00FF);			//  8 LSBs (byte 5)
	sendData[8] = unsigned short(0x06 & 0x00FF);			//  8 LSBs (byte 6)

	dataLength = 9;	// If numberElements == dataArraySize then write same data for *all* sites.
//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 9,    0,      dataLength);
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 100,    0,      dataLength);

	apiStatus = TevDD_PatternStart("WriteMemS");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, SPI_25LC256);
	TevWait_Now( Twc);	// Wait for Write Cycle to complete.

	apiStatus = TevDD_PatternStart("ReadStatus");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, SPI_25LC256);

	numberElements=1, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{
		capturedData[siteIndex][0] = capturedData[siteIndex][0] & 0xFF;	// Mask unwanted bits
		result[0][siteIndex] = (double)capturedData[siteIndex][0];		// Convert for datalog
	}

// Datalog the results.
	subTestNum = testNumber + 50;
	passMask &= MLog_Datalog("", subTestNum++, "STATUSreg", result[0], "HEX16",  0.00,  0.00, "WriteMemS", fBinName);

	apiStatus = TevDD_PatternStart("WriteEnable");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, SPI_25LC256);
//	TevWait_Now( 10 ms);	// Wait for Write Cycle to complete.
	apiStatus = TevDD_PatternStart("ReadStatus");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, SPI_25LC256);

	numberElements=1, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{
		capturedData[siteIndex][0] = capturedData[siteIndex][0] & 0xFF;	// Mask unwanted bits
		result[0][siteIndex] = (double)capturedData[siteIndex][0];		// Convert for datalog
	}

// Datalog the results.
	passMask &= MLog_Datalog("", subTestNum++, "STATUSreg", result[0], "HEX16",  2.00,  2.00, "WriteEnable", fBinName);




// ********************************************************************************
// ReadMemSR:   Command byte is hard coded.
// Read Memory Address using SEND waveform.
// 1. Run pattern that writes then reads from DUT => RECEIVE memory.
// 2. Retrieve RECEIVE memory and datalog.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

	address = 0x0 & 0xFFFF;		//16 bits   Register address to read

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[0] = unsigned short((address & 0xFF00) >> 8);	//  8 MSBs (upper byte)
	sendData[1] = unsigned short(address & 0x00FF);			//  8 LSBs (lower byte)

	dataLength = 2;	// If numberElements == dataArraySize then write same data for *all* sites.

//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 2,    0,      dataLength);

	apiStatus = TevDD_PatternStart("ReadMemSR");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, SPI_25LC256);

	numberElements=7, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{
		for (i=0; i<int(numberElements); i++)
		{		
			capturedData[siteIndex][i] = capturedData[siteIndex][i] & 0xFF;	// Mask unwanted bits
			result[i][siteIndex] = (double)capturedData[siteIndex][i];		// Convert for datalog
		}
	}

// Datalog the results.
	subTestNum = testNumber + 60;
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x0", result[0], "HEX16",  double(0x00),  double(0x00), "ReadMemSR", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x1", result[1], "HEX16",  double(0x01),  double(0x01), "ReadMemSR", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x2", result[2], "HEX16",  double(0x02),  double(0x02), "ReadMemSR", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x3", result[3], "HEX16",  double(0x03),  double(0x03), "ReadMemSR", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x4", result[4], "HEX16",  double(0x04),  double(0x04), "ReadMemSR", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x5", result[5], "HEX16",  double(0x05),  double(0x05), "ReadMemSR", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x6", result[6], "HEX16",  double(0x06),  double(0x06), "ReadMemSR", fBinName);




// ********************************************************************************
// WriteMemSL:   Command byte is hard coded.
// Write Memory Address using SEND waveform.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

	address = 0x0 & 0xFFFF;		//16 bits   Register address to start at.

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[0] = unsigned short((address & 0xFF00) >> 8);	//  8 MSBs (upper ADDR byte)
	sendData[1] = unsigned short( address & 0x00FF );		//  8 LSBs (lower ADDR byte)

	for (i=0; i<loopCnt; i++)
	{
		sendData[2+i] = unsigned short(i & 0x00FF);			//  8 LSBs (byte 0)
	}

	dataLength = 2+loopCnt;	// If numberElements == dataArraySize then write same data for *all* sites.
//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 100,    0,      dataLength);

	apiStatus = TevDD_PatternStart("WriteMemSL");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, SPI_25LC256);
	TevWait_Now( Twc);	// Wait for Write Cycle to complete.


// ********************************************************************************
// ReadMemSRL:   Command byte is hard coded.
// Read Memory Address using SEND waveform.
// 1. Run pattern that writes then reads from DUT => RECEIVE memory.
// 2. Retrieve RECEIVE memory and datalog.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

	address = 0x0 & 0xFFFF;		//16 bits   Register address to read

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[0] = unsigned short((address & 0xFF00) >> 8);		//  8 MSBs (upper byte)
	sendData[1] = unsigned short( address & 0x00FF );			//  8 LSBs (lower byte)

//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	dataLength = 2;	// If numberElements == dataArraySize then write same data for *all* sites.
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 2,    0,      dataLength);

	apiStatus = TevDD_PatternStart("ReadMemSRL");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, SPI_25LC256);

	numberElements=loopCnt, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{
		for (i=0; i<loopCnt; i++)
		{
			capturedData[siteIndex][i] = capturedData[siteIndex][i] & 0xFF;	// Mask unwanted bits
			result[i][siteIndex] = (double)capturedData[siteIndex][i];		// Convert for datalog
		}
	}

// Datalog the results.
	subTestNum = testNumber + 70;
	for (i=0; i<loopCnt; i++)
	{
		sprintf_s(testDlogName, sizeof(testDlogName), "Mem 0x%x", address+i);
		passMask &= MLog_Datalog("", subTestNum++, testDlogName, result[i], "HEX16",  double(address+i),  double(address+i), "ReadMemSRL full page", fBinName);
	}









// ********************************************************************************
// Example on how to modify the loop counter in the pattern 
// so we can select to write from 1 to 64 bytes at a time.
//	Use pattern that had ExecMode flag set "LoadForEdit" when loaded in ApplicationLoad().
//		if (loadForEdit)
//			{	// Keep a copy in CPU for run time pattern modification.
//				status = TevDD_SetExecMode("LoadForEdit", "1");
//				status = TevDD_SetExecMode("ReadHW", "1");
//			}
// ********************************************************************************

// Here we keep the pins settings unchanged since they are used the same for this apttern as well.
	apiStatus = TevDD_SetActivePatternState(pPHandles[SPI_25LC256_LFE], TRUE);			// Must have an active pattern

	if (0)
	{
// Setup VIO: Powerup DUT using DD48 resource
	pmuVoltageHi=2.5, pmuVoltageLo=-1.0, VIOforceV=1.8;
	apiStatus = TevDD_DriverMode ("VCC", "PMU");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)
	apiStatus = TevDD_RelayClose ("VCC", "DUT");	// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_PMUForceV  ("VCC",  pmuVoltageHi, pmuVoltageLo, "2mA", 3.3);	// 2uA, 20uA, 200uA, 2mA, 32mA

// Relays are left open for this app when pattern is loaded.
	TevWait_Now(1 ms);
	apiStatus = TevDD_RelayClose ("SPIPins",  "DUT");			// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_DriverMode ("SPIPins",  "DriveSense");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)
	}

// Lets change "lcnt 64" in the pattern in CPU memory to "lcnt 12" and then reload down to DD48.
// Once the local pattern image in DD48 memory has been modified we can skip this step.
// First we do the WriteMemSL: then ReadMemSRL:

	loopCnt = 12;

	if (!gPatternModified_SPI)
	{
		char	newOpcode[32] = "";
		char	vData[32]= "";
		unsigned long indexVec;

		sprintf_s(newOpcode, sizeof(newOpcode), "lcnt %d", loopCnt);

	// WriteMemSL:
		apiStatus = TevDD_GetVectorLabelIndex ("LblLcnt1", &indexVec);	// Zero based

		// type: "OpCode", "Tset", "PinData", "LoopCount", RepCount"
		apiStatus = TevDD_GetVectorData (NULL, "Opcode", indexVec, 1, &vData[0], sizeof(vData));	// Current Opcode
		apiStatus = TevDD_SetVectorData (NULL, "Opcode", indexVec, 1, (char*)newOpcode, int(strlen(newOpcode)) );
		apiStatus = TevDD_PatternUpdate ("$seq", indexVec, 1 );
		// Check updated Opcode
		apiStatus = TevDD_GetVectorData (NULL, "Opcode", indexVec, 1, &vData[0], sizeof(vData));

	// ReadMemSRL:
		apiStatus = TevDD_GetVectorLabelIndex ("LblLcnt2", &indexVec);	// Zero based

		// type: "OpCode", "Tset", "PinData", "LoopCount", RepCount"
		apiStatus = TevDD_GetVectorData (NULL, "Opcode", indexVec, 1, &vData[0], sizeof(vData));	// Current Opcode
		apiStatus = TevDD_SetVectorData (NULL, "Opcode", indexVec, 1, (char*)newOpcode, int(strlen(newOpcode)) );
		apiStatus = TevDD_PatternUpdate ("$seq", indexVec, 1 );
		// Check updated Opcode
		apiStatus = TevDD_GetVectorData (NULL, "Opcode", indexVec, 1, &vData[0], sizeof(vData));

		gPatternModified_SPI = TRUE;
	}

// Now we run as normal....


// ********************************************************************************
// WriteMemSL:   Command byte is hard coded.
// Write Memory Address using SEND waveform.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

//	writeCMD= 0x3 & 0x7;		// 8 bits   Write:0x2  Read:0x3
	address = 0x0 & 0xFFFF;		//16 bits   Register address to start at.

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[0] = unsigned short((address & 0xFF00) >> 8);	//  8 MSBs (upper ADDR byte)
	sendData[1] = unsigned short( address & 0x00FF );		//  8 LSBs (lower ADDR byte)

	for (i=0; i<loopCnt; i++)
	{
		sendData[2+i] = unsigned short(i*2 & 0x00FF);			//  8 LSBs (byte 0)
	}

	dataLength = 2+loopCnt;	// If numberElements == dataArraySize then write same data for *all* sites.
//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 100,    0,      dataLength);

	apiStatus = TevDD_PatternStart("WriteMemSL");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, SPI_25LC256_LFE);
	TevWait_Now( Twc);	// Wait for Write Cycle to complete.

// ********************************************************************************
// ReadMemSRL:   Command byte is hard coded.
// Read Memory Address using SEND waveform.
// 1. Run pattern that writes then reads from DUT => RECEIVE memory.
// 2. Retrieve RECEIVE memory and datalog.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

	address = 0x0 & 0xFFFF;		//16 bits   Register address to read

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[0] = unsigned short((address & 0xFF00) >> 8);		//  8 MSBs (upper byte)
	sendData[1] = unsigned short( address & 0x00FF );			//  8 LSBs (lower byte)

//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	dataLength = 2;	// If numberElements == dataArraySize then write same data for *all* sites.
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 2,    0,      dataLength);

	apiStatus = TevDD_PatternStart("ReadMemSRL");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, SPI_25LC256_LFE);

	numberElements=loopCnt, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{
		for (i=0; i<loopCnt; i++)
		{
			capturedData[siteIndex][i] = capturedData[siteIndex][i] & 0xFF;	// Mask unwanted bits
			result[i][siteIndex] = (double)capturedData[siteIndex][i];		// Convert for datalog
		}
	}


// Datalog the results.
	subTestNum = testNumber + 200;
	for (i=0; i<loopCnt; i++)
	{
		double limit = i*2;
		sprintf_s(testDlogName, sizeof(testDlogName), "Mem 0x%x", address+i);
		passMask &= MLog_Datalog("", subTestNum++, testDlogName, result[i], "HEX16",  limit,  limit, "ReadMemSRL_mod", fBinName);
	}


// Cleanup
	TevDD_PatternStop();		// Just in-case
	apiStatus = TevDD_DriverMode ("SPIPins", "OFF");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)
	TevWait_Now (400 us);	
	apiStatus = TevDD_PMUForceV  ("VCC",  pmuVoltageHi, pmuVoltageLo, "32mA", 0.0);	// 2uA, 20uA, 200uA, 2mA, 32mA
	TevWait_Now (4 ms);	
	apiStatus = TevDD_DriverMode ("VCC", "OFF");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)

	if (scopeTrigger)
	{	// **ALWAYS** cleanup DSTAR Triggers
		apiStatus = TevTrigger_DisconnectTerminals	("PFI2");		// CIF: Disconnect PFI2
		apiStatus = TevTrigger_DisconnectTerminals	("S4_DStar_A");	// DD48,CIF: Disconnect DSTAR
		apiStatus = TevDD_TriggerDisable			("S4_DStar_A");	// DD48: Disable DSTAR Trigger
	}

// Disable site if (gFastBinning & !gStopOnFirstFailure)
//	MLOG_DisableFailedSites		// also calls MLog_UserDisableSite(siteNumber)

	datalogTimer("Time_SPI", subTestNum++, 0);	// (UINT timer, 0, 1, 2 are the 3 timers), int testNum 

//	MLog_Footer	(passMask, headerName, "");				// For enhanced Dlogviewer display of a failed site.

	return passMask;	// Pass/Fail  0:TestStand displays as failed Test Step.
}





/* ********************************************************************************
// SerialWaveformsDD48:
// We use DD48 waveforms to send and receive serial data (SPI format).
;********************************************************************************************
; 24LC256 EEPROM (I2C)  (http://ww1.microchip.com/downloads/en/DeviceDoc/21203M.pdf)
;
;         8-pin   8-pin   8-pin   14-pin  8-pin   8-pin
; Name    PDIP    SOIC    TSSOP   TSSOP   MSOP    DFN     Function
;  A0     1       1       1       1              1       User Configurable Chip Select
;  A1     2       2       2       2              2       User Configurable Chip Select
; (NC)                         3,4,5   1,2            Not Connected
;  A2     3       3       3       6       3       3       User Configurable Chip Select
;  VSS    4       4       4       7       4       4       Ground
;  SDA    5       5       5       8       5       5       Serial Data
;  SCL    6       6       6       9       6       6       Serial Clock
; (NC)                         10,11,12              Not Connected
;  WP     7       7       7       13      7       7       Write-Protect Input
;  VCC    8       8       8       14      8       8       +1.8V to 5.5V (24AA256)
;                                                         +2.5V to 5.5V (24LC256)
;                                                         +1.8V to 5.5V (24FC256)

;VOL    Low-level output voltage       0.40 V      IOL = 3.0 ma @ VCC = 4.5V
;                                                   IOL = 2.1 ma @ VCC = 2.5V

;ICC Read  Operating current           400 uA      VCC = 5.5V, SCL = 400 kHz
;ICC Write Operating current           3   mA      VCC = 5.5V
;********************************************************************************************/
// ********************************************************************************
TEST_FUNCT int I2CWaveformsDD48(UINT testNumber)
{
   	MLOG_CheckFastBinning   // Return if Fastbinning is On and No Enabled Sites
	
	const char* testName = "I2CWaveformsDD48";
	char  headerName[MAX_HEADER_SIZE] = {""};
	formatHeadername(testName, headerName, sizeof (headerName));

	testNumber= 2000;				// testNumber is set in MLog_Header(). 

	TEV_STATUS		apiStatus = AXI_SUCCESS;
//	UINT	testNumber= 1;			// testNumber is set in MLog_Header(). 
//	int		subTestNum = 1;			// subTestNum is set in MLog_Datalog(); 
	int		subTestNum = testNumber + 1; // STDF requires unique testNumber and subTestNum 
	int		fail_bin = 18;
	char	fBinName[10];			// Fail bin name
	UINT	passMask= 0xFFFF;		// bit mask for each site, default is all sites pass // 0:TestStand displays as failed Test Step.
	UINT	site=0;

// Set datalog Header, testNumber, and fBinName
	sprintf_s(fBinName, 10, "%d", int(fail_bin));		// if gUseBinDisqualification
	formatHeadername(testName, headerName, sizeof (headerName));
    MLog_Header         (testNumber, headerName, fBinName);

	sprintf_s(fBinName, 10, "Bin%d", int(fail_bin));		// if gUseBinDisqualification

	gTimer0Val = TevDiag_GetXTime(0);	// UINT timer, 0, 1, 2 are the 3 timers
	TevUtil_TraceMessage("Start: I2CWaveformsDD48");

//	get_active_site( );		// Update global array


// User code
	char	testDlogName[30]={""};
	int		i = 0, loopCnt=64;
	double	Twc = 5 ms;				// Time write cycle as per datasheet.

	double	pmuVoltageLo = -1.80, pmuVoltageHi =  1.80, VIOforceV=1.8;
	double	result[200][MAX_SITES] = {0};
	unsigned short	sendData[20*MAX_SITES] = {0};	// site1[0], site2[1]
	UINT	dataLength	= 2;						// Must match Length (for each site) as defined in the pattern.
	UINT	bgBit =0;
	double	measPeriod = 5.01 us, settleDelay = 100 us;
	int		numSamples=5;

	unsigned short writeCMD= 0x02 & 0xFF;	// 8 bits
	unsigned short address = 0x0 & 0xFFFF;	//16 bits
	unsigned short regData = 0xf & 0xFF;	// 8 bits
	unsigned long  sendWord = 0xfff;		// 4x8=32 bits wide

	unsigned long numberElements, offset;
	UINT	defReg = 0;

	BOOL	scopeTrigger = FALSE;

	UINT	thisSite=1;
//	unsigned long	numVectorFails       [MAX_SITES];
//	double			resultsVectorFails   [MAX_SITES];

	apiStatus = TevDD_SetActivePatternState(pPHandles[I2C_24LC256], FALSE);			// Must have an active pattern
	
if (scopeTrigger)
{//	Setup trigger for scope.
	apiStatus = TevDD_TriggerEnablePattern ("S4_DStar_A");					// SEQuencer => DSTARA slot 4
	apiStatus = TevTrigger_ConnectTerminals("S4_DStar_A", "PFI2", false);	// DSTARA slot 4 => PFI2
}


// Setup VCC: Powerup DUT using DD48 resource
	pmuVoltageHi=2.5, pmuVoltageLo=-1.0, VIOforceV=1.8;
	apiStatus = TevDD_DriverMode ("VCC", "PMU");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)
	apiStatus = TevDD_RelayClose ("VCC", "DUT");	// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_PMUForceV  ("VCC",  pmuVoltageHi, pmuVoltageLo, "32mA", 3.3);	// 2uA, 20uA, 200uA, 2mA, 32mA

// Relays are left open for this app when pattern is loaded.
	TevWait_Now(1 ms);
	apiStatus = TevDD_RelayClose ("I2CPins",  "DUT");			// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_DriverMode ("I2CPins",  "DriveSense");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)

// DEBUG*********************************************
if (0)
{
	apiStatus = TevDD_PatternStart("WriteByte");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, I2C_24LC256);
	TevWait_Now( Twc);	// Wait for Write Cycle to complete.

	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
	apiStatus = TevDD_PatternStart("ReadCurrentAddr");				// Start Pattern
	checkForPatternTimedOut(50 ms, I2C_24LC256);
	numberElements=1, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]
}

if (0)
{
	apiStatus = TevDD_PatternStart("WritePage7");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, I2C_24LC256);
	TevWait_Now( Twc);	// Wait for Write Cycle to complete.

	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
	apiStatus = TevDD_PatternStart("ReadSeq7");					// Start Pattern
	checkForPatternTimedOut(50 ms, I2C_24LC256);
	numberElements=7, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{
		capturedData[siteIndex][0] = capturedData[siteIndex][0] & 0xFF;	// Mask unwanted bits
		result[2][siteIndex] = (double)capturedData[siteIndex][0];		// Convert for datalog
	}

// Datalog the results.
	subTestNum = testNumber + 1;
	passMask &= MLog_Datalog("", subTestNum, "Mem 0x1", result[2], "HEX16",  0.00,  256.0, "ReadMemR", fBinName);
}
// DEBUG*********************************************




// ********************************************************************************
// WriteByte:
// 1. Run pattern that writes to EEPROM.
// 2. Check for pattern failures ACKnowledge bits.
// ********************************************************************************
#if 0
// Now we are ready to write to DUT using hard coded command and address.
	apiStatus = TevDD_PatternStart("WriteByte");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, I2C_24LC256);
	TevWait_Now( Twc);	// Wait for Write Cycle to complete.

// Check ACKnowledge bits
// Note: SEQuencer for each DD48 will report failed vectors for all sites 
//       on that DD48 as a group. i.e. not per site. 
//       If you have more that one site per DD48 you will need to run them 
//       sequencially if need to get vector fails per site.
#if 0
	apiStatus = TevDD_NumberFailedVectors(numVectorFails);	// [0]site1, [1]site2, ...

// Now datalog Summary of Vector Failures
// Convert result to a double for datalogging
	for (thisSite = 0; thisSite < MAX_SITES; thisSite++)
	{
		if (active_site[thisSite]) 
		{
			resultsVectorFails [thisSite] = double(numVectorFails[thisSite]);
		}
	}
	passMask &= MLog_Datalog("", subTestNum++, "ACKnowledge",  resultsVectorFails   , "HEX16", 0, 0, "", fBinName);
#endif


// Measure VCC current (32mA range)
	apiStatus = TevDD_PMUSetMeasure("VCC", "MeasureI", settleDelay, measPeriod, numSamples, "CaptureDisable");
	TevWait_Now(1 ms);
	apiStatus = TevDD_PMUGetMeasure("VCC", result[111], numSamples, NULL);	// Idd

// Datalog the results.
	subTestNum = 1;
	passMask &= MLog_Datalog("",	subTestNum++, "Idd", 	      result[111], "uA"   ,  -0.1 uA,   100.0 uA, "", fBinName);
#endif




// ********************************************************************************
// Page Write up to 64 bytes  "WritePage7"
// Write 7 bytes starting at address 0x000 using hard coded command and starting address.
// 1. Run pattern that writes to EEPROM.
// 2. Don't Check for pattern failures ACKnowledge bits.
// ********************************************************************************
// Now we are ready to write to DUT using hard coded command and address.
#if 1
	apiStatus = TevDD_PatternStart("WritePage7");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, I2C_24LC256);
	TevWait_Now( Twc);	// Wait for Write Cycle to complete.

// Check ACKnowledge bits
// Note: SEQuencer for each DD48 will report failed vectors for all sites 
//       on that DD48 as a group. i.e. not per site. 
//       If you have more that one site per DD48 you will need to run them 
//       sequencially if need to get vector fails per site.
#if 0
	apiStatus = TevDD_NumberFailedVectors(numVectorFails);	// [0]site1, [1]site2, ...

// Now datalog Summary of Vector Failures
// Convert result to a double for datalogging
	for (thisSite = 0; thisSite < MAX_SITES; thisSite++)
	{
		if (active_site[thisSite]) 
		{
			resultsVectorFails [thisSite] = double(numVectorFails[thisSite]);
		}
	}
	passMask &= MLog_Datalog("", subTestNum++, "ACKnowledge",  resultsVectorFails   , "HEX16", 0, 0, "", fBinName);
#endif


// ***********************************
// ReadSeq7R:
// 1. Run pattern that writes then reads from DUT => RECEIVE memory.
// 2. Retrieve RECEIVE memory and datalog.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
//	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

// Now we are ready to write to DUT using hard coded command and address.
	apiStatus = TevDD_PatternStart("ReadSeq7R");					// Start Pattern
	checkForPatternTimedOut(50 ms, I2C_24LC256);

// Check ACKnowledge bits
// Note: SEQuencer for each DD48 will report failed vectors for all sites 
//       on that DD48 as a group. i.e. not per site. 
//       If you have more that one site per DD48 you will need to run them 
//       sequencially if need to get vector fails per site.
#if 0
	apiStatus = TevDD_NumberFailedVectors(numVectorFails);	// [0]site1, [1]site2, ...
#endif

	loopCnt = 7;
	numberElements=loopCnt, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{
		for (i=0; i<loopCnt; i++)
		{
			capturedData[siteIndex][i] = capturedData[siteIndex][i] & 0xFF;	// Mask unwanted bits
			result[i][siteIndex] = (double)capturedData[siteIndex][i];		// Convert for datalog
		}
	}

// Datalog the results.
	subTestNum = testNumber + 1;
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x0", result[0], "HEX16",  double(0x55),  double(0x55), "ReadSeq7R", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x1", result[1], "HEX16",  double(0xAA),  double(0xAA), "ReadSeq7R", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x2", result[2], "HEX16",  double(0x02),  double(0x02), "ReadSeq7R", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x3", result[3], "HEX16",  double(0x03),  double(0x03), "ReadSeq7R", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x4", result[4], "HEX16",  double(0x04),  double(0x04), "ReadSeq7R", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x5", result[5], "HEX16",  double(0x05),  double(0x05), "ReadSeq7R", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x6", result[6], "HEX16",  double(0x06),  double(0x06), "ReadSeq7R", fBinName);
#endif




#if 1
// ********************************************************************************
// ReadSeq7SR:   Command byte is hard coded.
// Read Memory Address using SEND waveform.
// 1. Run pattern that writes then reads from DUT => RECEIVE memory.
// 2. Retrieve RECEIVE memory and datalog.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

	address = 0x0000 & 0x7FFF;		//16 bits   Register address to read

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[0] = unsigned short((address & 0xFF00) >> 8);		//  8 MSBs (upper byte)
	sendData[1] = unsigned short( address & 0x00FF );			//  8 LSBs (lower byte)

//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	dataLength = 2;	// If numberElements == dataArraySize then write same data for *all* sites.
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 2,    0,      dataLength);

	apiStatus = TevDD_PatternStart("ReadSeq7SR");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, I2C_24LC256);

	loopCnt = 7;
	numberElements=loopCnt, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{
		for (i=0; i<loopCnt; i++)
		{
			capturedData[siteIndex][i] = capturedData[siteIndex][i] & 0xFF;	// Mask unwanted bits
			result[i][siteIndex] = (double)capturedData[siteIndex][i];		// Convert for datalog
		}
	}

// Datalog the results.
	subTestNum = testNumber + 10;

	sprintf_s(testDlogName, sizeof(testDlogName), "Mem 0x%x", address+0);
	passMask &= MLog_Datalog("", subTestNum++, testDlogName, result[0], "HEX16",  double(0x55),  double(0x55), "ReadSeq7SR", fBinName);

	sprintf_s(testDlogName, sizeof(testDlogName), "Mem 0x%x", address+1);
	passMask &= MLog_Datalog("", subTestNum++, testDlogName, result[1], "HEX16",  double(0xAA),  double(0xAA), "ReadSeq7SR", fBinName);

	sprintf_s(testDlogName, sizeof(testDlogName), "Mem 0x%x", address+2);
	passMask &= MLog_Datalog("", subTestNum++, testDlogName, result[2], "HEX16",  double(0x02),  double(0x02), "ReadSeq7SR", fBinName);

	sprintf_s(testDlogName, sizeof(testDlogName), "Mem 0x%x", address+3);
	passMask &= MLog_Datalog("", subTestNum++, testDlogName, result[3], "HEX16",  double(0x03),  double(0x03), "ReadSeq7SR", fBinName);

	sprintf_s(testDlogName, sizeof(testDlogName), "Mem 0x%x", address+4);
	passMask &= MLog_Datalog("", subTestNum++, testDlogName, result[4], "HEX16",  double(0x04),  double(0x04), "ReadSeq7SR", fBinName);

	sprintf_s(testDlogName, sizeof(testDlogName), "Mem 0x%x", address+5);
	passMask &= MLog_Datalog("", subTestNum++, testDlogName, result[5], "HEX16",  double(0x05),  double(0x05), "ReadSeq7SR", fBinName);

	sprintf_s(testDlogName, sizeof(testDlogName), "Mem 0x%x", address+6);
	passMask &= MLog_Datalog("", subTestNum++, testDlogName, result[6], "HEX16",  double(0x06),  double(0x06), "ReadSeq7SR", fBinName);
#endif





#if 1
// ********************************************************************************
// WritePage7S:   Command byte is hard coded.
// Write Memory Address using SEND waveform.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

	address = 0x0000 & 0x7FFF;		//16 bits   Register address to start at.

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[8] = unsigned short(0x06 & 0x00FF);			//  8 LSBs (byte 6)
	sendData[7] = unsigned short(0x05 & 0x00FF);			//  8 LSBs (byte 5)
	sendData[6] = unsigned short(0x04 & 0x00FF);			//  8 LSBs (byte 4)
	sendData[5] = unsigned short(0x03 & 0x00FF);			//  8 LSBs (byte 3)
	sendData[4] = unsigned short(0x02 & 0x00FF);			//  8 LSBs (byte 2)
	sendData[3] = unsigned short(0x01 & 0x00FF);			//  8 LSBs (byte 1)
	sendData[2] = unsigned short(0x00 & 0x00FF);			//  8 LSBs (byte 0)
	sendData[1] = unsigned short(address & 0x00FF);			//  8 LSBs (lower ADDR byte)
	sendData[0] = unsigned short((address & 0xFF00) >> 8);	//  8 MSBs (upper ADDR byte)

// Optional SEND command byte as well.
//	sendData[x] = unsigned short(0x00 & 0x00FF);			//  8 LSBs (lower COMMAND byte)
//	sendData[x] = unsigned short(0x05 & 0xFF00);			//  8 MSBs (upper COMMAND byte)

	dataLength = 9;	// If numberElements == dataArraySize then write same data for *all* sites.

//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 9,    0,      dataLength);

	apiStatus = TevDD_PatternStart("WritePage7S");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, I2C_24LC256);
	TevWait_Now( Twc);	// Wait for Write Cycle to complete.


// ReadSeq7SR:   Command byte is hard coded.
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[0] = unsigned short((address & 0xFF00) >> 8);	//  8 MSBs (upper byte)
	sendData[1] = unsigned short(address & 0x00FF);			//  8 LSBs (lower byte)

	dataLength = 2;	// If numberElements == dataArraySize then write same data for *all* sites.

//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 2,    0,      dataLength);

	apiStatus = TevDD_PatternStart("ReadSeq7SR");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, I2C_24LC256);

	numberElements=7, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{
		for (i=0; i<int(numberElements); i++)
		{
			capturedData[siteIndex][i] = capturedData[siteIndex][i] & 0xFF;	// Mask unwanted bits
			result[i][siteIndex] = (double)capturedData[siteIndex][i];		// Convert for datalog
		}
	}

// Datalog the results.
	subTestNum = testNumber + 20;
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x0", result[0], "HEX16",  double(0x00),  double(0x00), "WritePage7S", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x1", result[1], "HEX16",  double(0x01),  double(0x01), "WritePage7S", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x2", result[2], "HEX16",  double(0x02),  double(0x02), "WritePage7S", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x3", result[3], "HEX16",  double(0x03),  double(0x03), "WritePage7S", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x4", result[4], "HEX16",  double(0x04),  double(0x04), "WritePage7S", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x5", result[5], "HEX16",  double(0x05),  double(0x05), "WritePage7S", fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Mem 0x6", result[6], "HEX16",  double(0x06),  double(0x06), "WritePage7S", fBinName);
#endif




// ********************************************************************************
// WritePageSL:   Command byte is hard coded.
// Write Memory Address using SEND waveform.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

	address = 0x0 & 0xFFFF;		//16 bits   Register address to start at.

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[0] = unsigned short((address & 0xFF00) >> 8);	//  8 MSBs (upper ADDR byte)
	sendData[1] = unsigned short( address & 0x00FF );		//  8 LSBs (lower ADDR byte)

	loopCnt = 64;

	for (i=0; i<loopCnt; i++)
	{
		sendData[2+i] = unsigned short(i & 0x00FF);			//  8 LSBs (byte 0)
	}

	dataLength = 2+loopCnt;	// If numberElements == dataArraySize then write same data for *all* sites.
//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 100,    0,      dataLength);

	apiStatus = TevDD_PatternStart("WritePageSL");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, I2C_24LC256);
	TevWait_Now( Twc);	// Wait for Write Cycle to complete.


// ********************************************************************************
// ReadSeqSRL:   Command byte is hard coded.
// Read Memory Address using SEND waveform.
// 1. Run pattern that writes then reads from DUT => RECEIVE memory.
// 2. Retrieve RECEIVE memory and datalog.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

//	address = 0x0000 & 0xFFFF;		//16 bits   Register address to read

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[0] = unsigned short((address & 0xFF00) >> 8);		//  8 MSBs (upper byte)
	sendData[1] = unsigned short( address & 0x00FF );			//  8 LSBs (lower byte)

//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	dataLength = 2;	// If numberElements == dataArraySize then write same data for *all* sites.
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 2,    0,      dataLength);

	apiStatus = TevDD_PatternStart("ReadSeqSRL");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, I2C_24LC256);

	numberElements=loopCnt, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{
		for (i=0; i<loopCnt; i++)
		{
			capturedData[siteIndex][i] = capturedData[siteIndex][i] & 0xFF;	// Mask unwanted bits
			result[i][siteIndex] = (double)capturedData[siteIndex][i];		// Convert for datalog
		}
	}

// Datalog the results.
	subTestNum = testNumber + 30;
	for (i=0; i<loopCnt; i++)
	{
		sprintf_s(testDlogName, sizeof(testDlogName), "Mem 0x%x", address+i);
		passMask &= MLog_Datalog("", subTestNum++, testDlogName, result[i], "HEX16",  double(address+i),  double(address+i), "ReadSeqSRL full page", fBinName);
	}








// ********************************************************************************
// Example on how to modify the loop counter in the pattern 
// so we can select to write from 1 to 64 bytes at a time.
//	Use pattern that had ExecMode flag set "LoadForEdit" when loaded in ApplicationLoad().
//		if (loadForEdit)
//			{	// Keep a copy in CPU for run time pattern modification.
//				status = TevDD_SetExecMode("LoadForEdit", "1");
//				status = TevDD_SetExecMode("ReadHW", "1");
//			}
// ********************************************************************************

// Here we keep the pins settings unchanged since they are used the same for this apttern as well.
	apiStatus = TevDD_SetActivePatternState(pPHandles[I2C_24LC256_LFE], TRUE);			// Must have an active pattern

	if (0)
	{
// Setup VIO: Powerup DUT using DD48 resource
	pmuVoltageHi=2.5, pmuVoltageLo=-1.0, VIOforceV=1.8;
	apiStatus = TevDD_DriverMode ("VCC", "PMU");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)
	apiStatus = TevDD_RelayClose ("VCC", "DUT");	// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_PMUForceV  ("VCC",  pmuVoltageHi, pmuVoltageLo, "2mA", 3.3);	// 2uA, 20uA, 200uA, 2mA, 32mA

// Relays are left open for this app when pattern is loaded.
	TevWait_Now(1 ms);
	apiStatus = TevDD_RelayClose ("I2CPins",  "DUT");			// DUT|Hiv|Bus|CalBusForceH|CalBusForceL|CalBusSenseH|CalBusForceH
	apiStatus = TevDD_DriverMode ("I2CPins",  "DriveSense");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)
	}

// Lets change "lcnt 64" in the pattern in CPU memory to "lcnt 12" and then reload down to DD48.
// Once the local pattern image in DD48 memory has been modified we can skip this step.
// First we do the WriteMemSL: then ReadMemSRL:

	loopCnt = 12;

	if (!gPatternModified_I2C)
	{
		char	newOpcode[32] = "";
		char	vData[32]= "";
		unsigned long indexVec;

		sprintf_s(newOpcode, sizeof(newOpcode), "lcnt %d", loopCnt);

	// WriteMemSL:
		apiStatus = TevDD_GetVectorLabelIndex ("LblLcnt1", &indexVec);	// Zero based

		// type: "OpCode", "Tset", "PinData", "LoopCount", RepCount"
		apiStatus = TevDD_GetVectorData (NULL, "Opcode", indexVec, 1, &vData[0], sizeof(vData));	// Current Opcode
		apiStatus = TevDD_SetVectorData (NULL, "Opcode", indexVec, 1, (char*)newOpcode, int(strlen(newOpcode)) );
		apiStatus = TevDD_PatternUpdate ("$seq", indexVec, 1 );
		// Check updated Opcode
		apiStatus = TevDD_GetVectorData (NULL, "Opcode", indexVec, 1, &vData[0], sizeof(vData));

	// ReadMemSRL:
		apiStatus = TevDD_GetVectorLabelIndex ("LblLcnt2", &indexVec);	// Zero based

		// type: "OpCode", "Tset", "PinData", "LoopCount", RepCount"
		apiStatus = TevDD_GetVectorData (NULL, "Opcode", indexVec, 1, &vData[0], sizeof(vData));	// Current Opcode
		apiStatus = TevDD_SetVectorData (NULL, "Opcode", indexVec, 1, (char*)newOpcode, int(strlen(newOpcode)) );
		apiStatus = TevDD_PatternUpdate ("$seq", indexVec, 1 );
		// Check updated Opcode
		apiStatus = TevDD_GetVectorData (NULL, "Opcode", indexVec, 1, &vData[0], sizeof(vData));

		gPatternModified_I2C = TRUE;
	}

// Now we run as normal....


// ********************************************************************************
// WriteMemSL:   Command byte is hard coded.
// Write Memory Address using SEND waveform.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

	address = 0x0000 & 0xFFFF;		//16 bits   Register address to start at.

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[0] = unsigned short((address & 0xFF00) >> 8);	//  8 MSBs (upper ADDR byte)
	sendData[1] = unsigned short( address & 0x00FF );		//  8 LSBs (lower ADDR byte)

	for (i=0; i<loopCnt; i++)
	{
		sendData[2+i] = unsigned short(i*2 & 0x00FF);			//  8 LSBs (byte 0)
	}

	dataLength = 2+loopCnt;	// If numberElements == dataArraySize then write same data for *all* sites.
//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 100,    0,      dataLength);

	apiStatus = TevDD_PatternStart("WritePageSL");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, I2C_24LC256_LFE);
	TevWait_Now( Twc);	// Wait for Write Cycle to complete.

// ********************************************************************************
// ReadMemSRL:   Command byte is hard coded.
// Read Memory Address using SEND waveform.
// 1. Run pattern that writes then reads from DUT => RECEIVE memory.
// 2. Retrieve RECEIVE memory and datalog.
// ********************************************************************************
	apiStatus = TevDD_WaveConfig  ("CaptureData", "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("CaptureData", "StartI");		// in pattern lwseg CaptureID1_Data X X X X X CaptureID1_Type = starti
	apiStatus = TevDD_WaveConfig  ("sendData",   "LoopDisable");	// Configure a digital waveform memory engine (Type, Mode, Width) .wavetype <name> ={}
//	apiStatus = TevDD_WaveControl ("sendData",	   "StartI");		// in pattern lwseg sendPD_Data X X X X X sendPD_Type = starti

	address = 0x0 & 0xFFFF;		//16 bits   Register address to read

// We defined SEND wafevorm as 8 bits wide so we need to break up into 8 bit bytes.
	sendData[0] = unsigned short((address & 0xFF00) >> 8);		//  8 MSBs (upper byte)
	sendData[1] = unsigned short( address & 0x00FF );			//  8 LSBs (lower byte)

//				TevDD_WaveLoad (*waveName,   *dataArray, dataArraySize, offset, numberElements);
	dataLength = 2;	// If numberElements == dataArraySize then write same data for *all* sites.
	apiStatus = TevDD_WaveLoad ("sendData",   sendData, 2,    0,      dataLength);

	apiStatus = TevDD_PatternStart("ReadSeqSRL");					// Start Pattern write register
	checkForPatternTimedOut(50 ms, I2C_24LC256_LFE);

	numberElements=loopCnt, offset=0;		// Per site. read 8 bits
	readCaptureDataDD48(numberElements, offset);		// RECEIVE => capturedData[MAX_SITES][100]

// Format for datalogging
	MLOG_ForEachEnabledSite(siteIndex)
	{
		for (i=0; i<loopCnt; i++)
		{
			capturedData[siteIndex][i] = capturedData[siteIndex][i] & 0xFF;	// Mask unwanted bits
			result[i][siteIndex] = (double)capturedData[siteIndex][i];		// Convert for datalog
		}
	}


// Datalog the results.
	subTestNum = testNumber + 100;
	for (i=0; i<loopCnt; i++)
	{
		double limit = i*2;
		sprintf_s(testDlogName, sizeof(testDlogName), "Mem 0x%x", address+i);
		passMask &= MLog_Datalog("", subTestNum++, testDlogName, result[i], "HEX16",  limit,  limit, "ReadSeqSRL_mod", fBinName);
	}



	
// Cleanup
	TevDD_PatternStop();		// Just in-case
	apiStatus = TevDD_DriverMode ("I2CPins", "OFF");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)
	TevWait_Now (400 us);	
	apiStatus = TevDD_PMUForceV  ("VCC",  pmuVoltageHi, pmuVoltageLo, "32mA", 0.0);	// 2uA, 20uA, 200uA, 2mA, 32mA
	TevWait_Now (4 ms);	
	apiStatus = TevDD_DriverMode ("VCC", "OFF");	// (DriveSense|HIV|Off(w/clamps off)|VTT|PMU|PMULoad|DriveSenseNoLoad)

	if (scopeTrigger)
	{	// **ALWAYS** cleanup DSTAR Triggers
		apiStatus = TevTrigger_DisconnectTerminals	("PFI2");		// CIF: Disconnect PFI2
		apiStatus = TevTrigger_DisconnectTerminals	("S4_DStar_A");	// DD48,CIF: Disconnect DSTAR
		apiStatus = TevDD_TriggerDisable			("S4_DStar_A");	// DD48: Disable DSTAR Trigger
	}

// Disable site if (gFastBinning & !gStopOnFirstFailure)
//	MLOG_DisableFailedSites		// also calls MLog_UserDisableSite(siteNumber)
	
	subTestNum = testNumber + 130;
	datalogTimer("Time_I2C", subTestNum++, 0);	// (UINT timer, 0, 1, 2 are the 3 timers), int testNum 

//	MLog_Footer	(passMask, headerName, "");				// For enhanced Dlogviewer display of a failed site.

	return passMask;	// Pass/Fail  0:TestStand displays as failed Test Step.
}





/* ********************************************************************************
// Test utilizes AXIe_User_I2C Bus on the Loadboard.
// User has access to an uncommitted I2C bus for adding temperature sensors, 
// additional relay drivers, etc.
//  AXIe Instrument FPGA I2C engine is the Master.
//	AXIe Instrument has 5K pullups on SCL and SDA to 5 volts that is always on.
//  I2C engine will stop transmission if ACK for any byte is not received by Slave.
//  Transaction type can select 0, 1, 2 command address(es) sent.
//	Can write or read upto 4 bytes at a time.
//	SCL rate is fixed at 400KHz.
//	VHDM pinout:  A10:SDA   B10:SCL  C10,D10:User_I2C_+5V
//	AXIe_User_I2C bus is independant to the AXIe_DUT_ID_I2C bus.
//	7 bit Slave address = <device_type> + A2 + A1 + A0
//  User must control bytes written to EEPROM do not cross page boundries.
// ********************************************************************************/
TEST_FUNCT int User_I2C (UINT testNumber)
{
   	MLOG_CheckFastBinning   // Return if Fastbinning is On and No Enabled Sites
	
    const char* testName = "User_I2C_Bus";
	char  headerName[MAX_HEADER_SIZE] = {""};
	formatHeadername(testName, headerName, sizeof (headerName));

	testNumber= 3000;				// testNumber is set in MLog_Header(). 

	TEV_STATUS		apiStatus = AXI_SUCCESS;
//	UINT	testNumber= 1;			// testNumber is set in MLog_Header(). 
//	int		subTestNum = 1;			// subTestNum is set in MLog_Datalog(); 
	int		subTestNum = testNumber + 1; // STDF requires unique testNumber and subTestNum 
	int		fail_bin = 16;
	char	fBinName[10];			// Fail bin name
	UINT	passMask= 0xFFFF;		// bit mask for each site, default is all sites pass // 0:TestStand displays as failed Test Step.
	UINT	site=0;

// Set datalog Header, testNumber, and fBinName
	sprintf_s(fBinName, 10, "%d", int(fail_bin));		// if gUseBinDisqualification
	formatHeadername(testName, headerName, sizeof (headerName));
    MLog_Header         (testNumber, headerName, fBinName);

	sprintf_s(fBinName, 10, "Bin%d", int(fail_bin));		// if gUseBinDisqualification

	gTimer0Val = TevDiag_GetXTime(0);	// UINT timer, 0, 1, 2 are the 3 timers
	TevUtil_TraceMessage("Start: User_I2C");

	//get_active_site( );		// Update global array


// User code
// For details of AXIe I2C engine in FPGA see:
//	"AXIe Instrument Standard Common Register Definition"
//	"AXIe Instrument Standard User I2C Interface Guide"

	UINT slave_addr = 0x40;

	UINT I2C_status = 0;
	UINT slaveAddr = 0x40;				// PCA9535D Driver I2C device slave ADDR (type + A2,A1,A0)
	UINT slaveData = 0x0;				// Upto 4 bytes, LSbyte first
	UINT cmdRegAddr  = 0x0;				// (target Address [15:0]) Slave I2C Command or Special Addr Register (15:8, 7:0)
	UINT transactionType = WRITE_1_CMD;	// W:0,1,2, or R:4,5,6 number of Target Addr (Command) bytes
	UINT numDataBytes = 1;				// 1 to 4 bytes, data is stored in AXIe I2C Write Data Register (0x3)
	
	UINT chassis = 1;					// AX500
	UINT CIFslot = 1;					// CIF

	INT  nSamples = 1;
	UINT averageCount = 1;
	UINT channelNumber=1;	

	float number = 25.0, numberOUT=0.0;
	unsigned int numberUINT = 0;
	double numberIN_d = 4 * atan(1.00);	// pi = 3.1415926535897931
	double numberOUT_d;
	double Twc = 5 ms;

// AD7415-0: Digital Output Temperature Sensor
	UINT TSaddr  = 0x49;	// AD7415-0 A0
	short data[128];
	char myStr[128], tmpStr[5], tstStr[14] = {"Jim is a Dope"};
	char *pdest;
	double tempDegC[4];		// multisite
	double tempDegF[4];		// multisite
	double result[4];		// multisite

	float	myFloat=0.0;
	double	myDouble = 3.14159;

// PCA9535D: 16-bit I2C-bus and SMBus, low power I/O port with interrupt
	UINT U16addr = 0x20;	// 0x20 PCA9535D
	UINT U6addr  = 0x21;	// 0x21 PCA9535D
	UINT U18addr = 0x22;	// 0x22 PCA9535D
	UINT U2addr  = 0x23;	// 0x23 PCA9535D (Noise Source attenuator bits)

// 24AA1025/24LC1025/24FC1025 1024K I2C CMOS Serial EEPROM
	UINT U28addr = 0x50;	// 0x50	AT24C32 EEPROM 4Kx8=32K
	UINT U31addr = 0x51;	// 0x51	24AA1025 EEPROM
	UINT U30addr = 0x52;	// 0x52	24AA1025 EEPROM
	UINT U26addr = 0x53;	// 0x53	24AA1025 EEPROM


	AXIe_EnableI2C ( chassis, CIFslot );

// ********************************************************************************/
// Write/Read data to/from EEPROM 1 to 4 bytes at a time.
//  User must control bytes written to EEPROM to not cross page boundries.
//  Must wait for Write Cycle to complete.
// ********************************************************************************/
// UINT
	slaveAddr  = U28addr;			// 0x50	AT24C32 EEPROM 4Kx8=32K
	slaveData  = 0x20546520;		// Data to write " he "
	numDataBytes = 0x4;
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr, slaveData,  numDataBytes);
	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr, &slaveData, numDataBytes);

	slaveData = 0x54686520;			// UINT
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr, slaveData,  numDataBytes);	// UINT
	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr, &slaveData, numDataBytes);		// UINT

// Datalog Status (ACKnowledge bit detected ?)
	MLOG_ForEachEnabledSite(siteIndex)
		result [siteIndex] = double(I2C_status);
	

    passMask &= MLog_Datalog("", subTestNum++, "EEPROM Status", result, "HEX16", 0x5, 0x5, "ACKnowledge", fBinName);

// Datalog UINT
	MLOG_ForEachEnabledSite(siteIndex)
		result [siteIndex] = double(slaveData);
	
    passMask &= MLog_Datalog("", subTestNum++, "EEPROM UINT", result, "HEX32", 0x54686520, 0x54686520, "", fBinName);

	
// char
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr,    "Jim ", numDataBytes);		// char
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr+4,  "is a", numDataBytes);		// char
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr+8,  " Dop", numDataBytes);		// char
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr+12, "e",    1           );		// char
	strcpy_s(myStr, 64, "");
	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr,    tmpStr, numDataBytes);		// char
	strcat_s(myStr, 64, tmpStr);
	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr+4,  tmpStr, numDataBytes);		// char
	strcat_s(myStr, 64, tmpStr);
	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr+8,  tmpStr, numDataBytes);		// char
	strcat_s(myStr, 64, tmpStr);
	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr+12, tmpStr, 1);					// char
	strcat_s(myStr, 64, tmpStr);

	pdest = strstr(tstStr, myStr);
		
// Datalog char
	MLOG_ForEachEnabledSite(siteIndex)
	{
		if ( pdest != NULL )
			result [siteIndex] = 1.0;
		else
			result [siteIndex] = 0.0;
	}
    passMask &= MLog_Datalog("", subTestNum++, "EEPROM char", result, "", 1.0, 1.0, myStr, fBinName);


// float (Be careful of accuracy errors from rounding off errors !!)
	number = float(25.010101);
	float2UINTArray (&numberUINT, number);
	UINTArray2float (&numberUINT, &numberOUT);
	cmdRegAddr = 0;
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr,  number,    NULL);
	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr, &numberOUT, NULL);

	number = float(12.25);
	float2UINTArray (&numberUINT, number);
	UINTArray2float (&numberUINT, &numberOUT);
	cmdRegAddr += 4;
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr,  number,    NULL);
	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr, &numberOUT, NULL);

	number = float(1.0/10.0);
	float2UINTArray (&numberUINT, number);
	UINTArray2float (&numberUINT, &numberOUT);
	cmdRegAddr += 4;
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr,  number,    NULL);
	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr, &numberOUT, NULL);

	number = float(4 * atan(1.00));
	float2UINTArray (&numberUINT, number);
	UINTArray2float (&numberUINT, &numberOUT);
	cmdRegAddr += 4;
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr,  number,    NULL);
	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr, &numberOUT, NULL);

// Datalog float
	MLOG_ForEachEnabledSite(siteIndex)
		result [siteIndex] = double(numberOUT);
	
    passMask &= MLog_Datalog("", subTestNum++, "EEPROM float", result, "", 3.1415927, 3.1415928, "", fBinName);

// double
	numberIN_d = 25.00;
	cmdRegAddr += 8;
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr,  numberIN_d,  NULL);
	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr, &numberOUT_d, NULL);

	numberIN_d = 4 * atan(1.00);	// pi = 3.1415926535897931
	cmdRegAddr += 8;
	I2C_status = WriteUserEEPROM(slaveAddr, cmdRegAddr,  numberIN_d,  NULL);
	numberOUT_d = 0.0;
	I2C_status = ReadUserEEPROM (slaveAddr, cmdRegAddr, &numberOUT_d, NULL);

// Datalog double
	MLOG_ForEachEnabledSite(siteIndex)
		result [siteIndex] = numberOUT_d;
	
    passMask &= MLog_Datalog("", subTestNum++, "EEPROM double", result, "", 3.14159265, 3.14159266, "", fBinName);



//******************************************************************************************
// PCA9535D: 16-bit I2C-bus and SMBus, low power I/O port with interrupt
// PCA9535D Slave Addr = 0100(A2)(A1)(A0) (0x20 example)
// PCA9535D command registers
//	0: "Input port 0"
//	1: "Input port 1"
//	2: "Output port 0"
//	3: "Output port 1"
//	4: "Polarity Inversion port 0"
//	5: "Polarity Inversion port 1"
//	6: "Configuration port 1"
//	7: "Configuration port 1"
//******************************************************************************************
// Set U16 PCA9535D port 1 as OUTPUTS
	chassis = 1;					// AX500
	CIFslot = 1;					// CIF
	transactionType = WRITE_1_CMD;	// W:0,1,2, or R:4,5,6 number of Target Addr (Command) bytes
	cmdRegAddr = 0x7;				// PCA9535D command register "Configuration port 1"
	slaveAddr  = U16addr;
	numDataBytes = 1;				// 1 to 4 bytes, data is stored in AXIe I2C Write Data Register (0x3)
	slaveData = 0x00;				// Default = 0xFF (all pins as inputs)
	I2C_status = AXIe_I2C_Engine (chassis, CIFslot, transactionType, cmdRegAddr, slaveAddr, numDataBytes, &slaveData);	// Expect return = 0xa

// Datalog Status (ACKnowledge bit detected ?)
	MLOG_ForEachEnabledSite(siteIndex)
		result [siteIndex] = double(I2C_status);
	
    passMask &= MLog_Datalog("", subTestNum++, "PCA9535D Status", result, "HEX16", 0x5, 0x5, "ACKnowledge", fBinName);

// Set U16 PCA9535D port 1 OUTPUTS
	cmdRegAddr = 0x3;	// PCA9535D command register "Output port 1"
	slaveData = 0xff;	// Default = 0xFF
	I2C_status = AXIe_I2C_Engine (chassis, CIFslot, transactionType, cmdRegAddr, slaveAddr, numDataBytes, &slaveData);
	slaveData = 0x00;	// Default = 0xFF
	I2C_status = AXIe_I2C_Engine (chassis, CIFslot, transactionType, cmdRegAddr, slaveAddr, numDataBytes, &slaveData);


//******************************************************************************************
//  Read  AD7415-0 ±0.5°C Accurate, 10-Bit Digital Temperature Sensor in SOT-23
// The AD7415 is a complete temperature monitoring system in a 5-pin SOT-23 package. 
//	It contains a bandgap temperature sensor and 10-bit ADC to monitor and digitize 
//	the temperature reading to a resolution of 0.25°C. 
// Read Temperature Value register	(2 bytes, 10 bits used, 2's Complement)
//******************************************************************************************
	chassis = 1;					// AX500
	CIFslot = 1;					// CIF
	transactionType = READ_1_CMD;
	cmdRegAddr = 0x0;	// AD7415-0 Temperature Value register
	slaveAddr  = TSaddr;
	numDataBytes = 2;	// First Read 15:8, Second Read 7:0 (5:0 not used) 2's complement
	slaveData = 0x00;	
	I2C_status = AXIe_I2C_Engine (chassis, CIFslot, transactionType, cmdRegAddr, slaveAddr, numDataBytes, &slaveData);	// Expect return = 0x5

	if (I2C_status==0x5)	// No I2C errors reported
	{
		// Swap bytes (first byte read goes into LSByte of slaveData)
		data[0] = slaveData >> 8;
		data[1] = slaveData & 0xFF;
		data[2] = data[1] << 8;
		data[2] += data[0];

		// Convert 2's complement to binary
		if (data[2] & 0x8000)		// Neg value if MSBit = 1
		{
			data[2] = ~data[2];		// invert
			data[2] = data[2] + 1;	// add 1
		}

		data[2] = data[2] >> 6;		// Don't need bits 5:0
		data[2] &= 0x3FF; 
	}
	else
	{
		data[2] = -99*4;			// Force a bad result
	}


// Datalog Status (ACKnowledge bit detected ?)
// Convert to temperature and stuff array for each site

	MLOG_ForEachEnabledSite(siteIndex)
	{
		result [siteIndex] = double(I2C_status);
		tempDegC[siteIndex] = (double)data[2] / 4.0;			// convert to degrees C
		tempDegF[siteIndex] = tempDegC[siteIndex] * 9.0/5.0 + 32.0;		// convert to degrees F
	}
	
    passMask &= MLog_Datalog("", subTestNum++, "Temp Status", result,   "HEX16",  0x5,  0x5, "ACKnowledge", fBinName);
    passMask &= MLog_Datalog("", subTestNum++, "Temperature", tempDegC, "deg C", 18.0, 30.0, "AD7415-0",    fBinName);
	passMask &= MLog_Datalog("", subTestNum++, "Temperature", tempDegF, "deg F", 65.0, 85.0, "AD7415-0",    fBinName);
    
// Disable site if (gFastBinning & !gStopOnFirstFailure)
//	MLOG_DisableFailedSites		// also calls MLog_UserDisableSite(siteNumber)

	datalogTimer("Time_UserI2C", subTestNum++, 0);	// (UINT timer, 0, 1, 2 are the 3 timers), int testNum 

//	MLog_Footer	(passMask, headerName, "");				// For enhanced Dlogviewer display of a failed site.

	return passMask;	// Pass/Fail  0:TestStand displays as failed Test Step.
}



// DLL Requirements ========================================================
#ifdef _MANAGED
#pragma managed(push, off)
#endif

BOOL APIENTRY  DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{
    return TRUE;
}

#ifdef _MANAGED
#pragma managed(pop)
#endif