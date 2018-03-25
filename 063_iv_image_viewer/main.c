#include <x86.h>
#include <intr.h>
#include <pic.h>
#include <fb.h>
#include <kbc.h>
#include <fbcon.h>
#include <fs.h>
#include <common.h>
#include <iv.h>

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *_fb,
		  void *_fs_start)
{
	/* フレームバッファ周りの初期化 */
	fb_init(_fb);
	set_fg(255, 255, 255);
	set_bg(0, 70, 250);
	clear_screen();

	if (_fs_start == 0) {
		puts("NO FILESYSTEM FOUND.\r\n");
		while (1);
	}

	putd((unsigned long long)_fs_start, 20);

	/* CPU周りの初期化 */
	gdt_init();
	intr_init();
	putc('A');

	/* 周辺ICの初期化 */
	pic_init();
	kbc_init();
	putc('B');

	/* ファイルシステムの初期化 */
	fs_init(_fs_start);
	putc('C');

	/* 画像ビューアの初期化 */
	iv_init();
	putc('D');

	/* CPUの割り込み有効化 */
	enable_cpu_intr();
	putc('E');

	/* haltして待つ */
	while (1) {
		putc('F');
		cpu_halt();
	}
}
