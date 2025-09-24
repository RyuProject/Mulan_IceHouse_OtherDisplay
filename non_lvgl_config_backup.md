# 非LVGL配置备份记录
## 项目：Mulan_IceHouse_OtherDisplay
## 备份时间：2025/9/23

## 1. 硬件平台配置
- **目标芯片**: ESP32-P4 (CONFIG_IDF_TARGET_ESP32P4=y)
- **架构**: RISC-V (CONFIG_IDF_TARGET_ARCH_RISCV=y)
- **工具链**: GCC (CONFIG_IDF_TOOLCHAIN_GCC=y)

## 2. 蓝牙配置 (NimBLE)
- **启用状态**: 已启用 (CONFIG_BT_ENABLED=y)
- **协议栈**: NimBLE (CONFIG_BT_NIMBLE_ENABLED=y)
- **控制器**: 禁用 (CONFIG_BT_CONTROLLER_DISABLED=y)
- **日志级别**: INFO (CONFIG_BT_NIMBLE_LOG_LEVEL_INFO=y)
- **最大连接数**: 3 (CONFIG_BT_NIMBLE_MAX_CONNECTIONS=3)
- **角色**: Central, Peripheral, Broadcaster, Observer
- **GATT服务**: Client和Server都启用
- **安全**: 启用传统和安全连接

## 3. 文件系统配置
### FATFS配置
- **卷数量**: 2 (CONFIG_FATFS_VOLUME_COUNT=2)
- **扇区大小**: 4096 (CONFIG_FATFS_SECTOR_4096=y)
- **代码页**: 437 (美国)
- **超时**: 10000ms

### SPIFFS配置
- **最大分区**: 3 (CONFIG_SPIFFS_MAX_PARTITIONS=3)
- **页面大小**: 256
- **对象名称长度**: 32
- **启用缓存**: 是

## 4. NVS配置
- **加密**: 禁用
- **传统重复键兼容**: 禁用

## 5. 分区表配置
- **类型**: 自定义 (CONFIG_PARTITION_TABLE_CUSTOM=y)
- **文件名**: partitions.csv
- **偏移量**: 0x8000
- **MD5校验**: 启用

## 6. 应用配置
- **构建类型**: APP_2NDBOOT
- **生成二进制文件**: 启用
- **编译时间日期**: 启用

## 7. 重置LVGL的步骤

### 方法1: 完全重置配置
```bash
# 删除当前构建
rm -rf build/

# 重新配置（这将使用默认配置）
idf.py set-target esp32p4
idf.py menuconfig

# 重新构建
idf.py build
```

### 方法2: 手动重置LVGL相关配置
在 `menuconfig` 中重置以下LVGL相关配置：
1. Component config → LVGL → 恢复默认设置
2. 检查显示驱动配置
3. 确认缓冲区大小和双缓冲设置

### 方法3: 清除LVGL配置并重新配置
```bash
# 清除LVGL配置
rm -rf sdkconfig
rm -rf build/

# 重新配置
idf.py menuconfig
# 在菜单中重新配置：
# - 显示分辨率
# - 缓冲区大小  
# - 双缓冲设置
# - 刷新率

# 重新构建
idf.py build
```

## 8. 重要注意事项
1. **蓝牙配置**：当前使用NimBLE协议栈，重置时请保持蓝牙相关配置
2. **文件系统**：FATFS和SPIFFS配置需要保持兼容
3. **分区表**：使用自定义分区表partitions.csv
4. **硬件目标**：确保目标芯片始终为ESP32-P4

## 9. 验证步骤
重置后请验证：
1. 蓝牙功能正常
2. 文件系统访问正常
3. 显示功能恢复正常
4. 性能指标符合预期