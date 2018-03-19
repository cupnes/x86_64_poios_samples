#include <efi.h>
#include <common.h>
#include <mem.h>
#include <fb.h>
#include <file.h>

#define KERNEL_FILE_NAME	L"kernel.bin"
#define APPS_FILE_NAME	L"apps.img"
#define STACK_HEAP_SIZE	1048576	/* 1MB */

#define KERNEL_START	0x0000000000110000
#define APPS_START	0x0000000000200000
#define STACK_BASE	0x0000000000400000

/* #define ALLOCATION_SIZE	209715200	/\* 200MB *\/ */
#define ALLOCATION_PAGES	51200	/* 200MB */

/*
** STARTED UEFI: PageTable設定の動確
QEMUの場合は   0x0000 0100 0000 以降、
lenovoの場合は 0x0001 0000 0000 以降を
0x80000000 00000000 へマップする <- このアドレスで良い?

kernel_arg1(ST) = 0x000000007FA43F18    size = 0x0000000000000068
kernel_arg2(fb) = 0x0000000000406160    size = 0x0000000000000018

kernel_arg1(ST) = 0x000000007FA43F18    size = 0x0000000000000068
kernel_arg2(fb) = 0x0000000000406160    size = 0x0000000000000018

kernel_arg1(ST) = 0x000000007FA43F18    size = 0x0000000000000068
kernel_arg2(fb) = 0x0000000000406160    size = 0x0000000000000018
fb.base = 0x0000000080000000    size = 0x00000000001D4C00

kernel_arg1(ST) = 0x000000007FA43F18    size = 0x0000000000000068
kernel_arg2(fb) = 0x0000000000406160    size = 0x0000000000000018
fb.base = 0x0000000080000000    size = 0x00000000001D4C00

echo 'ibase=16;1D4C00' | bc
=> (/ 1920000.0 1024.0 1024.0)1.8310546875 MB
=> (/ 1920000.0 4096)468.75

=> runtimeで変化はしない模様

mapしなければならない資源
- kernel_arg1(ST)
- kernel_arg2(fb)
- fb自身の領域
- kernel.bin, apps.img, stack
- 自分自身のコードとデータ(EfiLoaderCode, EfiLoaderData)

QEMUの場合、
- kernel_arg2(fb)		: 0x0000000000406160	(24 byte  => 1     page)
- kernel_arg1(ST)		: 0x000000007FA43F18	(104 byte => 1     page)
- fb自身の領域			: 0x0000000080000000	(1.83 MB  => 469   pages)
- kernel.bin, apps.img, stack	: 好きな所		(200 MB程 => 51200 pages)

(* 200 1024 1024)209715200
(/ 209715200.0 4096)51200.0

200MB = C800000
*/

void efi_main(void *ImageHandle, struct EFI_SYSTEM_TABLE *SystemTable)
{
	unsigned long long status;
	struct EFI_FILE_PROTOCOL *root;
	struct EFI_FILE_PROTOCOL *file_kernel;
	struct EFI_FILE_PROTOCOL *file_apps;
	unsigned long long kernel_size;
	unsigned long long apps_size;
	unsigned char *p;
	unsigned int i;
	unsigned long long kernel_arg1;
	unsigned long long kernel_arg2;
	unsigned char has_apps = TRUE;

	efi_init(SystemTable);

	puts(L"Starting OS5 UEFI bootloader ...\r\n");

	/* unsigned long long allocation_addr; */
	/* status = ST->BootServices->AllocatePages( */
	/* 	AllocateAnyPages, EfiConventionalMemory, ALLOCATION_PAGES, */
	/* 	&allocation_addr); */
	/* assert(status, L"AllocatePages"); */
	/* puth(allocation_addr, 16); */
	/* puts(L"\r\n"); */

	/* while (1); */

	/* init_memmap(); */
	/* dump_available_memmap(); */

	/* while (1); */

	status = SFSP->OpenVolume(SFSP, &root);
	assert(status, L"SFSP->OpenVolume");


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

	puts(L"kernel_arg1(ST) = 0x");
	puth(kernel_arg1, 16);
	puts(L"\tsize = 0x");
	puth(sizeof(struct EFI_SYSTEM_TABLE), 16);
	puts(L"\r\n");
	puts(L"kernel_arg2(fb) = 0x");
	puth(kernel_arg2, 16);
	puts(L"\tsize = 0x");
	puth(sizeof(struct fb), 16);
	puts(L"\r\n");
	puts(L"fb.base = 0x");
	puth(fb.base, 16);
	puts(L"\tsize = 0x");
	puth(fb.size, 16);
	puts(L"\r\n");
	puts(L"stack_base = 0x");
	puth(STACK_BASE, 16);
	puts(L"\r\n");

	while (TRUE);

	exit_boot_services(ImageHandle);

	unsigned long long _sb = STACK_BASE, _ks = KERNEL_START;
	__asm__ ("	mov	%0, %%rsi\n"
		 "	mov	%1, %%rdi\n"
		 "	mov	%2, %%rsp\n"
		 "	jmp	*%3\n"
		 ::"r"(kernel_arg2), "r"(kernel_arg1), "r"(_sb), "r"(_ks));

	while (TRUE);
}
