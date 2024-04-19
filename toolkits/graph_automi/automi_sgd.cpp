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


#include <graphlab/util/stl_util.hpp>
#include <graphlab.hpp>

#include <Eigen/Dense>
#include "eigen_serialization.hpp"
#include <graphlab/macros_def.hpp>
#include <vector>

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


typedef Eigen::VectorXd vec_type;
typedef Eigen::MatrixXd mat_type;

//when using negative node id range, we are not allowed to use
//0 and 1 so we add 2.
const static int SAFE_NEG_OFFSET=2;
int iter = 0;

// bool isuser(uint node){
// 	return ((int)node) >= 0;
// }

// bool isuser_node(const graph_type::vertex_type& vertex){
// 	return isuser(vertex.id());
// }

/**
 * \brief The vertex data type which contains the latent pvec.
 */
struct vertex_data {
  static size_t NLATENT;
  std::vector<vec_type> pvec;

  vertex_data() {
    pvec.resize(NUM_SRC_NODES);
    for (size_t i = 0; i < NUM_SRC_NODES; i++) {
        pvec[i].resize(NLATENT);
        pvec[i].setRandom();    // radomize the latent vector
    }
  }

  // serialize
  void save(graphlab::oarchive& oarc) const {
    for (size_t i = 0; i < NUM_SRC_NODES; i++) {
      oarc << pvec[i];
    }
    // oarc << nupdates;
  }
  // deserialize
  void load(graphlab::iarchive& iarc) { 
    for (size_t i = 0; i < NUM_SRC_NODES; i++) {
      iarc >> pvec[i];
    }
    // iarc >> nupdates;
  }

};  // end of vertex data

/**
 * \brief The edge data stores the entry in the matrix.
 *
 * In addition the edge data sgdo stores the most recent error estimate.
 */
struct edge_data : public graphlab::IS_POD_TYPE {
	/**
	 * \brief The type of data on the edge;
	 *
	 * \li *Train:* the observed value is correct and used in training
	 * \li *Validate:* the observed value is correct but not used in training
	 * \li *Predict:* The observed value is not correct and should not be
	 *        used in training.
	 */
	enum data_role_type { TRAIN, VALIDATE, PREDICT  };

	/** \brief the observed value for the edge */
	float obs;

	/** \brief The train/validation/test designation of the edge */
	data_role_type role;

	/** \brief basic initialization */
	edge_data(float obs = 0, data_role_type role = PREDICT) :
		obs(obs), role(role) { }

}; // end of edge data

/**
 * \brief The graph type is defined in terms of the vertex and edge
 * data.
 */ 
typedef graphlab::distributed_graph<vertex_data, edge_data> graph_type;

/**
 * \brief Given a vertex and an edge return the other vertex in the
 * edge.
 */
inline graph_type::vertex_type
get_other_vertex(graph_type::edge_type& edge, 
		const graph_type::vertex_type& vertex) {
	return vertex.id() == edge.source().id()? edge.target() : edge.source();
}; // end of get_other_vertex


class msg_type {
  public:
    std::vector<vec_type> pvec;
    graphlab::automi_bitvec<bool> track;

    msg_type() { 
        pvec.resize(NUM_SRC_NODES);
        track = graphlab::automi_bitvec<bool>(NUM_SRC_NODES);
        track.set_all(false);    
    }

    // constructor for source vertices initialization
    msg_type(bool track_init, size_t idx) {
        pvec.resize(NUM_SRC_NODES);
        track = graphlab::automi_bitvec<bool>(NUM_SRC_NODES);
        track.set_all(false);
        track.set_single(track_init, idx);
    }

    msg_type(const graphlab::automi_bitvec<bool>& track_in) : track(track_in) {
        pvec.resize(NUM_SRC_NODES);
    }

    msg_type(const std::vector<vec_type>& pvec_in, const graphlab::automi_bitvec<bool>& track_in) : pvec(pvec_in), track(track_in) { }

    msg_type& operator+=(const msg_type& other) {
      graphlab::automi_bitvec<bool>::pair_op_or(track, other.track);
      for (size_t i = 0; i < NUM_SRC_NODES; i++) {
        if (pvec[i].size() == 0) {
            pvec[i] = other.pvec[i];
        } else if (other.pvec[i].size() != 0) {
            pvec[i] += other.pvec[i];
        }
      }
      return *this;
    }

    // serialize
    void save(graphlab::oarchive& oarc) const {
      for (size_t i = 0; i < NUM_SRC_NODES; i++) {
        oarc << pvec[i];
      }
      oarc << track;
    }

    // deserialize
    void load(graphlab::iarchive& iarc) {
      for (size_t i = 0; i < NUM_SRC_NODES; i++) {
        iarc >> pvec[i];
      }
      iarc >> track;
    }
};


class vertex_program:
      public graphlab::ivertex_program<graph_type, msg_type, msg_type> {

    graphlab::automi_bitvec<bool> vp_track;

public:
  static double TOLERANCE;
  static double LAMBDA;
  static double GAMMA;
  static double MAXVAL;
  static double MINVAL;
  static double STEP_DEC;
  static bool debug;
  static size_t MAX_UPDATES;

  /** The set of edges to gather along */
  edge_dir_type gather_edges(icontext_type& context, const vertex_type& vertex) const { 
    return graphlab::ALL_EDGES; 
  } // end of gather_edges

  msg_type gather(icontext_type& context, const vertex_type& vertex,
                     edge_type& edge) const {
    msg_type msg;
    vertex_type other_vertex = get_other_vertex(edge, vertex);
    for (size_t i = 0; i < NUM_SRC_NODES; i++) {
      double pred = vertex.data().pvec[i].dot(other_vertex.data().pvec[i]);
      // truncate predictions into allowed range
      pred = std::min(MAXVAL, pred);
      pred = std::max(MINVAL, pred);
      // compute the prediction error
      const float err = edge.data().obs - pred;
      if (edge.data().role == edge_data::TRAIN) {
        msg.pvec[i] = err * other_vertex.data().pvec[i];
      } else {
        msg.pvec[i] = vec_type::Zero(vertex_data::NLATENT);
      }
    }
    return msg;
  }

  void apply(icontext_type& context, vertex_type& vertex, const msg_type& msg_accum) {
    vertex_data& vdata = vertex.data();
    vp_track = msg_accum.track;
    for (size_t i = 0; i < NUM_SRC_NODES; i++) {
      if (!vp_track.test_bit(i)) {
        continue;
      }
      vec_type temp = sum.pvec[i] - LAMBDA * vdata.pvec[i];
      vdata.pvec[i] += GAMMA * temp;
    }
  } // end of apply

  /** The edges to scatter along */
  edge_dir_type scatter_edges(icontext_type& context, const vertex_type& vertex) const { 
    return graphlab::ALL_EDGES; 
  }; // end of scatter edges

  /** Scatter reschedules neighbors */  
  void scatter(icontext_type& context, const vertex_type& vertex, edge_type& edge) const {
    edge_data& edata = edge.data();
    if(edata.role == edge_data::TRAIN) {
      const vertex_type other_vertex = get_other_vertex(edge, vertex);
      msg_type msg = msg_type(vp_track); 
      context.signal(other_vertex, msg);
    }
  } // end of scatter function

  // serialize
  void save(graphlab::oarchive& oarc) const {
    oarc << vp_track;
  }

  // deserialize
  void load(graphlab::iarchive& iarc) {
    iarc >> vp_track;
  }
};  // end of vertex program

/**
 * \brief The graph loader function is a line parser used for
 * distributed graph construction.
 */
inline bool graph_loader(graph_type& graph, 
		const std::string& filename,
		const std::string& line) {

	// Parse the line
	std::stringstream strm(line);
	graph_type::vertex_id_type source_id(-1), target_id(-1);
	float obs(0);
	strm >> source_id >> target_id;

	if (source_id == graph_type::vertex_id_type(-1) || target_id == graph_type::vertex_id_type(-1)){
		logstream(LOG_WARNING)<<"Failed to read input line: "<< line << " in file: "  << filename << " (or node id is -1). " << std::endl;
		return true;
	}

	// Determine the role of the data
	edge_data::data_role_type role = edge_data::TRAIN;
	if(boost::ends_with(filename,".validate")) role = edge_data::VALIDATE;
	else if(boost::ends_with(filename, ".predict")) role = edge_data::PREDICT;

	if(role == edge_data::TRAIN || role == edge_data::VALIDATE){
		strm >> obs;
		if (obs < vertex_program::MINVAL || obs > vertex_program::MAXVAL){
			logstream(LOG_WARNING)<<"Rating values should be between " << vertex_program::MINVAL << " and " << vertex_program::MAXVAL << ". Got value: " << obs << " [ user: " << source_id << " to item: " <<target_id << " ] " << std::endl; 
			assert(false); 
		}
	}
	target_id = -(graphlab::vertex_id_type(target_id + SAFE_NEG_OFFSET));
	// Create an edge and add it to the graph
	graph.add_edge(source_id, target_id, edge_data(obs, role)); 
	return true; // successful load
} // end of graph_loader

size_t vertex_data::NLATENT = 5;
double vertex_program::TOLERANCE = 1e-3;
double vertex_program::LAMBDA = 0.001;
double vertex_program::GAMMA = 0.001;
size_t vertex_program::MAX_UPDATES = 20;
double vertex_program::MAXVAL = 1e+100;
double vertex_program::MINVAL = -1e+100;
double vertex_program::STEP_DEC = 0.9;


int main(int argc, char** argv) {
  // Initialize control plain using mpi
  graphlab::mpi_tools::init(argc, argv);
  graphlab::distributed_control dc;
  global_logger().set_log_level(LOG_INFO);

  // Parse command line options -----------------------------------------------
  graphlab::command_line_options 
    clopts("Collaborative Filtering (SGD).");
  std::string graph_dir;
  std::string query_dir;  // added for multi-instance
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
  if (graph_dir.length() > 0) { // Load the graph from file
    dc.cout() << "Loading graph" << std::endl;
    graph.load(graph_dir, graph_loader);
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

  for(size_t i = 0; i < sources.size(); ++i) {
    engine.signal_source(sources[i], msg_type(true, i));
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