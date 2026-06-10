/**
 * @file web_service.c
 * @brief 嵌入式 Web 服务实现
 *
 * 自实现轻量级 HTTP/1.0 服务器:
 *   - epoll 单线程多路复用
 *   - 支持 GET / POST
 *   - 内嵌 HTML 页面
 *   - REST API: /api/status, /api/led, /api/config, /api/command
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "web_service.h"
#include "log_service.h"
#include "config_service.h"
#include "device_service.h"
#include "hal_led.h"
#include "client_manager.h"
#include "cmd_parser.h"
#include "event_manager.h"

/* ============================================================
 * 运行状态
 * ============================================================ */

static volatile int g_web_running = 0;

/* ============================================================
 * 内嵌 HTML 页面 (C 字符串字面量,直接编入二进制)
 * ============================================================ */

static const char WEB_INDEX_HTML[] =
"<!DOCTYPE html>"
"<html lang=zh-CN><head><meta charset=UTF-8>"
"<meta name=viewport content=\"width=device-width,initial-scale=1\">"
"<title>Gate_orangepi 管理</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
"background:linear-gradient(135deg,#1e3c72 0%,#2a5298 100%);"
"min-height:100vh;padding:20px;color:#333}"
".container{max-width:900px;margin:0 auto}"
"h1{color:#fff;text-align:center;margin-bottom:20px;font-size:28px;"
"text-shadow:0 2px 4px rgba(0,0,0,.3)}"
".card{background:#fff;border-radius:12px;padding:20px;margin-bottom:16px;"
"box-shadow:0 4px 12px rgba(0,0,0,.15)}"
".card h2{font-size:18px;color:#1e3c72;margin-bottom:14px;"
"border-bottom:2px solid #eef;padding-bottom:8px}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px}"
".stat{background:#f8f9ff;padding:14px;border-radius:8px;text-align:center}"
".stat .label{font-size:12px;color:#888;margin-bottom:4px}"
".stat .value{font-size:22px;font-weight:600;color:#1e3c72}"
".btn{display:inline-block;padding:10px 20px;border:none;border-radius:6px;"
"cursor:pointer;font-size:14px;font-weight:500;transition:all .2s}"
".btn-primary{background:#1e3c72;color:#fff}"
".btn-primary:hover{background:#2a5298}"
".btn-success{background:#28a745;color:#fff}"
".btn-danger{background:#dc3545;color:#fff}"
".btn:disabled{opacity:.5;cursor:not-allowed}"
".led-indicator{display:inline-block;width:14px;height:14px;border-radius:50%;"
"margin-right:8px;vertical-align:middle;box-shadow:0 0 8px currentColor}"
".led-on{background:#28a745;color:#28a745}"
".led-off{background:#6c757d;color:#6c757d}"
"input,select{padding:8px 12px;border:1px solid #ddd;border-radius:6px;"
"font-size:14px;width:100%;margin-top:4px}"
"label{display:block;margin:10px 0;font-size:13px;color:#666}"
".console{background:#1e1e1e;color:#0f0;padding:14px;border-radius:6px;"
"font-family:'Courier New',monospace;font-size:13px;min-height:120px;"
"max-height:240px;overflow-y:auto;margin-top:10px}"
".toast{position:fixed;top:20px;right:20px;background:#28a745;color:#fff;"
"padding:12px 20px;border-radius:6px;box-shadow:0 4px 12px rgba(0,0,0,.2);"
"transform:translateX(400px);transition:transform .3s;z-index:1000}"
".toast.show{transform:translateX(0)}"
".toast.error{background:#dc3545}"
".row{display:flex;gap:8px;align-items:center;margin-top:8px}"
".row input{flex:1}"
"</style></head><body>"
"<div class=container>"
"<h1>🍊 Gate_orangepi 网关管理</h1>"

"<div class=card><h2>📊 系统状态</h2>"
"<div class=grid id=statusGrid>"
"<div class=stat><div class=label>LED 状态</div>"
"<div class=value><span id=ledDot class=\"led-indicator led-off\"></span>"
"<span id=ledText>--</span></div></div>"
"<div class=stat><div class=label>客户端数</div>"
"<div class=value id=clientCount>--</div></div>"
"<div class=stat><div class=label>温度</div>"
"<div class=value id=temp>--</div></div>"
"<div class=stat><div class=label>当前时间</div>"
"<div class=value id=time>--</div></div>"
"</div></div>"

"<div class=card><h2>💡 LED 控制</h2>"
"<button class=\"btn btn-success\" id=btnLedOn onclick=setLed(1)>开启 LED</button> "
"<button class=\"btn btn-danger\" id=btnLedOff onclick=setLed(0)>关闭 LED</button> "
"<button class=\"btn btn-primary\" id=btnLedToggle onclick=toggleLed()>切换</button>"
"</div>"

"<div class=card><h2>⚙️ 配置管理</h2>"
"<label>服务器端口 (server_port)"
"<input id=cfgServerPort type=number></label>"
"<label>状态刷新间隔/秒 (status_interval)"
"<input id=cfgStatusInterval type=number></label>"
"<label>日志文件路径 (log_file)"
"<input id=cfgLogFile type=text></label>"
"<label>Web 端口 (web_port)"
"<input id=cfgWebPort type=number></label>"
"<div class=row><button class=\"btn btn-primary\" onclick=saveConfig()>保存配置</button>"
"<button class=\"btn btn-primary\" onclick=loadConfig() style=margin-left:8px>重新加载</button></div>"
"</div>"

"<div class=card><h2>🖥️ 命令控制台</h2>"
"<div class=row>"
"<input id=cmdInput placeholder=\"输入命令,如 LED ON / GET STATUS / GET TEMP\""
"onkeypress=\"if(event.keyCode==13)sendCmd()\">"
"<button class=\"btn btn-primary\" onclick=sendCmd()>发送</button>"
"</div>"
"<div class=console id=console></div>"
"</div>"

"</div>"
"<div class=toast id=toast></div>"

"<script>"
"function $(id){return document.getElementById(id)}"
"function showToast(msg,isError){var t=$('toast');t.textContent=msg;"
"t.className='toast show'+(isError?' error':'');"
"setTimeout(function(){t.className='toast'+(isError?' error':'')},2000)}"
"function refreshStatus(){"
"fetch('/api/status').then(r=>r.json()).then(d=>{"
"$('ledDot').className='led-indicator '+(d.led?'led-on':'led-off');"
"$('ledText').textContent=d.led?'ON':'OFF';"
"$('clientCount').textContent=d.clients;"
"$('temp').textContent=(d.temp/10).toFixed(1)+'°C';"
"}).catch(e=>console.error(e))}"
"function setLed(on){"
"fetch('/api/led',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({on:on})}).then(r=>r.json()).then(d=>{"
"showToast(d.message);refreshStatus()}).catch(e=>showToast('失败',1))}"
"function toggleLed(){"
"fetch('/api/led/toggle',{method:'POST'}).then(r=>r.json()).then(d=>{"
"showToast(d.message);refreshStatus()})}"
"function loadConfig(){"
"fetch('/api/config').then(r=>r.json()).then(d=>{"
"$('cfgServerPort').value=d.server_port||8888;"
"$('cfgStatusInterval').value=d.status_interval||10;"
"$('cfgLogFile').value=d.log_file||'./gate_orangepi.log';"
"$('cfgWebPort').value=d.web_port||8080;"
"showToast('配置已加载')})}"
"function saveConfig(){"
"var d={server_port:$('cfgServerPort').value,"
"status_interval:$('cfgStatusInterval').value,"
"log_file:$('cfgLogFile').value,web_port:$('cfgWebPort').value};"
"fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify(d)}).then(r=>r.json()).then(x=>{"
"showToast(x.message)})}"
"function sendCmd(){"
"var i=$('cmdInput'),c=$('console'),cmd=i.value.trim();if(!cmd)return;"
"c.innerHTML+='> '+cmd+'<br>';i.value='';"
"fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({cmd:cmd})}).then(r=>r.json()).then(d=>{"
"c.innerHTML+=d.response+'<br>';c.scrollTop=c.scrollHeight;refreshStatus()})}"
"function updateTime(){var d=new Date();$('time').textContent="
"d.toTimeString().substring(0,8)}"
"refreshStatus();updateTime();"
"setInterval(refreshStatus,2000);setInterval(updateTime,1000);"
"loadConfig();"
"</script></body></html>";

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief 发送完整 HTTP 响应
 */
static int send_response(int fd, int status, const char *status_text,
                          const char *content_type, const char *body, int body_len)
{
    char header[512];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.0 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "\r\n",
        status, status_text, content_type, body_len);
    if (n < 0 || n >= (int)sizeof(header)) return -1;

    if (write(fd, header, n) < 0) return -1;
    if (body_len > 0 && body != NULL) {
        if (write(fd, body, body_len) < 0) return -1;
    }
    return 0;
}

/**
 * @brief URL 解码 (将 %XX 转为字符, + 转为空格)
 */
static void url_decode(char *dst, const char *src, int dst_size)
{
    int i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = {src[i+1], src[i+2], 0};
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

/**
 * @brief 简单 JSON 字符串字段提取
 * @return 0: 找到, -1: 未找到
 */
static int json_get_string(const char *json, const char *key, char *out, int out_size)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return -1;
    p += strlen(search);
    while (*p == ' ' || *p == ':' || *p == '\t') p++;
    if (*p != '"') return -1;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 0;
}

/**
 * @brief 简单 JSON 整数字段提取
 */
static int json_get_int(const char *json, const char *key, int *out)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return -1;
    p += strlen(search);
    while (*p == ' ' || *p == ':' || *p == '\t') p++;
    *out = atoi(p);
    return 0;
}

/* ============================================================
 * API 处理函数
 * ============================================================ */

static void api_status(int fd)
{
    device_status_t s;
    device_status_get_all(&s);
    int clients = client_count();

    char body[256];
    int n = snprintf(body, sizeof(body),
        "{\"led\":%d,\"clients\":%d,\"temp\":%d}",
        s.led_status, clients, s.temperature);
    send_response(fd, 200, "OK", "application/json", body, n);
}

static void api_led_set(int fd, const char *body)
{
    int on = 0;
    if (json_get_int(body, "on", &on) < 0) {
        const char *err = "{\"success\":false,\"message\":\"缺少 on 字段\"}";
        send_response(fd, 400, "Bad Request", "application/json", err, strlen(err));
        return;
    }

    if (on) {
        hal_led_on(0);
        device_status_set_led(1);
    } else {
        hal_led_off(0);
        device_status_set_led(0);
    }

    const char *resp = on
        ? "{\"success\":true,\"message\":\"LED 已开启\"}"
        : "{\"success\":true,\"message\":\"LED 已关闭\"}";
    send_response(fd, 200, "OK", "application/json", resp, strlen(resp));
    log_info("Web API: 设置 LED %s", on ? "ON" : "OFF");
}

static void api_led_toggle(int fd)
{
    int cur = device_status_get_led();
    int next = !cur;
    if (next) {
        hal_led_on(0);
    } else {
        hal_led_off(0);
    }
    device_status_set_led(next);

    char body[128];
    int n = snprintf(body, sizeof(body),
        "{\"success\":true,\"led\":%d,\"message\":\"LED 已切换\"}", next);
    send_response(fd, 200, "OK", "application/json", body, n);
    log_info("Web API: 切换 LED -> %s", next ? "ON" : "OFF");
}

static void api_config_get(int fd)
{
    const char *port = config_get("server_port", "8888");
    const char *interval = config_get("status_interval", "10");
    const char *log = config_get("log_file", "./gate_orangepi.log");
    const char *web_port = config_get("web_port", "8080");

    char body[512];
    int n = snprintf(body, sizeof(body),
        "{\"server_port\":\"%s\",\"status_interval\":\"%s\","
        "\"log_file\":\"%s\",\"web_port\":\"%s\"}",
        port, interval, log, web_port);
    send_response(fd, 200, "OK", "application/json", body, n);
}

static void api_config_set(int fd, const char *body)
{
    char val[256];

    if (json_get_string(body, "server_port", val, sizeof(val)) == 0) {
        config_set("server_port", val);
    }
    if (json_get_string(body, "status_interval", val, sizeof(val)) == 0) {
        config_set("status_interval", val);
    }
    if (json_get_string(body, "log_file", val, sizeof(val)) == 0) {
        config_set("log_file", val);
    }
    if (json_get_string(body, "web_port", val, sizeof(val)) == 0) {
        config_set("web_port", val);
    }

    config_save("./gate_orangepi.conf");

    const char *resp = "{\"success\":true,\"message\":\"配置已保存(部分配置需重启生效)\"}";
    send_response(fd, 200, "OK", "application/json", resp, strlen(resp));
    log_info("Web API: 配置已更新");
}

static void api_command(int fd, const char *body)
{
    char cmd_str[128];
    if (json_get_string(body, "cmd", cmd_str, sizeof(cmd_str)) < 0) {
        const char *err = "{\"success\":false,\"response\":\"缺少 cmd 字段\"}";
        send_response(fd, 400, "Bad Request", "application/json", err, strlen(err));
        return;
    }

    /* 复用现有业务解析与处理 */
    cmd_t cmd = parse_cmd(cmd_str);
    char response[256] = {0};
    event_process(cmd, response, sizeof(response));

    char out[512];
    int n = snprintf(out, sizeof(out),
        "{\"success\":true,\"response\":\"%s\"}", response);
    send_response(fd, 200, "OK", "application/json", out, n);
    log_info("Web API: 命令 %s -> %s", cmd_str, response);
}

/* ============================================================
 * 路由分发
 * ============================================================ */

static void route_request(int fd, const char *method, const char *path, const char *body)
{
    /* 主页 */
    if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
        send_response(fd, 200, "OK", "text/html; charset=utf-8",
                      WEB_INDEX_HTML, strlen(WEB_INDEX_HTML));
        return;
    }

    /* API 路由 */
    if (strcmp(path, "/api/status") == 0 && strcmp(method, "GET") == 0) {
        api_status(fd);
        return;
    }
    if (strcmp(path, "/api/led") == 0 && strcmp(method, "POST") == 0) {
        api_led_set(fd, body);
        return;
    }
    if (strcmp(path, "/api/led/toggle") == 0 && strcmp(method, "POST") == 0) {
        api_led_toggle(fd);
        return;
    }
    if (strcmp(path, "/api/config") == 0 && strcmp(method, "GET") == 0) {
        api_config_get(fd);
        return;
    }
    if (strcmp(path, "/api/config") == 0 && strcmp(method, "POST") == 0) {
        api_config_set(fd, body);
        return;
    }
    if (strcmp(path, "/api/command") == 0 && strcmp(method, "POST") == 0) {
        api_command(fd, body);
        return;
    }

    /* 404 */
    const char *notfound = "{\"success\":false,\"message\":\"Not Found\"}";
    send_response(fd, 404, "Not Found", "application/json", notfound, strlen(notfound));
}

/* ============================================================
 * HTTP 请求解析
 * ============================================================ */

#define MAX_REQ_LEN 4096
#define MAX_BODY_LEN 1024

typedef struct {
    char method[8];
    char path[256];
    char body[MAX_BODY_LEN];
} http_request_t;

static int parse_request(const char *raw, http_request_t *req)
{
    memset(req, 0, sizeof(*req));

    /* 解析 method */
    int i = 0;
    while (raw[i] && raw[i] != ' ' && i < (int)sizeof(req->method) - 1) {
        req->method[i] = raw[i];
        i++;
    }
    req->method[i] = '\0';
    while (raw[i] == ' ') i++;

    /* 解析 path(忽略 query string) */
    int j = 0;
    while (raw[i] && raw[i] != ' ' && raw[i] != '?' && j < (int)sizeof(req->path) - 1) {
        req->path[j++] = raw[i++];
    }
    req->path[j] = '\0';

    /* 跳到 header 结束 */
    const char *body_start = strstr(raw, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        int blen = strlen(body_start);
        if (blen >= (int)sizeof(req->body)) blen = sizeof(req->body) - 1;
        memcpy(req->body, body_start, blen);
        req->body[blen] = '\0';
    }
    return 0;
}

/* ============================================================
 * 客户端连接处理
 * ============================================================ */

static void handle_client(int fd)
{
    char buf[MAX_REQ_LEN];
    int total = 0;
    int n;

    /* 设置接收超时 */
    struct timeval tv = {2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (total < MAX_REQ_LEN - 1) {
        n = read(fd, buf + total, MAX_REQ_LEN - 1 - total);
        if (n <= 0) break;
        total += n;
        buf[total] = '\0';
        /* 简单判断: 头部结束或者 body 收完就处理 */
        if (strstr(buf, "\r\n\r\n")) {
            /* POST 还要看 Content-Length 是否收齐 */
            if (strncmp(buf, "GET", 3) == 0) break;
            const char *cl = strstr(buf, "Content-Length:");
            if (!cl) break;
            int content_len = atoi(cl + 15);
            const char *body = strstr(buf, "\r\n\r\n") + 4;
            int body_have = total - (int)(body - buf);
            if (body_have >= content_len) break;
        }
    }

    if (total == 0) {
        close(fd);
        return;
    }

    http_request_t req;
    parse_request(buf, &req);

    log_info("Web %s %s", req.method, req.path);
    route_request(fd, req.method, req.path, req.body);

    close(fd);
}

/* ============================================================
 * 主服务循环
 * ============================================================ */

int web_service_start(int port)
{
    int listen_fd, epoll_fd;
    struct sockaddr_in addr;
    struct epoll_event ev, events[64];
    int opt = 1;

    g_web_running = 1;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        log_error("Web 服务: socket() 失败: %s", strerror(errno));
        return -1;
    }

    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("Web 服务: bind 端口 %d 失败: %s", port, strerror(errno));
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, 16) < 0) {
        log_error("Web 服务: listen 失败: %s", strerror(errno));
        close(listen_fd);
        return -1;
    }

    /* 非阻塞 */
    int flags = fcntl(listen_fd, F_GETFL, 0);
    fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        log_error("Web 服务: epoll_create1 失败: %s", strerror(errno));
        close(listen_fd);
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    log_info("Web 服务已启动: http://0.0.0.0:%d", port);

    while (g_web_running) {
        int nfds = epoll_wait(epoll_fd, events, 64, 500);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == listen_fd) {
                /* 接受新连接 */
                while (1) {
                    struct sockaddr_in caddr;
                    socklen_t clen = sizeof(caddr);
                    int cfd = accept(listen_fd, (struct sockaddr *)&caddr, &clen);
                    if (cfd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        if (errno == EINTR) continue;
                        break;
                    }
                    /* 单次处理后立即关闭,简化模型 */
                    handle_client(cfd);
                }
            }
        }
    }

    log_info("Web 服务正在停止...");
    close(epoll_fd);
    close(listen_fd);
    g_web_running = 0;
    log_info("Web 服务已停止");
    return 0;
}

int web_service_is_running(void)
{
    return g_web_running;
}

void web_service_stop(void)
{
    g_web_running = 0;
}
