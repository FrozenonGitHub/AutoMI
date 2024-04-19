/*  
 * Copyright (c) 2009 Carnegie Mellon University. 
 *     All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an "AS
 *  IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 *  express or implied.  See the License for the specific language
 *  governing permissions and limitations under the License.
 *
 * For more about this software visit:
 *
 *      http://www.graphlab.ml.cmu.edu
 *
 */

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

int NUM_SRC_NODES;
int ITERATIONS = 0;

// typedef graphlab::vertex_id_type color_type;
typedef uint64_t color_type;
color_type default_color = 0;

struct vertex_data : graphlab::IS_POD_TYPE {
  color_type color;
  vertex_data() : color(default_color) { }

  explicit vertex_data(const color_type& color) : color(color) { }
};

/*
 * no edge data
 */
typedef graphlab::empty edge_data;
bool EDGE_CONSISTENT = false;


struct msg_type : graphlab::IS_POD_TYPE {
  color_type color;

  msg_type() : color(default_color) { }

  msg_type(color_type color_in) : color(color_in) { }

  /*
   * Combining with another collection of vertices.
   * Union it into the current set.
   */
  msg_type& operator+=(const msg_type& other) {
    color = color | other.color;
    return *this;
  }
};

/*
 * Define the type of the graph
 */
typedef graphlab::distributed_graph<vertex_data,
                                    edge_data> graph_type;

/**
 * \brief Get the other vertex in the edge.
 */
inline graph_type::vertex_type
get_other_vertex(const graph_type::edge_type& edge,
                 const graph_type::vertex_type& vertex) {
  return vertex.id() == edge.source().id()? edge.target() : edge.source();
}

/*
 * On gather, we accumulate a set of all adjacent colors.
 */
class vertex_program:
      public graphlab::ivertex_program<graph_type, msg_type, msg_type>,
      /* I have no data. Just force it to POD */
      public graphlab::IS_POD_TYPE  {
    
  bool changed = false;

public:
  // Gather on all edges
  edge_dir_type gather_edges(icontext_type& context,
                             const vertex_type& vertex) const {
    return graphlab::ALL_EDGES;
  } 

  /*
   * For each edge, figure out the ID of the "other" vertex
   * and accumulate a set of the neighborhood vertex IDs.
   */
  gather_type gather(icontext_type& context,
                     const vertex_type& vertex,
                     edge_type& edge) const {
    const vertex_type other = get_other_vertex(edge, vertex);
    msg_type msg(other.data().color);
    return msg;
  }

  /*
   * the gather result now contains the colors in the neighborhood.
   * pick a different color and store it 
   */
  void apply(icontext_type& context, vertex_type& vertex,
             const gather_type& msg_accum) {
    // find the smallest color not described in the neighborhood
    if (msg_accum.color != 0) {
      color_type temp = msg_accum.color + 1;
      color_type c = temp & ~msg_accum.color;
      if (vertex.data().color != c) {
        vertex.data().color = c;
        changed = true;
      }
    }
  }


  edge_dir_type scatter_edges(icontext_type& context,
                             const vertex_type& vertex) const {
    if (EDGE_CONSISTENT) return graphlab::NO_EDGES;
    else return graphlab::ALL_EDGES;
  } 



  void scatter(icontext_type& context,
              const vertex_type& vertex,
              edge_type& edge) const {
    const vertex_type other = get_other_vertex(edge, vertex);
    msg_type msg = msg_type();
    if (changed) {
      msg.color = vertex.data().color;
      context.signal(other, msg);
    }
  }

};


int main(int argc, char** argv) {
  // Initialize control plain using mpi
  graphlab::mpi_tools::init(argc, argv);
  graphlab::distributed_control dc;
  global_logger().set_log_level(LOG_INFO);

  // Parse command line options -----------------------------------------------
  graphlab::command_line_options 
    clopts("Single-instance Greedy Graph Coloring Algorithm.");
  std::string graph_dir;
  std::string query_dir;
  std::string format = "etuple";
  std::string exec_type = "synchronous";
  size_t powerlaw = 0;
  std::vector<unsigned int> sources;
  clopts.attach_option("graph", graph_dir,
                       "The graph file.  If none is provided "
                       "then a toy graph will be created");
  clopts.add_positional("graph");
  clopts.attach_option("format", format,
                       "graph format");
  clopts.attach_option("source", sources,
                       "The source vertices");

  clopts.attach_option("query", query_dir,
                       "The query file with source vertices."
                       "If none is provided, use default sources.");
  clopts.add_positional("query");

  clopts.attach_option("num_query", NUM_SRC_NODES, 
                       "Number of queries to load");

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
  clopts.attach_option("edgescope", EDGE_CONSISTENT,
                       "Use Locking. ");

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
  if (graph_dir.length() > 0) { // Load the graph from a file
    dc.cout() << "Loading graph in format: "<< format << std::endl;
    graph.load_uw_format(graph_dir, format);
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

  double runtime = 0.0;
  // Process source vertices one by one
  for(size_t i = 0; i < sources.size(); ++i) {
    engine.signal_source(sources[i]);
    timer.start();
    engine.start();
    const double instance_time = timer.current_time();
    runtime += instance_time;
    engine.reset();
  }

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
