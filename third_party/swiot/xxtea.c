#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TEA_ENSTR_MAX	512

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

static const char _hexbytes[] = "0123456789ABCDEF";

static unsigned int hex2bin(unsigned char *src, unsigned char *dst, size_t slen) {
	if (slen % 2 == 1)
		return 0;
	int i, dpos = 0;
	unsigned char islow = 0;
	while (slen--) {
		if ('0' <= (*src) && (*src) <= '9') {
			if (islow)
				*(dst + dpos) = (*(dst + dpos) << 4) + ((*src) - '0');
			else
				*(dst + dpos) = (*src) - '0';
		} else if ('a' <= (*src) && (*src) <= 'f') {
			if (islow)
				*(dst + dpos) = (*(dst + dpos) << 4) + ((*src) - 'a') + 10;
			else
				*(dst + dpos) = (*src) - 'a' + 10;
		} else if ('A' <= (*src) && (*src) <= 'F') {
			if (islow)
				*(dst + dpos) = (*(dst + dpos) << 4) + ((*src) - 'A') + 10;
			else
				*(dst + dpos) = (*src) - 'A' + 10;
		} else {
			return 0;
		}
		if (islow)
			dpos++;
		islow = !islow;
		src++;
	}
	return dpos;
}

static uint16_t toIntArray(uint32_t *presult, const uint8_t *pdat, uint16_t dl, uint8_t inclen)
{
	if (presult && pdat && dl)
	{
		uint16_t n = ((dl&3)==0) ? (dl>>2) : ((dl>>2)+1);
		uint16_t i = 0;
		if (inclen)
		{
			presult[n] = dl;
			n += 1;
		}
		for (i=0; i<dl; i++)
		{
			presult[i>>2] |= (0xff&pdat[i]) << ((i & 3) << 3);
		}
		return n;
	}
	return 0;
}

static uint16_t toByteArray(uint8_t *presult, uint32_t *pdat, uint16_t dl, uint8_t inclen)
{
	uint32_t n;
	int i = 0;
	if (presult && pdat && dl)
	{
		n = dl<<2;
		if (inclen)
		{
			printf("n:%d l:%d\n", n, pdat[dl-1]);
			if (pdat[dl-1] > n)
				return 0;
			n = pdat[dl-1];
		}

		for (i = 0; i < n; i++)
		{
			presult[i] = (uint8_t)((pdat[i>>2]>>((i&3)<<3)) & 0xff);
		}
		return n;
	}
	return 0;
}

#define MX (((uint32_t)z >> 5 ^ y << 2) + (((uint32_t)y >> 3) ^ (z << 4))) ^ ((sum ^ y)  + (k[(p & 3) ^ e] ^ z))

static void raw_encrypt(uint32_t *v, uint32_t vlen, uint32_t *k, uint16_t klen)
{
	uint32_t n = (uint32_t)vlen - 1;
	if (n < 1) return;
	int i = 0;
	uint32_t z = v[n], y = v[0], delta = 0x9E3779B9, sum = 0, e;
	uint32_t p, q = 6 + 52 / (n + 1);
	printf("no encrp: ");
	for (i=0; i<vlen; i++)
		printf("%08X ", v[i]);
	printf("\n");
	printf("q:%d ", q);
	while (q-- > 0)
	{
		sum += delta;
		e = (uint32_t)sum >> 2 & 3;
		for (p = 0; p < n; p++)
		{
			y = v[p + 1];
			v[p] += MX;
			z = v[p];
		}
		y = v[0];
		v[n] += MX;
		z = v[n];
	}
	printf("sum:%08X\n", sum);
	for (i=0; i<vlen; i++)
		printf("%08X ", v[i]);
	printf("\n");
}
static void raw_decrypt(uint32_t *v, uint32_t vlen, uint32_t *k, uint32_t klen)
{
	uint32_t n = (uint32_t)vlen - 1;
	if (n < 1) return;
	int i = 0;
	uint32_t z = v[n], y = v[0], delta = 0x9E3779B9, sum, e;
	uint32_t p, q = 6 + 52 / (n + 1);
	sum = q * delta;
	printf("no decrp: ");
	for (i=0; i<vlen; i++)
		printf("%08X ", v[i]);
	printf("\n");
	printf("q:%d sum:%08X\n", q, sum);
	while (sum != 0)
	{
		e = (uint32_t)sum >> 2 & 3;
		for (p = n; p > 0; p--)
		{
			z = v[p - 1];
			v[p] -= MX;
			y = v[p];
		}
		z = v[n];
		v[0] -= MX;
		y = v[0];
		sum -= delta;
	}
	for (i=0; i<vlen; i++)
		printf("%08X ", v[i]);
	printf("\n");
}

static uint16_t xxtea_encrypt(uint8_t *presult, const uint8_t *pdat, size_t dl, const uint8_t *pkey, size_t kl)
{
	if (!pdat || kl == 0) return 0;

	uint32_t _data[TEA_ENSTR_MAX>>2] = {0};
	uint32_t _key[8] = {0};
	memset(_data, 0, TEA_ENSTR_MAX);
	memset(_key, 0, 32);
	uint16_t datalen = toIntArray(_data, pdat, dl, 1);
	uint16_t keylen = toIntArray(_key, pkey, kl, 0);
	uint16_t retlen = 0;
	printf("dl:%d kl:%d\n", datalen, keylen);
	if (keylen>0 && datalen>0)
	{
		raw_encrypt(_data, datalen, _key, keylen);
		retlen = toByteArray(presult, _data, datalen, 0);
	}
	return retlen;
}

static uint16_t xxtea_decrypt(uint8_t *pdat, size_t dl, const uint8_t *pkey, size_t kl)
{
	if (!pdat || kl == 0) return 0;

	uint32_t _data[TEA_ENSTR_MAX>>2] = {0};
	uint32_t _key[8] = {0};
	memset(_data, 0, TEA_ENSTR_MAX);
	memset(_key, 0, 32);

	uint16_t datalen = toIntArray(_data, pdat, dl, 0);
	uint16_t keylen = toIntArray(_key, pkey, kl, 0);
	uint16_t retlen = 0;
	printf("dl:%d kl:%d\n", datalen, keylen);
	if (datalen>0 && keylen>0)
	{
		raw_decrypt(_data, datalen, _key, keylen);
		retlen = toByteArray(pdat, _data, datalen, 1);
	}
	return retlen;
}

static int en_xxtea(char *out, size_t olen, const char *buf, size_t len, const char *key, size_t kl)
{
	// uint8_t enstr[TEA_ENSTR_MAX] = {0};

	uint32_t i = 0;
	uint32_t nret = xxtea_encrypt(out, buf, len, key, kl);

	if (nret>(TEA_ENSTR_MAX>>1)) nret = TEA_ENSTR_MAX>>1;
	printf("nret:%d\n", nret);
	printf("enstr: ");
	for (i = 0; i<nret; i++)
	{
		printf("%02X",out[i]);
	}
	printf("\n");
	for (i = 0; i<nret; i++)
	{
		out[2*(nret-i-1)+1] = _hexbytes[out[nret-i-1] & 0xf];
		out[2*(nret-i-1)] = _hexbytes[out[nret-i-1] >> 4];
	}
	printf("%s\n", out);

	return 1;
}

static int de_xxtea(const char *buf, size_t len, const char *key, size_t kl)
{
	uint8_t enstr[TEA_ENSTR_MAX] = {0};
	memset(enstr, 0, TEA_ENSTR_MAX);
	int i = 0;
	if (len>TEA_ENSTR_MAX*2 || (len=hex2bin((uint8_t *)buf, enstr, len)) == 0)
		return 0;
	printf("enstr: ");
	for (i = 0; i<len; i++)
	{
		printf("%02X",enstr[i]);
	}
	printf("\n");
	uint32_t nret = xxtea_decrypt(enstr, len, key, kl);
	printf("%d %d\n", nret, len);
	enstr[nret]='\0';
	printf("%s\n", enstr);
	return 1;
}

/*
int main(void)
{
	en_xxtea("{\"ip\":\"127.0.0.1\",\"mac\":\"AA:BB:CC:DD:EE:FF\",\"manufactor\":\"test\",\"model\":\"1.0\",\"name\":\"demo\",\"refreshState\":\"0\",\"refreshTime\":\"20180622161935\",\"sn\":\"123456789012\",\"subDeviceData\":[],\"version\":\"1.0.1\"}",199,"AaBbCcDdEeFfGgHhIiJjKk1234567890",32);
	de_xxtea("2A22123899B54FE00B784DFAD8824457DC0094125DFD8438468F1E27F92B54EF24E2E711A1B88CA3219DB68A716F740245DECF789D0F162868D6B2EF71983E1C10C1D1B5954A6CFCA52C57013E9C9C56E2D1C02438DF03C25141EB9495230F0CAAF34A6B41898A62AB9F53CD012731ED89F5B73B96EE9E73B11242F92BC2172B7498522A97918CC767D8BBFA4F0B87B3A45072E51320DE7DD670F156515A1BDF61B4739C65497D3B4EDD74D57E402E5F9F186A459C0D0BC77CE24725E68C26169DD83F6571F02971A57B188E",408,"AaBbCcDdEeFfGgHhIiJjKk1234567890",32);
	return 0;
}*/