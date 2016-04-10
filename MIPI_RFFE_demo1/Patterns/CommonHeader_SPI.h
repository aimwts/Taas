;/*******************************************************************************************
;/  CommonHeader_SPI.dp
;/  Rev Eng	Date		Description
;/   1	JC	01/07/14	Initial issue
;********************************************************************************************
;; Put stuff here that is common to all patterns
#define PERIOD 1000ns   ; 1MHz

;Period sets
;.pset <name> = { [T0:<value>] [[,]C0:<divisor>] }
.pset PSET4 = { T0: PERIOD,  C0: 1 }

;Timing sets
;.tset <name> = { PSet: <period set name> }
.tset TSET4 = { PSet : PSET4 }

;Edgesets
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
.edgeset ESET4    = { DF1:NRZ, DF1_D0:0ns, DF1_D1:00ns,  DF1_D2:180ns, DF1_D3:180ns, CF1:Strobe, CF1_R0:0ns, CF1_R1:0ns, CF1_R2:0ns, CF1_R3:0ns }
.edgeset ESETclk  = { DF1:RZ,  DF1_D0:0ns, DF1_D1:140ns, DF1_D2:260ns, DF1_D3:240ns, CF1:Strobe, CF1_R0:0ns, CF1_R1:0ns, CF1_R2:0ns, CF1_R3:0ns }

;Level sets
.levelset Levels4 = { Vil:0, Vih:3.2, Vt:0, Vol:1.4, Voh:1.5, Vcl:-1.90, Vch:4.5, Vc:1.3, Iol:0.001, Ioh:0.001 }


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
 LevelSet:       Levels4,     ; .levelset <name>
 TSET4:          ESET4        ; <Name of .tset>:<Name of .edgeset>
}

.pin SCK = { TSET4: ESETclk }
.pin MISO = { }
.pin MOSI = { }
.pin CSb  = { }
.pin VCC =  { }

.pingroup SPIPins = { PinList:  CSb SCK MOSI MISO WPb }

; Serial SEND MEMORY waveform setup (8 bits)
.pingroup sendPins = { PinList:  MOSI }
.wavetype sendType = { Type:  Send, Mode: SerialMSB, Length: 100, Width: 8, PinGroup: sendPins}  ; Length = cycles per site.
.wavedata sendData = { WaveType: sendType}

; RECEIVE MEMORY waveform setup (8 bits)
.pingroup CapturePins = { PinList:  MISO }
.wavetype CaptureType = { Type:  Receive, Mode: SerialMSB, Length: 100, Width: 8, PinGroup: CapturePins}
.wavedata CaptureData = { WaveType: CaptureType}


; 6 sets of SEND & RECEIVE engines with 8 pins each per DD48.
;/--------------------- 4 pins per site for patterns. ---------------------------------------------
;
;
;                       M M
;                   C S O I
;                   S C S S
; op-codes          b K I O   TimeSet Extended op-codes
;/--------------------------------------------------------------------------------------------------


