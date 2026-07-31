import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import CONF_ID
import esphome.components.sensor as sensor
import esphome.components.text_sensor as text_sensor

CODEOWNERS = ["@Lioness-Jade"]
DEPENDENCIES = ["wifi"]

nat_ap_ns = cg.esphome_ns.namespace("nat_ap")
NatAp = nat_ap_ns.class_("NatAp", cg.Component)

PortForwardingProtocolEnum = nat_ap_ns.enum("PortForwardingProtocol")

PORT_FORWARDING_PROTOCOL_SCHEMA = cv.enum(
    {
        "TCP": PortForwardingProtocolEnum.PROTOCOL_TCP,
        "UDP": PortForwardingProtocolEnum.PROTOCOL_UDP,
        "TCP_UDP": PortForwardingProtocolEnum.PROTOCOL_TCP_UDP,
    },
    upper=True,
)

PORT_FORWARDING_RULE_SCHEMA = cv.Schema(
    {
        cv.Required("protocol"): PORT_FORWARDING_PROTOCOL_SCHEMA,
        cv.Required("external_port"): cv.port,
        cv.Required("internal_ip"): cv.ip_address,
        cv.Required("internal_port"): cv.port,
    }
)

SENSOR_SCHEMA = cv.Schema(
    {
        cv.Optional("upload_speed"): sensor.sensor_schema(
            unit_of_measurement="Mbps", accuracy_decimals=3
        ),
        cv.Optional("download_speed"): sensor.sensor_schema(
            unit_of_measurement="Mbps", accuracy_decimals=3
        ),
        cv.Optional("connected_devices"): sensor.sensor_schema(
            unit_of_measurement="", accuracy_decimals=0
        ),
        cv.Optional("gateway_health"): sensor.sensor_schema(
            unit_of_measurement="", accuracy_decimals=0
        ),
        cv.Optional("forward_throughput"): sensor.sensor_schema(
            unit_of_measurement="pkt/s", accuracy_decimals=0
        ),
        cv.Optional("sta_link_rate"): sensor.sensor_schema(
            unit_of_measurement="Mbps", accuracy_decimals=0
        ),
        cv.Optional("sta_channel"): sensor.sensor_schema(
            unit_of_measurement="", accuracy_decimals=0
        ),
    }
)

TEXT_SENSOR_SCHEMA = cv.Schema(
    {
        cv.Optional("client_mac_list"): text_sensor.text_sensor_schema(),
        cv.Optional("online_status"): text_sensor.text_sensor_schema(),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NatAp),
        cv.Optional("ap_ssid", default="ESPHomeAP"): cv.string,
        cv.Optional("ap_password", default="ESPHomeAPPass"): cv.All(
            cv.string, cv.Length(min=8)
        ),
        cv.Optional("ap_ip_address", default="192.168.4.1"): cv.ip_address,
        cv.Optional("hide_ssid", default=False): cv.boolean,
        cv.Optional("bridge_mode", default=False): cv.boolean,
        cv.Optional("port_forwarding"): cv.All(
            cv.ensure_list(PORT_FORWARDING_RULE_SCHEMA), cv.Length(min=0)
        ),
        cv.Optional("sensors"): SENSOR_SCHEMA,
        cv.Optional("text_sensors"): TEXT_SENSOR_SCHEMA,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_ap_ssid(config["ap_ssid"]))
    cg.add(var.set_ap_password(config["ap_password"]))
    cg.add(var.set_ap_ip_address(str(config["ap_ip_address"])))
    cg.add(var.set_hide_ssid(config["hide_ssid"]))
    cg.add(var.set_bridge_mode(config["bridge_mode"]))

    if "port_forwarding" in config:
        for rule in config["port_forwarding"]:
            cg.add(
                var.add_port_forwarding_rule(
                    rule["protocol"],
                    rule["external_port"],
                    str(rule["internal_ip"]),
                    rule["internal_port"],
                )
            )

    if "sensors" in config:
        sensors_config = config["sensors"]
        if "upload_speed" in sensors_config:
            sens = await sensor.new_sensor(sensors_config["upload_speed"])
            cg.add(var.set_upload_speed_sensor(sens))
        if "download_speed" in sensors_config:
            sens = await sensor.new_sensor(sensors_config["download_speed"])
            cg.add(var.set_download_speed_sensor(sens))
        if "connected_devices" in sensors_config:
            sens = await sensor.new_sensor(sensors_config["connected_devices"])
            cg.add(var.set_connected_devices_sensor(sens))
        if "gateway_health" in sensors_config:
            sens = await sensor.new_sensor(sensors_config["gateway_health"])
            cg.add(var.set_gateway_health_sensor(sens))
        if "forward_throughput" in sensors_config:
            sens = await sensor.new_sensor(sensors_config["forward_throughput"])
            cg.add(var.set_forward_throughput_sensor(sens))
        if "sta_link_rate" in sensors_config:
            sens = await sensor.new_sensor(sensors_config["sta_link_rate"])
            cg.add(var.set_sta_link_rate_sensor(sens))
        if "sta_channel" in sensors_config:
            sens = await sensor.new_sensor(sensors_config["sta_channel"])
            cg.add(var.set_sta_channel_sensor(sens))

    if "text_sensors" in config:
        text_sensors_config = config["text_sensors"]
        if "client_mac_list" in text_sensors_config:
            sens = await text_sensor.new_text_sensor(text_sensors_config["client_mac_list"])
            cg.add(var.set_client_mac_list_sensor(sens))
        if "online_status" in text_sensors_config:
            sens = await text_sensor.new_text_sensor(text_sensors_config["online_status"])
            cg.add(var.set_online_status_sensor(sens))

    await cg.register_component(var, config)

    cg.add_build_flag("-DIP_NAPT=1")
    cg.add_build_flag("-DIP_NAPT_PORTMAP=1")
    cg.add_build_flag("-DLWIP_IPV4_NAPT=1")

    add_idf_sdkconfig_option("CONFIG_LWIP_IP_FORWARD", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_IPV4_NAPT", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_IPV4_NAPT_PORTMAP", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_DHCP_OPTIONS", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_DHCPS", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_DHCP_DOES_ACD_CHECK", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_DHCPS_STATIC_ENTRIES", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_DHCPS_ADD_DNS", True)
    add_idf_sdkconfig_option("CONFIG_ESP_WIFI_SOFTAP_SUPPORT", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_L2_TO_L3_COPY", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_STATS", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_MEMP_NUM_NETCONN", "12")
    add_idf_sdkconfig_option("CONFIG_LWIP_MEMP_NUM_TCP_PCB", "12")
    add_idf_sdkconfig_option("CONFIG_LWIP_TCP_SND_BUF", "8192")
    add_idf_sdkconfig_option("CONFIG_LWIP_TCP_WND", "8192")

    cg.add_build_flag("-fno-tree-switch-conversion")
