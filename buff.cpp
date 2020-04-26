#include "buff.h"
#include<sys/uio.h>

const char Buffer::kCRLF[] = "\r\n";

const size_t Buffer::kCheapPrepend;
const size_t Buffer::kInitialSize;

ssize_t Buffer::readFd(int fd, int* savedErrno) //把fd上的内容读入缓冲区，即写缓冲区
{
	char extrabuf[65536]; //分配在栈上的空间，如果分配给该TCP的缓存不够，则用extrabuf，详见陈硕的linux多线程编程
	struct iovec vec[2];
	const size_t writable = writableBytes();
	vec[0].iov_base = begin() + writerIndex_;
	vec[0].iov_len = writable;
	vec[1].iov_base = extrabuf;
	vec[1].iov_len = sizeof extrabuf;
	const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1; //如果writable已经大于65536了，就无需再用栈了
	const ssize_t n = readv(fd, vec, iovcnt);

	if (n < 0) {
		*savedErrno = errno;
	}
	else if (static_cast<size_t>(n) <= writable) {
		writerIndex_ += n;
	}
	else {
		writerIndex_ = buffer_.size();
		append(extrabuf, n - writable);
	}
	return n;
}

bool Buffer::getLine(char *dest, size_t len) { /* 读取一行数据 */
	const char* end = findEOL();
	if (end == 0) return false; /* 没有找到换行符 */

	const char* start = peek();
	assert(end >= start); /* 保证size是一个正数,然后下面static_cast转换的时候才会正确 */
	ptrdiff_t size = end - start - 1;

	if (len < static_cast<size_t>(size)) {
		return false; /* 空间不够 */
	}
	std::copy(start, end - 1, dest);
	dest[size] = 0;
	retrieveUntil(end + 1);
	return true;
}

void Buffer::appendStr(const char* format, ...) { /* 格式化输入 */
	char extralbuf[256];
	memset(extralbuf, 0, sizeof extralbuf);
	va_list arglist;
	va_start(arglist, format);
	vsnprintf(extralbuf, sizeof extralbuf, format, arglist);
	va_end(arglist);
	append(extralbuf, strlen(extralbuf));
}
