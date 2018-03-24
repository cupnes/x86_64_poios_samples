#include <efi.h>
#include <common.h>
#include <mem.h>
#include <fb.h>
#include <file.h>

#define CONF_FILE_NAME	L"poiboot.conf"

#define KERNEL_FILE_NAME	L"kernel.bin"
#define APPS_FILE_NAME	L"apps.img"

#define MB		1048576	/* 1024 * 1024 */

#define KERNEL_START	0x0000000000110000
#define STACK_BASE	(KERNEL_START + (1 * MB))
	/* STACK_BASEはスタックの底のアドレス(上へ伸びる) */
#define APPS_START	0x0000000100000000

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
	stack_base = kernel_start + (1 * MB);
	apps_start = hexstrtoull(conf.apps_start);

	file_conf->Close(file_conf);

	puth(kernel_start, 16);
	if (kernel_start == KERNEL_START)
		puts(L"  OK");
	puts(L"\r\n");
	puth(stack_base, 16);
	if (stack_base == STACK_BASE)
		puts(L"  OK");
	puts(L"\r\n");
	puth(apps_start, 16);
	if (apps_start == APPS_START)
		puts(L"  OK");
	puts(L"\r\n");

	while (1);


	/* load the kernel */
	status = root->Open(
		root, &file_kernel, KERNEL_FILE_NAME, EFI_FILE_MODE_READ, 0);
	assert(status, L"root->Open(kernel)");

	kernel_size = get_file_size(file_kernel);

	struct header {
		void *bss_start;
		unsigned long long bss_size;
	} head;
	unsigned long long head_size = sizeof(head);
	status = file_kernel->Read(file_kernel, &head_size, (void *)&head);
	assert(status, L"file_kernel->Read(head)");

	kernel_size -= sizeof(head);
	status = file_kernel->Read(file_kernel, &kernel_size,
				   (void *)KERNEL_START);
	assert(status, L"file_kernel->Read(body)");

	ST->BootServices->SetMem(head.bss_start, head.bss_size, 0);

	puts(L"loaded kernel(first 8 bytes): ");
	p = (unsigned char *)KERNEL_START;
	for (i = 0; i < 8; i++) {
		puth(*p++, 2);
		putc(L' ');
	}
	puts(L"\r\n");

	file_kernel->Close(file_kernel);


	/* load the applications */
	status = root->Open(
		root, &file_apps, APPS_FILE_NAME, EFI_FILE_MODE_READ, 0);
	if (!check_warn_error(status, L"root->Open(apps)")) {
		puts(L"apps load failure. skip.\r\n");
		has_apps = FALSE;
	}

	if (has_apps) {
		apps_size = get_file_size(file_apps);

		status = file_apps->Read(file_apps, &apps_size, (void *)APPS_START);
		assert(status, L"file_apps->Read");

		puts(L"loaded apps(first 8 bytes): ");
		p = (unsigned char *)APPS_START;
		for (i = 0; i < 8; i++) {
			puth(*p++, 2);
			putc(L' ');
		}
		puts(L"\r\n");

		file_apps->Close(file_apps);
	}


	kernel_arg1 = (unsigned long long)ST;
	init_fb();
	kernel_arg2 = (unsigned long long)&fb;

	puts(L"kernel_arg1 = 0x");
	puth(kernel_arg1, 16);
	puts(L"\r\n");
	puts(L"kernel_arg2 = 0x");
	puth(kernel_arg2, 16);
	puts(L"\r\n");
	puts(L"stack_base = 0x");
	puth(STACK_BASE, 16);
	puts(L"\r\n");

	exit_boot_services(ImageHandle);

	unsigned long long _sb = STACK_BASE, _ks = KERNEL_START;
	__asm__ ("	mov	%0, %%rsi\n"
		 "	mov	%1, %%rdi\n"
		 "	mov	%2, %%rsp\n"
		 "	jmp	*%3\n"
		 ::"r"(kernel_arg2), "r"(kernel_arg1), "r"(_sb), "r"(_ks));

	while (TRUE);
}
