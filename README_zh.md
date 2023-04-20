# ESP 脱机烧录器


 - 开发板：[WEMOS S2 mini](https://www.wemos.cc/en/latest/s2/s2_mini.html)
 - 支持芯片：ESP32-S2、ESP32-S3

## 默认引脚

功能 | GPIO
-- | --
烧录 RX | 16
烧录 TX | 18
烧录 Reset | 33
烧录 IO0 | 35
监控 TX | 17
监控 RX | 21

## 烧录文件示例

直接复制 ESP32 项目下 `build` 对应文件到 S2 mini 模拟的 U 盘（S2 mini LED 常亮）：

 - hello_world/build/flasher_args.json
 - hello_world/build/hello_world.bin
 - hello_world/build/bootloader/bootloader.bin
 - hello_world/build/partition_table/partition-table.bin.bin

 其中 `flasher_args.json` 文件中的 `flash_files` 提供相对路径的烧录文件和地址列表。另外，也会核对 `extra_esptool_args` 中的 `chip` 与当前连接的芯片是否一直。

 ## 烧录

 需要先在 PC 卸载 S2 mini 模拟 U 盘（S2 mini LED 熄灭），然后单击 S2 mini 的 IO0 按键。

 S2 mini LED 快速闪烁表示正在烧录，熄灭表示烧录完成，慢闪表示出错。

## 注意

  - **WEMOS S2 mini** 的 LDO 无法满足 3.3V 供电热插拔第二块开发板（被烧录板），建议使用杜邦线对接两块开发板 VBUS/VIN 引脚。
  - 目前只支持 115200 波特率烧录 ESP32 芯片
