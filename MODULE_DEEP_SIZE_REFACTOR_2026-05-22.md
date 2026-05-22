# 模块深度减码记录 2026-05-22

## 目标

本轮目标是继续按“减小 Code 为第一优先级”处理数码管 LED、SOC、零电流校准、CAN、老化模式相关复杂度。原则是：

1. 功能状态只保留一个真实来源，外部无人消费的观察镜像不进入 Release。
2. 量产固件不为 Debug Watch、错误快照、影子命令保留写字段路径。
3. 不删除会改变保护、SOC 估算、低功耗唤醒、老化进度、CAN 协议可见行为的路径。

## 本轮实际裁剪

### 零电流校准

- `AFE_CURRENT_OBSERVE` 和 `g_stAfeCurrentObserve` 改为 `PROJECT_CFG_DEBUG_WATCH_ENABLE` 下才编译。
- Release 中不再维护零电流观察字段、启动采样计数镜像、校准方向镜像、CTLC 镜像。
- 零点状态机、启动零点学习、运行时自动零点、0.2A 输出 deadband、CADC 读取路径保持不变。
- `DataLoad_Current()` 去掉充放电两个中间电流变量，改成当前方向只计算一个 `report_current_mA`。
- `AfeCurrent_ObserveReset()` 降为 Debug Watch 专用，Release 不再生成“先置 IDLE 再马上置 STARTUP”的无效调用。

### CAN

- `g_stCanLowPowerStatus` 和 `g_stCanErrorSnapshot` 改为 `PROJECT_CFG_DEBUG_WATCH_ENABLE` 下才编译。
- Release 中不再维护低功耗状态镜像和错误计数镜像。
- `FeidaoCanRuntime` 仍然保留，是 CAN 低功耗调度、ACK 判断、RTC 唤醒服务、发送窗口的唯一功能状态。
- CAN 发送、无 ACK 降频、bus-off 恢复、RTC 唤醒广播、帧格式不变。

### 数码管 LED

- 删除 `LEDBAR_COMMAND` 枚举、`LedBar_Command` 全局变量和所有赋值。
- `LedBar_Command` 没有外部读者，也不参与显示决策；实际显示仍由 SOC、充电输入、故障状态和显示窗口控制。
- 按键释放门控、休眠唤醒显示、SOC 数字/百分号/充电图标逻辑不变。

## 本轮未裁剪

### SOC

SOC 的可见输出仍是上位机、CAN、LedBar、低功耗判断共同依赖的功能路径。本轮只保留上一轮已经完成的编译期默认配置裁剪，没有继续删除 SOC 估算链路。

原因：中段 tail、休眠 OCV、容量积分、快照保存仍会改变可见 SOC。未经过板端数据确认前直接删除会变成功能改动，不符合“功能不受影响”的前提。

### 老化模式

老化模式仍保持 Flash + BKP 双进度保存。BKP 秒级进度不是纯观察变量，用于掉电续跑精度；如要进一步删除，需要先确认量产流程允许只依赖 Flash 低频保存。

## 构建结果

本轮基线为上一轮已提交版本的 `FD_Release`：

```text
Before: Code=46220 RO-data=2568 RW-data=792 ZI-data=5256 ROM=49032
After:  Code=45672 RO-data=2568 RW-data=792 ZI-data=5160 ROM=48484
Delta:  Code -548, ZI -96, ROM -548
```

验证：

```text
py -3 tools\project_check.py -q
OK: 141, Warnings: 0, Errors: 0

FD_Release rebuild
Program Size: Code=45672 RO-data=2568 RW-data=792 ZI-data=5160
0 Error(s), 0 Warning(s)
```

## 后续可裁剪但需要确认

- 若确认量产无需 Keil Watch 读取零电流/CAN/SOC内部状态，可继续把剩余 Debug Watch API 收到 Debug target 专用头文件。
- 若确认老化掉电续跑不需要 BKP 秒级进度，可删除 BKP 进度镜像，只保留 Flash 进度。
- 若确认 SOC 中段 tail 和长休眠下修策略不是客户验收项，可用编译开关把对应函数和表裁掉。
