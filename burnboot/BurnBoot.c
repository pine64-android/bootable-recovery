
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "BootHead.h"
#include "Utils.h"

#define NAND_BLKBURNBOOT0 		_IO('v',127)
#define NAND_BLKBURNUBOOT 		_IO('v',128)

#define SECTOR_SIZE	512
#define SD_BOOT0_SECTOR_START	16
#define SD_BOOT0_SIZE_KBYTES	32
#define SD_UBOOT_SECTOR_START	32800
#define SD_UBOOT_SECTOR_START_PRE	38192
#define SD_UBOOT_SIZE_KBYTES	1024

#define SD_BOOT0_PERMISION "/sys/block/mmcblk0boot0/force_ro"
#define DEVNODE_PATH_SD_BOOT0 "/dev/block/mmcblk0boot0"

static int writeSdBoot(int fd, void *buf, off_t offset, size_t bootsize){
	if (lseek(fd, 0, SEEK_SET) == -1) {
		bb_debug("reset the cursor failed! the error num is %d:%s\n",errno,strerror(errno));
		return -1;
	}
	if (lseek(fd, offset, SEEK_CUR) == -1) {
		bb_debug("lseek failed! the error num is %d:%s\n",errno,strerror(errno));
		return -1;
	}
	int result = write(fd, buf, bootsize);
	return result;
}

static int readSdBoot(int fd ,off_t offset, size_t bootsize, void *buffer){
	memset(buffer, 0, bootsize);

	if (lseek(fd, 0, SEEK_SET) == -1) {
		bb_debug("reset the cursor failed! the error num is %d:%s\n",errno,strerror(errno));
		return -1;
	}

	if (lseek(fd, offset, SEEK_CUR) == -1) {
		bb_debug("lseek failed! the error num is %d:%s\n",errno,strerror(errno));
		return -1;
	}

	read(fd,buffer,bootsize);
	return 0;
}

static int openDevNode(const char *path){
	int fd = open(path, O_RDWR);

	if (fd == -1){
		bb_debug("open device node failed ! errno is %d : %s\n", errno, strerror(errno));
	}

	return fd;
}

int readSdBoot0(char *path, void *buffer){
	int fd = openDevNode(path);
	int ret = -1;
	if (fd >= 0) {
	    ret = readSdBoot(fd, SD_BOOT0_SECTOR_START * SECTOR_SIZE, SD_BOOT0_SIZE_KBYTES * 1024, buffer);
	    close(fd);
	}
	return ret;
}

int readSdUboot(char *path, void *buffer){
	int fd = openDevNode(path);
	int ret = -1;
	if (fd >= 0) {
	    ret = readSdBoot(fd, SD_UBOOT_SECTOR_START * SECTOR_SIZE, SD_UBOOT_SIZE_KBYTES * 1024, buffer);
	    close(fd);
	}
	return ret;
}

int burnSdBoot0(BufferExtractCookie *cookie, char *path){
	if (checkBoot0Sum(cookie)){
		bb_debug("illegal binary file!\n");
		return -1;
	}

	int fd = openDevNode(path);
	if (fd == -1){
		return -1;
	}

	int ret = writeSdBoot(fd, cookie->buffer, SD_BOOT0_SECTOR_START * SECTOR_SIZE, SD_BOOT0_SIZE_KBYTES * 1024);
	close(fd);
	if (ret > 0){
		bb_debug("burnSdBoot0 succeed! writed %d bytes\n", ret);
	}
    fd = openDevNode(DEVNODE_PATH_SD_BOOT0);
    if (fd > 0) {
        int pmsFd = open(SD_BOOT0_PERMISION,O_WRONLY);
        if (pmsFd > 0) {
            ret = write(pmsFd,"0",1);
            close(pmsFd);
        } else {
            bb_debug("can't open %s :%s \n", SD_BOOT0_PERMISION,strerror(errno));
            close(fd);
            return -1;
        }
        if (ret < 0) {
            bb_debug("can't write 0 to %s :%s \n", SD_BOOT0_PERMISION,strerror(errno));
            close(fd);
            return -1;
        }
        // wipe boot0 in mmcblk0boot0
        ret = writeSdBoot(fd, cookie->buffer, 0, SD_BOOT0_SIZE_KBYTES * 1024);
        if (ret > 0) {
            bb_debug("wipe boot0 in mmcblk0boot0 succeed! on %d writed %d bytes\n", 0, ret);
        }
        fsync(fd);
    }
	return ret;
}

int burnSdUboot(BufferExtractCookie *cookie, char *path){
	
	if (checkUbootSum(cookie) && checkBoot1Sum(cookie)){
		bb_debug("illegal uboot binary file!\n");
		return -1;
	}
    
    bb_debug("uboot binary length is %ld\n", cookie->len);
	int fd = openDevNode(path);
	if (fd == -1){
		return -1;
	}

	writeSdBoot(fd, cookie->buffer, SD_UBOOT_SECTOR_START_PRE * SECTOR_SIZE, cookie->len);
	int ret = writeSdBoot(fd, cookie->buffer, SD_UBOOT_SECTOR_START * SECTOR_SIZE, cookie->len);
	close(fd);

	if (ret > 0){
		bb_debug("burnSdUboot succeed! writed %d bytes\n", ret);
	}
	return ret;
}

int burnNandBoot0(BufferExtractCookie *cookie, char *path){

	if (checkBoot0Sum(cookie)){
		bb_debug("wrong boot0 binary file!\n");
		return -1;
	}

	int fd = openDevNode(path);
	if (fd == -1){
		return -1;
	}

	int ret = ioctl(fd,NAND_BLKBURNBOOT0,(unsigned long)cookie);

	if (ret) {
		bb_debug("burnNandBoot0 failed ! errno is %d : %s\n", errno, strerror(errno));
	}else{
		bb_debug("burnNandBoot0 succeed!");
	}

	close(fd);
	return ret;
}

int burnNandUboot(BufferExtractCookie *cookie, char *path){
	
	if (checkUbootSum(cookie) && checkBoot1Sum(cookie)){
		bb_debug("wrong uboot binary file!\n");
		return -1;
	}

	int fd = openDevNode(path);
	if (fd == -1){
		return -1;
	}

	int ret = ioctl(fd,NAND_BLKBURNUBOOT,(unsigned long)cookie);
	
	if (ret) {
		bb_debug("burnNandUboot failed ! errno is %d : %s\n", errno, strerror(errno));
	}else{
		bb_debug("burnNandUboot succeed!!");
	}

	close(fd);
	return ret;
}
