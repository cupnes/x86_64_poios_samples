#include <fs.h>
#include <common.h>

#define FS_START_ADDR	0x0000000000200000
#define END_OF_FS	0x00

struct file *open(char *name)
{
	struct file *f = (struct file *)FS_START_ADDR;
	while (f->name[0] != END_OF_FS) {
		if (!strcmp(f->name, name))
			return f;

		f = (struct file *)((unsigned long long)f + sizeof(struct file)
				    + f->size);
	}

	return NULL;
}

unsigned long long get_files(struct file *files[])
{
	struct file *f = (struct file *)FS_START_ADDR;
	unsigned int num;

	for (num = 0; f->name[0]; num++) {
		files[num] = f;
		f = (struct file *)((unsigned long long)f + sizeof(struct file)
				    + f->size);
	}

	return num;
}
