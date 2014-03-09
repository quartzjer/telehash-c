
#include <stdlib.h>
#include "switch.h"
#include "./ecc.h"
#include "./aes.h"
#include "./sha256.h"
#include "./sha1.h"
#include "./hmac.h"
#include "./base64_enc.h"
#include "./base64_dec.h"

typedef struct crypt_1a_struct
{
  uint8_t id_private[ECC_BYTES], id_public[ECC_BYTES *2], line_private[ECC_BYTES], line_public[ECC_BYTES *2];
  uint32_t seq;
  unsigned char keyOut[16], keyIn[16];
} *crypt_1a_t;

int RNG(uint8_t *p_dest, unsigned p_size)
{
  while(p_size--)
  {
    *p_dest = (uint8_t)random();
    p_dest++;
  }
  return 1;
}

int crypt_init_1a()
{
  ecc_set_rng(&RNG);
}

int crypt_new_1a(crypt_t c, unsigned char *key, int len)
{
  unsigned char hash[SHA1_HASH_BYTES];
  crypt_1a_t cs;
  
  if(!key || len <= 0) return 1;
  
  c->cs = malloc(sizeof(struct crypt_1a_struct));
  memset(c->cs, 0, sizeof (struct crypt_1a_struct));
  cs = (crypt_1a_t)c->cs;

  if(len == ECC_BYTES*2)
  {
    memcpy(cs->id_public,key,ECC_BYTES*2);
  }else{
    // try to base64 decode in case that's the incoming format
    if(key[len] != 0 || base64_binlength(key,0) != ECC_BYTES*2 || base64dec(cs->id_public,key,0)) return -1;
  }
  
  // generate fingerprint
  sha1(hash,cs->id_public,ECC_BYTES*2);

  // create line ephemeral key
  ecc_make_key(cs->line_public, cs->line_private);

  // alloc/copy in the public values (free'd by crypt_free)  
  c->part = malloc(SHA1_HASH_BYTES*2+1);
  c->keylen = ECC_BYTES*2;
  c->key = malloc(c->keylen);
  memcpy(c->key,cs->id_public,ECC_BYTES*2);
  util_hex(hash,SHA1_HASH_BYTES,(unsigned char*)c->part);

  return 0;
}


void crypt_free_1a(crypt_t c)
{
  if(c->cs) free(c->cs);
}

int crypt_keygen_1a(packet_t p)
{
  char b64[ECC_BYTES*4];
  uint8_t id_private[ECC_BYTES], id_public[ECC_BYTES*2];

  // create line ephemeral key
  ecc_make_key(id_public, id_private);

  base64enc(b64,id_public,ECC_BYTES*2);
  packet_set_str(p,"1a",b64);

  base64enc(b64,id_private,ECC_BYTES);
  packet_set_str(p,"1a_secret",b64);

  return 0;
}

int crypt_private_1a(crypt_t c, unsigned char *key, int len)
{
  unsigned long der_len = 4096;
  unsigned char der[der_len];
  crypt_1a_t cs = (crypt_1a_t)c->cs;
  
  if(!key || len <= 0) return 1;

  if(len == ECC_BYTES)
  {
    memcpy(cs->id_private,key,ECC_BYTES);
  }else{
    // try to base64 decode in case that's the incoming format
    if(key[len] != 0 || base64_binlength(key,0) != ECC_BYTES || base64dec(cs->id_private,key,0)) return -1;
  }

  c->isprivate = 1;
  return 0;
}

packet_t crypt_lineize_1a(crypt_t c, packet_t p)
{
  packet_t line;
  aes_context ctx;
  unsigned char iv[16], block[16], hmac[HMAC_SHA1_BYTES];
  size_t off = 0;
  crypt_1a_t cs = (crypt_1a_t)c->cs;

  line = packet_chain(p);
  packet_body(line,NULL,16+4+4+packet_len(p));
  memcpy(line->body,c->lineIn,16);
  memcpy(line->body+16+4,&(cs->seq),4);
  memset(iv,0,16);
  memcpy(iv+12,&(cs->seq),4);
  cs->seq++;

  aes_setkey_enc(&ctx,cs->keyOut,16);
  aes_crypt_ctr(&ctx,packet_len(p),&off,iv,block,packet_raw(p),line->body+16+4+4);

  hmac_sha1(hmac,cs->keyOut,16,line->body+16+4,4+packet_len(p));
  memcpy(line->body+16,hmac,4);

  return line;
}

packet_t crypt_delineize_1a(crypt_t c, packet_t p)
{
  packet_t line;
  aes_context ctx;
  unsigned char block[16], iv[16], hmac[HMAC_SHA1_BYTES];
  size_t off = 0;
  crypt_1a_t cs = (crypt_1a_t)c->cs;

  memset(iv,0,16);
  memcpy(iv+12,p->body+16+4,4);

  aes_setkey_dec(&ctx,cs->keyIn,16);
  aes_crypt_ctr(&ctx,p->body_len-(16+4+4),&off,iv,block,p->body+16+4+4,p->body+16+4+4);

  hmac_sha1(hmac,cs->keyIn,16,p->body+16+4,p->body_len-(16+4));
  if(memcmp(hmac,p->body+16,4) != 0) return packet_free(p);

  line = packet_parse(p->body+16+4+4, p->body_len-(16+4+4));
  packet_free(p);
  return line;
}

// makes sure all the crypto line state is set up, and creates line keys if exist
int crypt_line_1a(crypt_t c, packet_t inner)
{
  unsigned char line_public[ECC_BYTES*2], secret[ECC_BYTES], input[16+16+16], hash[SHA1_HASH_BYTES];
  char *hecc;
  crypt_1a_t cs;
  
  cs = (crypt_1a_t)c->cs;
  hecc = packet_get_str(inner,"ecc"); // it's where we stashed it
  if(!hecc || strlen(hecc) != ECC_BYTES*4) return 1;

  // do the diffie hellman
  util_unhex((unsigned char*)hecc,ECC_BYTES*4,line_public);
  if(!ecdh_shared_secret(line_public, cs->line_private, secret)) return 1;

  // make line keys!
  memcpy(input,secret,16);
  memcpy(input+16,c->lineOut,16);
  memcpy(input+32,c->lineIn,16);
  sha1(hash,input,16+16+16);
  memcpy(cs->keyOut,hash,16);

  memcpy(input+16,c->lineIn,16);
  memcpy(input+32,c->lineOut,16);
  sha1(hash,input,16+16+16);
  memcpy(cs->keyIn,hash,16);

  return 0;
}

// create a new open packet
packet_t crypt_openize_1a(crypt_t self, crypt_t c, packet_t inner)
{
  unsigned char secret[ECC_BYTES], iv[16], block[16];
  packet_t open;
  aes_context ctx;
  int inner_len;
  size_t off = 0;
  crypt_1a_t cs = (crypt_1a_t)c->cs, scs = (crypt_1a_t)self->cs;

  open = packet_chain(inner);
  packet_json(open,&(self->csid),1);
  inner_len = packet_len(inner);
  packet_body(open,NULL,20+40+inner_len);

  // copy in the line public key
  memcpy(open->body+20, cs->id_public, 40);

  // get the shared secret to create the iv+key for the open aes
  if(!ecdh_shared_secret(cs->id_public, cs->line_private, secret)) return packet_free(open);
  memset(iv,0,16);
  iv[15] = 1;

  // encrypt the inner
  aes_setkey_enc(&ctx,secret,16);
  aes_crypt_ctr(&ctx,inner_len,&off,iv,block,packet_raw(inner),open->body+20+40);

  // generate secret for hmac
  if(!ecdh_shared_secret(cs->id_public, scs->line_private, secret)) return packet_free(open);
  hmac_sha1(open->body,secret,16,open->body+20,40+inner_len);

  return open;
}

packet_t crypt_deopenize_1a(crypt_t self, packet_t open)
{
  unsigned char secret[ECC_BYTES], iv[16], block[16], b64[ECC_BYTES*2*2], hmac[HMAC_SHA1_BYTES];
  aes_context ctx;
  packet_t inner, tmp;
  size_t off = 0;
  crypt_1a_t cs = (crypt_1a_t)self->cs;

  if(open->body_len <= (20+40)) return NULL;
  inner = packet_new();
  packet_body(inner,NULL,open->body_len-(20+40));

  // get the shared secret to create the iv+key for the open aes
  if(!ecdh_shared_secret(open->body+20, cs->id_private, secret)) return packet_free(inner);
  memset(iv,0,16);
  iv[15] = 1;

  // decrypt the inner
  aes_setkey_dec(&ctx,secret,16);
  aes_crypt_ctr(&ctx,inner->body_len,&off,iv,block,open->body+20+40,inner->body);

  // load inner packet
  if((tmp = packet_parse(inner->body,inner->body_len)) == NULL) return packet_free(inner);
  packet_free(inner);
  inner = tmp;

  // generate secret for hmac
  if(inner->body_len != ECC_BYTES*2) return packet_free(inner);
  if(!ecdh_shared_secret(inner->body, cs->id_private, secret)) return packet_free(inner);

  // verify
  hmac_sha1(hmac,secret,16,open->body+20,open->body_len-20);
  if(memcmp(hmac,open->body,20) != 0) return packet_free(inner);

  // stash the hex line key w/ the inner
  util_hex(open->body+20,40,b64);
  packet_set_str(inner,"ecc",(char*)b64);

  return inner;
}

