#include <vector>
#include <string>
#include <fstream>


#include <graphlab.hpp>

// helper function to be moved elsewhere
bool nextLine(std::ifstream &input_fs, std::string &buf) {
  if (input_fs)
    getline(input_fs, buf);
  else 
    return false;
  return true;
}

#pragma AUTOMI
int NUM_SRC_NODES;
int ITERATIONS = 0;
typedef int ans_type;
ans_type default_ans = std::numeric_limits<ans_type>::max();
bool DIRECTED_GRAPH = true;

/**
 * \brief struct vertex_data for vertex property
 */
struct vertex_data : graphlab::IS_POD_TYPE {
    ans_type ans;

    vertex_data(ans_type ans = default_ans) : ans(ans) { }

    explicit vertex_data(const ans_type& ans_in) : ans(ans_in) { }
};

/**
 * \brief struct edge_data
 */
struct edge_data : graphlab::IS_POD_TYPE {
  ans_type dist;
  edge_data(ans_type dist = 1) : dist(dist) { }
}; // end of edge data

/**
 * \brief The graph type
 */
typedef graphlab::distributed_graph<vertex_data, edge_data> graph_type;

/**
 * \brief Get the other vertex in the edge.
 */
inline graph_type::vertex_type
get_other_vertex(const graph_type::edge_type& edge,
                 const graph_type::vertex_type& vertex) {
  return vertex.id() == edge.source().id()? edge.target() : edge.source();
}

/**
 * \brief struct msg_type
 */
struct msg_type : graphlab::IS_POD_TYPE {
    ans_type ans;
    msg_type(ans_type ans = default_ans) : ans(ans) {}

    msg_type& operator+=(const msg_type& other) {
        ans = std::min(ans, other.ans);
        return *this;
    }
};

/**
 * \brief The vertex_program class.
 */
class vertex_program :
  public graphlab::ivertex_program<graph_type, msg_type, msg_type>,
  public graphlab::IS_POD_TYPE {

  bool changed;

public:

  edge_dir_type gather_edges(icontext_type& context,
                             const vertex_type& vertex) const {
    return DIRECTED_GRAPH? graphlab::IN_EDGES : graphlab::ALL_EDGES;
  }

    // Gather function
    msg_type gather(icontext_type& context, const vertex_type& vertex,
                       edge_type& edge) const {
        const vertex_type other = get_other_vertex(edge, vertex);
        msg_type msg = msg_type();
        msg.ans = other.data().ans + edge.data().dist;
        msg.ans = other.data().ans;
        return msg;
    }

  void apply(icontext_type& context, vertex_type& vertex,
             const msg_type& msg_accum) {
    if (vertex.data().ans > msg_accum.ans) {
      vertex.data().ans = msg_accum.ans;
      changed = true;
    }
  }

  edge_dir_type scatter_edges(icontext_type& context,
                              const vertex_type& vertex) const {
    return DIRECTED_GRAPH? graphlab::OUT_EDGES : graphlab::ALL_EDGES;
  }

  void scatter(icontext_type& context, const vertex_type& vertex,
               edge_type& edge) const {
    const vertex_type other = get_other_vertex(edge, vertex);
    msg_type msg = msg_type();
    if (changed) {
        msg.ans = vertex.data().ans;
        context.signal(other, msg);
    }
  }

}; // end of vertex program


