#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include "csapp.h"
#include "epoll_ulti.h"
#include "http_handle.h"
#include "thread_pool.h"
#include "timer.h"
using namespace utility;

/*-
* 非阻塞版本的web server,主要利用epoll机制来实现多路IO复用.加上了线程池,这样以来可以实现更高
* 的性能.
*/

#define MAXEVENTNUM 100

void blockSigpipe()
{
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGPIPE);
	int rc = pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
	if (rc != 0) {
		printf("block sigpipe error\n");
	}
}

int http_close_conn(HttpHandle* request)
{
    printf("closing\n");
    close(request->getFd());
    return 0;
}

int main(int argc, char *argv[])
{
	int listenfd = Open_listenfd(8080); /* 8080号端口监听 */
	epoll_event events[MAXEVENTNUM];
	sockaddr clnaddr;
	socklen_t clnlen = sizeof(clnaddr);

	blockSigpipe(); /* 首先要将SIGPIPE消息阻塞掉 */

	int epollfd = Epoll_create(1024); /* 10基本上没有什么用处 */
	addfd(epollfd, listenfd, false); /* epollfd要监听listenfd上的可读事件 */
	ThreadPool pools(3, 30000);
	HttpHandle::setEpollfd(epollfd);
	HttpHandle handle[20000];

    tk_timer_init();
	while(true)
    {
        int time = tk_find_timer();
		int eventnum = Epoll_wait(epollfd, events, MAXEVENTNUM, -1);
        tk_handle_expire_timers();
		for (int i = 0; i < eventnum; ++i)
        {
			int sockfd = events[i].data.fd;
			if (sockfd == listenfd) 
            { /* 有连接到来 */
				while(true)
                {
					int connfd = accept(listenfd, &clnaddr, &clnlen);
					if (connfd == -1) 
                    {
						if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                        { /* 将连接已经建立完了 */
							break;
						}
						unix_error("accept error");
					}
					handle[connfd].init(connfd); /* 初始化 */
					addfd(epollfd, connfd, true); /* 加入监听 */
                    tk_add_timer(&handle[connfd], TIMEOUT_DEFAULT, http_close_conn);
				}
			}
			else
            { /* 有数据可读或者可写 */
				pools.append(boost::bind(&HttpHandle::process, &handle[sockfd])); /* 多线程版本 */
				//handle[sockfd].process(); // 单进程版本
			}
		}
	}
	return 0;
}

