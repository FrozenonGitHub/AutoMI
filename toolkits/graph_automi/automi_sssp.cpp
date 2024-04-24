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

/**
 * \brief Parameter #sources for vertex program
 */
int NUM_SRC_NODES;
int ITERATIONS = 0;

/**
 * \brief The type used to measure distances in the graph.
 */
typedef int ans_type;
ans_type default_ans = std::numeric_limits<ans_type>::max();
bool DIRECTED_GRAPH = true;

/**
 * \brief The current distance of the vertex.
 */
struct vertex_data {
  graphlab::automi_bitvec<ans_type> ans;

  vertex_data() {
    ans = graphlab::automi_bitvec<ans_type>(NUM_SRC_NODES);
    ans.set_all(std::numeric_limits<ans_type>::max());
  }

  explicit vertex_data(const graphlab::automi_bitvec<ans_type>& ans_in) : ans(ans_in) {}

  void save(graphlab::oarchive &oarc) const {
    oarc << ans;
  }

  void load(graphlab::iarchive& iarc) {
    iarc >> ans;
  }
};  // end of vertex data

/**
 * \brief The distance associated with the edge.
 */
struct edge_data : graphlab::IS_POD_TYPE {
  ans_type dist;
  edge_data(ans_type dist = 1) : dist(dist) { }
}; // end of edge data

/**
 * \brief The graph type encodes the distances between vertices and
 * edges
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
struct msg_type {
  graphlab::automi_bitvec<ans_type> ans;
  // constructor with single distance value, used during initialization
  msg_type() {
    ans = graphlab::automi_bitvec<ans_type>(NUM_SRC_NODES);
    ans.set_all(default_ans);
  }
  // constructor with single distance value, used during initialization
  msg_type(ans_type init_ans, size_t idx) {
    ans = graphlab::automi_bitvec<ans_type>(NUM_SRC_NODES);
    ans.set_all(default_ans);
    ans.set_single(init_ans, idx);
  }
  // constructor with distance array
  msg_type(const graphlab::automi_bitvec<ans_type>& ans_in) {
    ans = graphlab::automi_bitvec<ans_type>(ans_in);
  }

  msg_type& operator+=(const msg_type& other) {
    graphlab::automi_bitvec<ans_type>::pair_op_min(ans, other.ans);
    return *this;
  }
  // serialization method
  void save(graphlab::oarchive &oarc) const {
    oarc << ans;
  }
  void load(graphlab::iarchive& iarc) {
    iarc >> ans;
  }
};  // end of msg_type


/**
 * \brief The vertex program class.
 */
class vertex_program :
  public graphlab::ivertex_program<graph_type, msg_type, msg_type> {

    graphlab::automi_bitvec<bool> changed;

public: 

    edge_dir_type gather_edges(icontext_type& context,
                               const vertex_type& vertex) const {
        return DIRECTED_GRAPH ? graphlab::IN_EDGES : graphlab::ALL_EDGES;
    }

    // Gather function
    msg_type gather(icontext_type& context, const vertex_type& vertex,
                       edge_type& edge) const {
        const vertex_type other = get_other_vertex(edge, vertex);
        msg_type msg = msg_type();
        msg.ans.vec_op_add_update(other.data().ans, edge.data().dist);
        return msg;
    }

    // Apply function
    void apply(icontext_type& context, vertex_type& vertex,
                 const msg_type& msg_accum) {
        graphlab::automi_bitvec<bool> mask;
        mask.vec_op_cmpgt_update(vertex.data().ans, msg_accum.ans);
        vertex.data().ans.vec_op_set_mask(mask, msg_accum.ans);
        changed.vec_op_set_mask(mask, true);
    };

    // scatter_nbrs function
    edge_dir_type scatter_edges(icontext_type& context, 
                                const vertex_type& vertex) const {
      return DIRECTED_GRAPH? graphlab::OUT_EDGES : graphlab::ALL_EDGES;
    }

    // Scatter function
    void scatter(icontext_type& context, const vertex_type& vertex,
                 edge_type& edge) const {
        if (!changed.vec_all_zeros()) {
          const vertex_type other = get_other_vertex(edge, vertex);
          msg_type msg = msg_type(vertex.data().ans);
          context.signal(other, msg);
        }
    }

  void save(graphlab::oarchive &oarc) const {
    oarc << changed;
  }

  void load(graphlab::iarchive& iarc) {
    iarc >> changed;
  }

}; // end of vertex program


int main(int argc, char** argv) {
  // Initialize control plain using mpi
  graphlab::mpi_tools::init(argc, argv);
  graphlab::distributed_control dc;
  global_logger().set_log_level(LOG_INFO);

  // Parse command line options -----------------------------------------------
  graphlab::command_line_options 
    clopts("AutoMI Single Source Shortest Path Algorithm.");
  std::string graph_dir;
  std::string query_dir;
  std::string format = "wtuple";
  std::string exec_type = "synchronous";
  size_t powerlaw = 0;
  std::vector<unsigned int> sources;
  bool max_degree_source = false;
  clopts.attach_option("graph", graph_dir,
                       "The graph file.  If none is provided "
                       "then a toy graph will be created");
  clopts.add_positional("graph");
  clopts.attach_option("format", format,
                       "graph format");
  clopts.attach_option("source", sources,
                       "The source vertices");
  clopts.attach_option("max_degree_source", max_degree_source,
                       "Add the vertex with maximum degree as a source");

  clopts.attach_option("query", query_dir,
                       "The query file with source vertices."
                       "If none is provided, use default sources.");
  clopts.add_positional("query");

  clopts.attach_option("num_query", NUM_SRC_NODES, 
                       "Number of queries to load");

  clopts.attach_option("directed", DIRECTED_GRAPH,
                       "Treat edges as directed.");

  clopts.attach_option("iterations", ITERATIONS,
                       "If set, will force the use of the synchronous engine"
                       "overriding any engine option set by the --engine parameter. "
                       "Runs complete (non-dynamic) PageRank for a fixed "
                       "number of iterations. Also overrides the iterations "
                       "option in the engine");

  clopts.attach_option("engine", exec_type, 
                       "The engine type synchronous or asynchronous");

  clopts.attach_option("powerlaw", powerlaw,
                       "Generate a synthetic powerlaw out-degree graph. ");

  if(!clopts.parse(argc, argv)) {
    dc.cout() << "Error in parsing command line arguments." << std::endl;
    return EXIT_FAILURE;
  }

  if (ITERATIONS) {
    // make sure this is the synchronous engine
    dc.cout() << "--iterations set. Forcing Synchronous engine, and running "
              << "for " << ITERATIONS << " iterations." << std::endl;
    //clopts.get_engine_args().set_option("type", "synchronous");
    clopts.get_engine_args().set_option("max_iterations", ITERATIONS);
  }

  // Load the queries ----------------------------------------------------------
  if (query_dir.length() > 0) { // Load the queries from a file
    std::ifstream query_input_stream;
    query_input_stream.open(query_dir);
    std::string buf = "", str1 = "";
    nextLine(query_input_stream, buf);  // skipping line "num_queries=XXX,"
    buf.clear();
    nextLine(query_input_stream, buf);
    int startPos = 0, currentPos = 0;
    currentPos = buf.find(',', startPos);
    while (currentPos != std::string::npos) {
      str1 = buf.substr(startPos, currentPos - startPos);
      sources.push_back(static_cast<unsigned int>(std::stoul(str1)));
      startPos = currentPos + 1;
      currentPos = buf.find(',', startPos);
    }
    // remove excessive queries
    if (sources.size() > NUM_SRC_NODES) {
      sources.resize(NUM_SRC_NODES);
    }
    dc.cout() << "Loaded #queries: " << sources.size() << std::endl;
  } else {
    if (sources.empty()) {
      dc.cout() << "No source vertex provided. Using default 8 sources [1, 8]" << std::endl;
      sources = {1, 2, 3, 4, 5, 6, 7, 8};
      NUM_SRC_NODES = 8;
    } else {  // sources provided in command line options
      NUM_SRC_NODES = sources.size();
      dc.cout() << "Using " << NUM_SRC_NODES << " source vertices from command line options." << std::endl;
    }
  }

  // Build the graph ----------------------------------------------------------
  dc.cout() << "Loading graph." << std::endl;
  graphlab::timer timer;
  graph_type graph(dc, clopts);
  if(powerlaw > 0) { // make a synthetic graph
    dc.cout() << "Loading synthetic Powerlaw graph." << std::endl;
    graph.load_synthetic_powerlaw(powerlaw, false, 2, 100000000);
  } else if (graph_dir.length() > 0) { // Load the graph from a file
    dc.cout() << "Loading graph in format: "<< format << std::endl;
    graph.load_format(graph_dir, format);
  } else {
    dc.cout() << "graph or powerlaw option must be specified" << std::endl;
    clopts.print_description();
    return EXIT_FAILURE;
  }
  const double loading = timer.current_time();
  dc.cout() << "Loading graph. Finished in " 
            << loading << std::endl;

  // must call finalize before querying the graph
  dc.cout() << "Finalizing graph." << std::endl;
  timer.start();
  graph.finalize();
  const double finalizing = timer.current_time();
  dc.cout() << "Finalizing graph. Finished in " 
            << finalizing << std::endl;

  // NOTE: ingress time = loading time + finalizing time
  const double ingress = loading + finalizing;
  dc.cout() << "Final Ingress (second): " << ingress << std::endl;

  dc.cout() << "Final Ingress (second): " << ingress << std::endl;

  dc.cout() << "#vertices: " << graph.num_vertices()
            << " #edges:" << graph.num_edges() << std::endl;

  // Running The Engine -------------------------------------------------------
  graphlab::omni_engine<vertex_program> engine(dc, graph, exec_type, clopts);

  // Signal all the vertices in the source set
  for(size_t i = 0; i < sources.size(); ++i) {
    engine.signal_source(sources[i], msg_type(0, i));
  }

  timer.start();
  engine.start();
  const double runtime = timer.current_time();
  dc.cout() << "----------------------------------------------------------"
            << std::endl
            << "Final Runtime (seconds):   " << runtime 
            << std::endl
            << "Updates executed: " << engine.num_updates() << std::endl
            << "Update Rate (updates/second): " 
            << engine.num_updates() / runtime << std::endl;

  // Tear-down communication layer and quit -----------------------------------
  graphlab::mpi_tools::finalize();
  return EXIT_SUCCESS;
} // End of main


// We render this entire program in the documentation



