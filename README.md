# JBOX-S3：52 节点 / 8 网络 / 19 GPIO 的纯 API 可编程接线板

适用：**ESP32-S3-WROOM-1-N16R8** + **4 × CH446Q**，ESP-IDF **v5.5.x**。无网页，无 HTTP / WebSocket；控制采用 TCP 4747 二进制 API，BLE 仅用于首次 Wi-Fi 配网。

> **工程状态**：提供完整固件源代码与 PC 端 Python SDK，已运行主机侧单元测试；当前环境没有 ESP-IDF 工具链或真实硬件，因此**尚未实际 `idf.py build`、烧录或示波器验证**。首次投板前必须核验 CH446Q 供电/串行时序，尤其是电源与保护电路。

## 1. 主要能力

| 项目 | 能力 |
|---|---|
| 交叉矩阵 | 52 个逻辑节点，经 4×CH446Q 选择 NET0～NET7；每节点最多属于 1 个网络 |
| 控制网络 | **单客户端**、持久 TCP、固定 16 字节头、二进制请求/应答、无网页 |
| 身份验证 | 首次 128-bit nonce + HMAC-SHA256；后续每帧 16B HMAC（防篡改/重放）；不加密内容 |
| 配网 | ESP-IDF BLE Wi-Fi Provisioning（protocomm Security 1 + 每设备 PoP），成功后释放 BLE RAM |
| GPIO | 共 **19 路**，EXT1～9 在 CH446Q 上，另外 10 路直出；数字输入/输出/开漏/批量读取写入 |
| I²C | 2 个硬件 I²C master，可动态选择 SDA/SCL，7 位设备地址，Write/Read 和重复起始，自动缓存设备句柄 |
| SPI | 2 个硬件主机（SPI2/3），动态 MOSI/MISO/SCK/CS，模式 0～3，全双工 DMA，单帧最多 4096 字节 |
| UART | 硬件 UART1/UART2，动态 TX/RX，8 位数据，校验/停止位配置，4096B RX/2048B TX 驱动缓冲 |
| PWM | 4 路 LEDC，13 位 duty；10Hz～40kHz 受分频/分辨率约束 |
| ADC | ADC1 GPIO1～10，原始 12-bit 样本；GPIO11+ 等无 ADC1 |
| 故障策略 | 上电全断开、GPIO 高阻；路由先断后连；Wi-Fi 客户端断线后全断开/停止外围外设；电源网络拒绝冲突 |

### 重要限制

1. **8 组是同时独立网络数量**，不是 8 个引脚。每条网络可并联多个洞洞板孔位。
2. 禁止一条网络同时出现 5V / 3.3V / GND 的任意两个节点；禁止直连任意电源节点到 ESP32 GPIO；当前安全策略还禁止将两个 ESP32 EXT GPIO 直接并入同一网络（避免两个输出驱动争用）。
3. 每组只支持点集的电气连通，不保存“某两点之间一根独立导线”的拓扑，移除一个节点不会自动分割多个网络。
4. CH446Q 的导通电阻/寄生电容会影响 SPI、UART、I²C。**经矩阵连接的 SPI、UART 等高速信号必须逐档实测**。本代码的频率上限仅为软件限制，不代表硬件保证。建议 I²C 从 10～50kHz、SPI 从 100kHz、UART 从 9600～115200 开始验证。
5. CH446Q（5V 单电源）不能代替低阻供电线。5V、3.3V、GND 接入 X 端仅供低电流实验；屏幕背光、较大 LED、模块供电应使用独立限流/低阻电源口。
6. ESP32-S3 GPIO **不耐 5V**。必须为 EXT1～9 加硬件过压/限流保护。软件冲突检查不能防止用户在板外将其他信号线直接接 5V。
7. HMAC 只提供鉴权和完整性，**不提供加密**；仅在可信的 WPA2/WPA3 局域网中使用，不要把 4747 端口直接映射到公网。
8. 只支持一条在线控制 TCP 会话，串行执行硬件命令，避免多个客户端同时改动拓扑。每次会话断开，当前连接会主动清空；要持续工作，客户端应保持 TCP 会话（并配置心跳/自动重连）。

## 2. 引脚分配

### ESP32-S3 GPIO

| 用途 | GPIO |
|---|---|
| EXT1～EXT9（进入 CH446Q） | **1、2、3、4、5、6、7、8、9** |
| DAT / CK / RST | **11 / 12 / 13** |
| CH446Q U2/U3/U4/U5 的 STB | **14 / 15 / 16 / 17** |
| USB D− / D+ | **19 / 20** |
| UART0（调试）TX / RX | **43 / 44** |
| BOOT | **0** |
| EN | 复位 |
| 10 个直出可编程 GPIO | **10、18、21、38、39、40、41、42、47、48** |
| 禁止外接（N16R8 八线 PSRAM 占用） | **35、36、37** |
| 启动特殊脚（本工程不使用） | **45、46** |

**GPIO3 是启动配置管脚**：在未改变相关 JTAG eFuse 配置时可正常做 I/O，但复位瞬间不能任意强驱动。GPIO39～42 与外部 JTAG 复用，如启用硬件 JTAG 应避免占用。UART0 始终保留给调试。

### GPIO API 的 IO ID（0～18）

| IO ID | ESP32 GPIO | 是否经矩阵 |
|---:|---:|---|
| 0～8 | GPIO1～9 | 是，对应路由节点 40～48 |
| 9 | GPIO10 | 否 |
| 10 | GPIO18 | 否 |
| 11 | GPIO21 | 否 |
| 12 | GPIO38 | 否 |
| 13～16 | GPIO39～42 | 否 |
| 17 | GPIO47 | 否 |
| 18 | GPIO48 | 否 |

**注意**：`route node ID` 和 `IO ID` 是两个独立的编号体系。例如 IO0=ESP GPIO1，对应的路由节点是 **40**；洞洞板 H2-1 是节点 **0**。

### 52 个路由节点和 CH446Q X 对应关系

| CH446Q | X0～X15 |
|---|---|
| U2 (chip 0) | H2-1～H2-16，node **0～15** |
| U3 (chip 1) | H2-17～H2-20 + H6-1～H6-12，node **16～31** |
| U4 (chip 2) | H6-13～H6-20 + EXT1～EXT8，node **32～47** |
| U5 (chip 3) | EXT9、5V、3.3V、GND，node **48～51**（X4～X15 备用） |

四颗芯片：**Y0 全部接 NET0，Y1 全部接 NET1，……Y7 全部接 NET7。不同 NET 之间绝不能直接短接。**

CH446Q LQFP44 物理引脚（需按 WCH 版本核对）：

- X0～X15：`31,30,29,28,27,26,3,4,5,6,7,8,25,24,1,2`
- Y0～Y7：`33,35,37,39,17,15,13,11`
- VDD 38→5V，GND 12→GND，VEE 16→GND；P/-S 10→GND（串行）
- DAT 36→GPIO11，CK 34→GPIO12，RST 41→GPIO13
- STB 14：四颗分别→GPIO14、15、16、17
- AX0～AX3 的 43/18/19/42→GND；AY0～AY2 的 20/21/40→GND
- NC 9/22/23/32/44：悬空

**硬件上电安全**：CH446Q 的 **RST 高电平有效**，硬件建议 RST **10kΩ 上拉至 3.3V**，保证 ESP32 复位时开关清零；4 根 STB 分别 10kΩ 下拉至 GND。固件启动时再明确脉冲复位。切勿沿用“RST 10k 下拉”造成上电前状态不确定。

每颗 CH446Q 就近 100nF 去耦并参考官方供电规则布线。5V、3.3V、GND 进入矩阵需独立限流，非直接承担大电流。

## 3. 编译 / 烧录

ESP-IDF **5.5.x**（推荐相应 5.5 bugfix 发行版），不能直接照搬 6.x：ESP-IDF 6.x 的旧 `wifi_provisioning` 组件接口发生迁移。

```bash
cd ProgrammableMatrixS3
idf.py set-target esp32s3
idf.py menuconfig   # 首次确认 N16R8: 16MB Flash、Octal 8MB PSRAM、BLE 开启
idf.py build
idf.py -p COM5 flash monitor    # Windows 示例；Linux 为 /dev/ttyUSB0 等
```

`sdkconfig.defaults` 预设了 BLE、Octal PSRAM、16MB Flash 和快节拍 FreeRTOS 任务调度，但如有不同硬件版本，必须以实际模组参数为准。**不能因为源代码齐全就跳过硬件验证。**

首次启动 UART0 输出：

```text
DEVICE KEY (copy to client and keep private): <64个十六进制字符>
BLE SECURITY1 POP (keep private): <12个十六进制字符>
BLE service name: JBOX-xxxxxx
```

这两个密钥存在 NVS，重启不变。开发阶段打印到 UART0，生产设备应改为出厂安全写入/贴签，避免每次串口打印秘密。

## 4. BLE 配 Wi-Fi（不需要网页）

首次未配网时 BLE GATT 服务名是 `JBOX-xxxxxx`，采用标准 ESP-IDF provisioning `protocomm`（Security 1，PoP 非空）。使用 ESP-IDF 提供的 `esp_prov.py` 或使用支持 Espressif Security 1 的 BLE 客户端：

```bash
cd "$IDF_PATH/tools/esp_prov"
python esp_prov.py --transport ble --service_name JBOX-A1B2C3 \
  --sec_ver 1 --pop 123456ABCDEF \
  --ssid '你的WiFi名' --passphrase '你的WiFi密码'
```

示例设备名与 PoP 请替换成 **UART0 实际打印值**。成功后 UART0 输出 IP 地址，BLE 服务自动停止并回收内存；随后的操作只走 TCP API。

重新配网：已连接时发 `factory_reset()` 会擦除 NVS（**Wi-Fi 信息和设备密钥也会一起删除**）并重启；如果 IP 完全失联，可通过 `idf.py erase-flash` 重新擦除。建议下一版硬件加物理长按配网复位按键。

## 5. Python API 的最简单用法

PC 侧 **只需 Python 3.10+ 标准库**；不需要额外 pip 包。

```python
import os
from client.jbox_api import MatrixClient

with MatrixClient('192.168.1.88', os.environ['JBOX_KEY']) as device:
    print(device.caps())

    # 将 ESP GPIO1 -> H2-1，ESP GPIO2 -> H2-2
    # [40,0] 占用 NET0；[41,1] 占用 NET1
    device.route_set([[40, 0], [41, 1]])

    # 两个外部 IO 分别映射为 SDA、SCL，由 ESP32 硬件 I2C 执行
    device.i2c_setup(bus=0, sda=0, scl=1, hz=50000)
    print([hex(x) for x in device.i2c_scan(0)])
    device.i2c_close(0)

    # GPIO 开关：IO9 = ESP GPIO10，直接引出，不经矩阵
    device.gpio_config(9, mode=1, initial=0)
    device.gpio_write({9:1})
```

**I²C 必须外接实际 3.3V 上拉电阻**，推荐 4.7kΩ 起步并实测，不能直接把 SDA 或 SCL 连到 3.3V 电源节点，否则会造成永久高电平短接。LCD/OLED 电源从独立低阻接口供电。

命令行示例（PowerShell）：

```powershell
$env:JBOX_KEY="从设备UART0读出的64位十六进制密钥"
python client/jbox_api.py --host 192.168.1.88 info
python client/jbox_api.py --host 192.168.1.88 status
python client/jbox_api.py --host 192.168.1.88 connect 0 20
python client/jbox_api.py --host 192.168.1.88 gpio-read 0 1 9
```

每次独立 CLI 命令退出时，**固件会自动断开矩阵并让所有 GPIO 恢复高阻**，用于减少“远程用户掉线后仍上电”的风险。要持久保持连接，请在同一个 Python `with MatrixClient(...)` 会话里连续操作，不要用多次 CLI 命令维持电路。

## 6. 二进制 TCP API 协议

连接到 `IP:4747`，默认同一时间只有 **1 条控制会话**。所有整数**小端**，每条帧：

| 偏移 | 字段 | 长度 |
|---:|---|---:|
| 0 | ASCII 魔数 `JMX1` | 4 |
| 4 | 协议版本：`1` | 1 |
| 5 | Command ID | 1 |
| 6 | flags：`1`=response, `2`=含 HMAC | 2 |
| 8 | sequence（单会话递增 uint32） | 4 |
| 12 | payload length（<=8192） | 4 |
| 16 | payload | 0～8192 |
| 16+len | HMAC-SHA256 前 16 字节（认证后必须） | 16 |

应答的 payload 最前面带 `int32 status`，后面才是命令的返回数据。`0` 表示成功，`-1` 参数错误，`-2` 资源占用，`-3` 硬件失败，`-4` 危险连接，`-5` 不支持，`-6` 超时，`-9` 网络资源不足，`-10` 未配置。

### 鉴权顺序

1. 请求 `0x01 HELLO`（无 MAC），得到 16-byte nonce。
2. 请求 `0x02 AUTH`，payload=`HMAC-SHA256(device_key,nonce||"JMX-AUTH-v1")` 共 32B。
3. 会话密钥 `HMAC-SHA256(device_key,nonce||"JMX-SESSION-v1")`。
4. AUTH 回复及之后每一请求/回复均带 `HMAC(session_key,header||payload)[:16]`，请求 sequence 必须单调递增。

认证内容完整性但**不加密数据**。只在可信局域网使用。Python SDK 已自动处理会话协商、HMAC、分帧和序号，不建议手写协议客户端。

### 命令清单（payload 为二进制；所有长度最多 4096B）

| CMD | 请求字段 | 返回字段（除 status 外） |
|---|---|---|
| `0x10 CAPS` | 无 | `u8 version,nodes,nets,ios;u16 max_xfer,port;u8 gpio[19]` |
| `0x11 STATUS` | 无 | `u32 uptime_ms,free_heap;u64 nets[8];u8 mode[19],lease[19]` |
| `0x20 ROUTE_SET` | `u64 masks[8]` | `u64 masks[8]` |
| `0x21 CONNECT` | `u8 a,b`（node ID） | `u8 net;u64 masks[8]` |
| `0x22 REMOVE_NODE` | `u8 node` | `u64 masks[8]` |
| `0x23 ROUTE_CLEAR` | 无 | 无 |
| `0x30 IO_CONFIG` | `u8 io,mode,pull,initial` | 无 |
| `0x31 IO_WRITE` | `u32 mask,levels` | 无 |
| `0x32 IO_READ` | `u32 mask` | `u32 levels` |
| `0x33 PWM` | `u8 channel,io;u32 hz;u16 duty` | 无（io=255 停止通道） |
| `0x34 ADC_READ` | `u8 io` | `u32 adc_raw`（需先 `IO_CONFIG` 为 ADC） |
| `0x40 I2C_SETUP` | `u8 bus,sda_io,scl_io;u32 hz` | 无 |
| `0x41 I2C_CLOSE` | `u8 bus` | 无 |
| `0x42 I2C_XFER` | `u8 bus,addr;u16 txlen,rxlen;u8 tx[txlen]` | `u8 rx[rxlen]` |
| `0x50 SPI_SETUP` | `u8 bus,mosi,miso,sck,cs,mode;u32 hz` | 无（miso=255 写模式） |
| `0x51 SPI_XFER` | `u8 bus;u16 len;u8 tx[len]` | `u8 rx[len]` |
| `0x52 SPI_CLOSE` | `u8 bus` | 无 |
| `0x60 UART_SETUP` | `u8 bus,tx,rx;u32 baud;u8 parity,stopbits` | 无 |
| `0x61 UART_WRITE` | `u8 bus;u8 data[]` | `u16 bytes_written` |
| `0x62 UART_READ` | `u8 bus;u16 max_count,timeout_ms` | `u8 data[]` |
| `0x63 UART_CLOSE` | `u8 bus` | 无 |
| `0x70 SAFE` | 无 | 清路由、释放外设、所有 IO 高阻 |
| `0x7F FACTORY_RESET` | ASCII `YES!` | 清空 NVS，之后自动重启 |

**IO_CONFIG.mode**：0 输入、1 推挽输出、2 开漏、3 ADC；pull：0 无、1 上拉、2 下拉。`UART_SETUP`: bus0/1 分别硬件 UART1/UART2；parity 0 无 1 偶 2 奇；stopbits 1/2。`I2C_SETUP`: bus0/1 为 I²C0/I²C1。`SPI_SETUP`: bus0/1 为 SPI2/SPI3；MISO 可设 255（输出专用）。`I2C_XFER`: tx=0/rx=0 是硬件 ACK 探测。

同一个 IO 不能同时作为多个总线的引脚；尝试修改已经被 SPI/I²C/UART/PWM 占用的 IO 会收到 `-2`。关闭外设后可重新配置。

### 性能与可靠性设计

- Wi-Fi 禁用 modem-sleep（`WIFI_PS_NONE`），TCP 采用长连接、`TCP_NODELAY`、内核 keepalive，不做逐位网络远程 bit-bang。
- I²C 使用原生硬件 controller，设备句柄复用；SPI 使用硬件 master 和 DMA；UART 用中断 ring-buffer；GPIO 支持 19bit 批量写入/读取。
- 路由更新分两阶段：先断开**所有**变化的旧路径，再建立新路径，无需整体复位重写所有开关。32/52 节点映射有主机测试。
- 控制单会话避免多客户端并发抢占 UART/SPI 引脚；收发限定帧长、防内存越界、序号防重放、HMAC 防恶意写入。
- GPIO 上电先配置高阻；会话中断则全部关闭。BLE 仅在初次配网期间工作，减少长期 Wi-Fi 抖动与堆内存开销。
- **这些是设计机制，不是实测速度保证**。实际延迟、稳定波特率取决于 PCB 走线、CH446Q 电阻、电源、AP、负载与固件版本。

## 7. 测试

没有 ESP-IDF 工具链时可先运行 PC 单元测试：

```bash
python -m unittest discover -s tests -p 'test_client.py' -v
# 带 GCC 的 Linux/WSL：
gcc -std=c11 -Wall -Wextra -Werror -I main \
    main/route_plan.c tests/test_routes.c -o /tmp/jbox_routes
/tmp/jbox_routes
```

**建议真实硬件验收顺序**：先只接 MCU+CH446Q 控制线，验证 STB/RST 及每个交叉点的导通/断开；再验证 LED 的低电流网络、通用 GPIO；再以 10～50kHz I²C 测试上升沿与 ACK；然后逐步尝试 SPI、UART、PWM/ADC。没有做过流保护前不要连接 5V、3.3V、GND 的电源矩阵 X 引脚。

## 8. 外部文档

- WCH《CH446Q/CH446X》官方数据表：[wch.cn](https://www.wch.cn/downloads/CH446DS1.PDF.html?type=en)
- ESP-IDF 5.5.x：[Wi-Fi BLE Provisioning](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/provisioning/wifi_provisioning.html)
- ESP-IDF：[I²C Master](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/peripherals/i2c.html)
- ESP-IDF：[SPI Master](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/spi_master.html)
- ESP-IDF：[UART Driver](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/uart.html)

工程仅提供软件接口和基础故障隔离策略；**不构成已完成安全认证的电源路由硬件设计**。
