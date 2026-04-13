# 閫氫俊鍐?EEPROM 鏍囧織浣嶆槧灏勮〃

鏈枃鏄?`COMMUNICATION_EEPROM_FLAG_REFACTOR_DEBUG.md` 鐨勯厤濂楄〃锛岀洰鐨勫緢鍗曚竴锛?
- EEPROM 鍦板潃涓嶅彉
- 閫氫俊瀵勫瓨鍣ㄥ湴鍧€涓嶅彉
- 鍙妸褰撳墠杩囧鐨勫啓鏍囧織浣嶏紝鏀舵暃鎴愭洿灏戠殑鍧楃骇 dirty 浣?- 鏂逛究浣犲湪 Keil 鍦ㄧ嚎璋冭瘯鏃堕€愰」鏍稿

## 1. 寤鸿鐨勬柊鏍囧織缁撴瀯

### 1.1 鎬?dirty 浣嶅浘

寤鸿鏈€缁堝彧淇濈暀涓€涓€讳綅鍥撅細

- `u32EepromDirtyMask`

姣忎竴浣嶈〃绀轰竴涓ぇ鍧楋細

| 浣嶅彿 | 鍧楀悕 | 璇存槑 |
|---|---|---|
| 0 | `CALIB` | K/B 鏍″噯鍧?|
| 1 | `PROTECT` | 淇濇姢鍙傛暟鍧?|
| 2 | `RTC` | RTC 鍧?|
| 3 | `SOC_TABLE` | SOC 琛ㄥ潡 |
| 4 | `COPPERLOSS` | 閾滄崯鍧?|
| 5 | `OTHER1` | OtherElement1 鍧?|
| 6 | `HEAT_COOL` | 鐑鐞嗗潡 |
| 7 | `PRODUCT_INFO` | 浜у搧淇℃伅鍧?|
| 8 | `EVENT_RECORD` | 浜嬩欢璁板綍鍧?|
| 9 | `AFE_PARAM` | AFE 鍙傛暟鍧?|
| 10 | `OFFSET` | 褰撳墠鍋忕Щ閲忓潡 |
| 11 | `SYS_FLAG` | 绯荤粺鍔熻兘 / 寮€鍏崇姸鎬佸潡 |

### 1.2 寤鸿淇濈暀鐨勫皯閲忓瓙 mask

鍙粰澶у潡淇濈暀瀛?mask锛?
- `u32ProtectDirtyMask`
- `u32CalibDirtyMask`
- `u32Other1DirtyMask`
- `u32HeatCoolDirtyMask`

鍏朵粬鍧楀鏋滀竴娆″氨鏄暣鍧楁洿鏂帮紝鍙互鍙繚鐣欏潡绾?dirty锛屼笉鍐嶇粏鍒嗐€?
## 2. 鏃ф爣蹇楀埌鏂版爣蹇楃殑鏄犲皠

### 2.1 鏍″噯鐩稿叧

| 鏃ф爣蹇?| 褰撳墠鏉ユ簮 | 寤鸿鏂版爣蹇?|
|---|---|---|
| `u8E2P_KB_WriteFlag` | `Sci_WrRegs_0x10_CalibCoef()` / `EEPROM_ResetData_AllToDefault()` | `u32EepromDirtyMask.bit0` + `u32CalibDirtyMask` |

璇存槑锛?
- 鐜板湪 `KB_NUM` 鏄€愬鍐?- 鏈潵鍙户缁寜 `KB index` 鍒嗘鍐?- 瀵?Keil 璋冭瘯鏈€鍙嬪ソ鐨勬槸鐪?`u32CalibDirtyMask` 鏄惁姝ｇ‘鍑忎綅

### 2.2 淇濇姢鍙傛暟

| 鏃ф爣蹇?| 褰撳墠鏉ユ簮 | 寤鸿鏂版爣蹇?|
|---|---|---|
| `u32E2P_Pro_VolCur_WriteFlag` | `Sci_WrRegs_0x10_Protect()`銆乣Sci_WrReg_0x06_Reset_ProtectElement()` | `u32EepromDirtyMask.bit1` + `u32ProtectDirtyMask` |
| `u32E2P_Pro_Temp_WriteFlag` | 鍚屼笂 | `u32EepromDirtyMask.bit1` + `u32ProtectDirtyMask` |
| `u32E2P_Pro_Other_WriteFlag` | 鍚屼笂 | `u32EepromDirtyMask.bit1` + `u32ProtectDirtyMask` |

寤鸿鐨勫瓙鍖哄垎锛?
| 瀛愬潡 | 褰撳墠鑼冨洿 | 寤鸿瀛愪綅鑼冨洿 |
|---|---|---|
| 鐢靛帇 / 鐢垫祦淇濇姢 | `0x2000 ~ 0x201D` 涓€绫?| 0..29 |
| 娓╁害淇濇姢 | `0x201E ~ 0x2036` 涓€绫?| 30..54 |
| 鍏朵粬淇濇姢 | `0x2037 ~ 0x2040` 涓€绫?| 55..64 |

澶囨敞锛?
- 褰撳墠浠ｇ爜閲?3 涓啓鏍囧織瀹為檯涓婂彧鏄负浜嗗垎娈靛啓
- 浠庤皟璇曡搴︾湅锛屽畠浠畬鍏ㄥ彲浠ュ苟鎴愪竴涓繚鎶ゅ潡 dirty

### 2.3 RTC

| 鏃ф爣蹇?| 褰撳墠鏉ユ簮 | 寤鸿鏂版爣蹇?|
|---|---|---|
| `u32E2P_RTC_Element_WriteFlag` | `Sci_WrRegs_0x10_RTC()` | `u32EepromDirtyMask.bit2` |

璇存槑锛?
- RTC 鍧楁湰鏉ュ氨鏄暣鍧楁洿鏂?- 娌″繀瑕佸啀鎷嗘洿缁?
### 2.4 SOC 琛?
| 鏃ф爣蹇?| 褰撳墠鏉ユ簮 | 寤鸿鏂版爣蹇?|
|---|---|---|
| `u8E2P_SocTable_WriteFlag` | `Sci_WrRegs_0x10_SocTable()` | `u32EepromDirtyMask.bit3` |

璇存槑锛?
- 杩欐槸鍏稿瀷鏁村潡鍐欏叆
- 鐩存帴涓€涓潡绾?dirty 鍗冲彲

### 2.5 閾滄崯

| 鏃ф爣蹇?| 褰撳墠鏉ユ簮 | 寤鸿鏂版爣蹇?|
|---|---|---|
| `u8E2P_CopperLoss_WriteFlag` | `Sci_WrRegs_0x10_CopperLoss()` | `u32EepromDirtyMask.bit4` |

璇存槑锛?
- 涔熸槸鏁村潡鍐欏叆
- 寤鸿涓嶅啀鎷嗗瓧娈电骇 flag

### 2.6 OtherElement1

| 鏃ф爣蹇?| 褰撳墠鏉ユ簮 | 寤鸿鏂版爣蹇?|
|---|---|---|
| `u32E2P_OtherElement1_WriteFlag` | `Sci_WrRegs_0x10_Balance()`銆乣Sci_WrRegs_0x10_SysOther()`銆乣Sci_WrRegs_0x10_SleepElement()`銆乣Sci_WrRegs_0x10_SocElement()`銆乣Sci_WrRegs_0x10_SystemElement()` | `u32EepromDirtyMask.bit5` + `u32Other1DirtyMask` |

寤鸿鐨勫瓙浣嶏細

| 瀛愪綅 | 瀵瑰簲鍔熻兘 |
|---|---|
| 0..3 | 骞宠　鍙傛暟 |
| 4..7 | 鎵撳紑鏃堕棿 / MOS 鐩稿叧 |
| 8..11 | CS / CBC / 鐢垫祦妯″紡 |
| 12..15 | 鍐峰嵈鐩稿叧 |
| 16..23 | 鐫＄湢鐩稿叧 |
| 24..27 | SOC 鐩稿叧 |
| 28..31 | 绯荤粺涓叉暟 / 棰勫厖 / 鐢甸樆鐩稿叧 |

澶囨敞锛?
- 褰撳墠浠ｇ爜閲岃繖涓爣蹇楅泦鍚堟槸鏈€涔辩殑涓€鍧?- 鏈€閫傚悎鍏堟敼鎴愨€滃潡绾?dirty + 瀛?mask鈥?
### 2.7 鐑鐞?
| 鏃ф爣蹇?| 褰撳墠鏉ユ簮 | 寤鸿鏂版爣蹇?|
|---|---|---|
| `u32E2P_HeatCool_WriteFlag` | `Sci_WrRegs_0x10_HeatCoolElement()`銆乣Sci_WrReg_0x06_Reset_HeatCool()` | `u32EepromDirtyMask.bit6` + `u32HeatCoolDirtyMask` |

璇存槑锛?
- 寤鸿淇濈暀瀛?mask
- 浣嗕笉鍐嶈閫氫俊灞傚幓閫愪綅鐞嗚В EEPROM 鍐欓『搴?
### 2.8 浜у搧淇℃伅

| 鏃ф爣蹇?| 褰撳墠鏉ユ簮 | 寤鸿鏂版爣蹇?|
|---|---|---|
| `ProductionInfor.BMS_SerialNumber_WriteFlag` | `Sci_WrRegs_0x10_SN_Version()` | `u32EepromDirtyMask.bit7` |
| `ProductionInfor.BMS_HardWareVersion_WriteFlag` | `Sci_WrRegs_0x10_SN_Version()` | `u32EepromDirtyMask.bit7` |
| `ProductionInfor.BMS_SoftWareVersion_WriteFlag` | `Sci_WrRegs_0x10_SN_Version()` | `u32EepromDirtyMask.bit7` |

璇存槑锛?
- 杩欎笁椤归€昏緫涓婂睘浜庡悓涓€鍧?- 娌″繀瑕佸悇鑷淮鎶や竴濂楀啓鐘舵€?
### 2.9 浜嬩欢璁板綍

| 鏃ф爣蹇?| 褰撳墠鏉ユ簮 | 寤鸿鏂版爣蹇?|
|---|---|---|
| `gu8_Reset_EventRecord` | `Sci_WrReg_0x06_Reset_EventRecord()` | `u32EepromDirtyMask.bit8` 鎴栫嫭绔?`event_clear_pending` |

璇存槑锛?
- 浜嬩欢娓呯┖姣旇緝鐗规畩
- 鍙互淇濈暀涓€涓嫭绔嬬殑 `event_clear_pending`
- 浣嗕笉瑕佸拰鏅€氬弬鏁板啓鏍囧織娣峰湪涓€璧?
### 2.10 AFE 鍙傛暟

| 鏃ф爣蹇?| 褰撳墠鏉ユ簮 | 寤鸿鏂版爣蹇?|
|---|---|---|
| `AFE_PARAM_WRITE_Flag` | `Sci_WrRegs_0x10_SysOther()`銆乣Sci_WrRegs_0x10_SystemElement()`銆乣Sci_WrRegs_0x10_FlashConnect()` 鐩稿叧娴佺▼ | `u32EepromDirtyMask.bit9` |

璇存槑锛?
- AFE 鍙傛暟閫氬父鏄仈鍔ㄥ啓
- 瀵硅皟璇曟潵璇达紝鏈€濂芥妸瀹冨綋鎴愪竴涓嫭绔嬪潡

### 2.11 鍋忕Щ閲?
| 鏃ф爣蹇?| 褰撳墠鏉ユ簮 | 寤鸿鏂版爣蹇?|
|---|---|---|
| `curr_offset` / `OffsetValue_CHG` / `OffsetValue_DSG` | `DataLoad_CurrentCali_startup()`銆乣InitData_E2prom()` | `u32EepromDirtyMask.bit10` |

璇存槑锛?
- 杩欐槸杩愯鐩稿叧鎸佷箙鍖栧€?- 涓嶅缓璁啀鍜岄€氫俊鍙傛暟娣峰啓鍦ㄤ竴璧风悊瑙?
### 2.12 绯荤粺鍔熻兘浣?
| 鏃ф爣蹇?| 褰撳墠鏉ユ簮 | 寤鸿鏂版爣蹇?|
|---|---|---|
| `System_OnOFF_Func` 鐩稿叧浣?| `Sci_WrReg_0x06_SwitchON()`銆乣Sci_WrReg_0x06_SwitchOFF()`銆乣Sci_WrReg_0x06_BMS_FunctionON()`銆乣Sci_WrReg_0x06_BMS_FunctionOFF()` | `u32EepromDirtyMask.bit11` |

璇存槑锛?
- 杩欐槸杩愯鐘舵€佸拰 EEPROM 鎸佷箙鍖栫姸鎬佺殑浜ょ晫鍖?- 鏈€濂藉崟鐙竴涓潡鏉ヨ窡韪?
## 3. 鏃ф爣蹇楀浣曞噺灏?
寤鸿鐨勭缉鍑忔柟寮忓涓嬶細

### 3.1 浠庡鍙橀噺鍙樻垚涓€涓€绘帺鐮?
鎶婅繖浜涘彉閲忛€愭鍚堝苟锛?
- `u8E2P_KB_WriteFlag`
- `u32E2P_Pro_VolCur_WriteFlag`
- `u32E2P_Pro_Temp_WriteFlag`
- `u32E2P_Pro_Other_WriteFlag`
- `u32E2P_OtherElement1_WriteFlag`
- `u32E2P_HeatCool_WriteFlag`
- `u32E2P_RTC_Element_WriteFlag`
- `u8E2P_SocTable_WriteFlag`
- `u8E2P_CopperLoss_WriteFlag`

鍚堝苟鍚庡厛淇濈暀涓€涓細

- `u32EepromDirtyMask`

### 3.2 鍐嶆寜闇€瑕佷繚鐣欏皯閲忓瓙浣嶅浘

鍙繚鐣欙細

- `u32CalibDirtyMask`
- `u32ProtectDirtyMask`
- `u32Other1DirtyMask`
- `u32HeatCoolDirtyMask`

杩欐牱 Keil 閲岀湅鍙橀噺鏃堕潪甯哥洿瑙傦細

- `u32EepromDirtyMask` 鐪嬫湁娌℃湁浠诲姟
- 瀛?mask 鐪嬪叿浣撴槸鍝釜瀛楁缁?
## 4. Keil 璋冭瘯鏃跺缓璁湅鐨勫彉閲?
### 4.1 鍏堢湅鎬荤姸鎬?
- `u32EepromDirtyMask`
- `System_ErrFlag.u8ErrFlag_Com_EEPROM`
- `MCUO_E2PR_WP`

### 4.2 鍐嶇湅鍧楃骇鐘舵€?
- `u32ProtectDirtyMask`
- `u32CalibDirtyMask`
- `u32Other1DirtyMask`
- `u32HeatCoolDirtyMask`

### 4.3 鍐嶇湅褰撳墠涓氬姟鍙橀噺

- `g_u16CalibCoefK[]`
- `g_i16CalibCoefB[]`
- `PRT_E2ROMParas`
- `OtherElement`
- `Heat_Cool_Element`
- `ProductionInfor`
- `RTC_time`

### 4.4 鏂偣鎺ㄨ崘

鎸夎繖涓『搴忔渶濂借皟锛?
1. `Sci_WrRegs_0x10_*()`
2. `Sci_WrReg_0x06_*()`
3. `App_E2promDeal()`
4. `EEPROM_Process()` 浠ュ悗濡傛灉浣犻噸鏋勫嚭鏉ョ殑璇?5. `WriteEEPROM_ByteData_Circle()`
6. `WriteEEPROM_Word_NoZone()`
7. `WriteEEPROM_Byte()`

## 5. 瀹炴柦寤鸿

### 绗竴闃舵

涓嶅垹鏃?flag锛屽彧鏂板锛?
- `u32EepromDirtyMask`
- `u32ProtectDirtyMask`
- `u32Other1DirtyMask`
- `u32HeatCoolDirtyMask`

鐒跺悗鍦ㄩ€氫俊鍐欏叆鍚庡悓鏃剁疆鏂版棫鏍囧織锛屾柟渚垮鐓с€?
### 绗簩闃舵

纭鏂版祦绋嬬ǔ瀹氬悗锛?
- 鍏堝垹 `u8E2P_SocTable_WriteFlag`
- 鍐嶅垹 `u8E2P_CopperLoss_WriteFlag`
- 鍐嶅垹 `u32E2P_RTC_Element_WriteFlag`
- 鍐嶆敹 `u32E2P_Pro_*` 涓変釜淇濇姢鏍囧織

### 绗笁闃舵

鏈€缁堝彧淇濈暀鍧楃骇 dirty 鍜屽皯閲忓瓙 mask銆?
## 6. 缁撹

鏈€閫傚悎浣犺繖涓伐绋嬬殑鏀舵暃鏂瑰紡涓嶆槸鈥滃畬鍏ㄩ噸鍐欌€濓紝鑰屾槸锛?
- 鍦板潃鍜屽崗璁繚鎸佷笉鍔?- 鏍囧織浣嶄粠瀛楁绾ф敼鎴愬潡绾?- 灏戞暟澶у潡淇濈暀瀛?mask
- 璋冭瘯鏃朵紭鍏堢湅 dirty銆乄P銆佸洖璇荤粨鏋滃拰閿欒鏍囧織

杩欎細鏄庢樉闄嶄綆閫氫俊鍐?EEPROM 鐨勫鏉傚害锛屼篃鏇撮€傚悎 Keil 鍦ㄧ嚎瑙傚療銆?
## 7. 当前落地

- `EEPROM_DIRTY_BLOCK_SYS_FLAG` 已接到 `System_OnOFF_Func` 的保存路径
- `EEPROM_DIRTY_BLOCK_OFFSET` 已接到虚拟电流校准偏移量保存路径
- 通信侧不再直接写 `EEPROM_ADDR_SYS_FUNC_SELECT`
- `DataLoad_CurrentCali_startup()` 会在启动阶段主动刷新 `OFFSET` dirty
