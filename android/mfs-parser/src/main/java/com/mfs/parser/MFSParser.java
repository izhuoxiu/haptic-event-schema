package com.mfs.parser;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * MFS 解析器（核心传菜员）
 * 用法：MFSScript script = new MFSParser().parse("你的 HSF JSON 字符串");
 */
public class MFSParser {

    /**
     * 解析 HSF 文本
     * @param jsonString 完整的 HSF JSON 字符串
     * @return MFSScript 对象
     * @throws JSONException 如果 JSON 格式写错了会报错
     */
    public MFSScript parse(String jsonString) throws JSONException {
        // 1. 把字符串变成 JSON 对象
        JSONObject root = new JSONObject(jsonString);

        // 2. 校验基础信息（相当于检查外卖单是不是我家开的）
        String schema = root.optString("schema", "");
        if (!"HapticScriptFormat".equals(schema)) {
            throw new JSONException("无效的 MFS 文件：schema 不是 HapticScriptFormat");
        }

        String version = root.optString("version", "0.0");
        // 注：这里只做提示，不强制报错（向下兼容）

        // 3. 提取轨道数据 (tracks)
        JSONArray tracks = root.optJSONArray("tracks");
        if (tracks == null || tracks.length() == 0) {
            throw new JSONException("无效的 MFS 文件：tracks 数组为空");
        }

        // 4. 遍历所有轨道，把所有事件拍平到一个大列表里
        List<MFSEvent> allEvents = new ArrayList<>();
        for (int i = 0; i < tracks.length(); i++) {
            JSONObject track = tracks.getJSONObject(i);
            JSONArray events = track.optJSONArray("events");
            if (events == null) continue;

            for (int j = 0; j < events.length(); j++) {
                JSONObject evt = events.getJSONObject(j);

                // 读取必填字段
                int timeMs = evt.getInt("time_ms");       // 缺少会抛异常
                String eventRef = evt.getString("event_ref");

                // 读取选填字段，并给默认值（模仿白皮书默认值）
                int durationMs = evt.optInt("duration_ms", -1); // -1 代表持续
                double intensity = evt.optDouble("intensity", 0.5); // 默认 0.5

                // 如果脚本里写了 duration_ms，但词典里默认是 -1，我们照搬
                MFSEvent event = new MFSEvent(timeMs, eventRef, durationMs, intensity);
                allEvents.add(event);
            }
        }

        // 5. 按照时间戳从小到大排序（让震动按先后顺序排队）
        Collections.sort(allEvents, new Comparator<MFSEvent>() {
            @Override
            public int compare(MFSEvent o1, MFSEvent o2) {
                return Integer.compare(o1.getTimeMs(), o2.getTimeMs());
            }
        });

        // 6. 返回最终的脚本对象
        return new MFSScript(schema, version, allEvents);
    }

    /**
     * 简易测试方法（供你验证解析器是否正常工作）
     */
    public static void main(String[] args) {
        try {
            // 写一段测试用的 HSF 文本
            String testJson = "{\n" +
                    "  \"schema\": \"HapticScriptFormat\",\n" +
                    "  \"version\": \"0.2.2\",\n" +
                    "  \"tracks\": [\n" +
                    "    {\n" +
                    "      \"events\": [\n" +
                    "        { \"time_ms\": 0, \"event_ref\": \"F-101\", \"intensity\": 0.2 },\n" +
                    "        { \"time_ms\": 500, \"event_ref\": \"R-104\", \"intensity\": 0.8 }\n" +
                    "      ]\n" +
                    "    }\n" +
                    "  ]\n" +
                    "}";

            MFSParser parser = new MFSParser();
            MFSScript script = parser.parse(testJson);

            System.out.println("解析成功！版本：" + script.getVersion());
            System.out.println("总时长：" + script.getTotalDuration() + "ms");
            System.out.println("事件列表：");
            for (MFSEvent e : script.getAllEvents()) {
                System.out.println(e);
            }

        } catch (Exception e) {
            System.err.println("解析失败：" + e.getMessage());
        }
    }
}
