import UIKit
import CoreHaptics

@available(iOS 13.0, *)
public class HESEngine {
    public static let shared = HESEngine()
    private var engine: CHHapticEngine?
    
    // 完整词典（30个词条）
    private let dict: [String: (intensity: Float, duration: TimeInterval, type: String)] = [
        // 力量感
        "F-101": (0.2, 0.015, "click"), "F-102": (0.5, 0.03, "click"),
        "F-103": (0.8, 0.08, "heavy"), "F-104": (1.0, 0.15, "burst"),
        "F-105": (0.6, 0.2, "shake"), "F-106": (1.0, 0.4, "crush"),
        // 节奏感
        "R-101": (0.4, 0.03, "click"), "R-102": (0.5, 0.06, "double"),
        "R-103": (0.6, 0.3, "pulse"), "R-104": (0.8, 0.12, "heart"),
        "R-105": (0.5, 1.0, "wave"), "R-106": (0.4, 0.5, "roll"),
        // 空间感
        "S-201": (0.9, 1.5, "approach"), "S-202": (0.9, 1.5, "retreat"),
        "S-203": (0.5, 0.3, "through"), "S-204": (0.5, 0.8, "orbit"),
        "S-205": (0.6, 0.5, "expand"), "S-206": (0.6, 0.5, "contract"),
        // 运动感
        "M-301": (0.8, 1.0, "rise"), "M-302": (0.8, 1.0, "fall"),
        "M-303": (0.6, 0.8, "accel"), "M-304": (0.6, 0.8, "decel"),
        "M-305": (0.1, 2.0, "hover"), "M-306": (0.8, 0.05, "brake"),
        // 情绪感
        "E-401": (0.5, 0.6, "tension"), "E-402": (0.7, 0.8, "oppress"),
        "E-403": (0.8, 0.5, "release"), "E-404": (1.0, 0.4, "excite"),
        "E-405": (0.15, 1.5, "calm"), "E-406": (0.5, 0.3, "weird")
    ]
    
    private init() {
        guard CHHapticEngine.capabilitiesForHardware().supportsHaptics else { return }
        do {
            engine = try CHHapticEngine()
            try engine?.start()
        } catch { print("HES引擎初始化失败") }
    }
    
    /**
     👇 开发者只需要调用这一行代码！
     例如：play(code: "F-104")  -> 手机立刻爆发式震动
     例如：play(code: "E-405")  -> 超轻柔的平静震动
     */
    public func play(code: String, intensityScale: Float = 1.0) {
        guard let engine = engine, let params = dict[code] else { return }
        
        let finalIntensity = min(1.0, params.intensity * intensityScale)
        let sharpness: Float = params.type == "click" ? 0.8 : 
                               params.type == "heavy" ? 0.3 : 0.5
        
        var events: [CHHapticEvent] = []
        let time = 0.1
        
        switch params.type {
        case "burst":   // F-104 爆发 (连续+衰减)
            let burst = CHHapticEvent(eventType: .hapticContinuous,
                parameters: [
                    .init(parameterID: .hapticIntensity, value: finalIntensity),
                    .init(parameterID: .hapticSharpness, value: 0.1)
                ], relativeTime: time, duration: 0.15)
            events.append(burst)
            
        case "heart":   // R-104 心跳 (强-弱)
            events.append(contentsOf: [
                CHHapticEvent(eventType: .hapticTransient,
                    parameters: [.init(parameterID: .hapticIntensity, value: 1.0),
                                 .init(parameterID: .hapticSharpness, value: 0.3)],
                    relativeTime: time, duration: 0.08),
                CHHapticEvent(eventType: .hapticTransient,
                    parameters: [.init(parameterID: .hapticIntensity, value: 0.4),
                                 .init(parameterID: .hapticSharpness, value: 0.5)],
                    relativeTime: time + 0.18, duration: 0.04)
            ])
            
        case "double":  // R-102 双连击
            events.append(contentsOf: [
                CHHapticEvent(eventType: .hapticTransient,
                    parameters: [.init(parameterID: .hapticIntensity, value: finalIntensity),
                                 .init(parameterID: .hapticSharpness, value: sharpness)],
                    relativeTime: time, duration: 0.03),
                CHHapticEvent(eventType: .hapticTransient,
                    parameters: [.init(parameterID: .hapticIntensity, value: finalIntensity * 0.7),
                                 .init(parameterID: .hapticSharpness, value: sharpness)],
                    relativeTime: time + 0.08, duration: 0.03)
            ])
            
        default:  // 包括 click, heavy, approach, calm 等所有其他26个词条
            let event = CHHapticEvent(eventType: .hapticTransient,
                parameters: [
                    .init(parameterID: .hapticIntensity, value: finalIntensity),
                    .init(parameterID: .hapticSharpness, value: sharpness)
                ], relativeTime: time, duration: params.duration)
            events.append(event)
        }
        
        do {
            let pattern = try CHHapticPattern(events: events, parameters: [])
            let player = try engine.makePlayer(with: pattern)
            try player.start(atTime: 0)
        } catch { print("播放失败") }
    }
}
