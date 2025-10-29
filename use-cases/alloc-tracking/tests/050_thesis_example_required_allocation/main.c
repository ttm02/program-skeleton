#include <stdio.h>
#include <stdlib.h>

#include "defines.h"
#include "test.h"

network_t net;

#ifdef _PROTO_
void read_min( network_t *net )
#else
void read_min( net )
     network_t *net;
#endif
{
    net->n = 10;
    net->nodes      = (node_t *) calloc( net->n, sizeof(node_t) );
    net->dummy_arcs = (arc_t *)  calloc( net->n,   sizeof(arc_t) );
    net->arcs       = (arc_t *)  calloc( net->max_m,   sizeof(arc_t) );

    if( !( net->nodes && net->arcs && net->dummy_arcs ) )
    {
      printf( "read_min(): not enough memory\n" );
      return;
    }

    net->stop_nodes = net->nodes + net->n + 1;
}

int main() {
    read_min(&net);
    
    return 0;
}
