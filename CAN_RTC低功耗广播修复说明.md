# CAN通信回退到9ec9f26说明

## 背景

现场确认 `9ec9f26df29b1aa6c773fde20448e6363758ba1f` 是最后一版 CAN 可以通信的提交。检查后发现，当前 `Can_HDX.c` 和 `Can_HDX.h` 已经与该提交一致；继续存在的差异在 CAN 外围运行环境，包括 IO 低功耗配置和通信宏开关。

为按最后可通信版本恢复，本次将 CAN 通信相关运行路径对齐到 `9ec9f26`。

## 本次恢复文件

1. `103 + 309/Project/Source/Can_HDX.c`
   当前已经与 `9ec9f26` 一致，本次保持不再改动。

2. `103 + 309/Project/Source/Can_HDX.h`
   当前已经与 `9ec9f26` 一致，本次保持不再改动。

3. `103 + 309/Project/Source/conf/conf.c`
   恢复 `IOstatus_Base()` 和 `IOstatus_RTCMode()` 中的 IO 状态到 `9ec9f26`，包括 DC/2727 供电脚、SEG 脚和 RTC 模式 GPIO 配置。

4. `103 + 309/Project/Source/main.c`
   移除后续版本额外加入的无条件 `DBGMCU_Config(DBGMCU_STOP, ENABLE)`，恢复到 `9ec9f26`。

5. `103 + 309/Project/Source/main.h`
   恢复 `_COMMOM_UPPER_SCI2` 宏定义到 `9ec9f26` 状态。

## 验证

- 已确认上述运行相关文件相对 `9ec9f26` 无差异。
- 该回退优先恢复最后可通信基线。若后续仍要调整 CAN 低功耗策略，应在该基线上单项修改并实测 `GPIO_CMNT_EN`、CAN_TX、CANH/CANL。
