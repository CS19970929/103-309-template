# AFE 风险与整改路线

## 1. 风险分级

### P1：建议优先修

| 风险 | 影响 | 建议 |
| --- | --- | --- |
| `BALANCEM/BALANCEL` 结构体顺序错 | 连续读回寄存器后镜像值错位，影响调试、上报和后续校验 | 修正 `AFEDATA` 字段顺序并编译验证 |
| 串数来源不统一 | AFE 串数、采样数量、均衡数量、上报串数可能不一致 | 统一 `SNum/SeriesNum/SCONF4.CN` 来源 |
| AFE 初始化只读回 `SCONF4` | 其他配置寄存器写失败无法发现 | 建立 `0x40..0x54` 配置表和读回校验 |
| SPI API 无地址/长度边界 | 后续误调用可能读写非法寄存器或造成栈压力 | 增加范围检查和最大长度限制 |

### P2：建议随架构整理修

| 风险 | 影响 | 建议 |
| --- | --- | --- |
| 周期读 `FLAG2` 自动清 `VADC/CADC` 标志 | 如果后续依赖转换完成标志，会被提前消费 | 明确 FLAG2 读取所有权 |
| 均衡三字节写失败可能半更新 | SPI 异常时 AFE 均衡位可能与软件状态不同 | 失败后写全 0 或读回校验 |
| 清保护标志不检查 `SCONF2` 写返回 | `LTCLR` 未置位时清标志可能失败 | 检查返回值并读回确认 |
| 旧命名和空函数误导维护 | 容易误判功能已实现 | 建立 `sh36735xx` 新模块边界 |
| 禁用测试代码存在赋值判断 | 后续打开会引入逻辑错误 | 删除或修正 `#if 0` 测试块 |

## 2. 推荐整改顺序

### 第 1 步：低风险修正

- 修正 `AFEDATA` 中 `BALANCEM/BALANCEL` 顺序。
- 给 `sh36735_write_reg_u8()` 和 `sh36735_read_regs()` 增加地址/长度边界检查。
- 增加 SPI 错误统计结构，至少记录最后一次失败的 `cmd/reg/len`。

验证：

- Keil 编译 0 error。
- ST-Link 读 `0x55..0x57`，确认软件镜像顺序与寄存器一致。
- 故意读非法地址，确认返回失败且不破坏正常采样。

### 第 2 步：初始化配置表

- 新建 AFE 配置表，覆盖 `0x40..0x54`。
- 把当前 `main.c` 中的 AFE 初始化迁移到独立模块。
- 写完后读回校验；`LTCLR/OWD_TRG` 等特殊位使用 mask。
- 明确 `SCONF5`、`SCONF6`、`SCONF7` 的产品配置含义。

验证：

- 冷启动读回 `0x40..0x54`。
- AFE 软件复位后重新初始化并读回。
- 断电重启后串数、MOS、CADC、保护配置一致。

### 第 3 步：统一串数来源

- 确认产品目标是固定 19 串，还是 16/19/20 串通用。
- 若固定 19 串，删除或限制 EEPROM 修改串数的入口。
- 若通用串数，必须在 AFE 初始化前获得可信串数，并校验范围。
- `SCONF4.CN`、采样循环、均衡循环、上位机上报都使用同一值。

验证：

- 分别模拟 16/19/20 串参数。
- 确认未使用 cell 不参与均衡、不参与保护计算、不上报有效电压。
- 确认 SH3673520 未使用 VC 端硬件处理符合 datasheet。

### 第 4 步：保护、均衡、休眠闭环

- 清保护标志后读回 `FLAG1/FLAG2`。
- 均衡写后读回 `BALANCEH/M/L`。
- SPI 失败时关闭均衡或进入明确降级状态。
- 休眠前关闭均衡，唤醒后重新校验 AFE 配置。

验证：

- OV/UV/OCC/OCD/SC/温度保护触发与恢复。
- 负载移除解除短路流程。
- 均衡开启、关闭、SPI 异常时硬件状态。
- SLEEP 进入、唤醒、重新采样。

## 3. ST-Link 调试建议

优先设置断点：

- `InitAFE3520_Registers()`
- `sh36735_write_reg_u8()`
- `sh36735_read_regs()`
- `UpdateVoltageFromBqMaximo()`
- `SH_AFE_ClearProtectFlag()`
- `CB_AfeWriteBalanceMaskU24()`

建议 watch 的变量：

- `Registers_AFE1.sonf2.all`
- `Registers_AFE1.sonf3.all`
- `Registers_AFE1.sonf4`
- `Registers_AFE1.sonf5`
- `Registers_AFE1.sonf6`
- `Registers_AFE1.flag1.all`
- `Registers_AFE1.flag2.all`
- `Registers_AFE1.bstatus1.all`
- `Registers_AFE1.bstatus2.all`
- `sys_time.crc_err`
- `System_ErrFlag.u8ErrFlag_Com_AFE1`
- `SeriesNum`
- `OtherElement.u16Sys_SeriesNum`

建议手动读寄存器块：

- `0x40..0x46`：系统配置
- `0x47..0x57`：阈值和均衡
- `0x58..0x5C`：标志和状态
- `0x5D..0x96`：温度、电芯、电流、总压、C+

## 4. 完成标准

AFE 整理可以认为阶段完成，需要同时满足：

- Keil 构建 0 error，warning 数量不增加。
- ST-Link 可稳定读写 AFE，连续采样无 SPI 错误。
- AFE `0x40..0x54` 读回与配置表一致。
- 串数在 AFE、业务层、均衡、上报中一致。
- 保护清除和均衡写入都有读回闭环。
- 休眠/唤醒后 AFE 配置仍能自动恢复或重新初始化。
