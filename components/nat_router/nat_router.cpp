#include "nat_router.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string>
#include <cstring>

#include "lwip/ip_addr.h"
#include "lwip/lwip_napt.h"
#include "lwip/tcpip.h"
#include <esp_netif.h>
#include <esp_err.h>

namespace esphome {
namespace nat_router {

NatRouter* NatRouter::global_nat_router_instance = nullptr;

NatRouter::NatRouter()
    : ap_ssid_("ESPHomeAP"), ap_password_("ESPHomeAPPass"), ap_ip_address_("192.168.4.1"),
      hide_ssid_(false), bridge_mode_(false),
      esp_netif_ap(nullptr), esp_netif_sta(nullptr) {
    global_nat_router_instance = this;
}

void NatRouter::add_port_forwarding_rule(PortForwardingProtocol protocol, uint16_t external_port,
                                         const std::string& internal_ip_str, uint16_t internal_port) {
    PortForwardingRule rule;
    rule.protocol = protocol;
    rule.external_port = external_port;

    if (ip4addr_aton(internal_ip_str.c_str(), &rule.internal_ip) == 0) {
        ESP_LOGE(TAG, "端口重定向规则的内部 IP 地址无效：%s", internal_ip_str.c_str());
        return;
    }
    rule.internal_port = internal_port;
    forwarding_rules_.push_back(rule);
    ESP_LOGI(TAG, "已添加端口重定向规则（待应用）：外部 %u -> 内部 %s:%u (协议：%d)",
             external_port, internal_ip_str.c_str(), internal_port, (int)protocol);
}

void NatRouter::setup() {
    ESP_LOGI(TAG, "正在配置 NAT Router 组件...");

    esp_netif_ap = esp_netif_create_default_wifi_ap();
    if (!esp_netif_ap) {
        ESP_LOGE(TAG, "失败：无法创建默认 AP 接口。");
        this->mark_failed();
        return;
    }
    ESP_LOGI(TAG, "已创建默认 AP 接口。");

    esp_netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!esp_netif_sta) {
        ESP_LOGE(TAG, "失败：无法获取 STA 接口句柄 (WIFI_STA_DEF)。NAT 将无法工作。");
        this->mark_failed();
        return;
    }
    ESP_LOGI(TAG, "已获取 STA 接口。");

    configure_ap_interface();

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_LOGI(TAG, "Wi-Fi 模式已设置为 APSTA。");

    wifi_config_t ap_config = {};
    ap_config.ap.channel = 1;
    ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ap_config.ap.ssid_hidden = (uint8_t)hide_ssid_;
    ap_config.ap.max_connection = EXAMPLE_MAX_STA_CONN;
    ap_config.ap.beacon_interval = 100;
    ap_config.ap.pmf_cfg.capable = true;
    ap_config.ap.pmf_cfg.required = false;

    strncpy((char*)ap_config.ap.ssid, ap_ssid_.c_str(), sizeof(ap_config.ap.ssid) - 1);
    strncpy((char*)ap_config.ap.password, ap_password_.c_str(), sizeof(ap_config.ap.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_LOGI(TAG, "已应用 AP Wi-Fi 配置。");

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED,
                &NatRouter::s_wifi_event_handler_ap_connected, nullptr, &instance_sta_connected));
    ESP_LOGI(TAG, "已注册 AP_STACONNECTED 事件处理器。");

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED,
                &NatRouter::s_wifi_event_handler_ap_disconnected, nullptr, &instance_sta_disconnected));
    ESP_LOGI(TAG, "已注册 AP_STADISCONNECTED 事件处理器。");

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                &NatRouter::s_wifi_event_handler, nullptr, &instance_got_ip));
    ESP_LOGI(TAG, "已注册 STA IP 事件处理器。");

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi 已启动（AP 和 STA 均已启用）。");

    esp_netif_ip_info_t sta_ip_info;
    if (esp_netif_get_ip_info(esp_netif_sta, &sta_ip_info) == ESP_OK && sta_ip_info.ip.addr != 0) {
        ESP_LOGI(TAG, "STA 在 setup() 结束时已有 IP：" IPSTR "。立即启用 NAPT。", IP2STR(&sta_ip_info.ip));
        enable_napt();
    } else {
        ESP_LOGI(TAG, "NAT Router 组件已配置。等待 STA 获取 IP...");
    }
}

void NatRouter::loop() {
    uint32_t now = millis();

    if (now - last_sensor_update_ >= 1000) {
        update_connected_devices();
        update_sta_connection_info();
        publish_sensors();
        last_sensor_update_ = now;
    }

    update_network_stats();
    check_gateway_health();
}

void NatRouter::s_wifi_event_handler_ap_connected(void* arg, esp_event_base_t event_base,
                                                  int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "客户端已连接到 AP：MAC " MACSTR ", AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void NatRouter::s_wifi_event_handler_ap_disconnected(void* arg, esp_event_base_t event_base,
                                                     int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "客户端已从 AP 断开：MAC " MACSTR ", AID=%d, 原因=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

void NatRouter::s_wifi_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data) {
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "STA 已获取 IP：" IPSTR, IP2STR(&event->ip_info.ip));
        if (global_nat_router_instance) {
            global_nat_router_instance->enable_napt();
        }
    }
}

void NatRouter::enable_napt() {
    if (napt_enabled_) {
        ESP_LOGI(TAG, "NAPT 已启用。无需重复启用。");
        return;
    }

    ESP_LOGI(TAG, "正在启用 NAPT...");
    esp_netif_ip_info_t ap_ip_info;
    if (esp_netif_get_ip_info(esp_netif_ap, &ap_ip_info) != ESP_OK) {
        ESP_LOGE(TAG, "无法获取 AP IP 信息。将不会启用 NAPT。");
        return;
    }

    LOCK_TCPIP_CORE();

    if (bridge_mode_) {
        enable_bridge_mode();
    }

    ip_napt_enable(ap_ip_info.ip.addr, 1);

    UNLOCK_TCPIP_CORE();

    ESP_LOGI(TAG, "NAPT 已在 AP IP 上启用：" IPSTR, IP2STR(&ap_ip_info.ip));

    napt_enabled_ = true;

    apply_port_forwarding_rules();
}

void NatRouter::enable_bridge_mode() {
    ESP_LOGI(TAG, "启用桥接模式 - 全协议 IP 转发（ICMP/TCP/UDP）");
    ESP_LOGI(TAG, "桥接模式已激活（编译时 CONFIG_LWIP_IP_FORWARD=1 + 运行时 NAPT）");
}

void NatRouter::apply_port_forwarding_rules() {
    if (forwarding_rules_.empty()) {
        ESP_LOGI(TAG, "没有要应用的端口重定向规则。");
        return;
    }

    esp_netif_ip_info_t sta_ip_info;
    if (esp_netif_get_ip_info(esp_netif_sta, &sta_ip_info) != ESP_OK || sta_ip_info.ip.addr == 0) {
        ESP_LOGE(TAG, "无法获取 STA IP 信息或 STA 没有 IP。将不应用重定向规则。");
        return;
    }

    ESP_LOGI(TAG, "正在应用 %u 个端口重定向规则，外部 STA IP：" IPSTR,
             (unsigned)forwarding_rules_.size(), IP2STR(&sta_ip_info.ip));

    LOCK_TCPIP_CORE();

    for (const auto& rule : forwarding_rules_) {
        uint8_t proto_lwip;

        if (rule.protocol == PROTOCOL_TCP_UDP) {
            ESP_LOGI(TAG, "应用 TCP+UDP 规则：外部 %u -> 内部 %s:%u",
                     rule.external_port, ip4addr_ntoa(&rule.internal_ip), rule.internal_port);

            ip_portmap_add(IPPROTO_TCP, sta_ip_info.ip.addr, rule.external_port,
                           rule.internal_ip.addr, rule.internal_port);
            ip_portmap_add(IPPROTO_UDP, sta_ip_info.ip.addr, rule.external_port,
                           rule.internal_ip.addr, rule.internal_port);
            continue;
        }

        if (rule.protocol == PROTOCOL_TCP) {
            proto_lwip = IPPROTO_TCP;
        } else if (rule.protocol == PROTOCOL_UDP) {
            proto_lwip = IPPROTO_UDP;
        } else {
            ESP_LOGE(TAG, "端口重定向规则的协议未知。");
            continue;
        }

        ip_portmap_add(proto_lwip, sta_ip_info.ip.addr, rule.external_port,
                       rule.internal_ip.addr, rule.internal_port);

        ESP_LOGI(TAG, "已应用重定向规则：%s 外部：" IPSTR ":%u -> 内部：%s:%u",
                 (proto_lwip == IPPROTO_TCP ? "TCP" : "UDP"),
                 IP2STR(&sta_ip_info.ip), rule.external_port,
                 ip4addr_ntoa(&rule.internal_ip), rule.internal_port);
    }

    UNLOCK_TCPIP_CORE();
}

void NatRouter::configure_ap_interface() {
    ESP_LOGI(TAG, "正在配置 AP 接口（静态 IP、DNS 和 DHCP 选项）...");

    esp_err_t err = esp_netif_dhcps_stop(esp_netif_ap);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGW(TAG, "esp_netif_dhcps_stop() 意外失败：%s", esp_err_to_name(err));
    }

    esp_netif_ip_info_t ip_info{};
    ip4_addr_t ip4, gw4, sn4;

    if (ip4addr_aton(ap_ip_address_.c_str(), &ip4) == 0) {
        ESP_LOGE(TAG, "失败：AP IP 地址无效：%s。使用默认值 192.168.4.1。", ap_ip_address_.c_str());
        ip4addr_aton("192.168.4.1", &ip4);
    }

    gw4 = ip4;
    ip4addr_aton("255.255.255.0", &sn4);

    ip_info.ip.addr      = ip4.addr;
    ip_info.gw.addr      = gw4.addr;
    ip_info.netmask.addr = sn4.addr;

    err = esp_netif_set_ip_info(esp_netif_ap, &ip_info);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "AP 静态 IP 已配置：" IPSTR, IP2STR(&ip_info.ip));
    } else {
        ESP_LOGE(TAG, "esp_netif_set_ip_info() 失败：%s", esp_err_to_name(err));
    }

    esp_netif_dns_info_t dns_info{};
    if (esp_netif_sta && esp_netif_get_dns_info(esp_netif_sta, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
        uint32_t raw_dns_ip = dns_info.ip.u_addr.ip4.addr;
        ESP_LOGI(TAG, "从 STA 获取 DNS：%u.%u.%u.%u",
                 raw_dns_ip & 0xFF, (raw_dns_ip >> 8) & 0xFF,
                 (raw_dns_ip >> 16) & 0xFF, (raw_dns_ip >> 24) & 0xFF);

        dhcps_offer_t dhcps_dns_value = OFFER_DNS;
        err = esp_netif_dhcps_option(esp_netif_ap,
                                     ESP_NETIF_OP_SET,
                                     ESP_NETIF_DOMAIN_NAME_SERVER,
                                     &dhcps_dns_value,
                                     sizeof(dhcps_dns_value));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "esp_netif_dhcps_option(OFFER_DNS) 失败：%s", esp_err_to_name(err));
        }

        err = esp_netif_set_dns_info(esp_netif_ap, ESP_NETIF_DNS_MAIN, &dns_info);
        if (err == ESP_OK) {
            uint32_t raw_dns_ip2 = dns_info.ip.u_addr.ip4.addr;
            ESP_LOGI(TAG, "AP DHCP 将提供 DNS：%u.%u.%u.%u",
                     raw_dns_ip2 & 0xFF, (raw_dns_ip2 >> 8) & 0xFF,
                     (raw_dns_ip2 >> 16) & 0xFF, (raw_dns_ip2 >> 24) & 0xFF);
        } else {
            ESP_LOGW(TAG, "esp_netif_set_dns_info() 失败：%s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "无法从 STA 获取 DNS（可能尚未连接？）。AP 客户端可能使用默认 DNS 或无法工作。");
    }
}

} // namespace nat_router
} // namespace esphome
