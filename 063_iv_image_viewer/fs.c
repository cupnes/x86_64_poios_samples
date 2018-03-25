#include <fs.h>
#include <common.h>

#include <fbcon.h>

#define END_OF_FS	0x00

struct file *fs_start;

void fs_init(void *_fs_start)
{
	fs_start = _fs_start;
}

struct file *open(char *name)
{
	struct file *f = fs_start;
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
	struct file *f = fs_start;
	unsigned int num;

	for (num = 0; f->name[0] != END_OF_FS; num++) {
		if (num > 5)
			break;
		puts("F ");
		putd((unsigned long long)f, 20);
		puts("\r\n");
		files[num] = f;
		f = (struct file *)((unsigned long long)f + sizeof(struct file)
				    + f->size);
	}

	if (num > 5)
		while (1);

	return num;
}
