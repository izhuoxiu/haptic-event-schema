#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include "json.hpp"
#include <emscripten.h>
#include <emscripten/val>

// 简化命名
using json = nlohmann::json;

// ===================== 白皮书常量定义 =====================
// 五维优先级数值，越大优先级越高
#define PRIORITY_EMOTION    5
#define PRIORITY_MOTION     4
#define PRIORITY_FORCE      3
#define PRIORITY_SPACE      2
#define PRIORITY_RHYTHM     1
#define PRIORITY_DEFAULT    0

// 模态类型标记，用于回调区分
enum MfsModalType {
    MODAL_HAPTIC    = 1,    // 马达震动
    MODAL_RIBBON    = 2,    // 屏幕边缘彩带
    MODAL_FLASH     = 3     // 后置闪光灯
};

// 单个MFS事件结构体，对应HSF脚本event对象
struct MfsEvent {
    uint32_t time_ms;               // 触发时间戳
    uint32_t duration_ms;           // 持续时长
    std::string event_ref;          // 事件编码 F-101 / S-216
    float intensity;                // 强度 0~1
    int priority;                   // 优先级
    json ribbon_params;             // 彩带参数
    json flash_params;              // 闪光灯参数
    json trail_params;              // 轨迹参数(S-216)
};

// 单轨道结构体
struct MfsTrack {
    std::string track_id;
    int priority_override;          // 轨道自定义优先级
    std::vector<MfsEvent> events;   // 轨道事件列表
};

// 全局内核单例
class MFS_Core {
private:
    std::vector<MfsTrack> track_list;
    uint32_t last_tick_ms = 0;      // 上一次tick的视频时间
    // JS回调函数，由前端注册
    emscripten::val js_event_callback;

    // 根据event_ref获取默认优先级（白皮书五维铁律）
    int get_event_base_priority(const std::string& ref) {
        if(ref.substr(0,1) == "E") return PRIORITY_EMOTION;
        if(ref.substr(0,1) == "M") return PRIORITY_MOTION;
        if(ref.substr(0,1) == "F") return PRIORITY_FORCE;
        if(ref.substr(0,1) == "S") return PRIORITY_SPACE;
        if(ref.substr(0,1) == "R") return PRIORITY_RHYTHM;
        return PRIORITY_DEFAULT;
    }

    // 事件排序：按time_ms升序，保证时序正确
    void sort_all_events() {
        for(auto& track : track_list) {
            std::sort(track.events.begin(), track.events.end(),
                [](const MfsEvent& a, const MfsEvent& b) {
                    return a.time_ms < b.time_ms;
                });
        }
    }

    // 触发事件，调用前端JS回调分发三模态
    void dispatch_event(const MfsEvent& evt) {
        // 1. 震动模态 永远触发
        js_event_callback(MODAL_HAPTIC,
            evt.event_ref,
            evt.intensity,
            evt.duration_ms,
            json(evt).dump()
        );

        // 2. 彩带模态：存在参数才触发
        if(!evt.ribbon_params.is_null()) {
            js_event_callback(MODAL_RIBBON,
                evt.event_ref,
                evt.intensity,
                evt.duration_ms,
                evt.ribbon_params.dump()
            );
        }

        // 3. 闪光灯模态：存在参数才触发
        if(!evt.flash_params.is_null()) {
            js_event_callback(MODAL_FLASH,
                evt.event_ref,
                evt.intensity,
                evt.duration_ms,
                evt.flash_params.dump()
            );
        }
    }

public:
    // 绑定前端JS回调函数（页面初始化调用）
    void set_callback(emscripten::val cb) {
        js_event_callback = cb;
    }

    // 加载完整HSF JSON脚本（传入字符串）
    int load_hsf_script(const std::string& hsf_json_str) {
        try {
            json root = json::parse(hsf_json_str);
            // 清空旧数据
            track_list.clear();
            last_tick_ms = 0;

            // 遍历所有轨道
            for(auto& track_json : root["tracks"]) {
                MfsTrack track;
                track.track_id = track_json["id"];
                track.priority_override = track_json.contains("priority_override")
                    ? int(track_json["priority_override"]) : -1;

                // 遍历轨道内所有事件
                for(auto& evt_json : track_json["events"]) {
                    MfsEvent evt;
                    evt.time_ms = uint32_t(evt_json["time_ms"]);
                    evt.event_ref = evt_json["event_ref"];
                    evt.intensity = float(evt_json["intensity"]);
                    evt.duration_ms = evt_json.contains("duration_ms")
                        ? uint32_t(evt_json["duration_ms"]) : 0;

                    // 读取多模态配套参数
                    evt.ribbon_params = evt_json.contains("ribbon_parameters")
                        ? evt_json["ribbon_parameters"] : json();
                    evt.flash_params = evt_json.contains("led_parameters")
                        ? evt_json["led_parameters"] : json();
                    evt.trail_params = evt_json.contains("trail_parameters")
                        ? evt_json["trail_parameters"] : json();

                    // 计算事件优先级
                    int base_pri = get_event_base_priority(evt.event_ref);
                    evt.priority = (track.priority_override > 0)
                        ? track.priority_override : base_pri;

                    track.events.push_back(evt);
                }
                track_list.push_back(track);
            }

            // 全部事件按时间排序
            sort_all_events();
            return 0; // 加载成功
        } catch(...) {
            // JSON解析失败，返回错误码，自动降级
            return -1;
        }
    }

    // 核心时序tick函数：前端每帧传入当前视频毫秒
    void tick(uint32_t current_video_ms) {
        uint32_t start = last_tick_ms;
        uint32_t end = current_video_ms;
        last_tick_ms = current_video_ms;

        // 遍历所有轨道，匹配区间内事件
        for(auto& track : track_list) {
            for(auto& evt : track.events) {
                // 事件时间落在上一帧~当前帧之间，触发
                if(evt.time_ms > start && evt.time_ms <= end) {
                    dispatch_event(evt);
                }
            }
        }
    }

    // 重置内核（切换视频时调用）
    void reset() {
        track_list.clear();
        last_tick_ms = 0;
    }
};

// 全局唯一内核实例
static MFS_Core g_mfs_core;

// ===================== 导出给JS调用的全局函数（EMSCRIPTEN_KEEPALIVE不可删） =====================
// 1. 注册JS事件回调
EMSCRIPTEN_KEEPALIVE
void mfs_set_callback(emscripten::val js_cb) {
    g_mfs_core.set_callback(js_cb);
}

// 2. 加载HSF脚本字符串
EMSCRIPTEN_KEEPALIVE
int mfs_load_script(const char* json_str) {
    std::string s(json_str);
    return g_mfs_core.load_hsf_script(s);
}

// 3. 视频帧时间推送tick（核心调度入口）
EMSCRIPTEN_KEEPALIVE
void mfs_tick(uint32_t current_ms) {
    g_mfs_core.tick(current_ms);
}

// 4. 重置内核
EMSCRIPTEN_KEEPALIVE
void mfs_reset() {
    g_mfs_core.reset();
}
