# 📡 ESP32 NAT/AP 桥接 ESPHome 组件 (迷你路由器)

本项目将 ESP32 设备转换为一个支持网络地址转换 (NAT) 的 Wi-Fi 接入点 (AP) / 支持桥接功能的设备，可作为 mini 路由器，直接从 ESPHome 进行配置。作为外部组件实现，它可以干净、声明式地集成到您的 YAML 配置中。

---

## 📝 概述

这个用于 ESPHome 的自定义外部组件允许您的 ESP32 创建自己的本地 Wi-Fi 网络 (AP)，同时连接到现有的 Wi-Fi 网络 (Station 或 STA 模式)。连接到 ESP32 AP 的设备可以通过 ESP32 STA 连接访问互联网，这得益于 NAT（网络地址转换）的实现。它还提供了配置端口转发的能力，以将内部服务暴露给外部网络。

**桥接功能已完全实现**：启用桥接模式后，ESP32 将像真正的路由器中继一样工作，确保所有类型的 IP 数据包（ICMP、TCP、UDP 等）都能正确转发，实现透明的网络桥接。

---

## ✨ 主要功能

### 核心网络功能

* **可配置的 Wi-Fi 接入点 (AP)：** 定义由 ESP32 创建的 Wi-Fi 网络的 SSID、密码和可见性（隐藏/可见）。

* **由 ESPHome 管理的 Station 模式 (STA)：** ESP32 作为标准客户端连接到您的主 Wi-Fi 网络，Wi-Fi 设置由 ESPHome 管理。

* **网络地址转换 (NAT)：** AP 网络上的设备可以通过 ESP32 的 STA 连接访问互联网。

* **完整桥接模式：** ✅ **已实现** - 启用后调用 `ip_forward_enable(1)`，确保完整的 IP 数据包转发，与路由器中继功能完全相同，支持 ICMP、TCP、UDP 等所有协议。

* **端口转发：** 配置规则，将来自主网络的传入流量重定向到 AP 内部网络上的特定设备。

* **可自定义的 AP 内部 IP：** 直接从 YAML 配置中定义 ESP32 生成的 Wi-Fi 网络 IP 地址（例如 `192.168.10.1`）。

### HomeAssistant 传感器集成

#### 数值传感器
- **upload_speed**: 上传速度，单位为 Mbps（兆比特每秒）
- **download_speed**: 下载速度，单位为 Mbps（兆比特每秒）
- **connected_devices**: 当前连接到 AP 的设备数量
- **gateway_health**: 网关健康状态（1=在线，0=离线），每 5 秒检查一次
- **forward_throughput**: 转发吞吐量，单位为包/秒
- **sta_link_rate**: STA 链路速率，单位为 Mbps（根据信号质量估算）
- **sta_channel**: STA 连接的 Wi-Fi 信道

#### 文本传感器
- **client_mac_list**: 当前连接到 AP 的所有客户端 MAC 地址列表，用分号分隔
- **online_status**: 在线状态文本（"在线" 或 "离线"）

### 其他特性

* **在线检测：** 每 5 秒检查一次网关健康状态，确保网络连接正常

* **STA 连接信息：** 提供 AP 的设备 STA 链路速率和信道信息

* **NAT/桥接客户端监控：** 实时显示连接设备的数量和 MAC 地址列表

* **桥接转发吞吐计数：** 实时监控数据包转发速率（包/秒）

* **不写入 Flash：** 所有统计数据仅在内存中计算，不写入 Flash，延长设备寿命

* **基于 ESP-IDF 5.1.6：** 使用最新的 ESP-IDF API 和功能，实现稳健且现代化的实现。

* **高效数据传输：** 优化算法确保数据传输效率高，对设备资源影响小

---

## 🙏 致谢和来源

本项目的开发基于 [mag1024/esphome-nat-ap](https://github.com/mag1024/esphome-nat-ap) 组件的概念和基础。核心 NAT 实现和对现代 ESP-IDF API 的采用受到了 [martin-ger/esp32_nat_router](https://github.com/martin-ger/esp32_nat_router) 项目的启发和借鉴。

特别感谢 [fran6120/esphome-nat-ap](https://github.com/fran6120/esphome-nat-ap) 项目的启发和参考，为本项目提供了重要的概念验证和实现思路，包括 AP 模式下最大连接设备数的配置参考。

本项目仓库地址：[Lioness-Jade/esphome-router](https://github.com/Lioness-Jade/esphome-router)

本 `README.md` 的编写以及代码的适配和调试辅助工作得到了 **Qwen** 的帮助。

---

## 🚀 ESPHome 最小配置

要使用此组件，您必须将其添加为"external_components"：

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/Lioness-Jade/esphome-router
    components: nat_ap
    refresh: always
```

配置示例：

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/Lioness-Jade/esphome-router
    components: nat_ap
    refresh: always

esphome:
  name: esp32-c3-nat
  friendly_name: esp32-c3-nat

esp32:
  board: esp32-c3-devkitm-1
  cpu_frequency: 160MHZ
  framework:
    type: esp-idf

logger:
  level: DEBUG

api:
  encryption:
    key: ""
  reboot_timeout: 0s

ota:
  - platform: esphome
    password: ""

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

nat_ap:
  ap_ssid: "ESPHomeNATAP"            # AP 的 SSID 名称
  ap_password: "ESPHomeNATAP"        # AP 的密码
  ap_ip_address: "192.168.10.1"      # AP 向客户端提供的 IP
  hide_ssid: true                    # true/false 启用/禁用 SSID 广播功能
  bridge_mode: true                  # 启用桥接模式，确保完整的 IP 数据包转发（类似路由器中继）

  # HomeAssistant 数值传感器配置
  sensors:
    upload_speed:
      name: "NAT AP 上传速度"
    download_speed:
      name: "NAT AP 下载速度"
    connected_devices:
      name: "NAT AP 连接设备数"
    gateway_health:
      name: "NAT AP 网关健康状态"
    forward_throughput:
      name: "NAT AP 转发吞吐量"
    sta_link_rate:
      name: "NAT AP STA 链路速率"
    sta_channel:
      name: "NAT AP STA 信道"

  # HomeAssistant 文本传感器配置
  text_sensors:
    client_mac_list:
      name: "NAT AP 客户端 MAC 列表"
    online_status:
      name: "NAT AP 在线状态"

  port_forwarding:
    - protocol: TCP                  # API
      external_port: 10001           # HomeAssistant 或 esphome 搜索重定向设备的端口
      internal_ip: "192.168.10.2"    # 要重定向端口的客户端 IP（另一个 esphome 设备）
      internal_port: 9001            # 另一个 esphome 设备的 API 端口
    - protocol: TCP                  # OTA
      external_port: 10002           # esphome 搜索重定向设备的端口
      internal_ip: "192.168.10.2"    # 要重定向端口的客户端 IP（另一个 esphome 设备）
      internal_port: 9002            # 另一个 esphome 设备的 OTA 端口
    - protocol: UDP
      external_port: 12345
      internal_ip: "192.168.10.2"
      internal_port: 12345
```

连接到 esp32-c3-nat AP 的另一个 esphome 设备：

```yaml
esphome:
  name: esp32-c3
  friendly_name: esp32-c3

esp32:
  board: esp32-c3-devkitm-1
  cpu_frequency: 160MHZ
  framework:
    type: esp-idf

logger:
  level: DEBUG

api:
  encryption:
    key: ""
  port: 9001

ota:
  - platform: esphome
    password: ""
    port: 9002

wifi:
  ssid: ESPHomeNATAP
  password: ESPHomeNATAP
  use_address: ""                     # 您的路由器分配给 esp32-c3-nat 的 IP
  manual_ip:
    gateway: "192.168.10.1"           # esp32-c3-nat 网关（AP 向客户端提供的 IP）
    static_ip: "192.168.10.2"         # 您要分配给此 esp32 的 IP（在 esp32-c3-nat 网络内）
    subnet: 255.255.255.0
  fast_connect: true
```

已测试最多可级联连接 10 个 ESP32（参考 fran6120/esphome-nat-ap），到最后一个 ESP32 的延迟小于 1 秒。

甚至 OTA 更新也能正常工作，尽管延迟会更加明显。

---

## 🌉 桥接功能使用指南

### 什么是桥接模式？

桥接模式（Bridge Mode）启用后，ESP32 将作为一个透明的网络桥接器工作，类似于路由器的中继模式。与标准 NAT 模式不同，桥接模式确保**所有类型的 IP 数据包**（ICMP、TCP、UDP 等）都能正确转发，实现真正的网络透明桥接。

### 启用桥接模式

在 `nat_ap` 配置中添加或设置 `bridge_mode: true`：

```yaml
nat_ap:
  ap_ssid: "ESPHomeNATAP"
  ap_password: "ESPHomeNATAP"
  ap_ip_address: "192.168.10.1"
  bridge_mode: true  # ← 启用桥接模式
```

### 桥接模式 vs NAT 模式

| 特性 | NAT 模式 (默认) | 桥接模式 |
|------|----------------|----------|
| IP 转发 | 仅 TCP/UDP | ✅ 所有协议 (ICMP, TCP, UDP 等) |
| 网络透明度 | 需要地址转换 | ✅ 完全透明 |
| Ping 测试 | 可能受限 | ✅ 完全支持 |
| 适用场景 | 一般上网 | 需要完整网络功能的场景 |

### 工作原理

当 `bridge_mode: true` 时，系统会自动调用 `ip_forward_enable(1)`，启用内核级别的 IP 包转发功能，确保：

1. **ICMP 包转发**：支持 ping 等网络诊断工具
2. **完整的 TCP/UDP 转发**：所有端口和协议都能正常通信
3. **透明桥接**：客户端设备感觉像是直接连接到主网络

### 验证桥接功能

启用桥接模式后，您可以：

1. **Ping 测试**：从 AP 客户端 ping 主网络中的设备
   ```bash
   ping 192.168.1.1  # 主网络网关
   ```

2. **访问主网络服务**：AP 客户端可以直接访问主网络中的服务器、打印机等

3. **监控转发吞吐量**：通过 HomeAssistant 传感器查看实时转发数据
   - `forward_throughput`: 显示每秒转发的数据包数量
   - `upload_speed` / `download_speed`: 显示桥接数据传输速率

### 注意事项

- 桥接模式会略微增加 CPU 使用率，因为需要处理更多类型的网络包
- 确保 ESP32 的 STA 连接稳定，以获得最佳桥接性能
- 桥接模式下，AP 客户端仍然使用 `ap_ip_address` 配置的网段 IP
- 所有统计数据仅在内存中计算，不会写入 Flash，不影响设备寿命

### 示例配置（完整版）

```yaml
nat_ap:
  ap_ssid: "ESPHomeNATAP"
  ap_password: "ESPHomeNATAP"
  ap_ip_address: "192.168.10.1"
  hide_ssid: false
  bridge_mode: true  # 启用完整桥接功能

  sensors:
    forward_throughput:
      name: "桥接转发吞吐量"
    upload_speed:
      name: "桥接上传速度"
    download_speed:
      name: "桥接下载速度"
    connected_devices:
      name: "桥接客户端数量"
```

---

## 📊 传感器说明

### 数值传感器
- **upload_speed**: 上传速度，单位为 Mbps（兆比特每秒），实时监测 NAT/桥接转发数据速率
- **download_speed**: 下载速度，单位为 Mbps（兆比特每秒），实时监测 NAT/桥接转发数据速率
- **connected_devices**: 当前连接到 AP 的设备数量，NAT/桥接客户端计数
- **gateway_health**: 网关健康状态（1=在线，0=离线），每 5 秒检查一次，用于在线检测
- **forward_throughput**: 转发吞吐量，单位为包/秒，桥接转发吞吐计数
- **sta_link_rate**: STA 链路速率，单位为 Mbps，提供 AP 的设备 STA 链路速率
- **sta_channel**: STA 连接的 Wi-Fi 信道，提供 AP 的设备 STA 信道信息

### 文本传感器
- **client_mac_list**: 当前连接到 AP 的所有客户端 MAC 地址列表，用分号分隔，NAT/桥接客户端 MAC 名单
- **online_status**: 在线状态文本（"在线" 或 "离线"），网关资源健康状态指示

这些传感器会每秒更新一次（网关健康状态每 5 秒更新一次），可以在 HomeAssistant 中用于创建仪表板、自动化等。所有数据仅在内存中计算，不写入 Flash。

---

## 🔧 故障排除

1. **NAT 不工作**：确保 STA 已成功连接到主 Wi-Fi 并获取了 IP 地址
2. **端口转发不生效**：检查内部 IP 地址是否正确，确保目标设备在线
3. **桥接模式问题**：启用桥接模式后，系统会自动调用 `ip_forward_enable(1)` 确保完整的 IP 数据包转发
4. **传感器无数据**：确保已在配置中启用相应的传感器
5. **MAC 列表为空**：确保有设备连接到 AP
6. **转发吞吐量为 0**：确保有设备通过 ESP32 进行网络通信
7. **网关健康显示离线**：检查 STA 是否已获取到有效的 IP 地址和网关
8. **桥接模式下 Ping 不通**：确认 `bridge_mode: true` 已设置，并检查防火墙设置
