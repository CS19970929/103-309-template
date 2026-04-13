# 閫氫俊鍐?EEPROM 鏍囧織浣嶆敹鏁涗笌 Keil 璋冭瘯鏂规

鏈枃鍙璁轰竴浠朵簨锛?*涓嶆敼 EEPROM 鍦板潃銆佷笉鏀归€氫俊鍦板潃**锛屾妸褰撳墠杩囧鐨勫啓鏍囧織浣嶆敹鏁涙帀锛屽悓鏃惰閫昏緫鏇撮€傚悎 Keil 鍦ㄧ嚎璋冭瘯銆?
## 1. 褰撳墠鐜扮姸

### 1.1 閫氫俊鍐欏叆鍙?
`0x10` 鍐欏瘎瀛樺櫒鏈€缁堜細杩涘叆 `Sci_Deal_WrRegs_0x10()`锛屽啀鍒嗗彂鍒帮細

- `Sci_WrRegs_0x10_CalibCoef()`
- `Sci_WrRegs_0x10_Protect()`
- `Sci_WrRegs_0x10_SocTable()`
- `Sci_WrRegs_0x10_CopperLoss()`
- `Sci_WrRegs_0x10_RTC()`
- `Sci_WrRegs_0x10_Balance()`
- `Sci_WrRegs_0x10_SysOther()`
- `Sci_WrRegs_0x10_SleepElement()`
- `Sci_WrRegs_0x10_SocElement()`
- `Sci_WrRegs_0x10_SystemElement()`
- `Sci_WrRegs_0x10_HeatCoolElement()`
- `Sci_WrRegs_0x10_SN_Version()`
- `Sci_WrRegs_0x10_FlashConnect()`

杩欎簺鍑芥暟澶у鏁颁笉鏄洿鎺ュ啓 EEPROM锛岃€屾槸鍏堟敼 RAM锛屽啀鎶婁竴鍫嗗叏灞€鍐欐爣蹇椾綅鎷夎捣鏉ャ€?
### 1.2 鐜版湁鍐欐爣蹇?
褰撳墠鍐欐爣蹇楁瘮杈冨垎鏁ｏ紝鍏稿瀷鐨勬湁锛?
- `u8E2P_KB_WriteFlag`
- `u32E2P_Pro_VolCur_WriteFlag`
- `u32E2P_Pro_Temp_WriteFlag`
- `u32E2P_Pro_Other_WriteFlag`
- `u32E2P_OtherElement1_WriteFlag`
- `u32E2P_HeatCool_WriteFlag`
- `u32E2P_RTC_Element_WriteFlag`
- `u8E2P_SocTable_WriteFlag`
- `u8E2P_CopperLoss_WriteFlag`
- `gu8_Reset_EventRecord`
- `ProductionInfor.BMS_*_WriteFlag`

闂涓嶅湪浜庘€滄湁鏍囧織鈥濓紝鑰屽湪浜庘€滄爣蹇楄繃缁嗐€佽繃鏁ｃ€佽亴璐ｆ贩鏉傗€濄€?
### 1.3 褰撳墠璋冨害鏂瑰紡

`App_E2promDeal()` 鐜板湪浼氳疆璇㈣繖浜涙爣蹇楋紝鍙鍛戒腑灏辫皟鐢?`WriteEEPROM_ByteData_Circle()`銆?
杩欎釜妯″紡鏈韩娌￠敊锛屼絾瀹冪殑闂鏄細

- 涓婂眰閫氫俊鍜屼笅灞傚瓨鍌ㄨ€﹀悎澶繁
- 鏍囧織浣嶅鍒伴毦浠ヤ竴鐪肩湅鎳?- Keil 璋冭瘯鏃跺緢闅惧垽鏂€滆繖娆″埌搴曡鍐欏摢涓€鍧椻€?
## 2. 鐩爣

鐩爣淇濇寔寰堟槑纭細

- EEPROM 鍦板潃涓嶅彉
- 閫氫俊瀵勫瓨鍣ㄥ湴鍧€涓嶅彉
- 鍙傛暟鍚箟涓嶅彉
- 浣嗗啓鍏ラ€昏緫鏇寸畝鍗?- 鏇寸ǔ瀹?- 鏇撮€傚悎 Keil 鍦ㄧ嚎璋冭瘯

## 3. 鎺ㄨ崘鐨勬敹鏁涙柟妗?
### 3.1 鍙繚鐣欌€滃潡绾?dirty鈥?
寤鸿鎶婃墍鏈夊啓鏍囧織鏀舵暃鎴愪竴涓€?dirty 鎺╃爜锛屼緥濡傦細

- `u32EepromDirtyMask`

姣忎竴浣嶄唬琛ㄤ竴涓ぇ鍧楋細

- bit0: 鏍″噯鍧?- bit1: 淇濇姢鍧?- bit2: RTC 鍧?- bit3: SOC 琛ㄥ潡
- bit4: 閾滄崯鍧?- bit5: OtherElement1 鍧?- bit6: 鐑鐞嗗潡
- bit7: 浜у搧淇℃伅鍧?- bit8: 浜嬩欢璁板綍鍧?- bit9: AFE 鍙傛暟鍧?- bit10: 鍋忕Щ閲忓潡
- bit11: 绯荤粺鍔熻兘浣嶅潡

閫氫俊灞傚彧鍋氾細

1. 鏇存柊 RAM
2. `mark_dirty(block_id)`

EEPROM 灞傚彧鍋氾細

1. 鎵惧埌 dirty 鍧?2. 鍐欎竴涓渶灏忓崟鍏?3. 娓呮帀瀵瑰簲 dirty 浣?
### 3.2 澶у潡鍐嶄繚鐣欏皯閲忓瓙浣嶅浘

濡傛灉浣犺繕鎯充繚鐣欌€滃垎娈靛啓鈥濈殑浼樺娍锛屽彲浠ュ彧缁欏皯鏁板ぇ鍧椾繚鐣欏瓙 mask锛?
- `u32ProtectDirtyMask`
- `u32Other1DirtyMask`
- `u32HeatCoolDirtyMask`
- `u32CalibDirtyMask`

杩欐牱锛?
- 澶у潡鍙啓鍙樺姩瀛楁
- 灏忓潡鐩存帴鏁村潡鍐?- 涓嶉渶瑕佸啀鎵╁睍涓€鍫嗗叏灞€ flag

### 3.3 鏃ф爣蹇楃殑褰掑苟鎬濊矾

鍙互杩欐牱鍚堝苟锛?
| 鏃ф爣蹇?| 寤鸿褰掑睘 |
|---|---|
| `u8E2P_KB_WriteFlag` | 鏍″噯鍧?|
| `u32E2P_Pro_VolCur_WriteFlag` | 淇濇姢鍧?|
| `u32E2P_Pro_Temp_WriteFlag` | 淇濇姢鍧?|
| `u32E2P_Pro_Other_WriteFlag` | 淇濇姢鍧?|
| `u32E2P_OtherElement1_WriteFlag` | OtherElement1 鍧?|
| `u32E2P_HeatCool_WriteFlag` | 鐑鐞嗗潡 |
| `u32E2P_RTC_Element_WriteFlag` | RTC 鍧?|
| `u8E2P_SocTable_WriteFlag` | SOC 琛ㄥ潡 |
| `u8E2P_CopperLoss_WriteFlag` | 閾滄崯鍧?|
| `gu8_Reset_EventRecord` | 浜嬩欢璁板綍鍧?|
| `ProductionInfor.BMS_*_WriteFlag` | 浜у搧淇℃伅鍧?|

### 3.4 鏈€缁堟帴鍙ｅ缓璁?
寤鸿鏈€鍚庡彧淇濈暀杩欑被楂樺眰鎺ュ彛锛?
- `EEPROM_MarkDirty(block_id)`
- `EEPROM_Process()`
- `EEPROM_LoadAll()`
- `EEPROM_WriteWordVerified()`
- `EEPROM_ReadWord()`

閫氫俊灞備笉瑕佸啀鐩存帴璋冪敤搴曞眰鍗曞瓧鑺傚啓銆?
## 4. 鎺ㄨ崘鐨勬墽琛屾祦绋?
### 4.1 閫氫俊鍐欐祦绋?
```mermaid
flowchart TD
    A["鏀跺埌 0x06/0x10 鍐欏懡浠?] --> B["鏍￠獙鍦板潃鍜岄暱搴?]
    B --> C["鏇存柊 RAM 鍙傛暟"]
    C --> D["璁剧疆 dirty block / submask"]
    D --> E["绔嬪嵆搴旂瓟閫氫俊"]
    E --> F["EEPROM_Process 鍚庡彴钀界洏"]
    F --> G["鍐欐垚鍔熷垯娓?dirty"]
    F --> H["鍐欏け璐ュ垯缃敊璇爣蹇?]
```

### 4.2 EEPROM 钀界洏娴佺▼

1. 鍙湅 dirty mask
2. 鎸戜竴涓潡
3. 鎸夊潡鍐呴『搴忓啓
4. 姣忔鍐欏畬鍥炶鏍￠獙
5. 鎴愬姛鍚庢竻 dirty
6. 澶辫触鍒欎繚鐣?dirty锛屼笅涓€杞户缁?
杩欐牱鍙互閬垮厤锛?
- 閫氫俊閲屽爢澶ч噺钀界洏閫昏緫
- 涓€娆℃€у啓寰堝鍧?- 鍑洪敊鍚庝笉鐭ラ亾鍝竴姝ュ仠浜?
## 5. Keil 鍦ㄧ嚎璋冭瘯寤鸿

杩欎竴閮ㄥ垎鏄负浜嗕綘瀹為檯涓嬫柇鐐瑰ソ鐢ㄣ€?
### 5.1 寤鸿閲嶇偣瑙傚療鐨勫彉閲?
鍦?Keil Watch 閲屼紭鍏堢湅杩欎簺锛?
- `u32EepromDirtyMask`
- `u32ProtectDirtyMask`
- `u32Other1DirtyMask`
- `u32HeatCoolDirtyMask`
- `u32CalibDirtyMask`
- `u8E2P_KB_WriteFlag`  濡傛灉杩樻病鍒犲共鍑€
- `gu8_Reset_EventRecord`
- `System_ErrFlag.u8ErrFlag_Com_EEPROM`
- `u8FlashUpdateE2PROM`
- `MCUO_E2PR_WP`

濡傛灉鍏堝仛杩囨浮鐗堟湰锛岃繕鍙互缁х画鐩細

- `u32E2P_Pro_VolCur_WriteFlag`
- `u32E2P_Pro_Temp_WriteFlag`
- `u32E2P_Pro_Other_WriteFlag`
- `u32E2P_OtherElement1_WriteFlag`
- `u32E2P_HeatCool_WriteFlag`

### 5.2 寤鸿涓嬫柇鐐圭殑浣嶇疆

浼樺厛涓嬪湪杩欎簺鍑芥暟涓婏細

1. `Sci_Deal_WrRegs_0x10()`
2. 瀵瑰簲鐨?`Sci_WrRegs_0x10_*()` 鍒嗗彂鍑芥暟
3. `App_E2promDeal()`
4. `WriteEEPROM_ByteData_Circle()`
5. `WriteEEPROM_Word_NoZone()`
6. `WriteEEPROM_Byte()`
7. `ReadEEPROM_ByteData_StartUp()`
8. `InitData_E2prom()`

### 5.3 瑙傚療椤哄簭

寤鸿鎸夎繖涓『搴忕湅锛?
1. 閫氫俊鏀跺埌鍐欏懡浠ゅ悗锛孯AM 鏈夋病鏈夊厛鏀瑰
2. dirty 浣嶆湁娌℃湁琚纭疆浣?3. `App_E2promDeal()` 鏈夋病鏈夎鍛ㄦ湡璋冪敤
4. EEPROM 鍐欏嚱鏁版湁娌℃湁鐪熺殑杩涙潵
5. `MCUO_E2PR_WP` 鏈夋病鏈夊湪閫€鍑烘椂鎭㈠涓?1
6. `WriteEEPROM_Word_NoZone()` 鍥炶鏄惁涓€鑷?7. 澶辫触鏃堕敊璇爣蹇楁湁娌℃湁璁剧疆

### 5.4 Keil 璋冭瘯鎶€宸?
- 鍦?`Sci_WrRegs_0x10_*()` 閲屽厛瑙傚療 RAM 鏄惁宸茬粡鏇存柊
- 鍦?`App_E2promDeal()` 閲岀湅 dirty 浣嶆槸鍚﹁娑堣€?- 鍦?`WriteEEPROM_Byte()` 閲岀湅 WP 寮曡剼鏄惁鍦ㄥ紓甯歌矾寰勪笂鎭㈠
- 鍦?`WriteEEPROM_Word_NoZone()` 閲岀湅 `result` 鍜?`tmp16` 鏄惁涓€鑷?- 濡傛灉鎬€鐤戝湴鍧€鍐欓敊锛岀洿鎺ョ湅 `addr` 鍜屽搴旂殑 RAM 瀛楁鍊?
## 6. 寤鸿鐨勮繃娓℃柟妗?
涓嶅缓璁竴涓嬪瓙鎶婃墍鏈夋棫 flag 鍒犲厜銆?
鏇寸ǔ鐨勬柟寮忔槸涓夋璧帮細

### 绗竴姝?
鏂板锛?
- 鎬?dirty mask
- 鍧楃骇澶勭悊鍑芥暟
- 鏂扮殑缁熶竴璋冨害鍏ュ彛

### 绗簩姝?
璁?`Sci_WrRegs_0x10_*()` 鍙礋璐ｏ細

- 鏇存柊 RAM
- 璁剧疆鏂?dirty 浣?
鏃?flag 鍏堜繚鐣欎竴娈垫椂闂达紝渚夸簬瀵圭収璋冭瘯銆?
### 绗笁姝?
纭鏂版祦绋嬬ǔ瀹氬悗锛屽啀閫愭鍒犳帀锛?
- 瀛楁绾у啓鏍囧織
- 澶氫綑鐨?`else if` 鍒嗘敮
- 鏃х殑鏁ｄ贡鍐欏叆鍙?
## 7. 鏈€缁堝缓璁?
濡傛灉瑕佸吋椤锯€滃畨鍏ㄣ€佺ǔ瀹氥€佺畝鍗曘€佸ソ璋冭瘯鈥濓紝鎴戝缓璁渶缁堢粨鏋勬槸锛?
- 鍦板潃琛ㄤ笉鍙?- 閫氫俊鍦板潃涓嶅彉
- 鍙繚鐣欏潡绾?dirty
- 灏戦噺鍧椾繚鐣欏瓙 mask
- EEPROM 鍚庡彴缁熶竴澶勭悊
- Keil 閲岄噸鐐圭洴 dirty銆乄P銆佸啓鍥炵粨鏋滃拰閿欒鏍囧織

杩欐瘮鐜板湪鐨勬柟寮忔洿瀹规槗缁存姢锛屼篃鏇村鏄撳畾浣嶉€氫俊鍐?EEPROM 鐨勯棶棰樸€?
## 7. 旧标志与新标志对照

如果你要逐项替换当前的字段级写标志，可以直接看这份对照表：

- [COMMUNICATION_EEPROM_FLAG_MAPPING.md](E:/TODO/103%20+%20309%20-%20%E5%89%AF%E6%9C%AC%20-%20%E5%89%AF%E6%9C%AC/COMMUNICATION_EEPROM_FLAG_MAPPING.md)


## 8. 当前代码落地状态

截至本轮修改，以下内容已经接入代码：

- u32EepromDirtyMask、EEPROM_MarkDirty()、EEPROM_ClearDirty()、EEPROM_GetDirtyMask() 已加入
- 校准、保护、RTC、SOC 表、铜损、OtherElement1、热管理、产品信息、事件记录的写入口已接到块级 dirty
- RTC / SOC / CopperLoss 的后台写分支已恢复，避免写请求卡死
- WriteProID() 已在写完后自动清除产品信息 dirty

### 8.1 本轮新增落地

- `System_OnOFF_Func` 的保存已改为 `EEPROM_DIRTY_BLOCK_SYS_FLAG`，通信侧不再直接写 `EEPROM_ADDR_SYS_FUNC_SELECT`
- `DataLoad_CurrentCali_startup()` 生成的偏移值已改为 `EEPROM_DIRTY_BLOCK_OFFSET`
- `App_E2promDeal()` 现在统一轮询 `u32EepromDirtyMask` 和旧写标志，Keil 里可直接观察 `u32EepromDirtyMask`
- `DataLoad_CurrentCali_startup()` 内部会主动刷新 `OFFSET` dirty，避免首次校准只改 RAM 不落盘

### 8.2 调试优先级

Keil 在线调试时优先观察：

1. `u32EepromDirtyMask`
2. `g_u16CurrentCaliOffsetValue`
3. `System_OnOFF_Func.all`
4. `u8E2P_KB_WriteFlag`
5. `u32E2P_Pro_VolCur_WriteFlag`
6. `gu8_Reset_EventRecord`

