#ifndef WEB_SERVICE_H
#define WEB_SERVICE_H

/**
 * @file web_service.h
 * @brief 嵌入式 Web 服务模块
 *
 * 提供轻量级 HTTP 服务器,用于 Web 管理界面
 * 特性:
 *   - 单线程 + epoll 高并发模型
 *   - RESTful API 接口
 *   - 内嵌 HTML/CSS/JS 页面(零外部依赖)
 *   - 复用 device_service / config_service
 */

/**
 * @brief 启动 Web 服务(阻塞,需要在独立线程中调用)
 * @param port 监听端口
 * @return 0: 正常退出, -1: 启动失败
 *
 * @note 该函数会一直阻塞,直到 web_service_stop() 被调用或监听失败
 */
int web_service_start(int port);

/**
 * @brief 通知 Web 服务停止
 *
 * @note 仅将运行标志置 0,不会强制关闭已建立的连接
 *       调用后 web_service_start() 会在当前请求处理完毕后返回
 */
void web_service_stop(void);

/**
 * @brief 获取 Web 服务运行状态
 * @return 1: 运行中, 0: 未运行
 */
int web_service_is_running(void);

#endif /* WEB_SERVICE_H */
