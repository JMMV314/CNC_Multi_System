// mqtt_driver.cpp
#include "mqtt_driver.h"
#include "Logging.h"
#include "esp_log.h"
#include "esp_err.h"
#include "mqtt_client.h"
#include "Machine/Axes.h"
#include "System.h"
#include "Job.h"
#include "MotionEvents.h"
#include "Config.h"
#include <esp32-hal.h>
#include <WiFi.h>
#include <cmath>
#include <string>
#include <cstdio>
#include <cstring>

using namespace Machine;

MQTTDriver::MQTTDriver(const char* name)
    : Module(name)
{
    // Defaults
    host = "mqtt://test.mosquitto.org";  // test broker
    port = 1883;
    topic_base = "fluid/axis/";

    _last_pos[0] = _last_pos[1] = _last_pos[2] = 0.0f;
}

// Configuración (si quieres que luego sea editable por archivo config)
void MQTTDriver::group(Configuration::HandlerBase& handler) {
    handler.item("host", host);
    handler.item("port", port);
    handler.item("topic_base", topic_base);
}

// Inicialización MQTT
void MQTTDriver::init() {
    esp_mqtt_client_config_t cfg = {};

    // Prepare host/uri: accept either full URI (mqtt://...) or plain hostname.
    std::string s = host;
    // Trim leading/trailing spaces
    while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
    // Defensive: strip accidental leading ':' characters
    while (!s.empty() && s.front() == ':') s.erase(s.begin());

    // store parsed strings in members so c_str() remains valid after this function returns
    _parsed_uri.clear();
    _parsed_host.clear();

    if (s.rfind("mqtt://", 0) == 0 || s.rfind("mqtts://", 0) == 0 || s.rfind("tcp://", 0) == 0) {
        _parsed_uri = s;
        cfg.uri = _parsed_uri.c_str();
    } else {
        _parsed_host = s;
        cfg.host = _parsed_host.c_str();
        cfg.port = port;
    }

    client = esp_mqtt_client_init(&cfg);
    if (!client) {
        log_error("MQTTDriver: failed to init mqtt client");
        return;
    }
    // Defer starting the client until network is up. Starting the esp mqtt client
    // before the TCP/IP stack is initialized may cause lwIP asserts (Invalid mbox).
    log_info("MQTTDriver initialized (client created). Start will occur when WiFi is connected: host=" << host << " port=" << port);
}

// De-inicialización
void MQTTDriver::deinit() {
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = nullptr;
    }
}

// Publica la posición de un eje
void MQTTDriver::publish_axis(size_t axis_index, float position) {
    // Non-blocking: enqueue publish request into ring buffer. Fast path only.
    if (axis_index >= Axes::_numberAxis) return;

    char topic[48];
    snprintf(topic, sizeof(topic), "%s%c", topic_base.c_str(), Axes::axisName(axis_index));

    // Determine state: busy when a Job is active (file/route execution), otherwise free
    const char* state = Job::active() ? "busy" : "free";

    char payload[128];
    snprintf(payload, sizeof(payload), "{\"axis\":\"%c\",\"pos\":%.4f,\"state\":\"%s\"}", Axes::axisName(axis_index), position, state);

    enqueue_publish(topic, payload);
}

// Enqueue a publish message into the ring buffer. Overwrite oldest entry if full.
void MQTTDriver::enqueue_publish(const char* topic, const char* payload) {
    // copy into head entry
    PubEntry &e = _pub_queue[_pub_head];
    size_t tmax = sizeof(e.topic);
    strncpy(e.topic, topic, tmax - 1);
    e.topic[tmax - 1] = '\0';
    size_t plen = strlen(payload);
    size_t pmax = sizeof(e.payload);
    if (plen >= pmax) plen = pmax - 1;
    memcpy(e.payload, payload, plen);
    e.payload[plen] = '\0';
    e.payload_len = (uint16_t)plen;

    // advance head
    _pub_head = (_pub_head + 1) % PUB_QUEUE_SIZE;
    if (_pub_count < PUB_QUEUE_SIZE) {
        ++_pub_count;
    } else {
        // buffer full: advance tail to overwrite oldest (keep newest data)
        _pub_tail = (_pub_tail + 1) % PUB_QUEUE_SIZE;
        log_debug("MQTTDriver: pub queue full, overwriting oldest entry");
    }
}

// Flush one publish from the queue (returns true if flushed)
bool MQTTDriver::flush_one_publish() {
    if (_pub_count == 0) return false;
    if (!client) return false;

    PubEntry &e = _pub_queue[_pub_tail];
    // publish (non-blocking as esp client queues internally)
    esp_mqtt_client_publish(client, e.topic, e.payload, 0, 1, 0);

    // advance tail
    _pub_tail = (_pub_tail + 1) % PUB_QUEUE_SIZE;
    --_pub_count;
    return true;
}

// Polling para detectar cambios y publicar
void MQTTDriver::poll() {
    if (!client) return;
    log_debug("MQTTDriver: poll() called");

    // If the MQTT client was created but not started, only start it after WiFi is connected.
    if (! _started) {
        if (WiFi.isConnected()) {
            esp_mqtt_client_start(client);
            _started = true;
            log_info("MQTTDriver: client started after WiFi connected");
        } else {
            // Not connected yet; skip publishing
            log_debug("MQTTDriver: WiFi not connected yet, skipping publish");
            return;
        }
    }

    // Flush a few queued publishes per poll to avoid long blocking
    for (unsigned i = 0; i < _max_flush_per_poll; ++i) {
        if (!flush_one_publish()) break;
    }

    // Consume any motion submission events (posted by the planner when a move is queued)
    // and enqueue a single combined JSON publish for the submitted target(s).
    float ev_target[ MAX_N_AXIS ];
    size_t ev_n = 0;
    while (motion_event_pop(ev_target, ev_n)) {
        char topic[64];
        snprintf(topic, sizeof(topic), "%smove", topic_base.c_str());

        // Build JSON payload: {"state":"busy","targets":{"X":12.34,"Y":...}}
        char payload[256];
        int off = snprintf(payload, sizeof(payload), "{\"state\":\"busy\",\"targets\":{");
        for (size_t i = 0; i < ev_n; ++i) {
            char axis = Axes::axisName((int)i);
            int n = snprintf(payload + off, sizeof(payload) - off, "\"%c\":%.4f", axis, ev_target[i]);
            if (n < 0) break;
            off += n;
            if (i + 1 < ev_n) {
                if (off < (int)sizeof(payload) - 1) payload[off++] = ',';
            }
        }
        if (off < (int)sizeof(payload) - 1) {
            payload[off++] = '}';
            payload[off++] = '}';
            payload[off] = '\0';
        } else {
            // fallback to small payload
            snprintf(payload, sizeof(payload), "{\"state\":\"busy\"}");
        }

        enqueue_publish(topic, payload);
    }

    uint32_t now = millis();
    if (now - _last_publish_ms < _publish_interval_ms) return;
    _last_publish_ms = now;

    auto mpos = get_mpos();
    if (!mpos) return;

    int maxAxes = std::min((int)Axes::_numberAxis, 3);

    // First pass: detect changes and mark axes as moving
    for (int i = 0; i < maxAxes; ++i) {
        float current = mpos[i];
        float delta = fabs(current - _last_pos[i]);

        if (delta > _deadband_mm) {
            // position changed -> mark moving and remember when
            _last_pos[i] = current;
            _last_change_ms[i] = now;
            if (!_moving[i]) {
                _moving[i] = true;
                _axes_moved_mask |= (1u << i);
                _move_active = true;
                log_debug("MQTTDriver: axis " << Axes::axisName(i) << " started moving");
            }
        }
    }

    // Second pass: check for idle timeouts and update moving flags
    bool any_still_moving = false;
    for (int i = 0; i < maxAxes; ++i) {
        if (_moving[i]) {
            if ((now - _last_change_ms[i]) >= _idle_timeout_ms) {
                // axis became idle
                _moving[i] = false;
                log_debug("MQTTDriver: axis " << Axes::axisName(i) << " idle after " << (now - _last_change_ms[i]) << " ms");
            } else {
                any_still_moving = true;
            }
        }
    }

    // If a group move was active and now no axes are moving, publish final positions for all axes involved
    if (_move_active && !any_still_moving) {
        char _msgbuf[128];
        snprintf(_msgbuf, sizeof(_msgbuf), "MQTTDriver: group move finished (mask=0x%X). publishing final positions for all axes", _axes_moved_mask);
        log_info(_msgbuf);
        // publish final positions for the axes (or all axes up to maxAxes)
        for (int i = 0; i < maxAxes; ++i) {
            float final_pos = mpos[i];
            publish_axis(i, final_pos);
        }
        // reset group state
        _move_active = false;
        _axes_moved_mask = 0;
    }
}

// Module registration
namespace {
    ModuleFactory::InstanceBuilder<MQTTDriver> registration("mqtt", true);
}
