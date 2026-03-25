/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "MosCtrl.h"
#include "AFE.h"
#include "flash.h"
#include "Uart2App.h"

BOOL bAFEDsgMosFlg = 0;
BOOL bAFEChgMosFlg = 0;

/*************************************************************************************************
* 函数名: MosCtrl
* 参  数: 无
* 返回值: 无
* 描  述: MOS控制
*************************************************************************************************/
void MosCtrl(void)
{
	if(!bAFEWrFlag)
	{
		if(Info.bUV || Info.bPACKUV || Info.bOCD1 || Info.bOCD2 || Info.bSC || Info.bUTD || Info.bOTD
			|| Info.bICOT || Info.bSLEEP || Info.bAFE_ERR || (ucCmdAFEMode == 0x03) || (ucCmdAFEMode == 0x04))
		{
			if(bAFEDsgMosFlg)
			{
				bAFEDsgMosFlg = 0;
				SH_AFE_SetDsgMos(SH_DISABLE);
			}
		}
		else
		{
			if(!bAFEDsgMosFlg)
			{
				bAFEDsgMosFlg = 1;
				SH_AFE_SetDsgMos(SH_ENABLE);
			}
		}
		
		if(Info.bOV || Info.bPACKOV || Info.bOCC || Info.bUTC || Info.bOTC || Info.bCTO || Info.bICOT 
			|| Info.bSLEEP || Info.bAFE_ERR || (ucCmdAFEMode == 0x03) || (ucCmdAFEMode == 0x04))
		{
			if(bAFEChgMosFlg)
			{
				bAFEChgMosFlg = 0;
				
				SH_AFE_SetChgMos(SH_DISABLE);
			}
		}
		else
		{
			if(!bAFEChgMosFlg)
			{
				bAFEChgMosFlg = 1;
				
				SH_AFE_SetChgMos(SH_ENABLE);
			}
		}
	}
	else		// 更新AFE，关闭充放电MOS
	{
		if(bAFEDsgMosFlg)
		{
			bAFEDsgMosFlg = 0;
			SH_AFE_SetDsgMos(SH_DISABLE);
		}
		if(bAFEChgMosFlg)
		{
			bAFEChgMosFlg = 0;
			
			SH_AFE_SetChgMos(SH_DISABLE);
		}
	}
}


