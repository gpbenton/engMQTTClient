#include <stdint.h>
static uint16_t ran;

void seed(uint8_t pid, uint16_t pip)
{
	ran = ((((uint16_t) pid) << 8) ^ pip);
}

uint8_t decrypt(uint8_t dat)
{
	unsigned char i;

	for (i = 0; i < 5; ++i)
	{
		ran = (ran & 1) ? ((ran >> 1) ^ 62965U) : (ran >> 1);
	}
	return (uint8_t)(ran ^ dat ^ 90U);
}


int16_t crc(uint8_t const mes[], unsigned char siz)
{
	uint16_t rem = 0;
	unsigned char byte, bit;

	for (byte = 0; byte < siz; ++byte)
	{
		rem ^= (mes[byte] << 8);
		for (bit = 8; bit > 0; --bit)
		{
			rem = ((rem & (1 << 15)) ? ((rem << 1) ^ 0x1021) : (rem << 1));
		}
	}
	return rem;
}
