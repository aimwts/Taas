;/*******************************************************************************************
;/  CommonHeader_I2C.dp
;/  Rev Eng	Date		Description
;/   1	JC	01/07/14	Initial issue
;********************************************************************************************
;; Put stuff here that is common to all patterns
#define PERIOD 2500ns   ; 400KHz


;Period sets 
;.pset <name> = { [T0:<value>] [[,]C0:<divisor>] }
.pset PSET6 = { T0: PERIOD,  C0: 1 }

;Timing sets
;.tset <name> = { PSet: <period set name> }
.tset TSET6 = { PSet : PSET6 }  ; Normal
.tset TSET7 = { PSet : PSET6 }  ; START condition
.tset TSET8 = { PSet : PSET6 }  ; STOP condition
.tset TSET9 = { PSet : PSET6 }  ; ACK setup (tri-state driver 0n falling edge of #8 clock)

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

; SCL and SDA are Open Collector
#if 1
.edgeset SCL       = { DF1:RT,  DF1_D0:0ns,    DF1_D1:0ns,      DF1_D2:1500ns, DF1_D3:1000ns,   CF1:OFF,    CF1_R0:0ns,    CF1_R1:0ns, CF1_R2:0ns, CF1_R3:0ns }
.edgeset SCL_START = { DF1:NRZ, DF1_D0:1500ns, DF1_D1:1500ns,   DF1_D2:1000ns, DF1_D3:1000ns,   CF1:OFF,    CF1_R0:0ns,    CF1_R1:0ns, CF1_R2:0ns, CF1_R3:0ns }
.edgeset SCL_STOP  = { DF1:RT,  DF1_D0:0ns,    DF1_D1:0ns,      DF1_D2:1500ns, DF1_D3:1000ns,   CF1:OFF,    CF1_R0:0ns,    CF1_R1:0ns, CF1_R2:0ns, CF1_R3:0ns }

.edgeset SDA       = { DF1:NRZ, DF1_D0:300ns,  DF1_D1:301ns,    DF1_D2:0ns,    DF1_D3:0ns,      CF1:Strobe, CF1_R0:0ns,    CF1_R1:0ns, CF1_R2:0ns, CF1_R3:1000ns }
.edgeset SDA_ACK   = { DF1:RT,  DF1_D0:300ns,  DF1_D1:301ns,    DF1_D2:300ns,  DF1_D3:0ns,      CF1:Strobe, CF1_R0:0ns,    CF1_R1:0ns, CF1_R2:0ns, CF1_R3:600ns }
.edgeset SDA_START = { DF1:NRZ, DF1_D0:800ns,  DF1_D1:800ns,    DF1_D2:2000ns, DF1_D3:2000ns,   CF1:Strobe, CF1_R0:0ns,    CF1_R1:0ns, CF1_R2:0ns, CF1_R3:10ns }
.edgeset SDA_STOP  = { DF1:RT,  DF1_D0:0ns,    DF1_D1:0ns,      DF1_D2:2200ns, DF1_D3:2200ns,   CF1:Strobe, CF1_R0:0ns,    CF1_R1:0ns, CF1_R2:0ns, CF1_R3:10ns }
#endif

;Level sets (2mA pullup current via DCL)
.levelset Levels6 = { Vil:0, Vih:3.3, Vt:0, Vol:1.4, Voh:1.5, Vcl:-1.90, Vch:4.5, Vc:3.0, Iol:0.002, Ioh:0.00001 }


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
 StaticDrive:    Z,           ; Z, 1, 0
 LevelSet:       Levels6,     ; .levelset <name>
 TSET6:          SDA          ; <Name of .tset>:<Name of .edgeset>
}

.pin VCC = { }
.pin WPb = { }

;            Normal      START             STOP             ACKnowledge
.pin SCL = { TSET6: SCL, TSET7: SCL_START, TSET8: SCL_STOP, TSET9: SCL}
.pin SDA = { TSET6: SDA, TSET7: SDA_START, TSET8: SDA_STOP, TSET9: SDA_ACK}

.pingroup I2CPins = { PinList:  SCL SDA WPb }

; Serial SEND MEMORY waveform setup (8 bits)
.pingroup sendPins = { PinList:  SDA }
.wavetype sendType = { Type:  Send, Mode: SerialMSB, Length: 100, Width: 8, PinGroup: sendPins}  ; Length = cycles per site.
.wavedata sendData = { WaveType: sendType}

; RECEIVE MEMORY waveform setup (8 bits)
.pingroup CapturePins = { PinList:  SDA }
.wavetype CaptureType = { Type:  Receive, Mode: SerialMSB, Length: 100, Width: 8, PinGroup: CapturePins}
.wavedata CaptureData = { WaveType: CaptureType}


; 6 sets of SEND & RECEIVE engines with 8 pins each per DD48.
;/--------------------- 2 pins per site for patterns. ---------------------------------------------
;
;
;
;                   S S
;                   C D
; op-codes          L A  TimeSet Extended op-codes
;/--------------------------------------------------------------------------------------------------
