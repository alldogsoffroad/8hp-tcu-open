/* negative fixture (shadow_sim_scan.py hooks): a hook writing a live cell through a non-const volatile */
void fixture_hook(unsigned v) { *(volatile unsigned char *)0xFFF89CE8u = (unsigned char)v; }
