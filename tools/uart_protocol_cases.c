/* Combined with real protocol/transport C by test_uart_protocols.py. */
static const UINT8 data1_request[]={0x5A,0xA5,4,1,1,4,0xF0};
static const UINT8 data2_request[]={0x5A,0xA5,5,1,1,5,0xF0};
static const UINT8 data3_request[]={0x5A,0xA5,3,1,1,3,0xF0};
static const UINT8 counters_request[]={0xDD,0xA5,0xAA,0,0xFF,0x56,0x77};
static const UINT8 clear_request[]={0xDD,0x5A,1,2,0x28,0x28,0xFF,0xAD,0x77};

static UINT16 le(const UINT8 *b) { return (UINT16)(b[0] | (b[1]<<8)); }
static void receive(struct SCI_PORT *port, UINT8 byte)
{
    clock_us+=Sci_CharacterUs(port);
    port->usart->SR=USART_SR_RXNE; port->usart->DR=byte;
    Sci_PortIRQ(port); port->usart->SR=0; /* Real DR read clears RXNE. */
}
static void quiet(struct SCI_PORT *port)
{
    clock_us+=Sci_FrameGapUs(port)+Sci_CharacterUs(port)+20U;
    Sci_ServicePort(port);
}
static void request(struct SCI_PORT *port,const UINT8 *b,UINT16 count)
{
    UINT16 i;
    quiet(port);
    for (i=0;i<count;++i) receive(port,b[i]);
}
static UINT16 transmit(struct SCI_PORT *port, UINT8 *output)
{
    UINT16 count=0;
    while (port->usart->CR1 & USART_CR1_TXEIE)
    {
        port->usart->SR=USART_SR_TXE;
        Sci_PortIRQ(port); output[count++]=(UINT8)port->usart->DR;
        assert(count <= RS485_MAX_BUFFER_SIZE);
    }
    assert(port->usart->CR1 & USART_CR1_TCIE);
    port->usart->SR=USART_SR_TC; Sci_PortIRQ(port); port->usart->SR=0;
    return count;
}
static void modbus_request(UINT8 *b,UINT8 address,UINT8 command,UINT16 reg,UINT16 value)
{
    UINT16 crc;
    b[0]=address;b[1]=command;b[2]=(UINT8)(reg>>8);b[3]=(UINT8)reg;
    b[4]=(UINT8)(value>>8);b[5]=(UINT8)value;
    crc=Sci_CRC16RTU(b,6);b[6]=(UINT8)crc;b[7]=(UINT8)(crc>>8);
}
static void setup_report(void)
{
    UINT16 i;
    for(i=0;i<32;++i) g_stCellInfoReport.u16VCell[i]=(UINT16)(3300+i);
    g_stCellInfoReport.u16TempMax=650;
    g_stCellInfoReport.u16VCellMin=3300;
    g_stCellInfoReport.u16VCellTotle=2310;
    g_stCellInfoReport.SocElement.u16Soc=55;
    g_stCellInfoReport.SocElement.u16CapacityFactory=10000;
    g_stCellInfoReport.SocElement.u16CapacityFull=9000;
    g_stCellInfoReport.SocElement.u16Cycle_times=25;
    memcpy(ProductionInfor.BMS_SoftWareVersion,"1D",2);
    memcpy(ProductionInfor.BMS_SerialNumber,"12345678901",11);
    AFE_Parameters_RS485_Struction.u16VcellOvp.curValue=4200;
    AFE_Parameters_RS485_Struction.u16VcellUvp.curValue=2800;
    AFE_Parameters_RS485_Struction.u16VcellOvp_Rcv.curValue=4100;
    AFE_Parameters_RS485_Struction.u16VcellUvp_Rcv.curValue=3000;
    AFE_Parameters_RS485_Struction.u16TChgOTp.curValue=850;
    AFE_Parameters_RS485_Struction.u16IchgOcp_First.curValue=120;
    AFE_Parameters_RS485_Struction.u16IdsgOcp_First.curValue=300;
    measured_ma=-12345;
}
static void check_customer(struct SCI_PORT *port)
{
    UINT8 b[251]; UINT16 n;
    request(port,data1_request,sizeof(data1_request)); assert(Sci_ServicePort(port));
    n=transmit(port,b); assert(n==43 && b[2]==1 && b[3]==37);
    assert(le(b+4)==3300 && le(b+16)==3306 && b[18]==65);
    assert(le(b+20)==231 && le(b+22)==8766 && b[24]==55);
    assert(le(b+26)==4200 && le(b+28)==2800 && b[30]==45);
    assert(le(b+31)==10120 && le(b+33)==9700 && le(b+35)==1000);
    assert(le(b+37)==4100 && le(b+39)==3000);
    assert(b[41]==Customer_Bcc(b+2,39) && b[42]==0xF0);
    request(port,data2_request,sizeof(data2_request)); Sci_ServicePort(port);
    assert(transmit(port,b)==20 && b[0]==0xAA && b[1]==0xED && b[2]==15);
    assert(le(b+3)==25 && !memcmp(b+5,BMS_SOFTWARE_VERDION_DEFAULT,2));
    assert(!memcmp(b+7,"12345678901",11));
    assert(b[18]==Customer_Bcc(b+3,15) && b[19]==0xFA);
    request(port,data3_request,sizeof(data3_request)); Sci_ServicePort(port);
    assert(transmit(port,b)==22 && b[2]==3 && b[3]==16 && le(b+4)==900);
    assert(le(b+6)==0xFFFFU && le(b+8)==0xFFFFU);
    for(n=10;n<20;++n) assert(b[n]==0);
    SeriesNum=6; request(port,data1_request,7); Sci_ServicePort(port); transmit(port,b);
    assert(le(b+16)==61001); SeriesNum=7;
}

static void test_remaining_time(void)
{
    UINT8 b[251];
    UINT16 charge, discharge;
    const UINT32 load=0U;
    setup_report();
    g_stCellInfoReport.SocElement.u16CapacityFull=1000;
    g_stCellInfoReport.SocElement.u16CapacityNow=900;
    s_current_tick=tick=UINT32_MAX-10U;
    s_current_valid=1;
    measured_ma=1000;
    charge=(UINT16)((60000U+1000U-load-1U)/(1000U-load));
    assert(SOC_GetRemainingMinutes(1)==charge && !SOC_GetRemainingMinutes(0));
    request(&s_port1,data3_request,7); Sci_ServicePort(&s_port1);
    assert(transmit(&s_port1,b)==22 && le(b+6)==charge && !le(b+8));
    assert(b[20]==Customer_Bcc(b+2,18));
    measured_ma=-1000;
    discharge=(UINT16)((540000U+1000U+load-1U)/(1000U+load));
    request(&s_port2,data3_request,7); Sci_ServicePort(&s_port2);
    assert(transmit(&s_port2,b)==22 && !le(b+6) && le(b+8)==discharge);
    tick=s_current_tick+50U; assert(SOC_GetRemainingMinutes(0)==discharge);
    ++tick; assert(SOC_GetRemainingMinutes(0)==0xFFFFU); /* Across tick wrap. */
    request(&s_port2,data3_request,7); Sci_ServicePort(&s_port2);
    assert(transmit(&s_port2,b)==22 && le(b+6)==0xFFFFU && le(b+8)==0xFFFFU);
    s_current_tick=tick;
    s_current_valid=0;
    assert(SOC_GetRemainingMinutes(0)==0xFFFFU && SOC_GetRemainingMinutes(1)==0xFFFFU);
    s_current_valid=1; measured_ma=199;
    assert(SOC_GetRemainingMinutes(0)==0xFFFFU && SOC_GetRemainingMinutes(1)==0xFFFFU);
    request(&s_port1,data3_request,7); Sci_ServicePort(&s_port1);
    assert(transmit(&s_port1,b)==22 && le(b+6)==0xFFFFU && le(b+8)==0xFFFFU);
    measured_ma=-199; assert(SOC_GetRemainingMinutes(0)==0xFFFFU);
    measured_ma=0;
    assert(SOC_GetRemainingMinutes(0)==0xFFFFU && SOC_GetRemainingMinutes(1)==0xFFFFU);
    measured_ma=200;
    g_stCellInfoReport.SocElement.u16CapacityFull=65535;
    g_stCellInfoReport.SocElement.u16CapacityNow=0;
    assert(SOC_GetRemainingMinutes(1)==0xFFFEU && !SOC_GetRemainingMinutes(0));
    measured_ma=-200;
    g_stCellInfoReport.SocElement.u16CapacityNow=65535;
    assert(SOC_GetRemainingMinutes(0)==0xFFFEU);
    measured_ma=INT32_MIN; assert(SOC_GetRemainingMinutes(0)==1);
    measured_ma=INT32_MAX; assert(SOC_GetRemainingMinutes(1)==0);
    g_stCellInfoReport.SocElement.u16CapacityFull=0;
    assert(SOC_GetRemainingMinutes(0)==0xFFFFU && SOC_GetRemainingMinutes(1)==0xFFFFU);
    g_stCellInfoReport.SocElement.u16CapacityFull=100;
    measured_ma=1000; assert(!SOC_GetRemainingMinutes(1)); /* now > full clamps. */
    g_stCellInfoReport.SocElement.u16CapacityNow=0;
    measured_ma=-1000; assert(!SOC_GetRemainingMinutes(0)); /* Empty while discharging. */
    setup_report(); s_current_valid=0;
    puts("PASS remaining time: real SOC getter, charge/discharge LE fields, measured current, idle/invalid/stale, wrap, limits");
}

static void battery_number_request(UINT8 *b,const char *number)
{
    UINT16 crc;
    b[0]=1; b[1]=0x10; b[2]=0xFF; b[3]=0xF3;
    b[4]=0; b[5]=6; b[6]=12;
    memcpy(b+7,number,11); b[18]=0xFF;
    crc=Sci_CRC16RTU(b,19); b[19]=(UINT8)crc; b[20]=(UINT8)(crc>>8);
}

static void test_communication_settings(void)
{
    STORAGE_FLASH_CONFIG_DATA config;
    UINT8 mb[21], b[251], number[11];
    UINT16 i, mode;
    int before;
    memset(&config,0xFF,sizeof(config)); config.initializing=0;
    assert(Storage_SaveConfig(&config));
    InitUSART_CommonUpper();
    assert(s_port1.baud==9600 && s_port2.baud==9600 && StorageFlash_GetBaudMode()==1);
    modbus_request(mb,1,3,0xC004,11);
    request(&s_port1,mb,8); quiet(&s_port1);
    assert(transmit(&s_port1,b)==27 && b[2]==22 && !memcmp(b+3,"00000000000",11));
    for(i=14;i<25;++i) assert(b[i]==0xFF);
    modbus_request(mb,1,3,0xC004,12);
    request(&s_port1,mb,8); quiet(&s_port1); assert(transmit(&s_port1,b)==5 && b[1]==0x83);
    battery_number_request(mb,"BAT12345678");
    request(&s_port2,mb,21); quiet(&s_port2);
    assert(transmit(&s_port2,b)==8 && b[1]==0x10 && b[5]==6);
    assert(StorageFlash_ReadBatteryNumber(number) && !memcmp(number,"BAT12345678",11));
    before=mutations;
    request(&s_port2,mb,21); quiet(&s_port2); transmit(&s_port2,b);
    assert(mutations==before);
    modbus_request(mb,1,3,0xC004,11);
    request(&s_port2,mb,8); quiet(&s_port2);
    assert(transmit(&s_port2,b)==27 && !memcmp(b+3,number,11));
    battery_number_request(mb,"NEW12345678");
    cut_after=0; mutations=0;
    request(&s_port1,mb,21); quiet(&s_port1);
    assert(transmit(&s_port1,b)==5 && b[1]==0x90); cut_after=-1;
    assert(StorageFlash_ReadBatteryNumber(number) && !memcmp(number,"BAT12345678",11));
    mb[7]=0x80; i=Sci_CRC16RTU(mb,19); mb[19]=(UINT8)i; mb[20]=(UINT8)(i>>8);
    request(&s_port1,mb,21); quiet(&s_port1);
    assert(transmit(&s_port1,b)==5 && b[2]==RS485_ERROR_DATA_INVALID);
    battery_number_request(mb,"NEW12345678"); mb[18]=0;
    i=Sci_CRC16RTU(mb,19); mb[19]=(UINT8)i; mb[20]=(UINT8)(i>>8);
    request(&s_port1,mb,21); quiet(&s_port1);
    assert(transmit(&s_port1,b)==5 && b[2]==RS485_ERROR_DATA_INVALID);
    for(i=0;i<5;i+=4) {
        modbus_request(mb,1,6,0x1008,i);
        request(&s_port1,mb,8); quiet(&s_port1);
        assert(transmit(&s_port1,b)==5 && b[2]==RS485_ERROR_DATA_INVALID && !s_baud_change_pending);
    }
    cut_after=0; mutations=0; modbus_request(mb,1,6,0x1008,2);
    request(&s_port1,mb,8); quiet(&s_port1);
    assert(transmit(&s_port1,b)==5 && !s_baud_change_pending && s_port1.baud==9600);
    cut_after=-1;
    for(mode=2;mode<=4;++mode) {
        UINT16 target=(mode==4) ? 1 : mode;
        UINT32 old=s_port1.baud;
        UINT32 baud=(target==1) ? 9600U : (target==2 ? 19200U : 115200U);
        modbus_request(mb,1,6,0x1008,target);
        request(&s_port1,mb,8); quiet(&s_port1);
        request(&s_port2,data1_request,7); Sci_ServicePort(&s_port2);
        App_CommonUpper();
        assert(s_baud_change_pending && Sci_IsAnyPortBusy() && StorageFlash_GetBaudMode()==target);
        assert(s_port1.baud==old && s_port2.baud==old);
        assert(transmit(&s_port1,b)==8 && !memcmp(b,mb,8));
        App_CommonUpper(); assert(s_port1.baud==old); /* Other port still sending. */
        transmit(&s_port2,b); App_CommonUpper();
        assert(!s_baud_change_pending && s_port1.baud==baud && s_port2.baud==baud);
        Sci_StopForLowPower(); InitUSART_CommonUpper();
        assert(s_port1.baud==baud && s_port2.baud==baud); /* Persistent setting survives STOP/re-init. */
    }
    modbus_request(mb,0,6,0x1008,3);
    request(&s_port2,mb,8); quiet(&s_port2);
    assert(!s_port2.tx_length); App_CommonUpper(); assert(s_port1.baud==115200);
    modbus_request(mb,1,6,0x1008,2);
    request(&s_port2,mb,8); quiet(&s_port2);
    clock_us+=s_port2.tx_length*Sci_CharacterUs(&s_port2)+SCI_TX_MARGIN_US+1;
    App_CommonUpper(); assert(!s_baud_change_pending && s_port1.baud==19200);
    assert(StorageFlash_ReadBatteryNumber(number) && !memcmp(number,"BAT12345678",11));
    puts("PASS settings: real for-c034 dispatch/readback, invalid data/write failure, no-op save, old-rate TC ACK, both ports, broadcast, timeout, STOP restore");
}

static UINT16 report_word(const UINT8 *data,UINT16 offset) {
    return (UINT16)((data[offset*2U]<<8)|data[offset*2U+1U]);
}

static void serial_number_request(UINT8 *b,const UINT8 *serial)
{
    UINT16 crc;
    b[0]=1; b[1]=0x10; b[2]=0xFF; b[3]=0xF0;
    b[4]=0; b[5]=16; b[6]=32;
    memcpy(b+7,serial,32U);
    crc=Sci_CRC16RTU(b,39U); b[39]=(UINT8)crc; b[40]=(UINT8)(crc>>8);
}

static void check_customer_serial(struct SCI_PORT *port,const UINT8 *serial)
{
    UINT8 b[251];
    request(port,data2_request,sizeof(data2_request)); Sci_ServicePort(port);
    assert(transmit(port,b)==20 && b[0]==0xAA && b[1]==0xED && b[2]==15);
    assert(!memcmp(b+7,serial,11U) && b[18]==Customer_Bcc(b+3,15U) && b[19]==0xFA);
}

static void test_serial_number(void)
{
    STORAGE_FLASH_CONFIG_DATA config;
    UINT8 mb[41],b[251],serial[32],previous[32];
    UINT16 port_index,i,crc;
    int before;
    assert(Storage_LoadConfig(&config));
    config.rw.reserved[STORAGE_SERIAL_TAG_INDEX]=8U;
    config.rw.reserved[STORAGE_SERIAL_TAG_INV_INDEX]=8U;
    memcpy(config.reserved_legacy,"legacy-version",15U);
    assert(Storage_SaveConfig(&config)); mutations=0;
    InitProID(); InitUSART_CommonUpper();
    assert(!mutations && !memcmp(ProductionInfor.BMS_SerialNumber,BMS_SERIAL_NUMBER_DEFAULT,sizeof(BMS_SERIAL_NUMBER_DEFAULT)));
    check_customer_serial(&s_port1,ProductionInfor.BMS_SerialNumber);
    for(port_index=0U;port_index<2U;++port_index) {
        struct SCI_PORT *port=port_index ? &s_port2 : &s_port1;
        memset(serial,0,sizeof(serial)); memcpy(serial,port_index ? "2611170003" : "2611170002",10U);
        serial_number_request(mb,serial);
        request(port,mb,sizeof(mb)); quiet(port);
        assert(transmit(port,b)==8 && b[1]==0x10 && b[5]==16);
        assert(!memcmp(ProductionInfor.BMS_SerialNumber,serial,32U) && ProductionInfor.BMS_SerialNumberLength==11U);
        before=mutations;
        request(port,mb,sizeof(mb)); quiet(port); assert(transmit(port,b)==8 && mutations==before);
        memset(&ProductionInfor,0xA5,sizeof(ProductionInfor)); InitProID();
        assert(!memcmp(ProductionInfor.BMS_SerialNumber,serial,32U) && mutations==before);
        check_customer_serial(port,serial);
        modbus_request(mb,1,3,0xC002,48);
        request(port,mb,8); quiet(port);
        assert(transmit(port,b)==101 && b[2]==96 && !memcmp(b+3,serial,32U));
        assert(!memcmp(b+35,ProductionInfor.BMS_HardWareVersion,32U));
        assert(!memcmp(b+67,ProductionInfor.BMS_SoftWareVersion,32U));
        memcpy(previous,serial,32U); serial[0]='9'; serial_number_request(mb,serial);
        cut_after=0; mutations=0;
        request(port,mb,sizeof(mb)); quiet(port);
        assert(transmit(port,b)==5 && b[1]==0x90 && b[2]==RS485_ERROR_CMD_INVALID); cut_after=-1;
        assert(!memcmp(ProductionInfor.BMS_SerialNumber,previous,32U));
        InitProID(); check_customer_serial(port,previous);
        before=mutations; mb[40]^=1U;
        request(port,mb,sizeof(mb)); quiet(port); assert(!port->tx_length && mutations==before);
        for(i=0U;i<32U;++i) {
            memset(serial,'A',sizeof(serial)); serial[i]=0x7FU; serial_number_request(mb,serial);
            request(port,mb,sizeof(mb)); quiet(port);
            assert(transmit(port,b)==5 && b[2]==RS485_ERROR_DATA_INVALID && mutations==before);
        }
        memset(serial,0,sizeof(serial)); serial_number_request(mb,serial);
        request(port,mb,sizeof(mb)); quiet(port);
        assert(transmit(port,b)==5 && b[2]==RS485_ERROR_DATA_INVALID);
        serial[0]='A'; serial[2]='B'; serial_number_request(mb,serial);
        request(port,mb,sizeof(mb)); quiet(port);
        assert(transmit(port,b)==5 && b[2]==RS485_ERROR_DATA_INVALID && mutations==before);
        serial_number_request(mb,previous); mb[5]=15; mb[6]=30;
        crc=Sci_CRC16RTU(mb,37U); mb[37]=(UINT8)crc; mb[38]=(UINT8)(crc>>8);
        request(port,mb,39U); quiet(port);
        assert(transmit(port,b)==5 && b[2]==RS485_ERROR_DATA_INVALID && mutations==before);
        assert(!memcmp(ProductionInfor.BMS_SerialNumber,previous,32U));
    }
    memcpy(serial,"12345678901234567890123456789012",32U);
    serial_number_request(mb,serial); request(&s_port1,mb,sizeof(mb)); quiet(&s_port1);
    assert(transmit(&s_port1,b)==8 && ProductionInfor.BMS_SerialNumberLength==32U);
    InitProID(); check_customer_serial(&s_port2,serial);
    serial[0]='B'; serial_number_request(mb,serial); mb[0]=0;
    crc=Sci_CRC16RTU(mb,39U); mb[39]=(UINT8)crc; mb[40]=(UINT8)(crc>>8);
    request(&s_port2,mb,sizeof(mb)); quiet(&s_port2); assert(!s_port2.tx_length);
    InitProID(); check_customer_serial(&s_port1,serial);
    battery_number_request(mb,"BAT12345678"); request(&s_port1,mb,21U); quiet(&s_port1);
    assert(transmit(&s_port1,b)==8); InitProID();
    assert(!memcmp(ProductionInfor.BMS_SerialNumber,serial,32U));
    puts("PASS serial: real FFF0 -> Flash -> ProductionInfor -> C002/customer data2, both UARTs, reboot/default, 32-byte maximum, no-op, failure, CRC/ASCII/length, broadcast, independent FFF3");
}

static void test_report_cleanup(void) {
    struct { UINT8 data[RS485_RO_TOTAL_WORDS*2U]; UINT32 guard; } output;
    struct RS485MSG message;
    UINT16 i,word;
    memset(&message,0,sizeof(message)); memset(&output,0xa5,sizeof(output));
    output.guard=0x12345678UL;
    for(i=0;i<16U;++i) Registers_AFE1.Cell[i]=(UINT16)U16_SwapEndian(((3300U+i)*32U+4U)/5U);
    DataLoad_CellVolt();
    Sci_ACK_0x03_ReadRegs_Data(&message,output.data);
    assert(output.guard==0x12345678UL);
    for(i=0;i<SeriesNum;++i) assert(report_word(output.data,i)==3300U+i);
    for(;i<32U;++i) assert(report_word(output.data,i)==61001U);
    assert(report_word(output.data,96U)==0x4846U);
    assert(report_word(output.data,97U)==(UINT16)~0x4846U);
    for(i=98U;i<RS485_RO_TOTAL_WORDS;++i) assert(!report_word(output.data,i));
    assert(Sci_HostReadWords(0xc100U,1U,&word)==RS485_ERROR_ADDR_INVALID);
    message.u16Buffer[4]=0U;message.u16Buffer[5]=7U;
    Sci_WrReg_0x06_BMS_FunctionON(&message);
    assert(message.AckType==RS485_ACK_NEG && message.ErrorType==RS485_ERROR_DATA_INVALID);
    message.u16Buffer[5]=10U;g_stLowPowerRtcStatus.mode=NO_SLEEP;
    Sci_WrReg_0x06_BMS_FunctionON(&message);
    assert(g_stLowPowerRtcStatus.mode==DEEP_MODE);g_stLowPowerRtcStatus.mode=NO_SLEEP;
    puts("PASS report cleanup: real 61001 slots, unchanged D200 fault/padding offsets and length; C100/function7 retired, DEEP10 retained");
}

int main(void)
{
    UINT8 b[251], saved[251], mb[8], bad[9], mos[9]={0xDD,0x5A,0xE1,2,0,3,0xFF,0x1A,0x77};
    UINT16 i,n; int activity, before;
    UINT32 counters[11];
    host_storage_init(); StorageFlash_InitStatistics(RCC_CSR_PORRSTF,BOOT_FLAG_RESET_VALUE);
    setup_report(); InitUSART_CommonUpper();
    test_report_cleanup();
    /* Error recovery remains active after diagnostic counters are removed. */
    for(i=0;i<2;++i) {
        struct SCI_PORT *port=i ? &s_port2 : &s_port1;
        port->usart->SR=SCI_USART_ERRORS|USART_SR_RXNE|USART_SR_TXE|USART_SR_TC;
        Sci_PortIRQ(port);
        assert(port->protocol==SCI_DISCARD && primask==0);
        port->usart->SR=0;
        primask=1; receive(port,0x11); assert(primask==1); primask=0;
        Sci_ArmReceiver(port);
        port->usart->SR=0;
    }
    puts("PASS USART errors: both ports discard corrupt frames and retain interrupt masking");
    check_customer(&s_port1); check_customer(&s_port2);
    /* Each port changes protocol from frame to frame. Both replies remain private. */
    modbus_request(mb,1,3,0xD000,1);
    request(&s_port1,mb,8); request(&s_port2,data1_request,7);
    quiet(&s_port1); Sci_ServicePort(&s_port2);
    memcpy(saved,s_port1.message.u16Buffer,s_port1.tx_length);
    assert(transmit(&s_port2,b)==43);
    assert(transmit(&s_port1,b)==7 && !memcmp(b,saved,7));
    request(&s_port2,mb,8); quiet(&s_port2); assert(transmit(&s_port2,b)==7);
    check_customer(&s_port1);
    /* Customer packets are length-delimited; RTU's t3.5 does not split them. */
    quiet(&s_port1);
    for(i=0;i<3;++i) receive(&s_port1,data1_request[i]);
    clock_us+=5000U; Sci_ServicePort(&s_port1);
    for(i=3;i<7;++i) receive(&s_port1,data1_request[i]);
    assert(Sci_ServicePort(&s_port1) && transmit(&s_port1,b)==43);
    /* No execution/reply or valid communication activity for bad CRC/BCC/sum. */
    activity=comm_activity; before=writes;
    mb[7]^=1; request(&s_port1,mb,8); quiet(&s_port1);
    assert(!s_port1.tx_length && activity==comm_activity && writes==before); mb[7]^=1;
    memcpy(bad,data1_request,7); bad[5]^=1;
    request(&s_port2,bad,7); Sci_ServicePort(&s_port2);
    assert(!s_port2.tx_length && activity==comm_activity);
    modbus_request(mb,1,6,0x1102,7);
    for(i=0;i<8;++i) receive(&s_port2,mb[i]); /* No silence after the bad customer packet. */
    quiet(&s_port2); assert(writes==before && activity==comm_activity);
    modbus_request(mb,1,3,0xD000,1);
    memcpy(bad,mos,9); bad[7]^=1;
    request(&s_port2,bad,9); Sci_ServicePort(&s_port2);
    assert(!s_port2.tx_length && !host_off && activity==comm_activity);
    request(&s_port2,mos,9); Sci_ServicePort(&s_port2);
    assert(host_off==3 && transmit(&s_port2,b)==7 && b[2]==0);
    host_apply_ok=0; request(&s_port2,mos,9); Sci_ServicePort(&s_port2);
    assert(transmit(&s_port2,b)==7 && b[2]==0x80 && b[4]==0xFF && b[5]==0x80);
    host_apply_ok=1;
    /* Rising edges only; clear cannot fabricate a new edge for an active fault. */
    SH367309_Reg_Store.REG_BSTATUS1.bits.SC=1;
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp=1;
    Sci_CustomerUpdateCounters(); Sci_CustomerUpdateCounters();
    request(&s_port1,counters_request,7); Sci_ServicePort(&s_port1);
    assert(transmit(&s_port1,b)==29 && b[3]==22 && b[4]==0 && b[5]==1 && b[11]==1 && b[25]==1);
    request(&s_port1,clear_request,9); Sci_ServicePort(&s_port1); transmit(&s_port1,b);
    Sci_CustomerUpdateCounters(); assert(StorageFlash_ReadStatistics(counters));
    assert(counters[0]==0 && counters[3]==0 && counters[10]==1);
    SH367309_Reg_Store.REG_BSTATUS1.bits.SC=0; Sci_CustomerUpdateCounters();
    s_statistics.data.count[0]=70000; s_statistics.dirty=1; StorageFlash_FlushStatistics();
    SH367309_Reg_Store.REG_BSTATUS1.bits.SC=1;
    Sci_CustomerUpdateCounters(); assert(StorageFlash_ReadStatistics(counters) && counters[0]==70001);
    request(&s_port2,counters_request,7); Sci_ServicePort(&s_port2);
    assert(transmit(&s_port2,b)==29 && b[4]==0xFF && b[5]==0xFF);
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
    request(&s_port2,counters_request,7); Sci_ServicePort(&s_port2);
    assert(transmit(&s_port2,b)==29 && b[4]==0xFF && b[5]==0xFF && b[25]==1);
    mutations=0; cut_after=0;
    request(&s_port2,clear_request,9); Sci_ServicePort(&s_port2);
    assert(transmit(&s_port2,b)==7 && b[2]==0x80); cut_after=-1;
    s_statistics.available=0;
    request(&s_port1,counters_request,7); Sci_ServicePort(&s_port1);
    assert(transmit(&s_port1,b)==7 && b[2]==0x80 && b[3]==0);
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
    /* Exact RTU silence, legal one-character IDLE, and invalid >t1.5 gaps. */
    quiet(&s_port1); receive(&s_port1,mb[0]); receive(&s_port1,mb[1]); receive(&s_port1,mb[2]);
    clock_us+=Sci_CharacterUs(&s_port1)*12U/10U; Sci_ServicePort(&s_port1);
    for(i=3;i<8;++i) receive(&s_port1,mb[i]);
    clock_us+=Sci_FrameGapUs(&s_port1)+SCI_CLOCK_STEP_US-1U; assert(!Sci_ServicePort(&s_port1));
    ++clock_us; assert(Sci_ServicePort(&s_port1)); assert(transmit(&s_port1,b)==7);
    quiet(&s_port1); receive(&s_port1,mb[0]); receive(&s_port1,mb[1]);
    clock_us+=Sci_ByteGapUs(&s_port1)+20U; Sci_ServicePort(&s_port1);
    for(i=2;i<8;++i) receive(&s_port1,mb[i]);
    quiet(&s_port1); assert(!s_port1.tx_length);
    receive(&s_port1,0xEE); assert(!Sci_PortBusy(&s_port1));
    /* Timeout recovers, wrap works, and another port's TC cannot acknowledge IAP. */
    request(&s_port1,data1_request,7); Sci_ServicePort(&s_port1);
    s_port1.message.reset_after_tx=1;
    request(&s_port2,data2_request,7); Sci_ServicePort(&s_port2); transmit(&s_port2,b);
    assert(!u8FlashUpdateFlag);
    clock_us+=s_port1.tx_length*Sci_CharacterUs(&s_port1)+SCI_TX_MARGIN_US+1U;
    Sci_ServicePort(&s_port1); assert(!s_port1.tx_length && !u8FlashUpdateFlag);
    request(&s_port1,data1_request,7); Sci_ServicePort(&s_port1);
    s_port1.message.reset_after_tx=1; transmit(&s_port1,b); assert(u8FlashUpdateFlag);
    u8FlashUpdateFlag=0; clock_us=UINT32_MAX-1000U;
    check_customer(&s_port2);
    s_port1.baud=115200U; assert(Sci_ByteGapUs(&s_port1)==750 && Sci_FrameGapUs(&s_port1)==1750);
    check_customer(&s_port1);
    /* Every advertised one-byte length remains bounded, including 0xFF. */
    for(n=0;n<256;++n) {
        struct { struct RS485MSG m; UINT32 guard; } x;
        memset(&x,0,sizeof(x)); x.guard=0x12345678;
        Sci_CustomerFeed(&x.m,0xDD);Sci_CustomerFeed(&x.m,0xA5);Sci_CustomerFeed(&x.m,0xAA);
        Sci_CustomerFeed(&x.m,(UINT8)n);
        for(i=0;i<260;++i) { Sci_CustomerFeed(&x.m,0x33); assert(x.guard==0x12345678); }
    }
    test_remaining_time();
    test_communication_settings();
    test_serial_number();
    puts("PASS both UARTs: protocol switching/interleaving, all six customer commands, units, checksums, bounds");
    puts("PASS RTU: t1.5/t3.5, invalid gaps, wrap, TX timeout, per-port IAP ACK, STOP retry");
    return 0;
}
