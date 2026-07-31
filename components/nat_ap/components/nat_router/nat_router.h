#ifndef NAT_ROUTER_H
#define NAT_ROUTER_H

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_mac.h>

#include <string>
#include <vector>
#include <cstring>

#include "lwip/lwip_napt.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netif.h"
#include "lwip/dns.h"
#include "lwip/opt.h"
#include "lwip/stats.h"

#include "dhcpserver/dhcpserver.h"

static const char *TAG = "NatRouter";
#define EXAMPLE_MAX_STA_CONN 10

namespace esphome {
namespace nat_router {

enum PortForwardingProtocol {
    PROTOCOL_TCP,
    PROTOCOL_UDP,
    PROTOCOL_TCP_UDP
};

struct PortForwardingRule {
    PortForwardingProtocol protocol;
    uint16_t external_port;
    ip4_addr_t internal_ip;
    uint16_t internal_port;
};

struct ConnectedDevice {
    uint8_t mac[6];
    std::string mac_str;
    uint32_t last_seen;
    int8_t rssi;
};

class NatRouter : public Component {
public:
    NatRouter();

    void set_ap_ssid(const std::string& ssid) { ap_ssid_ = ssid; }
    void set_ap_password(const std::string& password) { ap_password_ = password; }
    void set_ap_ip_address(const std::string& ip_address) { ap_ip_address_ = ip_address; }
    void set_hide_ssid(bool hide) { hide_ssid_ = hide; }
    void set_bridge_mode(bool bridge) { bridge_mode_ = bridge; }

    void set_upload_speed_sensor(sensor::Sensor *s) { upload_speed_sensor_ = s; }
    void set_download_speed_sensor(sensor::Sensor *s) { download_speed_sensor_ = s; }
    void set_connected_devices_sensor(sensor::Sensor *s) { connected_devices_sensor_ = s; }
    void set_gateway_health_sensor(sensor::Sensor *s) { gateway_health_sensor_ = s; }
    void set_forward_throughput_sensor(sensor::Sensor *s) { forward_throughput_sensor_ = s; }
    void set_sta_link_rate_sensor(sensor::Sensor *s) { sta_link_rate_sensor_ = s; }
    void set_sta_channel_sensor(sensor::Sensor *s) { sta_channel_sensor_ = s; }

    void set_client_mac_list_sensor(text_sensor::TextSensor *s) { client_mac_list_sensor_ = s; }
    void set_online_status_sensor(text_sensor::TextSensor *s) { online_status_sensor_ = s; }

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
    bool bridge_mode_ = false;

    esp_netif_t *esp_netif_ap;
    esp_netif_t *esp_netif_sta;

    esp_event_handler_instance_t instance_sta_connected;
    esp_event_handler_instance_t instance_sta_disconnected;
    esp_event_handler_instance_t instance_got_ip;

    static NatRouter* global_nat_router_instance;

    std::vector<PortForwardingRule> forwarding_rules_;
    bool napt_enabled_ = false;

    std::vector<ConnectedDevice> connected_devices_;

    uint32_t last_pkts_sent_ = 0;
    uint32_t last_pkts_recv_ = 0;
    uint32_t upload_speed_bps_ = 0;
    uint32_t download_speed_bps_ = 0;
    uint32_t last_speed_update_ = 0;

    uint32_t last_fwd_packets_ = 0;
    uint32_t fwd_packets_per_sec_ = 0;

    int8_t sta_rssi_ = 0;
    uint8_t sta_channel_ = 0;
    uint16_t sta_phy_rate_ = 0;

    bool gateway_online_ = false;
    uint32_t gateway_check_time_ = 0;

    uint32_t last_sensor_update_ = 0;

    sensor::Sensor *upload_speed_sensor_ = nullptr;
    sensor::Sensor *download_speed_sensor_ = nullptr;
    sensor::Sensor *connected_devices_sensor_ = nullptr;
    sensor::Sensor *gateway_health_sensor_ = nullptr;
    sensor::Sensor *forward_throughput_sensor_ = nullptr;
    sensor::Sensor *sta_link_rate_sensor_ = nullptr;
    sensor::Sensor *sta_channel_sensor_ = nullptr;

    text_sensor::TextSensor *client_mac_list_sensor_ = nullptr;
    text_sensor::TextSensor *online_status_sensor_ = nullptr;

    static void s_wifi_event_handler_ap_connected(void* arg, esp_event_base_t event_base,
                                                  int32_t event_id, void* event_data);
    static void s_wifi_event_handler_ap_disconnected(void* arg, esp_event_base_t event_base,
                                                     int32_t event_id, void* event_data);
    static void s_wifi_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data);

    void enable_napt();
    void enable_bridge_mode();
    void apply_port_forwarding_rules();
    void configure_ap_interface();

    void update_connected_devices();
    void update_network_stats();
    void update_sta_connection_info();
    void check_gateway_health();
    void publish_sensors();
};

} // namespace nat_router
} // namespace esphome

#endif // NAT_ROUTER_H
