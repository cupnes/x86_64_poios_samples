#include <fb.h>
#include <fbcon.h>

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *_fb)
{
	fb_init(_fb);
	set_fg(255, 255, 255);
	set_bg(0, 70, 250);
	clear_screen();

	puts("HELLO WORLD!");

	while (1);
}
