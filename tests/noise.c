#include <MiniFB.h>

#define WIDTH 800
#define HEIGHT 600
static unsigned int s_buffer[WIDTH * HEIGHT];

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
	int noise, carry, seed = 0xbeef;

	if (!mfb_open("Noise Test", WIDTH, HEIGHT))
		return 0;

	for (;;)
	{
		int i, state;

		for (i = 0; i < WIDTH * HEIGHT; ++i)
		{
			noise = seed;
			noise >>= 3;
			noise ^= seed;
			carry = noise & 1;
			noise >>= 1;
			seed >>= 1;
			seed |= (carry << 30);
			noise &= 0xFF;
			s_buffer[i] = MFB_RGB(noise, noise, noise); 
		}

		state = mfb_update(s_buffer);

		if (state < 0)
			break;
	}

	mfb_close();

	return 0;
}
