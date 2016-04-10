;/*******************************************************************************************
;/  CommonHeader.dp
;/  Rev Eng	Date		Description
;/   1	JC	01/07/14	Initial issue
;/   2	JC	07/29/14	Capture_Data => CaptureData
;********************************************************************************************
;; Put stuff here that is common to all patterns
#define PERIOD 1000.0e-9 ; 1MHz

;;.target DDP = { Mode:Single } ; this is in pattern file(s)

; Period Sets
.pset PSET0 = {  T0: PERIOD,  C0: 1 }	; Set timebase to 16MHz
.pset PSET1 = {  T0: PERIOD,  C0: 1 }	; Set timebase to 16MHz
.pset PSET2 = {  T0: PERIOD,  C0: 1 }	; Set timebase to 16MHz
.tset TSET0 = {PSet: PSET0}
.tset TSET1 = {PSet: PSET1}
.tset TSET2 = {PSet: PSET2}

; Fill in required timing here so we don't always have to use APIs in the code.
;.edgeset EsetData1={DriveFormat:NRZ, ; TevDD_EdgeSetModify ()
;D0:0s,      ; D0 (Driver on/enabled)
;D1:1.0E-9s, ; D1 (drive w/tg1)
;D2:4.9E-6s, ; D2 (return to)
;D3:4.9E-6s, ; D3 (not used)
;
;CompareFormat:Strobe, ; TevDD_EdgeSetModify ()
;R0:0s,      ; R0 (tri-state driver)
;R1:0s,      ; R1 (no action/ignored)
;R2:1.0E-6s, ; R2 (Window open mode)
;R3:2.0E-6s} ; R3 (Window close/Strobe mode)


;All pins default setting:      Driver on   drive w/tg1	 	  return to	  	     not used			 		tri-state     ignored       open          close/strobe
;.edgeset ESET0   = { DF1:RZ,     DF1_D0:0.0, DF1_D1:3.0ns,     DF1_D2:0.5*PERIOD, DF1_D3:0.0ns, 	CF1:Strobe, CF1_R0:0.0ns, CF1_R1:0.0ns, CF1_R2:0.0ns, CF1_R3:0.75*PERIOD}
.edgeset ESET0   = { DF1:RZ,     DF1_D0:0.0, DF1_D1:100.0ns,    DF1_D2:0.5*PERIOD, DF1_D3:0.0ns, 	CF1:Strobe, CF1_R0:0.0ns, CF1_R1:0.0ns, CF1_R2:0.0ns, CF1_R3:0.75*PERIOD}

;SDATA NRZ for most vectors.
.edgeset ESET1   = { DF1:NRZ,    DF1_D0:0.0, DF1_D1:0.0ns,     DF1_D2:0.4*PERIOD, DF1_D3:0.0ns, 	CF1:Strobe, CF1_R0:0.0ns, CF1_R1:0.0ns, CF1_R2:0.0ns, CF1_R3:0.75*PERIOD }
;SDATA RT for Bus Park.
.edgeset ESET2   = { DF1:RT,     DF1_D0:0.0, DF1_D1:0.0ns,     DF1_D2:0.5*PERIOD, DF1_D3:0.0ns, 	CF1:Strobe, CF1_R0:0.0ns, CF1_R1:0.0ns, CF1_R2:0.0ns, CF1_R3:0.75*PERIOD }


#define  DL  0.0V     ; dvl  Driver Low       range -2.0 to +6.0V
#define  DH  1.8V     ; dvh  Driver High      range -2.0 to +6.0V
#define  CL  0.8V     ; cvl  Comparator Low   range -2.0 to +6.0V
#define  CH  1.0V     ; cvh  Comparator High  range -2.0 to +6.0V
#define  IL  0.05mA   ; Isnk Load I Sink   (pull-up current)   (to emulate OFF set to lowest value ==> 0.0) range 0 to +12mA
#define  IH  0.05mA   ; Isrc Load I Source (pull-down current) (to emulate OFF set to lowest value ==> 0.0) range 0 to +12mA
#define  Vcomm 1.0V   ; Commutation voltage   range -2.0 to +6.0V
; Voltage clamp high (Vch) range = -1.0 to +6.0
; Voltage clamp low  (Vcl) range = -2.0 to +5.0
.levelset Levels1 = { Vil:DL, Vih:DH,   Vt:0, Vol:CL,  Voh:CH,  Vcl:-1.0,  Vch:2.5, Vc:Vcomm, Iol:IL,     Ioh:IH }
; 1mA pullup, 1uA pulldown on DUT.
.levelset Levels2 = { Vil:DL, Vih:DH,   Vt:0, Vol:CL,  Voh:CH,  Vcl:-1.99, Vch:4.5, Vc:Vcomm, Iol:1.0mA,  Ioh:1.0uA }
.levelset Levels3 = { Vil:DL, Vih:0.8,  Vt:0, Vol:0.4, Voh:0.5, Vcl:-1.99, Vch:4.5, Vc:0.4,   Iol:IL,     Ioh:IH}


;Define the default PIN settings used by all the pins in the pattern.
.pin DEF_PIN =
{
 ClockEdges:     enabled,     ; enable(d), disable(d)
 ClockReference: T0,          ; T0,; T0, C0
 RelayMode:      Idle         ; DriveSense, DUT, PMU, Idle(open)
 FailMask:       disabled,    ; enable(d), disable(d)
 DriverMode:     DrvSense,    ; Hiv, Off, DriveSense/DrvSense, Vtt, PMU
 DataMode:       Dynamic,     ; Dynamic, Static
 ComparatorMode: Normal,      ; Normal
 MarkerMode:     Quad,        ; Normal/Quad, Dual, SingleDrive, SingleCompare
;StaticLoad:     I,           ; Z:off, I:current source
 StaticLoad:     Z,           ; Z:off, I:current source
 StaticDrive:    0,           ; Z, 1, 0
 LevelSet:       Levels1,     ; .levelset <name>
 TSET0:          ESET0,       ; <Name of .tset>:<Name of .edgeset>
 TSET1:          ESET0        ; <Name of .tset>:<Name of .edgeset>
}

;Pins with default settings
.pin SCLK   = {} ; {TSET0: ESET0, TSET1: ESET0}
.pin SCLK_A = {}

;Pins with unigue edgesets.
.pin SDATA   = {TSET0: ESET1, TSET1: ESET2}
.pin SDATA_A = {TSET0: ESET1, TSET1: ESET2}


;PMU_pins: Default PIN settings for all pins primarily used as PMU.
;PMU default settings: OFF: FV=0, 32mA range, clampHigh=0v, clampLow=0v,  Measure mode=voltage.
.pin VIO = {ClockEdges:disabled, RelayMode:Idle, FailMask:enabled, DriverMode:Off, StaticLoad:Z}


; PINGROUPS
; These are the pins being used in the pattern, total of 4
.pingroup patternPins = { PinList: SCLK SDATA SCLK_A SDATA_A}
.pingroup ALL_DD48    = { PinList: SCLK SDATA SCLK_A SDATA_A VIO}



; Serial SEND MEMORY waveform setup (18 bits)
.pingroup send18Pins = { PinList:  SDATA }
.wavetype send18Type = { Type:  Send, Mode: SerialMSB, Length: 2, Width: 18, PinGroup: send18Pins}  ; Length = cycles per site.
.wavedata send18Data = { WaveType: send18Type}

; Serial SEND MEMORY waveform setup (9 bits)
.pingroup send9Pins = { PinList:  SDATA }
.wavetype send9Type = { Type:  Send, Mode: SerialMSB, Length: 16, Width: 9, PinGroup: send9Pins}  ; Length = cycles per site.
.wavedata send9Data = { WaveType: send9Type}

; RECEIVE MEMORY waveform setup (9 bits)
.pingroup CapturePins = { PinList:  SDATA }
.wavetype CaptureType = { Type:  Receive, Mode: SerialMSB, Length: 100, Width: 9, PinGroup: CapturePins}
.wavedata CaptureData = { WaveType: CaptureType}

; Example C compiler directive
#if 0
; SEND MEMORY setup (100 vector, 2 bits wide)
.pingroup sendPins = { PinList:  SDATA SDATA_A }
.wavetype sendType = { Type:  Send, Mode: Parallel, Length: 100, Width: 2, PinGroup: sendPins}  ; Length = cycles per site.
.wavedata sendData = { WaveType: sendType}
#endif


;/--------------------- 4 pins per site for patterns. ---------------------------------------------
;                                     S
;                                 S S D
;                               S D C A
;                               C A L T
;                               L T K A
; op-codes                      K A A A TimeSet  Extended op-codes
;/--------------------------------------------------------------------------------------------------

