//Single Source Shortest Path
//works for both directed graph and undirected graph

#include "ol/pregel-ol-dev.h"
#include "utils/type.h"

string in_path = "/toy_out"; //for running with a directed graph
//string in_path = "/toy_ug"; //for running with an undirected graph
string out_path = "/ol_out";

//input line format: vertexID \t numOfNeighbors neighbor1 neighbor2 ...
//edge lengths are assumed to be 1

//output line format: v \t minimum_hops

//--------------------------------------------------

//Step 1: define static field of vertex: edge list
struct Edge{
    int nb_id;
    int len;

	Edge() {}

    Edge(int nb_id, int len) {
        this->nb_id = nb_id;
        this->len = len;
    }
};

ibinstream & operator<<(ibinstream & m, const Edge & e){
	m << e.nb_id;
	m << e.len;
	return m;
}

obinstream & operator>>(obinstream & m, Edge & e){
	m >> e.nb_id;
	m >> e.len;
	return m;
}

struct SPNQValue
{
	vector<Edge> edges;
};

ibinstream & operator<<(ibinstream & m, const SPNQValue & v){
	m<<v.edges;
	return m;
}

obinstream & operator>>(obinstream & m, SPNQValue & v){
	m>>v.edges;
	return m;
}

struct QValue
{
    int ans;
    QValue() {
        ans = INT_MAX;
    }

	QValue(int ans) : ans(ans) {}
};

//--------------------------------------------------

//Step 2: define query type: here, it is int (src)
//Step 3: define query-specific vertex state: here, it is hop_to_src (int)
//Step 4: define msg type: here, it is int

//--------------------------------------------------
//Step 5: define vertex class

class SPVertex : public VertexOL<VertexID, QValue, SPNQValue, int, int>
{
    public:

        //Step 5.1: define UDF1: query -> vertex's query-specific init state
        virtual QValue init_value(int& query)
        {
            if(id==query) return QValue(0);
            else return QValue(INT_MAX);
        }

        //Step 5.2: vertex.compute
        virtual void compute(MessageContainer& messages) {
            if (superstep() == 1) {
                // broadcast to activate neighbors of source vertex
                vector<Edge>& edges = nqvalue().edges;
                for (int i = 0; i < edges.size(); i++) {
                    send_message(edges[i].nb_id, edges[i].len);
                }
            } else {
                int temp = qvalue().ans;
                for (int i = 0; i < messages.size(); i++) {
                    if (messages[i] < temp) {
                        temp = messages[i];
                    }
                }
                if (temp < qvalue().ans) {
                    qvalue().ans = temp;
                    vector<Edge>& edges = nqvalue().edges;
                    for (int i = 0; i < edges.size(); i++) {
                        send_message(edges[i].nb_id, qvalue().ans + edges[i].len);
                    }
                } else {
					vote_to_halt();
				}
            }
        }
};

//--------------------------------------------------
//Step 6: define worker class
class SPWorkerOL:public WorkerOL_auto<SPVertex>
{
	public:
		char buf[50];
        // Create a random number generator
        // std::random_device rd;

		//Step 6.1: UDF: line -> vertex
		virtual SPVertex* toVertex(char* line)
		{
			char * pch;
			pch=strtok(line, "\t");
			SPVertex* v=new SPVertex;
			int id=atoi(pch);
			v->id=id;
			strtok(NULL, " ");//skip neighbor_number

			while(pch=strtok(NULL, " "))
			{
				int nb=atoi(pch);

				pch=strtok(NULL, " ");
				int weight = atoi(pch);

				v->nqvalue().edges.push_back(Edge(nb, weight));
			}
			return v;
		}

		//Step 6.2: UDF: query string -> query (src_id)
		virtual int toQuery(char* line)
		{
			return atoi(line);	// one query (int src_id) per line
			// char * pch;
			// pch=strtok(line, " ");
			// return atoi(pch);
			// pch=strtok(NULL, " ");
			// int dst=atoi(pch);
			// return intpair(src, dst);
		}

		//Step 6.3: UDF: vertex init
		virtual void init(VertexContainer& vertex_vec)
		{
			int src=get_query();
			int pos=get_vpos(src);
			if(pos!=-1) activate(pos);
		}

		//Step 6.4: UDF: task_dump
		virtual void dump(SPVertex* vertex, BufferedWriter& writer)
		{
			// if(vertex->id != get_query().v2) return;
			sprintf(buf, "%d\t%d\n", vertex->id, vertex->qvalue().ans);
			writer.write(buf);
		}
};

// TODO: check if default "old" is INT_MAX?

class SPCombiner:public Combiner<int>
{
    public:
        virtual void combine(int& old, const int& new_msg) {
			if (old > new_msg) old = new_msg;
        }
};

int main(int argc, char* argv[]){
	WorkerParams param;
    if (argc < 2) {
        if (_my_rank==MASTER_RANK) {
            cout << "Usage: ./run <in_path> [out_path]" << endl;
            return -1;
        }
    }
	// param.input_path=in_path;
	// param.output_path=out_path;
    param.input_path = argv[1];
	if (argc > 2) {
		param.output_path = argv[2];
	} else {
		param.output_path = out_path;
	}

	param.force_write=false;
	param.native_dispatcher=false;

	SPWorkerOL worker;
	SPCombiner combiner;
    bool use_combiner = true;
	if(use_combiner) worker.setCombiner(&combiner);
	worker.run(param);
	return 0;
}