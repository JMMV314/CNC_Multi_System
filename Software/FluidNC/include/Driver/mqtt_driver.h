#pragma once

#include <string>
#include <cstdint>

#include "src/Configuration/Configurable.h"
#include "src/Module.h"
#include "mqtt_client.h"

class MQTTDriver : public Module, public Configuration::Configurable {
public:
    MQTTDriver(const char* name);

    void group(Configuration::HandlerBase& handler) override;
    void init() override;
    void deinit() override;
    void poll();

private:
    // config
    std::string host;
    int         port = 1883;
    std::string topic_base;

    // mqtt
    esp_mqtt_client_handle_t client = nullptr;
    bool _started = false;

    // publish tuning
    float _last_pos[3] = {0.0f, 0.0f, 0.0f};   // X, Y, Z
    uint32_t _last_publish_ms = 0;
    uint32_t _publish_interval_ms = 50;        // publish at most every 50 ms
    float    _deadband_mm = 0.0001f;           // publish if change > deadband
    // parsed strings to keep lifetime for c_str()
    std::string _parsed_uri;
    std::string _parsed_host;

    // movement detection: publish after axis is idle
    bool _moving[3] = {false, false, false};
    uint32_t _last_change_ms[3] = {0,0,0};
    uint32_t _idle_timeout_ms = 200;
    // group-move tracking
    bool _move_active = false;
    uint32_t _axes_moved_mask = 0;
    // Non-blocking publish queue (ring buffer)
    static constexpr size_t PUB_QUEUE_SIZE = 32;
    static constexpr size_t PUB_TOPIC_MAX = 48;
    static constexpr size_t PUB_PAYLOAD_MAX = 128;

    struct PubEntry {
        char topic[PUB_TOPIC_MAX];
        char payload[PUB_PAYLOAD_MAX];
        uint16_t payload_len;
    };

    PubEntry _pub_queue[PUB_QUEUE_SIZE];
    uint16_t _pub_head = 0; // next write
    uint16_t _pub_tail = 0; // next read
    uint16_t _pub_count = 0;

    // enqueue and flush helpers
    void enqueue_publish(const char* topic, const char* payload);
    bool flush_one_publish();
    unsigned _max_flush_per_poll = 4;

    // helpers
    void publish_axis(size_t axis_index, float position);
};
