#include "hash.h"
unsigned int dictGenHashFunction(const unsigned char *buf, int len) {
	unsigned int hash = 5381;
	while (len--)
		hash = ((hash << 5) + hash) + (*buf++); /* hash * 33 + c */
	return hash;

}