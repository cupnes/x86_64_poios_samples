#ifndef _FS_H_
#define _FS_H_

#define FILE_NAME_LEN	28

struct file {
	char name[FILE_NAME_LEN];
	unsigned int size;
	unsigned char data[0];
};

struct file *open(char *name);

#endif
