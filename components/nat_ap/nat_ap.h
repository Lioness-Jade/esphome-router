#ifndef NAT_AP_H
#define NAT_AP_H

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <string>
#include <vector>
#include <esp_mac.h>

#include "lwip/lwip_napt.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netif.h>
#include <lwip/dns.h>
#include "lwip/opt.h"

#include "dhcpserver/dhcpserver.h"

static const char *TAG = "NatAp";
#define EXAMPLE_MAX_STA_CONN 10  // AP 模式下最大连接设备数（参考 fran6120/esphome-nat-ap）

namespace esphome {
namespace nat_ap {

// 网络重定向协议类型枚举
enum PortForwardingProtocol {
    PROTOCOL_TCP,
    PROTOCOL_UDP,
    PROTOCOL_TCP_UDP
};

// 端口重定向规则结构体
struct PortForwardingRule {
    PortForwardingProtocol protocol;
    uint16_t external_port;
    ip4_addr_t internal_ip;
    uint16_t internal_port;
};

// 已连接设备信息结构体
struct ConnectedDevice {
    uint8_t mac[6];
    std::string mac_str;
    uint32_t last_seen;
    int8_t rssi;           // 信号强度
    uint16_t phy_rate;     // 物理层速率（单位：Mbps）
};

class NatAp : public Component {
public:
    NatAp(); // 默认构造函数

    void set_ap_ssid(const std::string& ssid) { ap_ssid_ = ssid; }
    void set_ap_password(const std::string& password) { ap_password_ = password; }
    void set_ap_ip_address(const std::string& ip_address) { ap_ip_address_ = ip_address; }
    void set_hide_ssid(bool hide) { hide_ssid_ = hide; }
    void set_bridge_mode(bool bridge) { bridge_mode_ = bridge; }
    
    // 设置 HomeAssistant 传感器用于实时网速监控
    void set_upload_speed_sensor(sensor::Sensor *sensor) { upload_speed_sensor_ = sensor; }
    void set_download_speed_sensor(sensor::Sensor *sensor) { download_speed_sensor_ = sensor; }
    void set_connected_devices_sensor(sensor::Sensor *sensor) { connected_devices_sensor_ = sensor; }
    void set_gateway_health_sensor(sensor::Sensor *sensor) { gateway_health_sensor_ = sensor; }
    void set_forward_throughput_sensor(sensor::Sensor *sensor) { forward_throughput_sensor_ = sensor; }
    void set_sta_link_rate_sensor(sensor::Sensor *sensor) { sta_link_rate_sensor_ = sensor; }
    void set_sta_channel_sensor(sensor::Sensor *sensor) { sta_channel_sensor_ = sensor; }
    
    // 设置文本传感器
    void set_client_mac_list_sensor(text_sensor::TextSensor *sensor) { client_mac_list_sensor_ = sensor; }
    void set_online_status_sensor(text_sensor::TextSensor *sensor) { online_status_sensor_ = sensor; }

    void add_port_forwarding_rule(PortForwardingProtocol protocol, uint16_t external_port,
                                  const std::string& internal_ip_str, uint16_t internal_port);

    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return esphome::setup_priority::AFTER_WIFI; }

protected:
    std::string ap_ssid_;
    std::string ap_password_;
    std::string ap_ip_address_;
    bool hide_ssid_;
    bool bridge_mode_ = false;  // 桥接模式标志

    esp_netif_t *esp_netif_ap;
    esp_netif_t *esp_netif_sta;

    esp_event_handler_instance_t instance_sta_connected;
    esp_event_handler_instance_t instance_sta_disconnected;
    esp_event_handler_instance_t instance_got_ip;

    static NatAp* global_nat_ap_instance;

    std::vector<PortForwardingRule> forwarding_rules_;
    bool napt_enabled_ = false;

    // 已连接设备列表
    std::vector<ConnectedDevice> connected_devices_;
    
    // 网速统计（字节/秒）
    uint32_t last_bytes_sent_ = 0;
    uint32_t last_bytes_recv_ = 0;
    uint32_t upload_speed_bps_ = 0;
    uint32_t download_speed_bps_ = 0;
    uint32_t last_speed_update_ = 0;
    
    // 转发吞吐量统计（包/秒）
    uint32_t last_fwd_packets_ = 0;
    uint32_t fwd_packets_per_sec_ = 0;
    
    // STA 连接信息
    int8_t sta_rssi_ = 0;
    uint8_t sta_channel_ = 0;
    uint16_t sta_phy_rate_ = 0;
    
    // 网关健康状态
    bool gateway_online_ = false;
    uint32_t gateway_check_time_ = 0;
    
    // HomeAssistant 传感器
    sensor::Sensor *upload_speed_sensor_ = nullptr;
    sensor::Sensor *download_speed_sensor_ = nullptr;
    sensor::Sensor *connected_devices_sensor_ = nullptr;
    sensor::Sensor *gateway_health_sensor_ = nullptr;  // 1=在线，0=离线
    sensor::Sensor *forward_throughput_sensor_ = nullptr;  // 转发包数/秒
    sensor::Sensor *sta_link_rate_sensor_ = nullptr;  // STA 链路速率
    sensor::Sensor *sta_channel_sensor_ = nullptr;  // STA 信道
    
    // 文本传感器
    text_sensor::TextSensor *client_mac_list_sensor_ = nullptr;  // 客户端 MAC 列表
    text_sensor::TextSensor *online_status_sensor_ = nullptr;  // 在线状态文本

    // 内部方法
    static void s_wifi_event_handler_ap_connected(void* arg, esp_event_base_t event_base,
                                                  int32_t event_id, void* event_data);

    static void s_wifi_event_handler_ap_disconnected(void* arg, esp_event_base_t event_base,
                                                     int32_t event_id, void* event_data);

    static void s_wifi_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data);
    void enable_napt();
    void apply_port_forwarding_rules();
    void configure_ap_interface();
    void update_connected_devices();
    void update_network_stats();
    void update_sta_connection_info();
    void check_gateway_health();
    void publish_sensors();
    void enable_bridge_mode();
};

} // namespace nat_ap
} // namespace esphome

#endif // NAT_AP_H
