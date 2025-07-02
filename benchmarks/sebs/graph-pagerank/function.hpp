
#include <igraph.h>
#include <cfloat>

igraph_real_t graph_pagerank(int size)
{
  igraph_t graph;
  igraph_vector_t pagerank;
  igraph_real_t value;


  igraph_barabasi_game(
    /* graph=    */ &graph,
    /* n=        */ size,
    /* power=    */ 1,
    /* m=        */ 10,
    /* outseq=   */ NULL,
    /* outpref=  */ 0,
    /* A=        */ 1.0,
    /* directed= */ 0,
    /* algo=     */ IGRAPH_BARABASI_PSUMTREE_MULTIPLE,
    /* start_from= */ 0
  );

  igraph_vector_init(&pagerank, 0);
  igraph_pagerank(&graph, IGRAPH_PAGERANK_ALGO_PRPACK,
                  &pagerank, &value,
                  igraph_vss_all(), IGRAPH_DIRECTED,
                  /* damping */ 0.85, /* weights */ NULL,
                  NULL /* not needed with PRPACK method */);

  /* Check that the eigenvalue is 1, as expected. */
  if (fabs(value - 1.0) > 32*DBL_EPSILON) {
      fprintf(stderr, "PageRank failed to converge.\n");
      return 1;
  }

  igraph_real_t result = VECTOR(pagerank)[0];

  igraph_vector_destroy(&pagerank);
  igraph_destroy(&graph);
  
  return result;
}


