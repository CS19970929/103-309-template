#!/usr/bin/env python3
"""CAN filter configuration test - verify BMS app filter matches expected frames."""

import sys


def test_can_filter():
    """Test CAN filter configuration for BMS app."""
    
    print("=" * 60)
    print("CAN 滤波器配置验证")
    print("=" * 60)
    
    # BMS App configuration
    CAN_ADRESS_STD_ID = 0x00
    FEIDAO_CAN_APP_CMD_ID = 0x60
    
    # Expected frame IDs
    EXPECTED_CMD_ID = ((CAN_ADRESS_STD_ID << 7) | FEIDAO_CAN_APP_CMD_ID)  # 0x0060
    EXPECTED_ACK_ID = ((CAN_ADRESS_STD_ID << 7) | 0x61)  # 0x0061
    
    # Filter 0 configuration
    FILTER0_ID_HIGH = (0x0060 << 5)  # = 0x0C00
    FILTER0_MASK_HIGH = (0x7FF << 5)  # = 0xFFE0
    
    # Filter 1 configuration
    FILTER1_ID_LOW = (0x01 << 3)  # = 0x0008 (IDE bit)
    FILTER1_MASK_LOW = (0x01 << 3)  # = 0x0008 (match IDE only)
    
    print(f"\nBMS App CAN 地址: 0x{CAN_ADRESS_STD_ID:02X}")
    print(f"期望的命令帧 ID: 0x{EXPECTED_CMD_ID:04X}")
    print(f"期望的应答帧 ID: 0x{EXPECTED_ACK_ID:04X}")
    
    print(f"\n滤波器 0 配置:")
    print(f"  IdHigh = 0x{FILTER0_ID_HIGH:04X}")
    print(f"  MaskIdHigh = 0x{FILTER0_MASK_HIGH:04X}")
    
    print(f"\n滤波器 1 配置:")
    print(f"  IdLow = 0x{FILTER1_ID_LOW:04X}")
    print(f"  MaskIdLow = 0x{FILTER1_MASK_LOW:04X}")
    
    # Test cases
    test_cases = [
        # (name, ide, id, dlc, expected_match, description)
        ("Comm Tool 命令 (0x0060)", 0, 0x0060, 8, True, "标准帧，Comm Tool 发送"),
        ("BMS 应答 (0x0061)", 0, 0x0061, 8, False, "标准帧，BMS 发送（不需要接收）"),
        ("其他标准帧 (0x0100)", 0, 0x0100, 8, False, "标准帧，其他设备"),
        ("IAP 控制帧 (0x14F8F000)", 1, 0x14F8F000, 8, True, "扩展帧，IAP 控制"),
        ("IAP 数据帧 (0x14000000)", 1, 0x14000000, 8, True, "扩展帧，IAP 数据"),
        ("广播帧 (0x14F80200)", 1, 0x14F80200, 8, True, "扩展帧，BMS 广播"),
        ("其他扩展帧 (0x18000000)", 1, 0x18000000, 8, True, "扩展帧，其他设备"),
    ]
    
    print(f"\n{'=' * 60}")
    print("测试用例")
    print(f"{'=' * 60}")
    
    all_passed = True
    for name, ide, frame_id, dlc, expected, desc in test_cases:
        # Simulate filter matching
        if ide == 0:  # Standard frame
            # Filter 0: match 0x0060
            id_high = (frame_id << 5) & 0xFFFF
            match = ((id_high ^ FILTER0_ID_HIGH) & FILTER0_MASK_HIGH) == 0
        else:  # Extended frame
            # Filter 1: match IDE bit only
            id_low = (0x01 << 3)  # IDE bit
            match = ((id_low ^ FILTER1_ID_LOW) & FILTER1_MASK_LOW) == 0
        
        status = "✅ PASS" if match == expected else "❌ FAIL"
        if match != expected:
            all_passed = False
        
        print(f"\n{status} {name}")
        print(f"  描述: {desc}")
        print(f"  IDE={ide}, ID=0x{frame_id:08X}, DLC={dlc}")
        print(f"  期望: {'接收' if expected else '丢弃'}, 实际: {'接收' if match else '丢弃'}")
    
    print(f"\n{'=' * 60}")
    if all_passed:
        print("✅ 所有测试通过！")
    else:
        print("❌ 存在测试失败！")
    print(f"{'=' * 60}")
    
    return all_passed


if __name__ == "__main__":
    success = test_can_filter()
    sys.exit(0 if success else 1)
