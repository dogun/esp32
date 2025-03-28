#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <time.h>

uint8_t crc8(const uint8_t *data, size_t length) {
	uint8_t polynomial = 0x07;
	uint8_t initial_value = 0x00;

	uint8_t crc = initial_value;
	for (int i = 0; i < length; i++) {
		crc ^= data[i];
		for (int j = 0; j < 8; j++) {
			if (crc & 0x80) {
				crc = (crc << 1) ^ polynomial;
			} else {
				crc <<= 1;
			}
		}
	}
	return crc;
}


// 配置串口参数
int set_serial_params(int fd, int baudrate, int databits, int stopbits, char parity) {
    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        perror("tcgetattr");
        return -1;
    }

    // 设置波特率
    switch (baudrate) {
        case 9600:
            cfsetispeed(&options, B9600);
            cfsetospeed(&options, B9600);
            break;
        case 115200:
            cfsetispeed(&options, B115200);
            cfsetospeed(&options, B115200);
            break;
        default:
            fprintf(stderr, "Unsupported baudrate\n");
            return -1;
    }

    // 关闭硬件流控制
    options.c_cflag &= ~CRTSCTS;

    // 设置数据位
    options.c_cflag &= ~CSIZE;
    switch (databits) {
        case 7:
            options.c_cflag |= CS7;
            break;
        case 8:
            options.c_cflag |= CS8;
            break;
        default:
            fprintf(stderr, "Unsupported data bits\n");
            return -1;
    }

    // 设置校验位
    switch (parity) {
        case 'n':
        case 'N':
            options.c_cflag &= ~PARENB;
            options.c_cflag &= ~PARODD;
            break;
        case 'o':
        case 'O':
            options.c_cflag |= PARENB;
            options.c_cflag |= PARODD;
            break;
        case 'e':
        case 'E':
            options.c_cflag |= PARENB;
            options.c_cflag &= ~PARODD;
            break;
        default:
            fprintf(stderr, "Unsupported parity\n");
            return -1;
    }

    // 设置停止位
    switch (stopbits) {
        case 1:
            options.c_cflag &= ~CSTOPB;
            break;
        case 2:
            options.c_cflag |= CSTOPB;
            break;
        default:
            fprintf(stderr, "Unsupported stop bits\n");
            return -1;
    }

    // 启用接收
    options.c_cflag |= CREAD | CLOCAL;

    // 设置本地模式
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // 设置输入模式
    options.c_iflag &= ~(IXON | IXOFF | IXANY);

    // 设置输出模式
    options.c_oflag &= ~OPOST;

    // 设置读取超时时间
    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = 100;

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        perror("tcsetattr");
        return -1;
    }

    return 0;
}

FILE* data_fd = NULL;
char curr_file_name[16] = {0};
void write_data(char* data, int len) {
    // 获取当前时间的时间戳
    time_t current_time = time(NULL);
    if (current_time == -1) {
        perror("Failed to get current time");
        return;
    }

    // 将时间戳转换为本地时间结构体
    struct tm *local_time = localtime(&current_time);
    if (local_time == NULL) {
        perror("Failed to convert to local time");
        return;
    }

    // 提取年、月、日信息
    int year = local_time->tm_year + 1900;
    int month = local_time->tm_mon + 1;
    int day = local_time->tm_mday;
	
	char file_name[16] = {0};
	sprintf(file_name, "%04d%02d%02d.data", year, month, day);
	if (strcmp(curr_file_name, file_name) == 0 && data_fd != NULL) {
		//使用当前文件描述符
	}else {
		if (data_fd != NULL) fclose(data_fd);
		strcpy(curr_file_name, file_name);
		data_fd = fopen(file_name, "a");
		if (data_fd == NULL) {
			perror("open file error");
			return;
		}
	}
	
	//解析包
	uint8_t client_id = data[0];
	uint8_t type = data[1];
	uint8_t crc = data[2];
	uint8_t l = data[3];
	uint8_t crc1 = crc8(data + 4, l);
	if (!(crc == crc1 || (crc1 == 0 && crc == 111))) {
		printf("data error crc: %d, crc1: %d, len: %d, client: %d, type: %d\n", crc, crc1, l, client_id, type);
		return;
	}
	
    // 写入内容
	char wbuf[64] = {0};
	sprintf(wbuf, "%ld: %d,%d", current_time, client_id, type);
    size_t written = fwrite(wbuf, sizeof(char), strlen(wbuf), data_fd);
    if (written != strlen(wbuf)) {
        perror("写入失败");
        fclose(data_fd);
        return;
    }
	int i = 0;
	uint32_t* b = (uint32_t*)(data + 4);
	for (i = 0; i < l / 4; ++i) {
		memset(wbuf, 0, sizeof(wbuf));
		sprintf(wbuf, ",%d", b[i]);
		written = fwrite(wbuf, sizeof(char), strlen(wbuf), data_fd);
		if (written != strlen(wbuf)) {
			perror("写入失败1");
			fclose(data_fd);
			return;
		}
	}
	memset(wbuf, 0, sizeof(wbuf));
	sprintf(wbuf, "\n");
	written = fwrite(wbuf, sizeof(char), strlen(wbuf), data_fd);
	if (written != strlen(wbuf)) {
		perror("写入失败2");
		fclose(data_fd);
		return;
	}
	
	fflush(data_fd);
}

int read_one(int fd, char* bf) {
	int n = 0;
	memset(bf, 0, 1);
	n = read(fd, bf, 1);
	return n;
}

int main() {
    int fd;
    char buffer[128];
	int buffer_p = 0;
	char rb[1];
    int n;
	
	char a = 0x10;
	char b = 0x24;

START:
    // 打开串口设备
    fd = open("/dev/ttyS0", O_RDONLY | O_NOCTTY);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    // 配置串口参数
    if (set_serial_params(fd, 9600, 8, 1, 'N') == -1) {
        close(fd);
        return -1;
    }

    // 循环读取串口数据
	int index = 0;
    while (1) {
		//printf("START\n");
		n = read_one(fd, rb);
		//printf("READ %d\n", n);
        if (n == 1) {
			if (rb[0] == a) {
				n = read_one(fd, rb);
				if (rb[0] == b) { //是包头
					//先保存老包
					write_data(buffer, buffer_p);
					//清空buffer，位置置0
					memset(buffer, 0, sizeof(buffer));
					buffer_p = 0;
				} else {
					//不是包头，放入buffer
					buffer[buffer_p++] = a;
					buffer[buffer_p++] = rb[0];
				}
			}else {
				buffer[buffer_p++] = rb[0];
			}
        } else if(n == 0) {
			perror("read 0");
			close(fd);
			goto START;
		} else {
            perror("READ ERROR");
            break;
        }
    }

    // 关闭串口设备
    close(fd);
	// 关闭数据文件
	fclose(data_fd);

    return 0;
}
