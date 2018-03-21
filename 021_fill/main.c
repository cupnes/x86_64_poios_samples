#include <fb.h>

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *fb)
{
	fb_init(fb);
	set_bg(0, 255, 0);
	clear_screen();

	while (1);
}
