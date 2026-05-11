# BMS CAN IAP Bootloader

本目录是 BMS 侧独立 IAP/Bootloader，实现《飞道CAN通信协议V1.6》第七节“固件升级部分”的 CAN 升级协议。

## 地址规则

| 区域 | 地址 |
| --- | --- |
| IAP 起始地址 | `0x08000000` |
| IAP 最大结束地址 | `0x080047FF` |
| App 起始地址 | `0x08004800` |
| App 安全结束地址 | `0x0801BFFF` |
| 升级标志地址 | `0x0801F800` |

禁止把 App bin 裸写到 `0x08000000`。App 必须从 `0x08004800` 运行和升级。

## 构建

使用仓库脚本构建 IAP：

```powershell
.\tools\build_iap_bootloader.ps1 -Clean
```

脚本会先把源码复制到临时 ASCII staging 目录，再调用 Keil ARMCC，避免当前仓库路径中的空格、加号和中文导致 ARMCC 参数解析错误。

输出产物：

```text
103 + 309/Project/Users/Objects/IAP/FD_IAP.bin
103 + 309/Project/Users/Objects/IAP/FD_IAP.axf
103 + 309/Project/Users/Listings/IAP/FD_IAP.map
```

当前构建脚本会检查 `FD_IAP.bin` 不超过 `0x4800` 字节。

## 协议

CAN 使用 29bit 扩展帧：

```text
SrcID[28:24] | DstID[23:19] | CtrlCMD[18:16] | Index[15:8] | ChdIndex[7:0]
```

当前节点配置：

| 节点 | 值 |
| --- | --- |
| 主机/升级器 | `0x10` |
| BMS/IAP | `0x14` |
| 广播 | `0x1F` |

升级流程：

1. 主机发送 A-0 起始帧：`Ctrl=0x00, Index=0x04, Chd=0x00`。
2. IAP 校验文件大小、总长包数、App 边界，返回 A-1：`Ctrl=0x02, Index=0x04, Chd=0x01`。
3. 主机逐长包发送 B-0 起始帧、`Ctrl=0x05` 数据帧和 `Ctrl=0x06` 结束帧。
4. IAP 先缓存完整 2048 字节长包，只有长包 CRC 正确才写 App Flash。
5. 最后 IAP 校验总 CRC、App 向量表，成功后清升级标志并复位进 App。

## 失败策略

| 失败场景 | 行为 |
| --- | --- |
| 起始参数非法 | A-1 返回不可升级，不擦 App |
| 长包序号错误 | 返回 `0xFF`，停留 IAP |
| 长包 CRC 错误 | 返回 `0x01`，该长包不写 Flash，停留 IAP |
| 总 CRC 错误 | 返回 `0x02`，不跳 App，停留 IAP |
| App 向量表非法 | 返回 `0xFF`，不跳 App，停留 IAP |
| 断电或通信中断 | 下次上电仍可进入 IAP 重新升级 |

核心稳定性规则：IAP 永远不擦写 `0x08000000..0x080047FF`，并且只有完整镜像 CRC 正确后才清升级标志。
