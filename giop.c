/*
 * giop.c
 *
 *  Created on: May 13, 2018
 *      Author: root
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include "main.h"
#include "gpio.h"
#include "aux_func.h"

/*
 * @function gpio_edge_poll
 * 	鏌ヨ鎸囧畾鐨勬枃浠舵弿杩扮鏄惁浜嬩欢鍙戠敓
 * @param1 fd, 鐩戝惉鐨勬枃浠舵弿杩扮
 * @param2 trig, AUX浜嬩欢绫诲瀷
 * @param3 timeouts锛� 瓒呮椂璁℃暟娆℃暟銆倀imeout =0,浠ｈ〃璁℃暟娆℃暟鏃犳晥銆�
 * */

/* AUX鍔熻兘锛�
 * 1銆� 鍙戦�佹椂锛屽綋妯″潡鎶婃墍鏈夋暟鎹兘鏀惧埌RF鑺墖鍚庯紝涓旀渶鍚庝竴鍖呮暟鎹嵁鍚姩鍙戝皠鍚庯紝AUX杈撳嚭楂樼數骞炽��
 * 2銆� 鎺ユ敹鏃讹紝褰撴ā鍧楁妸鎵�鏈夋暟鎹兘鏀惧埌RF鑺墖鍚庯紝涓旀渶鍚庝竴鍖呮暟鎹嵁鍚姩鍙戝皠鍚庯紝AUX杈撳嚭楂樼數骞炽��
 * */


#ifdef 	USE_RADIO_AUX_PIN

int gpio_edge_poll(int fd, AUX_Trig_t trig, int timeouts)
{
	int ret =-1;
	char buff[10];
	struct pollfd fds[1];
	fds[0].fd = fd;
	fds[0].events  = POLLPRI;		//event : urgent data to read
	char event[4] ={0,};
	sprintf(event,"%s", (trig== TX_AUX ? ("TX"): (trig== RX_AUX? "RX": "CFG")));
//	do
//	{
		ret = poll(fds, 1, timeouts);//-1 );
		if (ret != -1 && fds[0].revents & POLLPRI)
		{
//			ret = lseek(fd, 0, SEEK_SET);
//			if (ret == -1)
//				LOG_MSG(ERR_LEVEL,"lseek\n");
//			ret = read(fd, buff, 10);
//			if(ret == -1)
//				LOG_MSG(ERR_LEVEL,"read\n");
//			elsegg
//			{
////				LOG_MSG(INFO_LEVEL,"poll read %s_aux= %c\n",event, buff[0]);
//			}
//			break;
		}
//		/*
//		 *timeouts涓�0鏃讹紝鈥滄绛夆�濅簨浠跺彂鐢�
//		 * */
//		if(timeouts ==0)
//			continue;
//	  printf(". \n");
//	}while(timeouts-- >0);

//	if(timeouts != 0 && timeouts==0)
//	{
//		printf("\n===================================%s event timeout\n", event);
//	}

	return 0;
}

#else
	int gpio_edge_poll(int fd, AUX_Trig_t trig, unsigned int timeouts)
	{
		return 0;
	}
#endif


/*
 * 鏌ヨAUX涓婂崌娌挎槸鍚﹀埌鏉�,涓婂崌娌垮埌鏉ワ紝鍒欒繑鍥�1,鍚﹀垯杩斿洖0
 * */
int gpio_edge_poll_try(int fd)
{
	int ret =-1;
	char buff[10];
	struct pollfd fds[1];
	fds[0].fd = fd;
	fds[0].events  = POLLPRI;

	ret = poll(fds, 1, 0);

	if (ret == -1 )
		LOG_MSG(ERR_LEVEL,"poll\n");
	if (fds[0].revents & POLLPRI)
	{
		ret = lseek(fd, 0, SEEK_SET);
		if (ret == -1)
			LOG_MSG(ERR_LEVEL,"lseek\n");
		ret = read(fd, buff, 10);
		if(ret == -1  )
		{
			LOG_MSG(ERR_LEVEL,"try poll read error\n");
		}
		else if(ret ==0 )
		{
			LOG_MSG(ERR_LEVEL,"try poll read return 0\n");
		}
		else
		{
			LOG_MSG(INFO_LEVEL,"try poll read gpio= %c\n", buff[0]);
		}
		return 1;
	}

	return 0;
}


/*
 * @function gpio_export
 * @descprition锛氭墽琛宔cho pin > export
 * */
int gpio_export(int pin)
{
	char buffer[64];
	int len;
	int fd;

	char path[64]={0,};
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);

	/*
	 * access杩斿洖璋冪敤杩涚▼鏄惁鍙互瀵规寚瀹氭枃浠舵墽琛屾煇绉嶆搷浣�
	 * F_OK,鏂囦欢瀛樺湪鍚�
	 * W_OK,鏂囦欢鍙啓鍚�
	 * 鎴愬姛鏃惰繑鍥�0,澶辫触鏃惰繑鍥�-1.
	 * */
	if((access(path, F_OK)) !=-1)
	{
		//LOG_MSG(INFO_LEVEL,"%s existed\n",path );
		return 0;
	}

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (fd < 0)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open export for writing!\n");
		return (-1);
	}
	len = snprintf(buffer, sizeof(buffer), "%d", pin);
	if (write(fd, buffer, len) < 0) {
		LOG_MSG(ERR_LEVEL,"Failed to export gpio!");
		return -1;
	}
	close(fd);
	return 0;
}

int gpio_unexport(int pin)
{
	char buffer[64];
	int len;
	int fd;

	char path[64]={0,};
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);

	/*
	 * access杩斿洖璋冪敤杩涚▼鏄惁鍙互瀵规寚瀹氭枃浠舵墽琛屾煇绉嶆搷浣�
	 * F_OK,鏂囦欢瀛樺湪鍚�
	 * W_OK,鏂囦欢鍙啓鍚�
	 * 鎴愬姛鏃惰繑鍥�0,澶辫触鏃惰繑鍥�-1.
	 * */
	if((access(path, F_OK)) ==-1)
	{
		LOG_MSG(INFO_LEVEL,"%s not existed\n",path);
		return -1;
	}

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (fd < 0)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open unexport for writing!\n");
		return -1;
	}
	len = snprintf(buffer, sizeof(buffer), "%d", pin);
	if (write(fd, buffer, len) < 0)
	{
		LOG_MSG(ERR_LEVEL,"Failed to unexport gpio!");
		return -1;
	}
	close(fd);
	return 0;
}


//dir: 0-->IN, 1-->OUT
int gpio_direction(int pin, int dir)
{
	static const char dir_str[] = "in\0out";
	char path[64];
	int fd;
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
	fd = open(path, O_WRONLY);

	if (fd < 0)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open gpio direction for writing!\n");
		return -1;
	}
	if (write(fd, &dir_str[dir == 0 ? 0 : 3], dir == 0 ? 2 : 3) < 0)
	{
		LOG_MSG(ERR_LEVEL,"Failed to set direction!\n");
		return -1;
	}

	close(fd);
	return 0;
}

//value: 0-->LOW, 1-->HIGH
int gpio_write(int pin, int value)
{
	static const char values_str[] = "01";
	char path[64];
	int fd;
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_WRONLY);
	if (fd < 0) {
		LOG_MSG(ERR_LEVEL,"Failed to open gpio value for writing!\n");
		return -1;
	}
	if (write(fd, &values_str[value == 0 ? 0 : 1], 1) < 0) {
		LOG_MSG(ERR_LEVEL,"Failed to write value!\n");
		return -1;
	}
	close(fd);
	return 0;
}

int open_gpio_rw_fd(int pin)
{
	char path[64];
	char value_str[3];
	int fd;
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_RDONLY);
	if (fd < 0)
	{
		LOG_MSG(ERR_LEVEL,"Failed to open gpio value for reading!\n");
		return -1;
	}
	return (fd);
}

int gpio_read(int pin)
{
	char path[64];
	char value_str[3];
	int fd;
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		LOG_MSG(ERR_LEVEL,"Failed to open gpio value for reading!\n");
		return -1;
	}
	if (read(fd, value_str, 3) < 0)
	{
		LOG_MSG(ERR_LEVEL,"Failed to read value!\n");
		return -1;
	}
	close(fd);
	return (atoi(value_str));
}

int gpio_high_wait(int pin)
{
	char path[64];
	char value_str[3]={0,};
	int fd;
	int level = 0;

	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		LOG_MSG(ERR_LEVEL,"Failed to open gpio value for reading!\n");
		return -1;
	}

	while(1)
	{
		if (read(fd, value_str, 3) < 0)
		{
			LOG_MSG(ERR_LEVEL,"Failed to read value!\n");
			return -1;
		}
		level = atoi(value_str);
		if(level == 1)
		{
			close(fd);
			return 0;
		}
		//usleep(200);
	}
	close(fd);
	return 0;
}


// none琛ㄧず寮曡剼涓鸿緭鍏ワ紝鏄腑鏂紩鑴�
// rising琛ㄧず寮曡剼涓轰腑鏂緭鍏ワ紝涓婂崌娌胯Е鍙�
// falling琛ㄧず寮曡剼涓轰腑鏂緭鍏ワ紝涓嬮檷娌胯Е鍙�
// both琛ㄧず寮曡剼涓轰腑鏂緭鍏ワ紝杈规部瑙﹀彂
// 0-->none, 1-->rising, 2-->falling, 3-->both
int gpio_edge(int pin, int edge)
{
	const char dir_str[] = "none\0rising\0falling\0both";
	char ptr;
	char path[64];
	int fd;
	switch (edge) {
	case 0:
		ptr = 0;
		break;
	case 1:
		ptr = 5;	//rising
		break;
	case 2:
		ptr = 12;	//falling
		break;
	case 3:
		ptr = 20;	//both
		break;
	default:
		ptr = 0;
	}
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/edge", pin);
	fd = open(path, O_WRONLY);
	if (fd < 0) {
		LOG_MSG(ERR_LEVEL,"Failed to open gpio edge for writing!\n");
		return -1;
	}
	if (write(fd, &dir_str[ptr], strlen(&dir_str[ptr])) < 0) {
		LOG_MSG(ERR_LEVEL,"Failed to set edge!\n");
		return -1;
	}
	close(fd);
	return 0;

}
//
//
//int test_gipo()
//{
//	int fd;
//	//璁剧疆绔彛鏂瑰悜/sys/class/gpio/gpio4# echo out > direction
//	fd = open(E32_MX_GPIO_DIR, O_WRONLY);
//	if (fd == -1)
//	{
//		LOG_MSG(ERR_LEVEL,"ERR: Radio hard reset pin direction open error.\n");
//		return EXIT_FAILURE;
//	}
//	write(fd, SYSFS_GPIO_OUT_ENBL, sizeof(SYSFS_GPIO_OUT_ENBL));
//	close(fd);
//
//	//杈撳嚭澶嶄綅淇″彿: 鎷夐珮>100ns
//	//璁剧疆绔彛鏂瑰悜/sys/class/gpio/gpio4# echo 1 > value
//	fd = open(E32_MX_GPIO_VAL, O_RDWR);
//	if (fd == -1)
//	{
//		LOG_MSG(ERR_LEVEL,"ERR: Radio hard reset pin value open error.\n");
//		return EXIT_FAILURE;
//	}
//
//	while (1)
//	{
//		write(fd, SYSFS_GPIO_VAL_H, sizeof(SYSFS_GPIO_VAL_H));
//		usleep(1000000);	//1s
//		write(fd, SYSFS_GPIO_VAL_L, sizeof(SYSFS_GPIO_VAL_L));
//		usleep(1000000);
//	}
//	close(fd);
//	LOG_MSG(INFO_LEVEL,"INFO: Radio hard reset pin value open error.\n");
//	return 0;
//}
