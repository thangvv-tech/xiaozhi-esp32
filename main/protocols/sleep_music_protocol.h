#ifndef _SLEEP_MUSIC_PROTOCOL_H_
#define _SLEEP_MUSIC_PROTOCOL_H_

#include <web_socket.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <memory>

#define SLEEP_MUSIC_PROTOCOL_CONNECTED_EVENT (1 << 0)

class SleepMusicProtocol {
public:
    static SleepMusicProtocol& GetInstance();
    
    bool OpenAudioChannel();
    void CloseAudioChannel();
    bool IsAudioChannelOpened() const;
    
    // 高级封装方法
    bool StartSleepMusic();  // 启动睡眠音乐（包含语音对话停止逻辑）
    bool StopSleepMusic();   // 停止睡眠音乐

private:
    SleepMusicProtocol();
    ~SleepMusicProtocol();
    
    EventGroupHandle_t event_group_handle_;
    std::unique_ptr<WebSocket> websocket_;
    bool is_connected_ = false;
    bool is_connecting_ = false; // 避免重复连接与阻塞
    uint32_t last_connect_attempt_ms_ = 0; // 重试退避
    static constexpr uint32_t CONNECT_TIMEOUT_MS = 8000; // 单次连接超时
    static constexpr uint32_t RETRY_BACKOFF_MS = 10000;  // 无网或失败后的退避
    
    // 睡眠音乐服务器配置
    static constexpr int SAMPLE_RATE = 24000;  // 24kHz
    static constexpr int CHANNELS = 2;         // 立体声
    static constexpr int FRAME_DURATION_MS = 60; // 60ms帧时长
    
    void OnAudioDataReceived(const char* data, size_t len);
    bool IsNetworkReady() const;
};

#endif
