//==============================================================================
//
// Title:       Mlog.h
// Purpose:     This module provides an API interface to multi site data log 
//				data services.  By employing different modules which support 
//              the API's below, different Data log environments can be employed.
//
// Copyright (c) 2011 Test Evolution Corp.
// All Rights Reserved
// Eng	Rev	Date______  Description_____________________________________________
// MRL	 1  09/24/2012  Merged Datalog & DatalogSTDF into 1
// MRL	 2  01/04/2013  Added Fastbinning and 1 of N filtering support
// MRL	 3  01/10/2013  Bug Fixes
// MRL	 4  10/24/2013  Add MLog_LogSiteState ()
//	JC	 5	05/07/2014  Add MLog_GetBin() to use in app cpp file(s).
// MRL	 6	07/23/2014  Add MLog_Footer()
// MRL	 7  08/24/2014  Added Direct Binning mode
//=================================================================================
#include "AXIData.h"

#ifndef __MVP_DATALOG_H__
#define __MVP_DATALOG_H__

#define MLOG_ForEachSite(siteIndex)			for (UINT siteIndex = 0; siteIndex < gNumSites; siteIndex++)
#define MLOG_ForEachEnabledSite(siteIndex)	for (UINT siteIndex = 0; siteIndex < gNumSites; siteIndex++) if (MLog_IsSiteEnabled(siteIndex)) 
#define MLOG_CheckFastBinning 			if (!MLog_AnyEnabledSites() && MLog_GetFastBinning()) return 0;
#define MLOG_DisableFailedSites 		MLog_DisableSites();

#define SITE_PASS_MASK unsigned int
#define SITE_MAJOR			0
#define PIN_MAJOR			1



typedef enum { MLOG_BIN_FAIL = 0, MLOG_BIN_PASS = 1} MLOG_BINTYPE;
typedef enum { MLOG_DISQ_BINNING = 0, MLOG_DIRECT_BINNING = 1} MLOG_BIN_MODE;

UINT			MLog_GetBin (UINT siteNumber);

void			MLog_UserDisableSite(UINT siteNumber); // Must be defined in user code
void			MLog_SetUserProductionVariables(void); // Must be defined in user code
bool			MLog_CustomizeDlogFileName(char* dlogFilePath, char* ext);      // Must be defined in user code

const char*		MLog_MakeSiteMVP(char *mvp, UINT siteNumber);

void			MVP_SetDataOrder		(UINT order);
double			MVP_GetResult			(const char mvpName[], double resultData[], UINT siteIndex, UINT channelIndex);
void			MVP_GetArray			(const char mvpName[], double resultData[], UINT numberOfPoints, UINT siteIndex, UINT channelIndex, double *resultArray);

void		    MLog_Header             (UINT testNumber, const char* testName, const char* comment);
void			MLog_Footer				(SITE_PASS_MASK passMask, const char* testName, const char* comment);
SITE_PASS_MASK  MLog_Datalog            (const char *mvpName, UINT testNumber, const char* testName, double mvpResultData[],
                                         char *units, double minLimit,
                                         double maxLimit, const char* comment, const char* binName);
void            MLog_SetTestFailed      (UINT siteNumber);
SITE_PASS_MASK  MLog_LogState           (const char *mvpName, UINT testNumber, const char* testName, double mvpResultData[],
                                         char *units, const char* comment, const char* binName);

int				MLog_LogSiteState		(const char *mvpName, unsigned int testNumberIn, const char* testName, double result,
								 char *units, const char* comment, const char* binName, int siteNumber);

void			MLog_SetNumberOfBins(UINT numberOfBins);
void			MLog_SetupBin(UINT softBin, UINT hardBin, const char *binName, MLOG_BINTYPE binType);
BOOL			MLog_IsSiteEnabled(UINT siteIndex); // zero Based
BOOL			MLog_AnyEnabledSites(void);
void			MLog_Set1ofNFilter(BOOL state, UINT N);
void			MLog_Get1ofNFilter(BOOL *state, UINT *N);

// Application Load/Unload Start/Stop support functions. 
void            MLog_OnProgramLoad      (const char* name);
void            MLog_OnProgramUnload    (void);
void            MLog_SetBinResult       (int siteNumber, const char* binName);
void            MLog_SetFastBinning     (BOOL fastBinning);
void			MLog_SetStopOnFirstFail (BOOL stopFirstFail);
void            MLog_SetBinningMode     (MLOG_BIN_MODE mode);
MLOG_BIN_MODE   MLog_GetBinningMode     (void);
void			MLog_OnStartDut			(void);

BOOL            MLog_GetFastBinning     (void);
void			MLog_DisableSites(void);
void			MLog_SetXY(int X, int Y, UINT siteNumber);

// STDF APIs
void	MLog_SetOperatorName(const char *operatorName);
void	MLog_SetSubLotId(const char *subLotId);
void	MLog_SetTestCode(const char *testCode);
void	MLog_SetTesterSerialNumber(const char *testerSerialNumber);
void	MLog_SetLoadboardId(const char *loadboardId);
void    MLog_SetModeCode(const char *modeCode);
void    MLog_SetFabProcessId(const char *fabProcessId);
void	MLog_SetPartType(const char *partType);
void	MLog_SetJobRev(const char *rev);

// Limit File APIs
int				MLog_ReadLimitsFile(char* filePath);
const char*		MLog_GetLimitTestName(int testNumber);
char*			MLog_GetLimitUnits(int testNumber);
const char*		MLog_GetLimitComment(int testNumber);
double			MLog_GetLowerLimit(int testNumber);
double			MLog_GetUpperLimit(int testNumber);
int				MLog_GetLimitFailBin(int testNumber);
const char*		MLog_GetLimitHeaderName(int headerIndex);
const char*		MLog_GetLimitHeaderValue(int headerIndex);
int				MLog_GetNumberOfLimitHeaders(void);

// Offset File APIs
int				MLog_ReadOffsetsFile(char* filePath);
const char*		MLog_GetOffsetTestName(int testNumber);
double			MLog_GetOffset(int testNumber, int siteIndex);
const char*		MLog_GetOffsetHeaderName(int headerIndex);
const char*		MLog_GetOffsetHeaderValue(int headerIndex);
int				MLog_GetNumberOfOffsetHeaders(void);


SITE_PASS_MASK  MLog_DatalogWithFileLimits(const char *mvpName, UINT testNumber, double mvpResultData[], bool correctWithOffset = false);

extern  UINT    gNumSites;
extern  UINT    gDutSerialNumbers[32];
#endif