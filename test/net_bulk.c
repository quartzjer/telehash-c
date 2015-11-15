#include "ext.h"
#include "net_loopback.h"
#include "unit_test.h"

int bulked = 0;
void bulk_handler(link_t link, e3x_channel_t chan, void *arg)
{
  lob_t packet;

  while((packet = e3x_channel_receiving(chan)))
  {
    e3x_channel_send(chan,e3x_channel_packet(chan));
    lob_free(packet);
    bulked++;
  }
  
}

e3x_channel_t chan = NULL;
lob_t bulk_on_open(link_t link, lob_t open)
{
  if(lob_get_cmp(open,"type","bulk")) return open;
  
  LOG("incoming bulk start");

  // create new channel, set it up, then receive this open
  chan = link_channel(link, open);
  link_handle(link,chan,bulk_handler,NULL);
  lob_t reply = lob_new();
  lob_set(reply,"c",lob_get(open,"c"));
  int ret = e3x_channel_receive(chan,open,0); // consumes the open
  printf("Done setting up channel ret: %d\n", ret);

  link_direct(link,reply,NULL);

  return NULL;
}


int main(int argc, char **argv)
{
  lob_t opt = lob_new();
  lob_set(opt,"force","1a");
  e3x_init(opt);
  lob_free(opt);

  mesh_t meshA = mesh_new(3);
  fail_unless(meshA);
  lob_t secretsA = e3x_generate();
  fail_unless(secretsA);
  fail_unless(!mesh_load(meshA, secretsA, lob_linked(secretsA)));
  lob_free(secretsA);

  mesh_t meshB = mesh_new(3);
  fail_unless(meshB);
  lob_t secretsB = mesh_generate(meshB);
  fail_unless(secretsB);
  lob_free(secretsB);

  net_loopback_t pair = net_loopback_new(meshA,meshB);
  fail_unless(pair);
  
  link_t linkAB = link_get(meshA, meshB->id->hashname);
  link_t linkBA = link_get(meshB, meshA->id->hashname);
  fail_unless(linkAB);
  fail_unless(linkBA);

  lob_t hs = link_resync(linkAB);
  fail_unless(hs);
  lob_free(hs);

  mesh_on_open(meshA, "bulk", bulk_on_open);
  lob_t bulkBA = lob_new();
  lob_set(bulkBA,"type","bulk");
  uint32_t cid = e3x_exchange_cid(linkBA->x, NULL);
  lob_set_uint(bulkBA,"c",cid);
  fail_unless(link_receive(linkAB, bulkBA, NULL));
  fail_unless(chan);

  int i;
  for(i=0;i<100;i++)
  {
    lob_t bulk = lob_new();
    lob_set_uint(bulk,"c",cid);
    fail_unless(link_receive(linkAB, bulk, NULL));
  }
  
  LOG("bulked %d",bulked);
  fail_unless(bulked == i+1);
  
  mesh_free(meshA);
  mesh_free(meshB);
  net_loopback_free(pair);

  return 0;
}
