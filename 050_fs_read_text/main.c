#include <x86.h>
#include <intr.h>
#include <pic.h>
#include <fb.h>
#include <kbc.h>
#include <fbcon.h>

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *_fb,
		  void *_fs_start)
{
	/* フレームバッファ周りの初期化 */
	fb_init(_fb);
	set_fg(255, 255, 255);
	set_bg(0, 70, 250);
	clear_screen();

	/* CPU周りの初期化 */
	gdt_init();
	intr_init();

	/* 周辺ICの初期化 */
	pic_init();
	kbc_init();

	/* CPUの割り込み有効化 */
	enable_cpu_intr();

	/* fs.img(テキストファイル)を読む */
	char *hello_str = (char *)_fs_start;
	puts(hello_str);

	/* haltして待つ */
	while (1)
		cpu_halt();
}
