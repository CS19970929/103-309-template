#ifndef FAULT_H
#define FAULT_H

enum FaultFlag {
	CellOvp_First = 1,
	CellUvp_First,
	BatOvp_First,
	BatUvp_First,
	IchgOcp_First,
	IdischgOcp_First,
	CellChgOTp_First,
	CellChgUTp_First,
	CellDsgOTp_First,
	CellDsgUTp_First,
	MosOTp_First,
	VdeltaOvp_First,
	CellSocUp_First,
	
	CellOvp_Second,
	CellUvp_Second,
	BatOvp_Second,
	BatUvp_Second,
	IchgOcp_Second,
	IdischgOcp_Second,
	CellChgOTp_Second,
	CellChgUTp_Second,
	CellDsgOTp_Second,
	CellDsgUTp_Second,
	MosOTp_Second,
	VdeltaOvp_Second,
	CellSocUp_Second,

	CellOvp_Third,
	CellUvp_Third,
	BatOvp_Third,
	BatUvp_Third,
	IchgOcp_Third,
	IdischgOcp_Third,
	CellChgOTp_Third,
	CellChgUTp_Third,
	CellDsgOTp_Third,
	CellDsgUTp_Third,
	MosOTp_Third,
	VdeltaOvp_Third,
	CellSocUp_Third
};


union FAULT_FLAG_FIRST {
    UINT16 all;
    struct Fault_Flag_First {
		UINT8 CellOvp_First     :1;
		UINT8 CellUvp_First     :1;
		UINT8 BatOvp_First      :1;
		UINT8 BatUvp_First      :1;
		
		UINT8 IchgOcp_First  	:1;
		UINT8 IdischgOcp_First  :1;
		UINT8 CellChgOTp_First  :1;
		UINT8 CellChgUTp_First  :1;
		
		UINT8 CellDsgOTp_First  :1;
		UINT8 CellDsgUTp_First  :1;
		UINT8 MosOTp_First		:1;
		UINT8 VdeltaOvp_First	:1;
		
		UINT8 CellSocUp_First	:1;
		UINT8 Rcv				:3;
     }bits;	
};

union FAULT_FLAG_SECOND {
    UINT16 all;
    struct Fault_Flag_Second {
		UINT8 CellOvp_Second    :1;
		UINT8 CellUvp_Second    :1;
		UINT8 BatOvp_Second     :1;
		UINT8 BatUvp_Second     :1;
		
		UINT8 IchgOcp_Second    :1;
		UINT8 IdischgOcp_Second :1;
		UINT8 CellChgOTp_Second :1;
		UINT8 CellDsgOTp_Second :1;
		
		UINT8 CellChgUTp_Second :1;
		UINT8 CellDsgUTp_Second :1;
		UINT8 MosOTp_Second     :1;
		UINT8 VdeltaOvp_Second	:1;
		
		UINT8 CellSocUp_Second	:1;
		UINT8 Rcv				:3;
     }bits;	
};

union FAULT_FLAG_THIRD {
    UINT16 all;
    struct Fault_Flag_Third {
		UINT8 CellOvp_Third     :1;
		UINT8 CellUvp_Third     :1;
		UINT8 BatOvp_Third      :1;
		UINT8 BatUvp_Third      :1;
		
		UINT8 IchgOcp_Third     :1;
		UINT8 IdischgOcp_Third  :1;
		UINT8 CellChgOTp_Third  :1;
		UINT8 CellChgUTp_Third  :1;
		
		UINT8 CellDsgOTp_Third  :1;
		UINT8 CellDsgUTp_Third  :1;
		UINT8 MosOTp_Third      :1;
		UINT8 VdeltaOvp_Third   :1;
		
		UINT8 CellSocUp_Third   :1;
		UINT8 Rcv				:3;
     }bits;
};


struct PRT_E2ROM_PARAS {
//--------------parameters store sequence and its address allocation-----------
	UINT16	u16VcellOvp_First;
	UINT16	u16VcellOvp_Second;
	UINT16	u16VcellOvp_Third;
	UINT16	u16VcellOvp_Rcv;
	UINT16	u16VcellOvp_Filter;

	UINT16	u16VcellUvp_First;
	UINT16	u16VcellUvp_Second;
	UINT16	u16VcellUvp_Third;
	UINT16	u16VcellUvp_Rcv;
	UINT16	u16VcellUvp_Filter;
	
	UINT16	u16VbusOvp_First;
	UINT16	u16VbusOvp_Second;
	UINT16	u16VbusOvp_Third;
	UINT16	u16VbusOvp_Rcv;
	UINT16	u16VbusOvp_Filter;

	UINT16	u16VbusUvp_First;
	UINT16	u16VbusUvp_Second;
	UINT16	u16VbusUvp_Third;
	UINT16	u16VbusUvp_Rcv;	
	UINT16	u16VbusUvp_Filter;

	UINT16	u16IchgOcp_First;
	UINT16	u16IchgOcp_Second;
	UINT16	u16IchgOcp_Third;
	UINT16	u16IchgOcp_Rcv;
	UINT16	u16IchgOcp_Filter;

	UINT16	u16IdsgOcp_First;
	UINT16	u16IdsgOcp_Second;
	UINT16	u16IdsgOcp_Third;
	UINT16	u16IdsgOcp_Rcv;
	UINT16	u16IdsgOcp_Filter;
	
	UINT16	u16TChgOTp_First;
	UINT16	u16TChgOTp_Second;
	UINT16	u16TChgOTp_Third;
	UINT16	u16TChgOTp_Rcv;
	UINT16	u16TChgOTp_Filter;

	UINT16	u16TchgUTp_First;
	UINT16	u16TchgUTp_Second;
	UINT16	u16TchgUTp_Third;
	UINT16	u16TchgUTp_Rcv;
	UINT16	u16TchgUTp_Filter;

	UINT16	u16TdischgOTp_First;
	UINT16	u16TdischgOTp_Second;
	UINT16	u16TdischgOTp_Third;
	UINT16	u16TdischgOTp_Rcv;
	UINT16	u16TdischgOTp_Filter;

	UINT16	u16TdischgUTp_First;
	UINT16	u16TdischgUTp_Second;
	UINT16	u16TdischgUTp_Third;
	UINT16	u16TdischgUTp_Rcv;
	UINT16  u16TdischgUTp_Filter;

	UINT16	u16TmosOTp_First;
	UINT16	u16TmosOTp_Second;
	UINT16	u16TmosOTp_Third;
	UINT16	u16TmosOTp_Rcv;
	UINT16	u16TmosOTp_Filter;

	UINT16	u16VdeltaOvp_First;
	UINT16	u16VdeltaOvp_Second;
	UINT16	u16VdeltaOvp_Third;
	UINT16	u16VdeltaOvp_Rcv;
	UINT16	u16VdeltaOvp_Filter;

	UINT16	u16SocUp_First;
	UINT16	u16SocUp_Second;
	UINT16	u16SocUp_Third;
	UINT16	u16SocUp_Rcv;
	UINT16	u16SocUp_Filter;
};

#define COV_1           3500
#define COV_2           3600
#define COV_3           3650
#define COV_recover     3500
#define COV_filter1      100
#define COV_filter2     100
#define COV_filter3     100

#define CUV_1           3000
#define CUV_2           3000
#define CUV_3           2900
#define CUV_recover     3000
#define CUV_filter1      100
#define CUV_filter2     100
#define CUV_filter3     100

#define BOV_1           (350 * SNum)
#define BOV_2           (360 * SNum)
#define BOV_3           (365 * SNum)
#define BOV_recover     (350 * SNum)
// #define BOV_1           (2800)
// #define BOV_2           (2920)
// #define BOV_3           (3000)
// #define BOV_recover     (2800)
#define BOV_filter1      100 
#define BOV_filter2     100 
#define BOV_filter3     100 


#define BUV_1           (300 * SNum)
#define BUV_2           (300 * SNum)
#define BUV_3           (290 * SNum)
#define BUV_recover     (300 * SNum)
#define BUV_filter1      100 
#define BUV_filter2     100 
#define BUV_filter3     100 


#define OTC_1           ((40 + 40) * 10)
#define OTC_2           ((50 + 40) * 10)
#define OTC_3           ((55 + 40) * 10)
#define OTC_recover     ((50 + 40) * 10)
#define OTC_filter1       100
#define OTC_filter2      100
#define OTC_filter3      100

#define UTC_1           ((5 + 40) * 10)
#define UTC_2           ((3 + 40) * 10)
#define UTC_3           ((0 + 40) * 10)
#define UTC_recover     ((3 + 40) * 10)
#define UTC_filter1      100
#define UTC_filter2      100
#define UTC_filter3      100

#define OTD_1           ((50 + 40) * 10)
#define OTD_2           ((50 + 40) * 10)
#define OTD_3           ((60 + 40) * 10)
#define OTD_recover     ((50 + 40) * 10)
#define OTD_filter1      100
#define OTD_filter2      100
#define OTD_filter3      100

#define UTD_1           ((-10 + 40) * 10)
#define UTD_2           ((-15 + 40) * 10)
#define UTD_3           ((-20 + 40) * 10)
#define UTD_recover     ((-10 + 40) * 10)
#define UTD_filter1      100
#define UTD_filter2      100
#define UTD_filter3      100

#define mos_1           ((75 + 40) * 10)
#define mos_2           ((85 + 40) * 10)
#define mos_3           ((95 + 40) * 10)
#define mos_recover     ((80 + 40) * 10)
#define mos_filter1      100
#define mos_filter2      100
#define mos_filter3      100

#define VDELTER_1       600
#define VDELTER_2       800
#define VDELTER_3       1000
#define VDELTER_recover 800
#define VDELTER_filter1  100
#define VDELTER_filter2  100
#define VDELTER_filter3  100

#define socLow_1        20
#define socLow_2        10
#define socLow_3        5
#define socLow_recover  11
#define socLow_filter1   100
#define socLow_filter2   100
#define socLow_filter3   100

#define OCC_1       (100) 
#define OCC_2       (150) 
#define OCC_3       (200) 
#define OCC_recover (100) 
#define OCC_filter1  (100 * 5) 
#define OCC_filter2  (100 * 5) 
#define OCC_filter3  10 

#define ODC_1       (100) 
#define ODC_2       (150) 
#define ODC_3       (200) 
#define ODC_recover (100) 
#define ODC_filter1  (100 * 5) 
#define ODC_filter2  (100 * 5) 
#define ODC_filter3  10


/* Each legacy protection group has four threshold/recovery words and one
 * delay word. Store shared bounds once per group; wire layout is unchanged. */
#define BMS_PROTECT_GROUP_WORDS 5U
#define BMS_PROTECT_DELAY_MIN 1U
#define BMS_PROTECT_DELAY_MAX 50000U
#define E2P_PROTECT_GROUP_LIMITS { \
    {1000, 5000},  /* cell overvoltage */ \
    {1000, 5000},  /* cell undervoltage */ \
    {300, 20000},  /* pack overvoltage */ \
    {300, 20000},  /* pack undervoltage */ \
    {10, 50000},   /* charge overcurrent */ \
    {10, 50000},   /* discharge overcurrent */ \
    {400, 2000},   /* charge overtemperature */ \
    {0, 800},      /* charge undertemperature */ \
    {400, 2000},   /* discharge overtemperature */ \
    {0, 800},      /* discharge undertemperature */ \
    {400, 2000},   /* MOS overtemperature */ \
    {10, 2000},    /* cell voltage difference */ \
    {0, 50}       /* low SOC */ \
}


#define E2P_PROTECT_DEFAULT_PRT	{/*单节过压*/COV_1,	COV_2,	COV_3,	COV_recover,	COV_filter3,\
								 /*单节低压*/CUV_1,	CUV_2,	CUV_3,	CUV_recover,	CUV_filter3,\
								 /*总压过压*/BOV_1, BOV_2,	BOV_3,  BOV_recover, 	BOV_filter3,\
								 /*总压低压*/BUV_1, BUV_2,	BUV_3,  BUV_recover, 	BUV_filter3,\
		        				 /*充电过流*/OCC_1,	OCC_2,	OCC_3,	OCC_recover,	OCC_filter3,\
		        				 /*放电过流*/ODC_1,	ODC_2,	ODC_3,	ODC_recover,	ODC_filter3,\
								 /*充电高温*/OTC_1,	OTC_2,	OTC_3,	OTC_recover,	OTC_filter3,\
								 /*充电低温*/UTC_1,	UTC_2,	UTC_3,	UTC_recover,	UTC_filter3,\
								 /*放电高温*/OTD_1,	OTD_2,	OTD_3,	OTD_recover,	OTD_filter3,\
								 /*放电低温*/UTD_1,	UTD_2,	UTD_3,	UTD_recover,	UTD_filter3,\
		        				 /*驱动高温*/mos_1,	mos_2,	mos_3,	mos_recover,	mos_filter3,\
		        				 /*压差过大*/VDELTER_1,	VDELTER_2,	VDELTER_3,	VDELTER_recover,	VDELTER_filter3,\
		        				 /*电量过低*/socLow_1,	socLow_2,	socLow_3,	socLow_recover,		socLow_filter3}









#define Record_len 10

extern struct PRT_E2ROM_PARAS PRT_E2ROMParas;
extern union FAULT_FLAG_FIRST Fault_Flag_Fisrt;
extern union FAULT_FLAG_SECOND Fault_Flag_Second;
extern union FAULT_FLAG_THIRD Fault_Flag_Third;

extern UINT16 Fault_record_Third[Record_len];

extern UINT16 Fault_record_Third2[Record_len];

extern UINT8  FaultPoint_Third;
extern UINT8  FaultPoint_Third2;

void FaultWarnRecord2(enum FaultFlag num);

#endif	/* FAULT_H */
