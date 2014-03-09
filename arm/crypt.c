#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "crypt.h"
#include "util.h"
#include "platform.h"

unsigned char *crypt_rand(unsigned char *s, int len)
{
  FILE *fp;
  fp = fopen("/dev/urandom", "r");
  fread(&s, 1, len, fp);
  fclose(fp);
  return s;
}

unsigned char *crypt_hash(unsigned char *input, unsigned long len, unsigned char *output)
{
  sha256((uint8_t (*)[32])output,input,len);
  return output;
}

char *crypt_err()
{
  return 0;
}

int crypt_init()
{
  int ret = -1;
  int i = 0;
  crypt_supported = malloc(8);
  memset(crypt_supported,0,8);
#ifdef CS_1a
  ret = crypt_init_1a();
  if(ret) return ret;
  crypt_supported[i++] = 0x1a;
#endif
#ifdef CS_2a
  ret = crypt_init_2a();
  if(ret) return ret;
  crypt_supported[i++] = 0x2a;
#endif
#ifdef CS_3a
  ret = crypt_init_3a();
  if(ret) return ret;
  crypt_supported[i++] = 0x3a;
#endif
  return ret;
}

crypt_t crypt_new(char csid, unsigned char *key, int len)
{
  crypt_t c;
  int err = 1;

  if(!csid || !key || !len) return NULL;
  c = malloc(sizeof (struct crypt_struct));
  memset(c, 0, sizeof (struct crypt_struct));
  c->csid = csid;
  sprintf(c->csidHex,"%02x",csid);
  crypt_rand(c->lineOut,16);
  util_hex(c->lineOut,16,c->lineHex);
  c->atOut = platform_seconds();

#ifdef CS_1a
  if(csid == 0x1a) err = crypt_new_1a(c, key, len);
#endif
#ifdef CS_2a
  if(csid == 0x2a) err = crypt_new_2a(c, key, len);
#endif
#ifdef CS_3a
  if(csid == 0x3a) err = crypt_new_3a(c, key, len);
#endif
  
  if(!err) return c;

  crypt_free(c);
  return NULL;
}

void crypt_free(crypt_t c)
{
  if(!c) return;
#ifdef CS_1a
  if(c->csid == 0x1a) crypt_free_1a(c);
#endif
#ifdef CS_2a
  if(c->csid == 0x2a) crypt_free_2a(c);
#endif
#ifdef CS_3a
  if(c->csid == 0x3a) crypt_free_3a(c);
#endif
  if(c->part) free(c->part);
  if(c->key) free(c->key);
  free(c);
}

int crypt_keygen(char csid, packet_t p)
{
  if(!p) return 1;

#ifndef CS_1a
  if(csid == 0x1a) return crypt_keygen_1a(p);
#endif
#ifdef CS_2a
 printf("inside CS_2a\n");

  if(csid == 0x2a) return crypt_keygen_2a(p);
#endif
#ifdef CS_3a
 printf("inside CS_3a\n");

  if(csid == 0x3a) return crypt_keygen_3a(p);
#endif 

  return 1;
}

int crypt_private(crypt_t c, unsigned char *key, int len)
{
  int ret;
  if(!c) return 1;
  if(c->isprivate) return 0; // already loaded

#ifdef CS_1a
  if(c->csid == 0x1a && (ret = crypt_private_1a(c,key,len))) return ret;
#endif
#ifdef CS_2a
  if(c->csid == 0x2a && (ret = crypt_private_2a(c,key,len))) return ret;
#endif
#ifdef CS_3a
  if(c->csid == 0x3a && (ret = crypt_private_3a(c,key,len))) return ret;
#endif
  
  c->isprivate = 1;
  return 0;
}

packet_t crypt_lineize(crypt_t c, packet_t p)
{
  if(!c || !p || !c->lined) return NULL;
#ifdef CS_1a
  if(c->csid == 0x1a) return crypt_lineize_1a(c,p);
#endif
#ifdef CS_2a
  if(c->csid == 0x2a) return crypt_lineize_2a(c,p);
#endif
#ifdef CS_3a
  if(c->csid == 0x3a) return crypt_lineize_3a(c,p);
#endif
  return NULL;
}

packet_t crypt_delineize(crypt_t c, packet_t p)
{
  if(!c || !p) return NULL;
  if(!c->lined) return packet_free(p);
#ifdef CS_1a
  if(c->csid == 0x1a) return crypt_delineize_1a(c,p);
#endif
#ifdef CS_2a
  if(c->csid == 0x2a) return crypt_delineize_2a(c,p);
#endif
#ifdef CS_3a
  if(c->csid == 0x3a) return crypt_delineize_3a(c,p);
#endif
  return NULL;
}

packet_t crypt_openize(crypt_t self, crypt_t c, packet_t inner)
{
  if(!c || !self || self->csid != c->csid) return NULL;

  packet_set_str(inner,"line",(char*)c->lineHex);
  packet_set_int(inner,"at",(int)c->atOut);
  packet_body(inner,self->key,self->keylen);

#ifdef CS_1a
  if(c->csid == 0x1a) return crypt_openize_1a(self,c,inner);
#endif
#ifdef CS_2a
  if(c->csid == 0x2a) return crypt_openize_2a(self,c,inner);
#endif
#ifdef CS_3a
  if(c->csid == 0x3a) return crypt_openize_3a(self,c,inner);
#endif

  return NULL;
}

packet_t crypt_deopenize(crypt_t self, packet_t open)
{
  packet_t ret = NULL;
  if(!open || !self) return NULL;

#ifdef CS_1a
  if(self->csid == 0x1a && (ret = crypt_deopenize_1a(self,open))) return ret;
#endif
#ifdef CS_2a
  if(self->csid == 0x2a && (ret = crypt_deopenize_2a(self,open))) return ret;
#endif
#ifdef CS_3a
  if(self->csid == 0x3a && (ret = crypt_deopenize_3a(self,open))) return ret;
#endif

  return NULL;
}

int crypt_line(crypt_t c, packet_t inner)
{
  int ret = 1;
  unsigned long at;
  char *hline;
  unsigned char lineid[16];

  if(!inner) return ret;
  if(!c) return packet_free(inner)||1;

  at = strtol(packet_get_str(inner,"at"), NULL, 10);
  hline = packet_get_str(inner,"line");
  if(!hline || at <= 0 || at <= c->atIn || strlen(hline) != 32) return packet_free(inner)||1;
  util_unhex((unsigned char*)hline,32,lineid);
  c->lined = (memcmp(lineid,c->lineIn,16) == 0)?2:1; // flag for line reset state
  memcpy(c->lineIn,lineid,16); // needed for crypt_line_*

#ifdef CS_1a
  if(c->csid == 0x1a) ret = crypt_line_1a(c,inner);
#endif
#ifdef CS_2a
  if(c->csid == 0x2a) ret = crypt_line_2a(c,inner);
#endif
#ifdef CS_3a
  if(c->csid == 0x3a) ret = crypt_line_3a(c,inner);
#endif
  if(ret) return ret;

  c->atIn = at;
  packet_free(inner);
  return 0;
}
