#ifndef _DEFINES_H
#define _DEFINES_H

#include "prototyp.h"

typedef struct node node_t;
typedef struct node *node_p;

typedef struct arc arc_t;
typedef struct arc *arc_p;

struct node
{
  int orientation;
  long depth; 
  int number;
  int time;
};

struct arc
{
  node_p tail, head;
  int ident;
  arc_p nextout, nextin;
};

typedef struct network
{
  char inputfile[200];
  char clustfile[200];
  long n, n_trips;
  long max_m, m, m_org, m_impl;
  long max_residual_new_m, max_new_m;
  long primal_unbounded;
  long dual_unbounded;
  long perturbed;
  long feasible;
  long eps;
  long opt_tol;
  long feas_tol;
  long pert_val;
  long bigM;
  double optcost;  
  node_p nodes, stop_nodes;
  arc_p arcs, stop_arcs;
  arc_p dummy_arcs, stop_dummy; 
  long iterations;
  long bound_exchanges;
  long checksum;
} network_t;


#endif