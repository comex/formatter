#include "sha1.h"

typedef struct{
	unsigned char key[0x40];
	SHA1Context hash_ctx;
} hmac_ctx;

void hmac_init(hmac_ctx *ctx, const char *key, int key_size);

void hmac_update(hmac_ctx *ctx, const u8 *data, int size);
  
void hmac_final(hmac_ctx *ctx, unsigned char *hmac);

void fs_hmac_set_key(const char *key, int key_size);

void fs_hmac_meta(const unsigned char *super_data, short super_blk, unsigned char *hmac);

void fs_hmac_data(const unsigned char *data, int uid, const unsigned char *name, 
		    int entry_n, int x, short blk, unsigned char *hmac);
