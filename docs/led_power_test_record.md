# LED/按键/电源测试记录

日期：2026-05-27

## 已执行

- Keil MDK 命令行构建：
  - 工程：`103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`
  - 目标：`Target 1`
  - 编译器：ARMCC V5.06 update 7 build 960
  - 结果：0 Error(s), 0 Warning(s)
  - 固件大小：Code 51212, RO-data 2932, RW-data 1512, ZI-data 6160
  - 产物：`103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.axf`、`.bin`

## 需要上板验证

- 休眠后 PA9 短按是否立即显示备份域 SOC。
- PA9 短按后 8 秒是否熄灯并重新进入 STOP。
- PA9 长按满 3 秒是否播放 L1 到 L5 开机动画并完成上电。
- 休眠时 PA0 充电唤醒是否跳过 3 秒确认并进入完整初始化。
- 工作态 SOC 19/20/39/40/59/60/79/80/100 分档显示。
- SOC < 20、CellUvp、BatUvp、SocLow 是否 L1 黄灯闪烁。
- 充电态各 SOC 档位是否“已达档位常亮，下一档闪烁”。
- 工作态短按是否立即进入 L1 灭、L2-L5 闪烁的关机确认。
- 关机确认 8 秒超时是否恢复工作灯显且 MOS 保持。
- 关机确认长按 3 秒后是否 L5 到 L2 依次熄灭，并在动画结束后 CHG/DSG 都关闭、AFE_Sleep。
