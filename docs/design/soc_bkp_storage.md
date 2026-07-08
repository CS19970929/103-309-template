# SOC BKP 存储模式说明

状态：CURRENT
适用芯片：STM32F103C8T6

## 背景

STM32F103C8T6 属于 medium-density STM32F103x8/xB 器件，备份域只有 10 个 16-bit backup data registers，也就是 BKP_DR1 到 BKP_DR10，共 20 bytes。项目内其它 BKP 寄存器用途已停用或迁移，BKP_DR1..BKP_DR10 全部保留给 SOC 快照。

## 编译宏

`PROJECT_CFG_SOC_STORAGE_MODE` 控制 SOC 快照存储方案：

| 宏值 | 含义 |
|---|---|
| `PROJECT_CFG_SOC_STORAGE_MODE_BKP_ONLY` | 只使用 BKP 存储 SOC 快照，不读写 SOC Flash 快照。当前默认值，用于验证无 Flash SOC 存储方案。 |
| `PROJECT_CFG_SOC_STORAGE_MODE_BKP_FLASH` | 运行态先写 BKP，进入深度休眠前再把 dirty SOC 快照写入 Flash。启动时优先读 BKP，BKP 无效再回退 Flash。 |

默认配置在 `103 + 309/Project/Source/conf/Project_BuildGuard.h` 中定义为 `BKP_ONLY`。需要验证组合方案时，可在编译命令或工程配置中定义：

```c
#define PROJECT_CFG_SOC_STORAGE_MODE PROJECT_CFG_SOC_STORAGE_MODE_BKP_FLASH
```

## BKP 布局

| BKP 寄存器 | 内容 |
|---|---|
| `BKP_DR1` | magic `0x5C0C` |
| `BKP_DR2` | magic inverse |
| `BKP_DR3` | low byte: SOC, high byte: snapshot flags |
| `BKP_DR4` | `cycle_x100` low 16-bit |
| `BKP_DR5` | `cycle_x100` high 16-bit |
| `BKP_DR6` | `cap_now_as10` low 16-bit |
| `BKP_DR7` | `cap_now_as10` high 16-bit |
| `BKP_DR8` | `dsg_acc_as10` low 16-bit |
| `BKP_DR9` | `dsg_acc_as10` high 16-bit |
| `BKP_DR10` | CRC16 |

CRC 覆盖 magic、版本、SOC/flags、`cycle_x100`、`cap_now_as10`、`dsg_acc_as10` 和当前容量参数换算出的 factory capacity。因此容量配置变化后，旧 BKP 快照会自动失效，系统会按当前电压 OCV 或默认 SOC 重新初始化。

## 存储流程

运行态 SOC 变化时，`soc_save_if_needed()` 只写 BKP。`soc`、`cycle_x100`、`cap_full_as10`、`cap_now_as10`、`dsg_acc_as10` 或 `snapshot_flags` 任一变化都会刷新 BKP；BKP 没有 Flash 擦写寿命问题，所以 `cycle_x100` 小步变化不会带来 Flash 磨损。

在 `BKP_ONLY` 模式下：

- 启动只读 BKP。
- BKP 无效时按 OCV 或默认 60% 初始化，并写入 BKP。
- `SOC_SaveSnapshotBeforeSleep()` 不写 SOC Flash。
- `SOC_ResetStoredSnapshotToDefault()` 只写 BKP。

在 `BKP_FLASH` 模式下：

- 启动优先读 BKP。
- BKP 无效时回退读取 SOC Flash，并同步写回 BKP。
- 运行态刷新 BKP 后只置 dirty 标志。
- `SOC_SaveSnapshotBeforeSleep()` 仅在 dirty 时写 SOC Flash，写成功后清 dirty。

## 风险与边界

`BKP_ONLY` 的主要风险是数据保持依赖备份域供电和 RTC 备份域状态。如果 VBAT/VDD 都丢失、备份域被硬件清除，或者 RTC 时钟源重建触发 `BKP_DeInit()`，SOC 快照会丢失；由于该模式不回退 SOC Flash，下一次启动会按 OCV 或默认值重建。

`BKP_FLASH` 能降低上述风险，因为深睡前会把最新 dirty 快照写入 Flash；但它仍然避免运行态频繁擦写 Flash。风险是如果运行态变化后尚未进入深睡就发生完全掉电，Flash 里的 SOC 可能落后于 BKP。

睡眠启动标志已迁移到 `FLASH_ADDR_SLEEP_FLAG`，fault snapshot 的 `0xD200/0xD201` 只保留 RAM 运行期镜像，不再使用 BKP。当前协议地址保持不变，但 fault reason 不再保证复位后保留。
