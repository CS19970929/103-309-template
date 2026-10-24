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
    Sci_ServicePort(port,0);
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
    request(port,data1_request,sizeof(data1_request)); assert(Sci_ServicePort(port,0));
    n=transmit(port,b); assert(n==43 && b[2]==1 && b[3]==37);
    assert(le(b+4)==3300 && le(b+16)==3306 && b[18]==65);
    assert(le(b+20)==231 && le(b+22)==8766 && b[24]==55);
    assert(le(b+26)==4200 && le(b+28)==2800 && b[30]==45);
    assert(le(b+31)==10120 && le(b+33)==9700 && le(b+35)==1000);
    assert(le(b+37)==4100 && le(b+39)==3000);
    assert(b[41]==Customer_Bcc(b+2,39) && b[42]==0xF0);
    request(port,data2_request,sizeof(data2_request)); Sci_ServicePort(port,0);
    assert(transmit(port,b)==20 && b[0]==0xAA && b[1]==0xED && b[2]==15);
    assert(le(b+3)==25 && !memcmp(b+5,"1D12345678901",13));
    assert(b[18]==Customer_Bcc(b+3,15) && b[19]==0xFA);
    request(port,data3_request,sizeof(data3_request)); Sci_ServicePort(port,0);
    assert(transmit(port,b)==22 && b[2]==3 && b[3]==16 && le(b+4)==900);
    for(n=6;n<20;++n) assert(b[n]==0);
    SeriesNum=6; request(port,data1_request,7); Sci_ServicePort(port,0); transmit(port,b);
    assert(le(b+16)==61001); SeriesNum=7;
}

int main(void)
{
    UINT8 b[251], saved[251], mb[8], bad[9], mos[9]={0xDD,0x5A,0xE1,2,0,3,0xFF,0x1A,0x77};
    UINT16 i,n; int activity, before;
    setup_report(); InitUSART_CommonUpper();
    check_customer(&s_port1); check_customer(&s_port2);
    /* Each port changes protocol from frame to frame. Both replies remain private. */
    modbus_request(mb,1,3,0xD000,1);
    request(&s_port1,mb,8); request(&s_port2,data1_request,7);
    quiet(&s_port1); Sci_ServicePort(&s_port2,0);
    memcpy(saved,s_port1.message.u16Buffer,s_port1.tx_length);
    assert(transmit(&s_port2,b)==43);
    assert(transmit(&s_port1,b)==7 && !memcmp(b,saved,7));
    request(&s_port2,mb,8); quiet(&s_port2); assert(transmit(&s_port2,b)==7);
    check_customer(&s_port1);
    /* Customer packets are length-delimited; RTU's t3.5 does not split them. */
    quiet(&s_port1);
    for(i=0;i<3;++i) receive(&s_port1,data1_request[i]);
    clock_us+=5000U; Sci_ServicePort(&s_port1,0);
    for(i=3;i<7;++i) receive(&s_port1,data1_request[i]);
    assert(Sci_ServicePort(&s_port1,0) && transmit(&s_port1,b)==43);
    /* No execution/reply or valid communication activity for bad CRC/BCC/sum. */
    activity=comm_activity; before=writes;
    mb[7]^=1; request(&s_port1,mb,8); quiet(&s_port1);
    assert(!s_port1.tx_length && activity==comm_activity && writes==before); mb[7]^=1;
    memcpy(bad,data1_request,7); bad[5]^=1;
    request(&s_port2,bad,7); Sci_ServicePort(&s_port2,0);
    assert(!s_port2.tx_length && activity==comm_activity);
    modbus_request(mb,1,6,0x1102,7);
    for(i=0;i<8;++i) receive(&s_port2,mb[i]); /* No silence after the bad customer packet. */
    quiet(&s_port2); assert(writes==before && activity==comm_activity);
    modbus_request(mb,1,3,0xD000,1);
    memcpy(bad,mos,9); bad[7]^=1;
    request(&s_port2,bad,9); Sci_ServicePort(&s_port2,0);
    assert(!s_port2.tx_length && !host_off && activity==comm_activity);
    request(&s_port2,mos,9); Sci_ServicePort(&s_port2,0);
    assert(host_off==3 && transmit(&s_port2,b)==7 && b[2]==0);
    host_apply_ok=0; request(&s_port2,mos,9); Sci_ServicePort(&s_port2,0);
    assert(transmit(&s_port2,b)==7 && b[2]==0x80 && b[4]==0xFF && b[5]==0x80);
    host_apply_ok=1;
    /* Rising edges only; clear cannot fabricate a new edge for an active fault. */
    SH367309_Reg_Store.REG_BSTATUS1.bits.SC=1;
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp=1;
    Sci_CustomerUpdateCounters(); Sci_CustomerUpdateCounters();
    request(&s_port1,counters_request,7); Sci_ServicePort(&s_port1,0);
    assert(transmit(&s_port1,b)==29 && b[3]==22 && b[4]==0 && b[5]==1 && b[11]==1 && b[25]==1);
    request(&s_port1,clear_request,9); Sci_ServicePort(&s_port1,0); transmit(&s_port1,b);
    Sci_CustomerUpdateCounters(); assert(s_counters[0]==0 && s_counters[3]==0 && s_counters[10]==1);
    SH367309_Reg_Store.REG_BSTATUS1.bits.SC=0; Sci_CustomerUpdateCounters();
    s_counters[0]=0xFFFF; SH367309_Reg_Store.REG_BSTATUS1.bits.SC=1;
    Sci_CustomerUpdateCounters(); assert(s_counters[0]==0xFFFF);
    /* Exact RTU silence, legal one-character IDLE, and invalid >t1.5 gaps. */
    quiet(&s_port1); receive(&s_port1,mb[0]); receive(&s_port1,mb[1]); receive(&s_port1,mb[2]);
    clock_us+=Sci_CharacterUs(&s_port1)*12U/10U; Sci_ServicePort(&s_port1,0);
    for(i=3;i<8;++i) receive(&s_port1,mb[i]);
    clock_us+=Sci_FrameGapUs(&s_port1)+SCI_CLOCK_STEP_US-1U; assert(!Sci_ServicePort(&s_port1,0));
    ++clock_us; assert(Sci_ServicePort(&s_port1,0)); assert(transmit(&s_port1,b)==7);
    quiet(&s_port1); receive(&s_port1,mb[0]); receive(&s_port1,mb[1]);
    clock_us+=Sci_ByteGapUs(&s_port1)+20U; Sci_ServicePort(&s_port1,0);
    for(i=2;i<8;++i) receive(&s_port1,mb[i]);
    quiet(&s_port1); assert(!s_port1.tx_length);
    receive(&s_port1,0xEE); assert(!Sci_PortBusy(&s_port1));
    /* Wake validation never writes. Re-init retains and executes the held frame once. */
    before=writes; modbus_request(mb,1,6,0x1102,7); request(&s_port1,mb,8);
    clock_us+=Sci_FrameGapUs(&s_port1)+SCI_CLOCK_STEP_US;
    assert(Sci_VerifyWakeFrames() && s_port1.frame_pending==2 && writes==before);
    InitUSART_CommonUpper(); App_CommonUpper(); assert(writes==before+1);
    transmit(&s_port1,b);
    /* Timeout recovers, wrap works, and another port's TC cannot acknowledge IAP. */
    request(&s_port1,data1_request,7); Sci_ServicePort(&s_port1,0);
    s_port1.message.reset_after_tx=1;
    request(&s_port2,data2_request,7); Sci_ServicePort(&s_port2,0); transmit(&s_port2,b);
    assert(!u8FlashUpdateFlag);
    clock_us+=s_port1.tx_length*Sci_CharacterUs(&s_port1)+SCI_TX_MARGIN_US+1U;
    Sci_ServicePort(&s_port1,0); assert(!s_port1.tx_length && !u8FlashUpdateFlag);
    request(&s_port1,data1_request,7); Sci_ServicePort(&s_port1,0);
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
    puts("PASS both UARTs: protocol switching/interleaving, all six customer commands, units, checksums, bounds");
    puts("PASS RTU: t1.5/t3.5, invalid gaps, wrap, TX timeout, per-port IAP ACK, wake validation without writes");
    return 0;
}
