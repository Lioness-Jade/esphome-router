#include "nat_router.h"

#include <esp_log.h>
#include <cstring>

#include "lwip/stats.h"

namespace esphome {
namespace nat_router {

void NatRouter::update_connected_devices() {
    wifi_sta_list_t sta_list;
    memset(&sta_list, 0, sizeof(sta_list));

    if (esp_wifi_ap_get_sta_list(&sta_list) != ESP_OK) {
        return;
    }

    connected_devices_.resize(sta_list.num);

    for (int i = 0; i < sta_list.num; i++) {
        memcpy(connected_devices_[i].mac, sta_list.sta[i].mac, 6);

        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 sta_list.sta[i].mac[0], sta_list.sta[i].mac[1],
                 sta_list.sta[i].mac[2], sta_list.sta[i].mac[3],
                 sta_list.sta[i].mac[4], sta_list.sta[i].mac[5]);
        connected_devices_[i].mac_str = mac_str;
        connected_devices_[i].last_seen = millis();
        connected_devices_[i].rssi = sta_list.sta[i].rssi;
    }
}

void NatRouter::update_network_stats() {
    uint32_t current_time = millis();

    if (current_time - last_speed_update_ >= 1000) {
        uint32_t current_pkts_sent = (uint32_t)lwip_stats.ip.xmit;
        uint32_t current_pkts_recv = (uint32_t)lwip_stats.ip.recv;

        uint32_t pkts_sent_delta = (current_pkts_sent >= last_pkts_sent_) ?
                                   (current_pkts_sent - last_pkts_sent_) : 0;
        uint32_t pkts_recv_delta = (current_pkts_recv >= last_pkts_recv_) ?
                                   (current_pkts_recv - last_pkts_recv_) : 0;

        upload_speed_bps_ = pkts_sent_delta * 1000;
        download_speed_bps_ = pkts_recv_delta * 1000;

        uint32_t current_fwd_packets = (uint32_t)lwip_stats.ip.fw;
        fwd_packets_per_sec_ = (current_fwd_packets >= last_fwd_packets_) ?
                               (current_fwd_packets - last_fwd_packets_) : 0;
        last_fwd_packets_ = current_fwd_packets;

        last_pkts_sent_ = current_pkts_sent;
        last_pkts_recv_ = current_pkts_recv;
        last_speed_update_ = current_time;
    }
}

void NatRouter::update_sta_connection_info() {
    wifi_ap_record_t ap_info;
    memset(&ap_info, 0, sizeof(ap_info));

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        sta_rssi_ = ap_info.rssi;
        sta_channel_ = ap_info.primary;

        if (sta_rssi_ > -50) {
            sta_phy_rate_ = 72;
        } else if (sta_rssi_ > -65) {
            sta_phy_rate_ = 54;
        } else if (sta_rssi_ > -75) {
            sta_phy_rate_ = 24;
        } else {
            sta_phy_rate_ = 11;
        }
    }
}

void NatRouter::check_gateway_health() {
    uint32_t current_time = millis();

    if (current_time - gateway_check_time_ >= 5000) {
        esp_netif_ip_info_t sta_ip_info;
        if (esp_netif_get_ip_info(esp_netif_sta, &sta_ip_info) == ESP_OK && sta_ip_info.ip.addr != 0) {
            gateway_online_ = true;
        } else {
            gateway_online_ = false;
        }
        gateway_check_time_ = current_time;
    }
}

void NatRouter::publish_sensors() {
    if (upload_speed_sensor_ != nullptr) {
        float upload_mbps = (float)upload_speed_bps_ * 8.0f / 1000000.0f;
        upload_speed_sensor_->publish_state(upload_mbps);
    }

    if (download_speed_sensor_ != nullptr) {
        float download_mbps = (float)download_speed_bps_ * 8.0f / 1000000.0f;
        download_speed_sensor_->publish_state(download_mbps);
    }

    if (connected_devices_sensor_ != nullptr) {
        connected_devices_sensor_->publish_state((float)connected_devices_.size());
    }

    if (gateway_health_sensor_ != nullptr) {
        gateway_health_sensor_->publish_state(gateway_online_ ? 1.0f : 0.0f);
    }

    if (forward_throughput_sensor_ != nullptr) {
        forward_throughput_sensor_->publish_state((float)fwd_packets_per_sec_);
    }

    if (sta_link_rate_sensor_ != nullptr) {
        sta_link_rate_sensor_->publish_state((float)sta_phy_rate_);
    }

    if (sta_channel_sensor_ != nullptr) {
        sta_channel_sensor_->publish_state((float)sta_channel_);
    }

    if (client_mac_list_sensor_ != nullptr) {
        std::string mac_list;
        for (size_t i = 0; i < connected_devices_.size(); i++) {
            if (i > 0) mac_list += "; ";
            mac_list += connected_devices_[i].mac_str;
        }
        client_mac_list_sensor_->publish_state(mac_list);
    }

    if (online_status_sensor_ != nullptr) {
        online_status_sensor_->publish_state(gateway_online_ ? "在线" : "离线");
    }
}

} // namespace nat_router
} // namespace esphome
