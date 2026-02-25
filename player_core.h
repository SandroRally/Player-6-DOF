#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#pragma comment(lib, "ws2_32.lib")
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>

static const int PACKET_SIZE = 311;
static const double MY_PI = 3.14159265358979323846;
static inline double deg2rad(double d) { return d * MY_PI / 180.0; }
static inline double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
static double perf_counter() {
    static LARGE_INTEGER freq = {}; if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart;
}

// ==================== DATA STRUCTURES ====================
struct Motion { double surge_g=0, sway_g=0, heave_g=0; };
struct Orientation { double roll=0, pitch=0, yaw=0; };
struct RawGyro { double x=0, y=0, z=0; };
struct TelemetrySample {
    double timestamp=0; Motion motion; Orientation orientation;
    double speed_kmh=0, giri=0; int cambio=0; RawGyro raw_gyro;
};
struct Settings {
    double video_offset=0, gain_surge=1, gain_sway=1, gain_heave=0.8;
    double gain_roll=1, gain_pitch=1, gain_tl=1.5;
    bool enable_surge=true, enable_sway=true, enable_heave=true;
    bool enable_roll=true, enable_pitch=true, enable_tl=true;
    bool invert_surge=false, invert_sway=false, invert_heave=false;
    bool invert_roll=false, invert_pitch=false, invert_tl=false;
    double base_smooth=0.7, deadzone=0.03, limit_g=2.0, limit_angle=10.0;
    double tl_threshold=4.0, heave_reset_threshold=0.1;
    bool use_synthetic_roll=true, use_synthetic_pitch=true, compensate_gravity=true;
    int output_freq=60;
};

// ==================== JSON PARSER ====================
class JsonParser {
    static void skipWS(const std::string& s, size_t& p) {
        while (p<s.size()&&(s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r')) p++;
    }
    static std::string parseString(const std::string& s, size_t& p) {
        skipWS(s,p); if(p>=s.size()||s[p]!='"') return ""; p++;
        std::string r; while(p<s.size()&&s[p]!='"'){if(s[p]=='\\'){p++;if(p<s.size())r+=s[p];}else r+=s[p];p++;}
        if(p<s.size())p++; return r;
    }
    static double parseNumber(const std::string& s, size_t& p) {
        skipWS(s,p); size_t st=p;
        if(p<s.size()&&(s[p]=='-'||s[p]=='+'))p++;
        while(p<s.size()&&((s[p]>='0'&&s[p]<='9')||s[p]=='.'||s[p]=='e'||s[p]=='E'||s[p]=='+'||s[p]=='-')){
            if((s[p]=='+'||s[p]=='-')&&p>st&&s[p-1]!='e'&&s[p-1]!='E')break; p++;}
        return atof(s.substr(st,p-st).c_str());
    }
    static bool parseBool(const std::string& s, size_t& p) {
        skipWS(s,p); if(s.compare(p,4,"true")==0){p+=4;return true;} if(s.compare(p,5,"false")==0){p+=5;} return false;
    }
    static void skipValue(const std::string& s, size_t& p) {
        skipWS(s,p); if(p>=s.size())return;
        if(s[p]=='"'){parseString(s,p);return;}
        if(s[p]=='{'){int d=1;p++;while(p<s.size()&&d>0){if(s[p]=='{')d++;else if(s[p]=='}')d--;else if(s[p]=='"'){parseString(s,p);continue;}p++;}return;}
        if(s[p]=='['){int d=1;p++;while(p<s.size()&&d>0){if(s[p]=='[')d++;else if(s[p]==']')d--;else if(s[p]=='"'){parseString(s,p);continue;}p++;}return;}
        while(p<s.size()&&s[p]!=','&&s[p]!='}'&&s[p]!=']'&&s[p]!=' '&&s[p]!='\n'&&s[p]!='\r')p++;
    }
    static void parseMotion(const std::string& s, size_t& p, Motion& m) {
        skipWS(s,p); if(s[p]!='{'){skipValue(s,p);return;} p++;
        while(p<s.size()){skipWS(s,p);if(s[p]=='}'){p++;break;}if(s[p]==','){p++;continue;}
        std::string k=parseString(s,p);skipWS(s,p);if(s[p]==':')p++;
        if(k=="surge_g")m.surge_g=parseNumber(s,p);else if(k=="sway_g")m.sway_g=parseNumber(s,p);
        else if(k=="heave_g")m.heave_g=parseNumber(s,p);else skipValue(s,p);}
    }
    static void parseOrientation(const std::string& s, size_t& p, Orientation& o) {
        skipWS(s,p); if(s[p]!='{'){skipValue(s,p);return;} p++;
        while(p<s.size()){skipWS(s,p);if(s[p]=='}'){p++;break;}if(s[p]==','){p++;continue;}
        std::string k=parseString(s,p);skipWS(s,p);if(s[p]==':')p++;
        if(k=="roll")o.roll=parseNumber(s,p);else if(k=="pitch")o.pitch=parseNumber(s,p);
        else if(k=="yaw")o.yaw=parseNumber(s,p);else skipValue(s,p);}
    }
    static void parseGyro(const std::string& s, size_t& p, RawGyro& g) {
        skipWS(s,p); if(s[p]!='{'){skipValue(s,p);return;} p++;
        while(p<s.size()){skipWS(s,p);if(s[p]=='}'){p++;break;}if(s[p]==','){p++;continue;}
        std::string k=parseString(s,p);skipWS(s,p);if(s[p]==':')p++;
        if(k=="x")g.x=parseNumber(s,p);else if(k=="y")g.y=parseNumber(s,p);
        else if(k=="z")g.z=parseNumber(s,p);else skipValue(s,p);}
    }
    static bool parseSample(const std::string& s, size_t& p, TelemetrySample& sam) {
        skipWS(s,p); if(s[p]!='{')return false; p++;
        while(p<s.size()){skipWS(s,p);if(s[p]=='}'){p++;break;}if(s[p]==','){p++;continue;}
        std::string k=parseString(s,p);skipWS(s,p);if(s[p]==':')p++;skipWS(s,p);
        if(k=="timestamp")sam.timestamp=parseNumber(s,p);else if(k=="motion")parseMotion(s,p,sam.motion);
        else if(k=="orientation")parseOrientation(s,p,sam.orientation);else if(k=="speed_kmh")sam.speed_kmh=parseNumber(s,p);
        else if(k=="giri")sam.giri=parseNumber(s,p);else if(k=="cambio")sam.cambio=(int)parseNumber(s,p);
        else if(k=="raw_gyro")parseGyro(s,p,sam.raw_gyro);else skipValue(s,p);}
        return true;
    }
    static bool parseSamplesArray(const std::string& s, size_t& p, std::vector<TelemetrySample>& samples) {
        skipWS(s,p); if(s[p]!='[')return false; p++;
        while(p<s.size()){skipWS(s,p);if(s[p]==']'){p++;break;}if(s[p]==','){p++;continue;}
        TelemetrySample sam; if(!parseSample(s,p,sam))return false; samples.push_back(sam);}
        return true;
    }
    static bool parseSettings(const std::string& s, size_t& p, Settings& st) {
        skipWS(s,p); if(s[p]!='{'){skipValue(s,p);return true;} p++;
        while(p<s.size()){skipWS(s,p);if(s[p]=='}'){p++;break;}if(s[p]==','){p++;continue;}
        std::string k=parseString(s,p);skipWS(s,p);if(s[p]==':')p++;skipWS(s,p);
        if(k=="video_offset")st.video_offset=parseNumber(s,p);
        else if(k=="gain_surge")st.gain_surge=parseNumber(s,p);else if(k=="gain_sway")st.gain_sway=parseNumber(s,p);
        else if(k=="gain_heave")st.gain_heave=parseNumber(s,p);else if(k=="gain_roll")st.gain_roll=parseNumber(s,p);
        else if(k=="gain_pitch")st.gain_pitch=parseNumber(s,p);else if(k=="gain_tl")st.gain_tl=parseNumber(s,p);
        else if(k=="enable_surge")st.enable_surge=parseBool(s,p);else if(k=="enable_sway")st.enable_sway=parseBool(s,p);
        else if(k=="enable_heave")st.enable_heave=parseBool(s,p);else if(k=="enable_roll")st.enable_roll=parseBool(s,p);
        else if(k=="enable_pitch")st.enable_pitch=parseBool(s,p);else if(k=="enable_tl")st.enable_tl=parseBool(s,p);
        else if(k=="invert_surge")st.invert_surge=parseBool(s,p);else if(k=="invert_sway")st.invert_sway=parseBool(s,p);
        else if(k=="invert_heave")st.invert_heave=parseBool(s,p);else if(k=="invert_roll")st.invert_roll=parseBool(s,p);
        else if(k=="invert_pitch")st.invert_pitch=parseBool(s,p);else if(k=="invert_tl")st.invert_tl=parseBool(s,p);
        else if(k=="base_smooth")st.base_smooth=parseNumber(s,p);else if(k=="deadzone")st.deadzone=parseNumber(s,p);
        else if(k=="limit_g")st.limit_g=parseNumber(s,p);else if(k=="limit_angle")st.limit_angle=parseNumber(s,p);
        else if(k=="tl_threshold")st.tl_threshold=parseNumber(s,p);
        else if(k=="heave_reset_threshold")st.heave_reset_threshold=parseNumber(s,p);
        else if(k=="use_synthetic_roll")st.use_synthetic_roll=parseBool(s,p);
        else if(k=="use_synthetic_pitch")st.use_synthetic_pitch=parseBool(s,p);
        else if(k=="compensate_gravity")st.compensate_gravity=parseBool(s,p);
        else if(k=="output_freq")st.output_freq=(int)parseNumber(s,p);
        else skipValue(s,p);}
        return true;
    }
public:
    static bool load(const std::wstring& path, std::vector<TelemetrySample>& samples, Settings& settings) {
        std::ifstream ifs(path.c_str(), std::ios::binary); if(!ifs.is_open()) return false;
        std::string content((std::istreambuf_iterator<char>(ifs)),std::istreambuf_iterator<char>()); ifs.close();
        size_t pos=0; skipWS(content,pos); if(pos>=content.size()) return false;
        if(content[pos]=='{'){pos++;
            while(pos<content.size()){skipWS(content,pos);if(content[pos]=='}')break;if(content[pos]==','){pos++;continue;}
            std::string key=parseString(content,pos);skipWS(content,pos);if(content[pos]==':')pos++;skipWS(content,pos);
            if(key=="samples")parseSamplesArray(content,pos,samples);
            else if(key=="settings")parseSettings(content,pos,settings);
            else skipValue(content,pos);}
        } else if(content[pos]=='[') parseSamplesArray(content,pos,samples);
        else return false;
        return !samples.empty();
    }
};

// ==================== VLC DYNAMIC LOADER ====================
typedef void* (*fn_vlc_new)(int, const char* const*);
typedef void  (*fn_vlc_release)(void*);
typedef void* (*fn_vlc_media_new_path)(void*, const char*);
typedef void  (*fn_vlc_media_release)(void*);
typedef void* (*fn_vlc_mp_new)(void*);
typedef void  (*fn_vlc_mp_release)(void*);
typedef void  (*fn_vlc_mp_set_media)(void*, void*);
typedef int   (*fn_vlc_mp_play)(void*);
typedef void  (*fn_vlc_mp_stop)(void*);
typedef void  (*fn_vlc_mp_pause)(void*);
typedef void  (*fn_vlc_mp_set_pause)(void*, int);
typedef void  (*fn_vlc_mp_set_time)(void*, int64_t);

struct VlcLoader {
    std::wstring found_path; // cartella dove vlc.exe e' stato trovato
    std::wstring vlc_exe;    // percorso completo di vlc.exe
    bool available = false;
    HANDLE hProcess = NULL;  // handle del processo VLC

    // Cerca VLC nel registro di Windows
    std::wstring find_vlc_in_registry() {
        const wchar_t* keys[] = {
            L"SOFTWARE\\VideoLAN\\VLC",
            L"SOFTWARE\\WOW6432Node\\VideoLAN\\VLC"
        };
        for (auto key : keys) {
            HKEY hKey;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                wchar_t path[MAX_PATH] = {};
                DWORD size = MAX_PATH * sizeof(wchar_t);
                DWORD type = REG_SZ;
                if (RegQueryValueExW(hKey, L"InstallDir", NULL, &type, (LPBYTE)path, &size) == ERROR_SUCCESS) {
                    RegCloseKey(hKey);
                    return std::wstring(path);
                }
                RegCloseKey(hKey);
            }
        }
        return L"";
    }

    bool check_vlc_exe(const std::wstring& dir) {
        std::wstring exe = dir + L"\\vlc.exe";
        DWORD attr = GetFileAttributesW(exe.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES) {
            found_path = dir;
            vlc_exe = exe;
            return true;
        }
        return false;
    }

    bool init() {
        // 1. Registro
        std::wstring reg = find_vlc_in_registry();
        if (!reg.empty() && check_vlc_exe(reg)) { available = true; return true; }
        // 2. Percorsi standard
        if (check_vlc_exe(L"C:\\Program Files\\VideoLAN\\VLC")) { available = true; return true; }
        if (check_vlc_exe(L"C:\\Program Files (x86)\\VideoLAN\\VLC")) { available = true; return true; }
        return false;
    }

    // Lancia VLC con un video
    void play_video(const std::wstring& video_path, double start_time_sec = 0, bool start_paused = false) {
        if (!available || video_path.empty()) return;
        stop(); // chiudi VLC precedente
        Sleep(100);

        // Forza backslash
        std::wstring p = video_path;
        for (auto& c : p) if (c == '/') c = '\\';

        // Costruisci gli argomenti:
        std::wstring args = L"--no-video-title-show";
        if (start_time_sec > 0) {
            wchar_t buf[64]; swprintf(buf, 64, L" --start-time=%.3f", start_time_sec);
            args += buf;
        }
        if (start_paused) args += L" --start-paused";
        
        args += L" -- \"" + p + L"\"";

        SHELLEXECUTEINFOW sei = {};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"open";
        sei.lpFile = vlc_exe.c_str();
        sei.lpParameters = args.c_str();
        sei.nShow = SW_SHOW;
        if (ShellExecuteExW(&sei)) {
            hProcess = sei.hProcess;
        }
    }

    // Invia il comando di pausa/play a VLC (simula il tasto spazio)
    void toggle_pause() {
        if (!hProcess) return;
        DWORD pid = GetProcessId(hProcess);
        
        struct EnumData { DWORD pid; HWND hwnd; };
        EnumData data = { pid, NULL };

        // Cerca la finestra principale di VLC per questo processo
        EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
            EnumData* ed = (EnumData*)lp;
            DWORD p; GetWindowThreadProcessId(hwnd, &p);
            if (p == ed->pid) {
                wchar_t className[256];
                GetClassNameW(hwnd, className, 256);
                // La classe della finestra video di VLC inizia solitamente con "VLC"
                if (wcsstr(className, L"VLC") != NULL) {
                    ed->hwnd = hwnd;
                    return FALSE; 
                }
            }
            return TRUE;
        }, (LPARAM)&data);

        if (data.hwnd) {
            // Invia tasto Spazio per toggle pausa
            PostMessageW(data.hwnd, WM_KEYDOWN, VK_SPACE, 0);
            PostMessageW(data.hwnd, WM_KEYUP, VK_SPACE, 0);
        }
    }

    // Ferma VLC
    void stop() {
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
            hProcess = NULL;
        }
    }

    void cleanup() { stop(); }
};

// ==================== DEOVR CLIENT ====================
class DeoVRClient {
    SOCKET sock = INVALID_SOCKET;
    std::atomic<bool> connected{false};
    std::atomic<bool> stop_ping{false};
    std::thread ping_thread;
    double last_send = 0;
    void send_raw(const char* json_str) {
        if(!connected.load()||sock==INVALID_SOCKET) return;
        if(json_str==NULL) { uint32_t z=0; send(sock,(char*)&z,4,0); }
        else {
            uint32_t len=(uint32_t)strlen(json_str);
            send(sock,(char*)&len,4,0); send(sock,json_str,(int)len,0);
        }
        last_send = perf_counter();
    }
    void ping_loop() {
        while(!stop_ping.load()&&connected.load()){
            if(perf_counter()-last_send>0.8) send_raw(NULL);
            Sleep(100);
        }
    }
public:
    bool video_playing = false;
    bool connect(const char* host="127.0.0.1", int port=23554) {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(sock==INVALID_SOCKET) return false;
        DWORD timeout=2000; setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,(char*)&timeout,sizeof(timeout));
        setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));
        sockaddr_in addr={}; addr.sin_family=AF_INET; addr.sin_port=htons((u_short)port);
        inet_pton(AF_INET, host, &addr.sin_addr);
        if(::connect(sock,(sockaddr*)&addr,sizeof(addr))!=0){closesocket(sock);sock=INVALID_SOCKET;return false;}
        u_long mode=1; ioctlsocket(sock,FIONBIO,&mode);
        connected=true; stop_ping=false; last_send=perf_counter();
        ping_thread = std::thread(&DeoVRClient::ping_loop, this);
        ping_thread.detach();
        return true;
    }
    void disconnect() {
        stop_ping=true; connected=false;
        if(sock!=INVALID_SOCKET){closesocket(sock);sock=INVALID_SOCKET;}
    }
    bool is_connected() { if(!connected.load()) return connect(); return true; }
    void load(const std::wstring& path) {
        // Convert wstring (UTF-16) to UTF-8 for JSON
        int size = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, NULL, 0, NULL, NULL);
        std::string p_utf8(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &p_utf8[0], size, NULL, NULL);
        if(!p_utf8.empty() && p_utf8.back() == '\0') p_utf8.pop_back();

        for(auto&c:p_utf8)if(c=='\\')c='/';
        char buf[2048]; snprintf(buf,sizeof(buf),"{\"path\":\"%s\"}",p_utf8.c_str()); send_raw(buf);
    }
    void play_cmd() {
        video_playing = true;
        for (int i=0; i<3; ++i) { send_raw("{\"playerState\":0}"); Sleep(50); }
    }
    void pause_cmd() {
        video_playing = false;
        for (int i=0; i<3; ++i) { send_raw("{\"playerState\":1}"); Sleep(50); }
    }
    void rewind_and_pause() {
        video_playing = false;
        send_raw("{\"playerState\":1,\"currentTime\":0.0}");
    }
    void play_from(double t) {
        video_playing = true;
        char b[128]; snprintf(b, 128, "{\"playerState\":0,\"currentTime\":%.3f}", t);
        for (int i=0; i<3; ++i) { send_raw(b); Sleep(50); }
    }
    void seek(double t) { char b[128]; snprintf(b,128,"{\"currentTime\":%.3f}",t); send_raw(b); }
    void stop_cmd(const std::wstring& path) {
        video_playing = false;
        if (path.empty()) {
            rewind_and_pause();
            return;
        }
        int size = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, NULL, 0, NULL, NULL);
        std::string p_utf8(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &p_utf8[0], size, NULL, NULL);
        if(!p_utf8.empty() && p_utf8.back() == '\0') p_utf8.pop_back();

        for(auto&c:p_utf8)if(c=='\\')c='/';
        char b[2048]; snprintf(b,sizeof(b),"{\"path\":\"%s\",\"playerState\":1,\"currentTime\":0.0}",p_utf8.c_str());
        send_raw(b);
    }
};

// ==================== MOTION PLAYER ====================
class MotionPlayer {
public:
    std::string udp_ip="127.0.0.1"; int udp_port=5300;
    Settings settings;
    std::vector<TelemetrySample> samples;
    int detected_freq=60;
    std::atomic<bool> is_playing{false}, is_paused{false};
    std::atomic<int> packets_sent{0};
    std::atomic<double> live_offset_sec{0.0};
    std::unordered_map<std::string,double> prev_values, prev_raw, adaptive_ref;
    std::deque<double> yaw_history;
    bool first_sample=true; double ref_roll=0, ref_pitch=0;
    double heave_velocity=0, last_timestamp=-1, prev_heave=0;
    VlcLoader vlc;
    DeoVRClient deovr;
    std::string player_mode = "VLC"; // "VLC" o "DEOVR"
    std::wstring video_path;
    // Callback per aggiornare la GUI
    std::function<void(const char*)> on_status;

    bool load_file(const std::wstring& path) {
        samples.clear(); settings=Settings();
        if(!JsonParser::load(path, samples, settings)) return false;
        if(samples.size()>=2){
            double s=(samples[0].timestamp>1000)?1000.0:1.0;
            double dt=(samples[1].timestamp-samples[0].timestamp)/s;
            if(dt>0){detected_freq=(int)(1.0/dt+0.5);settings.output_freq=detected_freq;}
        }
        return true;
    }
    double get_duration() {
        if(samples.empty()) return 0;
        double s=(samples[0].timestamp>1000)?1000.0:1.0;
        return (samples.back().timestamp-samples.front().timestamp)/s;
    }
    double limit_rate(double cur, const std::string& k, double mc) {
        auto it=prev_raw.find(k); if(it==prev_raw.end()){prev_raw[k]=cur;return cur;}
        double d=cur-it->second; if(fabs(d)>mc) d=d>0?mc:-mc;
        double o=it->second+d; prev_raw[k]=o; return o;
    }
    double smart_filter(double cur, const std::string& k, double sf, double dz) {
        auto it=prev_values.find(k); if(it==prev_values.end()){prev_values[k]=cur;return cur;}
        double d=cur-it->second; if(fabs(d)<dz) return it->second;
        double es=fabs(d)>0.3?0.2:sf;
        double sm=it->second*es+cur*(1.0-es); prev_values[k]=sm; return sm;
    }
    double adaptive_orientation(double cur, const std::string& k) {
        auto it=adaptive_ref.find(k); if(it==adaptive_ref.end()){adaptive_ref[k]=cur;return 0.0;}
        it->second=it->second*0.995+cur*0.005; return cur-it->second;
    }
    void create_packet(uint8_t* pkt, double t, double spd, double su, double sw, double he,
                       double ro, double pi, double ya, double gx, double gy, double gz,
                       double hv, double rpm, int gear) {
        if(!settings.enable_surge)su=0;else su*=settings.invert_surge?-1:1;
        if(!settings.enable_sway)sw=0;else sw*=settings.invert_sway?-1:1;
        if(!settings.enable_heave)he=0;else he*=settings.invert_heave?-1:1;
        if(!settings.enable_roll)ro=0;else ro*=settings.invert_roll?-1:1;
        if(!settings.enable_pitch)pi=0;else pi*=settings.invert_pitch?-1:1;
        double ld=settings.limit_angle, lg=settings.limit_g;
        ro=clampd(ro,-ld,ld);pi=clampd(pi,-ld,ld);
        su=clampd(su,-lg,lg);sw=clampd(sw,-lg,lg);he=clampd(he,-lg,lg);
        double vx=0,vz=spd;
        if(settings.enable_tl){
            yaw_history.push_back(ya);if(yaw_history.size()>20)yaw_history.pop_front();
            double avg=0;for(double v:yaw_history)avg+=v;avg/=(double)yaw_history.size();
            double dr=ya-avg,th=settings.tl_threshold;
            if(fabs(dr)<th)dr=0;else dr=dr>0?(dr-th):(dr+th);
            double dv=dr*settings.gain_tl;if(settings.invert_tl)dv=-dv;
            double rd=deg2rad(dv);vx=spd*sin(rd);vz=spd*cos(rd);
        }
        memset(pkt,0,PACKET_SIZE); const double G=9.81;
        auto pi32=[&](int o,int32_t v){memcpy(pkt+o,&v,4);};
        auto pu32=[&](int o,uint32_t v){memcpy(pkt+o,&v,4);};
        auto pf32=[&](int o,float v){memcpy(pkt+o,&v,4);};
        pi32(0,1); pu32(4,(uint32_t)(t*1000));
        pf32(16,(float)rpm);
        pf32(20,(float)(-sw*G));pf32(24,(float)(he*G));pf32(28,(float)(su*G));
        pf32(32,(float)vx);pf32(36,(float)hv);pf32(40,(float)vz);
        pf32(44,(float)gx);pf32(48,(float)gz);pf32(52,(float)gy);
        pf32(56,0.0f);pf32(60,(float)deg2rad(pi));pf32(64,(float)deg2rad(ro));
        pf32(68,0.5f);pf32(72,0.5f);pf32(76,0.5f);pf32(80,0.5f);
        pi32(212,1); pf32(244,(float)spd);
        pkt[307]=(uint8_t)gear;
    }
    void send_up_command() {
        SOCKET s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); if(s==INVALID_SOCKET)return;
        sockaddr_in a={};a.sin_family=AF_INET;a.sin_port=htons((u_short)udp_port);
        inet_pton(AF_INET,udp_ip.c_str(),&a.sin_addr);
        uint8_t pkt[PACKET_SIZE]; double st=perf_counter();
        while(perf_counter()-st<2.0){double n=perf_counter()-st;double h=sin(n*3)*0.3;
        create_packet(pkt,n,0,0,0,h,0,0,0,0,0,0,0,0,0);
        sendto(s,(char*)pkt,PACKET_SIZE,0,(sockaddr*)&a,sizeof(a));Sleep(16);}
        closesocket(s);
    }
    void reset_state() {
        prev_values.clear();prev_raw.clear();adaptive_ref.clear();yaw_history.clear();
        first_sample=true;ref_roll=0;ref_pitch=0;heave_velocity=0;last_timestamp=-1;prev_heave=0;packets_sent=0;
    }
    void play_video() {
        double offset = settings.video_offset;
        if(player_mode=="VLC" && vlc.available) {
            if (video_path.empty()) return;
            if(offset<0) {
            } else if(offset>0) {
                vlc.play_video(video_path, offset, false);
            } else {
                vlc.play_video(video_path, 0, false);
            }
        } else if(player_mode=="DEOVR_LOCAL") {
            if (video_path.empty()) return;
            if(!deovr.is_connected()) { if(on_status) on_status("DeoVR non connesso!"); return; }
            deovr.load(video_path);
        } else if(player_mode=="DEOVR_WEB") {
            if(!deovr.is_connected()) { if(on_status) on_status("DeoVR non connesso!"); return; }
            deovr.rewind_and_pause();
            // Il video e' gia' nel browser, non serve load_time.
            // Solo un piccolo ritardo per lasciare processare il rewind.
            std::thread([this, offset]() {
                if (offset < 0) {
                    Sleep(100 + (int)(fabs(offset) * 1000));
                } else {
                    Sleep(100);
                }
                if (is_playing.load()) deovr.play_from(0);
            }).detach();
        }
    }
    void launch_vlc_delayed() {
        if(vlc.available && !video_path.empty())
            vlc.play_video(video_path, 0, false);
    }
    void stop_video() {
        if(player_mode=="VLC"&&vlc.available) vlc.stop();
        else if(player_mode=="DEOVR_LOCAL") deovr.stop_cmd(video_path);
        else if(player_mode=="DEOVR_WEB") deovr.rewind_and_pause();
    }
    void playback_thread_func() {
        SOCKET sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(sock==INVALID_SOCKET){is_playing=false;return;}
        sockaddr_in addr={};addr.sin_family=AF_INET;addr.sin_port=htons((u_short)udp_port);
        inet_pton(AF_INET,udp_ip.c_str(),&addr.sin_addr);
        double start_ts=samples.front().timestamp, end_ts=samples.back().timestamp;
        double ts_scale=(start_ts>1000)?1000.0:1.0;
        int ofreq=settings.output_freq; double oint=1.0/ofreq;
        double offset=settings.video_offset;
        double start_delay = (offset>0)?offset:0;
        // Per DeoVR (Local e Web): offset dai settings + correzione base di -0.4s
        if(player_mode=="DEOVR_LOCAL" || player_mode=="DEOVR_WEB") {
            double deovr_off = offset + 0.4;
            if(deovr_off<0) start_delay=0;
            else start_delay=deovr_off;
        }
        double gs=settings.gain_surge,gw=settings.gain_sway,gh=settings.gain_heave;
        double gr=settings.gain_roll,gp=settings.gain_pitch;
        double sm=settings.base_smooth,dz=settings.deadzone;
        bool usr=settings.use_synthetic_roll,usp=settings.use_synthetic_pitch;
        bool cg=settings.compensate_gravity; double ht=settings.heave_reset_threshold;
        int cnt=(int)samples.size(),idx=0;
        double tdur=(end_ts-start_ts)/ts_scale;
        int tframes=(int)(tdur*ofreq);
        double rstart=perf_counter()+start_delay;
        uint8_t pkt[PACKET_SIZE];
        for(int frame=0;frame<tframes;frame++){
            if(!is_playing.load())break;
            if(is_paused.load()){double ps=perf_counter();while(is_paused.load()&&is_playing.load())Sleep(10);rstart+=perf_counter()-ps;}
            double fe=frame*oint, trt=rstart+fe;
            
            double live_off=live_offset_sec.load();
            double vts=start_ts+(fe+live_off)*ts_scale;
            if(vts<start_ts) vts=start_ts;
            else if(vts>end_ts) vts=end_ts;
            
            while(perf_counter()<trt){double r=trt-perf_counter();if(r>0.002)Sleep((DWORD)(r*500));}
            if(!is_playing.load())break;
            
            while(idx<cnt-1&&samples[idx+1].timestamp<=vts)idx++;
            while(idx>0&&samples[idx].timestamp>vts)idx--;
            const auto&s1=samples[idx]; const auto&s2=(idx<cnt-1)?samples[idx+1]:s1;
            double fac=0; if(s2.timestamp>s1.timestamp) fac=(vts-s1.timestamp)/(s2.timestamp-s1.timestamp);
            double rsu=s1.motion.surge_g+(s2.motion.surge_g-s1.motion.surge_g)*fac;
            double rsw=s1.motion.sway_g+(s2.motion.sway_g-s1.motion.sway_g)*fac;
            double rhe=s1.motion.heave_g+(s2.motion.heave_g-s1.motion.heave_g)*fac;
            if(last_timestamp>=0){if(prev_heave*rhe<0&&fabs(rhe)>ht)heave_velocity=0;
                double dt=(vts-last_timestamp)/ts_scale;heave_velocity+=rhe*9.81*dt;heave_velocity=clampd(heave_velocity,-2.5,2.5);
            }else heave_velocity=0;
            last_timestamp=vts;prev_heave=rhe;
            double rro=s1.orientation.roll+(s2.orientation.roll-s1.orientation.roll)*fac;
            double rpi=s1.orientation.pitch+(s2.orientation.pitch-s1.orientation.pitch)*fac;
            double rya=s1.orientation.yaw+(s2.orientation.yaw-s1.orientation.yaw)*fac;
            double igx=s1.raw_gyro.x+(s2.raw_gyro.x-s1.raw_gyro.x)*fac;
            double igy=s1.raw_gyro.y+(s2.raw_gyro.y-s1.raw_gyro.y)*fac;
            double igz=s1.raw_gyro.z+(s2.raw_gyro.z-s1.raw_gyro.z)*fac;
            double spd=s1.speed_kmh+(s2.speed_kmh-s1.speed_kmh)*fac;
            double rpm=s1.giri+(s2.giri-s1.giri)*fac;
            int gear=s1.cambio;
            if(first_sample){ref_roll=rro;ref_pitch=rpi;first_sample=false;}
            double ro_out,pi_out;
            if(usr)ro_out=-rsw*5.0;else ro_out=adaptive_orientation(rro,"roll");
            if(usp)pi_out=-rsu*5.0;else pi_out=adaptive_orientation(rpi,"pitch");
            if(cg&&fabs(ro_out)<45){rsu-=sin(deg2rad(pi_out));rsw-=-sin(deg2rad(ro_out))*cos(deg2rad(pi_out));}
            rsu=limit_rate(rsu,"surge",0.5);rsw=limit_rate(rsw,"sway",0.5);
            double osu=smart_filter(rsu*gs,"surge",sm,dz);
            double osw=smart_filter(rsw*gw,"sway",sm,dz);
            double ohe=rhe*gh;
            ro_out=smart_filter(ro_out*gr,"roll",sm,dz*0.5);
            pi_out=smart_filter(pi_out*gp,"pitch",sm,dz*0.5);
            create_packet(pkt,fe,spd/3.6,osu,osw,ohe,ro_out,pi_out,rya,igx,igy,igz,heave_velocity,rpm,gear);
            sendto(sock,(char*)pkt,PACKET_SIZE,0,(sockaddr*)&addr,sizeof(addr));
            packets_sent++;
            if(frame%(ofreq*5)==0&&frame>0&&on_status){
                char buf[128]; snprintf(buf,128,"Playing... %.0f%%  RPM:%.0f  Gear:%d  Pkts:%d",
                    100.0*frame/tframes, rpm, gear, packets_sent.load());
                on_status(buf);
            }
        }
        closesocket(sock); is_playing=false;
        if(on_status) on_status("Playback terminato");
    }
    void play() {
        if(samples.empty()||is_playing.load()) return;
        reset_state(); is_playing=true; is_paused=false;
        play_video();
        std::thread t(&MotionPlayer::playback_thread_func, this); t.detach();
    }
    void stop() { is_playing=false; is_paused=false; stop_video(); }
    void toggle_pause() {
        if(!is_playing.load()) return;
        is_paused = !is_paused.load();
        if(player_mode == "VLC") vlc.toggle_pause();
        else if(player_mode == "DEOVR_LOCAL" || player_mode == "DEOVR_WEB") {
            if(is_paused.load()) deovr.pause_cmd();
            else deovr.play_cmd();
        }
    }
};
