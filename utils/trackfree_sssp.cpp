#include <graphlab.hpp>
#include <limits>
int NUM_SRC_NODES;
int ITERATIONS = 0;
typedef int ans_type;
ans_type default_ans = std::numeric_limits<ans_type>::max();
bool DIRECTED_GRAPH = true;
struct vertex_data {
  graphlab::automi_bitvec<ans_type> ans;
  vertex_data() {
    ans = graphlab::automi_bitvec<ans_type>(NUM_SRC_NODES);
    ans.set_all(default_ans);
  }
  explicit vertex_data(const graphlab::automi_bitvec<ans_type>& ans) : ans(ans) {}
  void save(graphlab::oarchive &oarc) const {
    oarc << ans;
  }
  void load(graphlab::iarchive &iarc) {
    iarc >> ans;
  }
}; // end of vertex_data

struct edge_data : graphlab::IS_POD_TYPE {
  ans_type dist;
  edge_data(ans_type dist = 1) : dist(dist) {}
}; // end of edge_data

struct msg_type {
  graphlab::automi_bitvec<ans_type> ans;
  msg_type() {
    ans = graphlab::automi_bitvec<ans_type>(NUM_SRC_NODES);
    ans.set_all(default_ans);
  }
  msg_type(ans_type ans_in, size_t idx) {
    ans = graphlab::dimitra_bitvec<ans_type>(NUM_SRC_NODES);
    ans.set_all(default_ans);
    ans.set_single(ans_in, idx);
  }
  msg_type(const graphlab::automi_bitvec<ans_type>& ans_in) {
    ans = graphlab::automi_bitvec<ans_type>(ans_in);
  }
  msg_type& operator+=(const msg_type& other) {
    graphlab::automi_bitvec<ans_type>::pair_op_min(ans, other.ans);
    return *this;
  }
  void save(graphlab::oarchive &oarc) const {
    oarc << ans;
  }
  void load(graphlab::iarchive& iarc) {
    iarc >> ans;
  }
}; // end of msg_type

typedef graphlab::distributed_graph<vertex_data, edge_data> graph_type;
inline graph_type::vertex_type
get_other_vertex(const graph_type::edge_type& edge,
                 const graph_type::vertex_type& vertex) {
  return edge.source().id() == vertex.id() ? edge.target() : edge.source();
}

class vertex_program : public graphlab::ivertex_program<graph_type, msg_type, msg_type> {
  graphlab::automi_bitvec<bool> changed;
public:

  edge_dir_type gather_edges(icontext_type& context,  
                             const vertex_type& vertex) const {  
    return DIRECTED_GRAPH? graphlab::IN_EDGES : graphlab::ALL_EDGES;  
  }
  msg_type gather(icontext_type & context, const vertex_type & vertex, edge_type & edge) const {
    const vertex_type other = get_other_vertex ( edge , vertex ) ;
    msg_type msg = msg_type ( ) ;
    msg.ans.vec_op_add_update(other.data().ans, edge.data().dist);
    msg.ans.vec_op_set(other.data().ans);
    return msg;
  }

  void apply(icontext_type & context, vertex_type & vertex, const msg_type & msg_accum) {

    graphlab::automi_bitvec<bool> mask_1;
    mask_1.vec_op_cmpgt_update(vertex.data().ans, msg_accum.ans);
      vertex.data().ans.vec_op_set_mask(mask_1, msg_accum.ans);
      changed.vec_op_set_mask(mask_1, true);
  }

  void scatter(icontext_type & context, const vertex_type & vertex, edge_type & edge) const {
    const vertex_type other = get_other_vertex ( edge , vertex ) ;
    msg_type msg = msg_type ( ) ;
    graphlab::automi_bitvec<bool> mask_1;
    mask_1.vec_op_set(changed);
      msg.ans.vec_op_set_mask(mask_1, vertex.data().ans);
      if (!mask_1.vec_all_zeros()) {
        context.signal(other, msg);
      }
  }

  edge_dir_type scatter_edges(icontext_type& context,  
                              const vertex_type& vertex) const {  
    return DIRECTED_GRAPH? graphlab::OUT_EDGES : graphlab::ALL_EDGES;  
  }

  void save(graphlab::oarchive &oarc) const {
    oarc << changed;
  }

  void load(graphlab::iarchive& iarc) {
    iarc >> changed;
  }

}; // end of vertex program class
