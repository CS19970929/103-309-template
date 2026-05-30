# 数据格式

## 参数表 CSV

字段：

`group,name,c_name,modbus_addr,data_type,scale,unit,min,max,default,access,save_policy,description`

约定：

- `modbus_addr` 支持十进制或 `0x` 十六进制。
- `data_type` 支持 `bool/u8/s8/u16/s16/u32/s32`。
- `u32/s32` 占用 2 个连续 Modbus register。
- `min/max/default` 为固件原始整数值。
- `scale` 用于文档和 dump 的物理值换算。

## Modbus 寄存器表 CSV

字段：

`name,addr,data_type,scale,unit,access,description`

## CAN 协议 JSON

```json
{
  "messages": {
    "0x180": {
      "name": "BMS_STATUS",
      "signals": [
        {"name": "pack_voltage", "byte": 0, "length": 2, "scale": 0.1, "unit": "V"}
      ]
    }
  }
}
```

## CAN 日志文本

每行：

```text
time_s can_id data_hex
0.000 0x180 13887D0064000000
```

## BMS 事件码 JSON

```json
{
  "events": {
    "0x0001": {"name": "CELL_OVP", "level": "FAULT"}
  }
}
```

## 保护仿真时间序列 CSV

字段：

`time_s,cell_mv,current_a,temp_c`

## SOC 仿真时间序列 CSV

字段：

`time_s,voltage_mv,current_a,state`

约定：`current_a` 放电为负，充电为正。

## HardFault JSON

常用字段：

`r0,r1,r2,r3,r12,lr,pc,psr,cfsr,hfsr,bfar,mmfar`
