#include <efi.h>
#include <common.h>
#include <mem.h>
#include <fb.h>
#include <file.h>

#define CONF_FILE_NAME	L"poiboot.conf"

#define KERNEL_FILE_NAME	L"kernel.bin"
#define APPS_FILE_NAME	L"apps.img"

#define MB		1048576	/* 1024 * 1024 */

#define READ_UNIT	16384	/* 16KB */

void put_n_bytes(unsigned char *addr, unsigned int num);
void safety_read(struct EFI_FILE_PROTOCOL *src, void *dst);

void efi_main(void *ImageHandle, struct EFI_SYSTEM_TABLE *SystemTable)
{
	unsigned long long status;
	struct EFI_FILE_PROTOCOL *root;
	struct EFI_FILE_PROTOCOL *file_conf;
	struct EFI_FILE_PROTOCOL *file_kernel;
	struct EFI_FILE_PROTOCOL *file_apps;
	unsigned long long kernel_start;
	unsigned long long stack_base;
	unsigned long long apps_start;
	unsigned long long kernel_size;
	unsigned long long apps_size;
	unsigned char *p;
	unsigned int i;
	unsigned long long kernel_arg1;
	unsigned long long kernel_arg2;
	unsigned long long kernel_arg3;
	unsigned char has_apps = TRUE;

	efi_init(SystemTable);

	puts(L"Starting OS5 UEFI bootloader ...\r\n");

	status = SFSP->OpenVolume(SFSP, &root);
	assert(status, L"SFSP->OpenVolume");


	/* load config file */
	status = root->Open(
		root, &file_conf, CONF_FILE_NAME, EFI_FILE_MODE_READ, 0);
	assert(status, L"Can't open config file.");

	struct config {
		char kernel_start[17];
		char apps_start[17];
	} conf;
	unsigned long long conf_size = sizeof(conf);
	status = file_conf->Read(file_conf, &conf_size, (void *)&conf);
	kernel_start = hexstrtoull(conf.kernel_start);
	stack_base = kernel_start + (1 * MB);	/* stack_baseはスタックの底のアドレス(上へ伸びる) */
	apps_start = hexstrtoull(conf.apps_start);

	file_conf->Close(file_conf);


	/* load the kernel */
	status = root->Open(
		root, &file_kernel, KERNEL_FILE_NAME, EFI_FILE_MODE_READ, 0);
	assert(status, L"root->Open(kernel)");

	kernel_size = get_file_size(file_kernel);
	puts(L"kernel_size=");
	puth(kernel_size, 16);
	puts(L"\r\n");
	unsigned long long tmp_kernel_size = kernel_size;

	struct header {
		void *bss_start;
		unsigned long long bss_size;
	} head;
	unsigned long long head_size = sizeof(head);
	status = file_kernel->Read(file_kernel, &head_size, (void *)&head);
	assert(status, L"file_kernel->Read(head)");

	kernel_size -= sizeof(head);
	tmp_kernel_size -= sizeof(head);
	status = file_kernel->Read(file_kernel, &kernel_size,
				   (void *)kernel_start);
	assert(status, L"file_kernel->Read(body)");

	ST->BootServices->SetMem(head.bss_start, head.bss_size, 0);

	puts(L"loaded kernel(first 8 bytes): ");
	p = (unsigned char *)kernel_start;
	for (i = 0; i < 8; i++) {
		puth(*p++, 2);
		putc(L' ');
	}
	puts(L"\r\n");

	file_kernel->Close(file_kernel);

	unsigned char *last = (unsigned char *)(
		(unsigned long long)kernel_start + tmp_kernel_size - 16);
	puts(L"last kernel addr: ");
	puth((unsigned long long)last, 16);
	puts(L"\r\n");

	puts(L"loaded kernel(last 16 bytes): ");
	put_n_bytes(last, 16);

	/* load the applications */
	status = root->Open(
		root, &file_apps, APPS_FILE_NAME, EFI_FILE_MODE_READ, 0);
	if (!check_warn_error(status, L"root->Open(apps)")) {
		puts(L"apps load failure. skip.\r\n");
		has_apps = FALSE;
	}

	if (has_apps) {
		root->Flush(root);
		file_apps->Flush(file_apps);

		apps_size = get_file_size(file_apps);

		puts(L"apps_size=");
		puth(apps_size, 16);
		puts(L"\r\n");
		unsigned long long tmp_apps_size = apps_size;

		/* status = file_apps->Read(file_apps, &apps_size, (void *)apps_start); */
		/* assert(status, L"file_apps->Read"); */

		safety_read(file_apps, (void *)apps_start);

		puts(L"A: ");
		puth(apps_size, 16);
		puts(L", ");
		puth(tmp_apps_size, 16);
		puts(L"\r\n");

		/* if (apps_size < tmp_apps_size) { */
		/* 	puts(L"don't finished to read apps.img.\r\n"); */
		/* 	while (1); */
		/* } */

		puts(L"loaded apps(first 8 bytes): ");
		p = (unsigned char *)apps_start;
		for (i = 0; i < 8; i++) {
			puth(*p++, 2);
			putc(L' ');
		}
		puts(L"\r\n");

		file_apps->Close(file_apps);

		puts(L"tmp_apps_size=");
		puth(tmp_apps_size, 16);
		puts(L"\r\n");
		unsigned char *alb = (unsigned char *)(apps_start + tmp_apps_size - 1);
		puts(L"loaded last byte");
		puth((unsigned long long)alb, 16);
		puts(L": ");
		puth(*alb, 2);
		puts(L"\r\n");
	}


	kernel_arg1 = (unsigned long long)ST;
	init_fb();
	kernel_arg2 = (unsigned long long)&fb;
	if (has_apps == TRUE)
		kernel_arg3 = apps_start;
	else
		kernel_arg3 = 0;

	puts(L"kernel_arg1 = 0x");
	puth(kernel_arg1, 16);
	puts(L"\r\n");
	puts(L"kernel_arg2 = 0x");
	puth(kernel_arg2, 16);
	puts(L"\r\n");
	puts(L"kernel_arg3 = 0x");
	puth(kernel_arg3, 16);
	puts(L"\r\n");
	puts(L"stack_base = 0x");
	puth(stack_base, 16);
	puts(L"\r\n");

	if (has_apps == TRUE) {
		struct file {
			char name[28];
			unsigned int size;
			unsigned char data[0];
		} *f = (struct file *)apps_start;
		int num;
		for (num = 0; f->name[0] != 0x00; num++) {
			if (num > 5)
				break;
			puth((unsigned long long)f, 16);
			puts(L"\r\n");
			puts(L"name=");
			put_n_bytes((unsigned char *)f->name, 16);
			puts(L"size=");
			puth(f->size, 16);
			puts(L"\r\n");
			puts(L"data(B)=");
			put_n_bytes(f->data, 16);
			f = (struct file *)(
				(unsigned long long)f->data + f->size);
			puts(L"data(E)=");
			put_n_bytes((unsigned char *)((unsigned long long)f - 16), 16);
		}
	}

	exit_boot_services(ImageHandle);

	unsigned long long _sb = stack_base, _ks = kernel_start;
	__asm__ ("	mov	%0, %%rdx\n"
		 "	mov	%1, %%rsi\n"
		 "	mov	%2, %%rdi\n"
		 "	mov	%3, %%rsp\n"
		 "	jmp	*%4\n"
		 ::"m"(kernel_arg3), "m"(kernel_arg2), "m"(kernel_arg1),
		  "m"(_sb), "m"(_ks));

	while (TRUE);
}

void put_n_bytes(unsigned char *addr, unsigned int num)
{
	unsigned int i;
	for (i = 0; i < num; i++) {
		puth(*addr++, 2);
		putc(L' ');
	}
	puts(L"\r\n");
}

void safety_read(struct EFI_FILE_PROTOCOL *src, void *dst)
{
	long long size = get_file_size(src);
	unsigned char *d = dst;

	while (size > 0) {
		unsigned long long unit = READ_UNIT;
		unsigned long long status = src->Read(src, &unit, (void *)d);
		assert(status, L"safety_read");
		d += unit;
		size -= unit;
	}
}
