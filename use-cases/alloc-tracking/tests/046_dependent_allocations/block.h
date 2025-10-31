
#ifdef MA_MAIN
#define EXTERN
#else
#define EXTERN extern
#endif

typedef long long num_sz;

typedef struct {
   num_sz number;
   num_sz num_prime;
   int level;
   int refine;
   int new_proc;
   int b_type;
   num_sz parent;       // if original block -1,
                     // else if on node, number in structure
                     // else (-2 - parent->number)
   int parent_node;
   int child_number;
   int nei_refine[6];
   int nei_level[6];  /* 0 to 5 = W, E, S, N, D, U; use -2 for boundary */
   int nei[6][2][2];  /* negative if off processor (-1 - proc) */
   int cen[3];
   double ****array;
} block;
EXTERN block *blocks;

EXTERN num_sz *num_blocks;
EXTERN num_sz *local_num_blocks;

EXTERN int max_num_blocks;
EXTERN int num_refine;
EXTERN int num_vars;
EXTERN int x_block_size, y_block_size, z_block_size;
EXTERN int init_block_x, init_block_y, init_block_z;
