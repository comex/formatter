#include "string.h"
#include "fs_hmac.h"

void wbe32(void *ptr, u32 val) { *(u32*)ptr = val; }
void wbe16(void *ptr, u16 val) { *(u16*)ptr = val; }

// reversing done by gray

static unsigned char hmac_key[20];

void hmac_init(hmac_ctx *ctx, const char *key, int key_size) {

	int i;

	key_size = key_size<0x40 ? key_size: 0x40;

	memset(ctx->key,0,0x40);
	memcpy(ctx->key,key,key_size);

	for(i=0;i<0x40;++i)
		ctx->key[i] ^= 0x36; // ipad

	SHA1Reset(&ctx->hash_ctx);
	SHA1Input(&ctx->hash_ctx,ctx->key,0x40);
}

void hmac_update(hmac_ctx *ctx, const u8 *data, int size)
{
	SHA1Input(&ctx->hash_ctx,data,size);
}

void hmac_final(hmac_ctx *ctx, unsigned char *hmac) 
{
	int i;
	unsigned char hash[0x14];
	memset(hash, 0, 0x14);

	SHA1Result(&ctx->hash_ctx);

	// this sha1 implementation is buggy, needs to switch endian
	for(i=0;i<5;++i) {
		wbe32(hash + 4*i, ctx->hash_ctx.Message_Digest[i]);
	}

	for(i=0;i<0x40;++i)
		ctx->key[i] ^= 0x36^0x5c; // opad

	SHA1Reset(&ctx->hash_ctx);
	SHA1Input(&ctx->hash_ctx,ctx->key,0x40);
	SHA1Input(&ctx->hash_ctx,hash,0x14);
	SHA1Result(&ctx->hash_ctx);

	//hexdump(ctx->hash_ctx.Message_Digest, 0x14);

	for(i=0;i<5;++i){
		wbe32(hash + 4*i, ctx->hash_ctx.Message_Digest[i]);
	}
	memcpy(hmac, hash, 0x14);

	//hexdump(hmac, 0x14);
}

void fs_hmac_set_key(const char *key, int key_size)
{
	memset(hmac_key,0,0x14);
	memcpy(hmac_key,key,key_size<0x14?key_size:0x14);
}

void fs_hmac_generic(const unsigned char *data, int size, const unsigned char *extra, int extra_size, unsigned char *hmac)
{
	hmac_ctx ctx;

	hmac_init(&ctx,(const char *)hmac_key,0x14);
	
	hmac_update(&ctx,extra,extra_size);
	hmac_update(&ctx,data,size);

	hmac_final(&ctx,hmac);
}

void fs_hmac_meta(const unsigned char *super_data, short super_blk, unsigned char *hmac)
{
	unsigned char extra[0x40];

	memset(extra,0,0x40);
	wbe16(extra + 0x12, super_blk);

	fs_hmac_generic(super_data,0x40000,extra,0x40,hmac);
}

void fs_hmac_data(const unsigned char *data, int uid, const unsigned char *name, int entry_n, int x3, short blk, unsigned char *hmac)
{
	//int i,j;
	unsigned char extra[0x40];

	memset(extra,0,0x40);
	
	wbe32(extra, uid);

	memcpy(extra+4,name,12);

	wbe16(extra + 0x12, blk);
	wbe32(extra + 0x14, entry_n);
	wbe32(extra + 0x18, x3);
	
//	fprintf(stderr,"extra (%s): \nX ",name);
//	for(i=0;i<0x20;++i){
//	    fprintf(stderr,"%02X ",extra[i]);
//	    if(!((i+1)&0xf)) fprintf(stderr,"\nX ");
//	}

	fs_hmac_generic(data,0x4000,extra,0x40,hmac);
}

