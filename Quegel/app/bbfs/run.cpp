//Boolean BFS
//works for both directed graph and undirected graph

#include "ol/pregel-ol-dev.h"
#include "utils/type.h"

string in_path = "/toy_out"; //for running with a directed graph
//string in_path = "/toy_ug"; //for running with an undirected graph
string out_path = "/ol_out";

//input line format: vertexID \t numOfNeighbors neighbor1 neighbor2 ...
//edge lengths are assumed to be 1

//output line format: v \t reachability(true/false)

//--------------------------------------------------

//Step 1: define static field of vertex: adj-list
struct BFSValue
{
	vector<int> nbs;
};

ibinstream & operator<<(ibinstream & m, const BFSValue & v){
	m<<v.nbs;
	return m;
}

obinstream & operator>>(obinstream & m, BFSValue & v){
	m>>v.nbs;
	return m;
}

//--------------------------------------------------

//Step 2: define query type: here, it is int (src)
//Step 3: define query-specific vertex state: here, it is reachability (bool)
//Step 4: define msg type: here, it is just a signal, can be anything like char

//--------------------------------------------------
//Step 5: define vertex class

class BFSVertex:public VertexOL<VertexID, bool, BFSValue, bool, int>
{
	public:

        //Step 5.1: define UDF1: query -> vertex's query-specific init state
        virtual bool init_value(int& query) {
            if(id==query) return true;
            else return false;
        }

        //Step 5.2: vertex.compute
        virtual void compute(MessageContainer& messages) {
            if(superstep()==1) {
                // broadcast to activate neighbors of source vertex?
                vector<int>& nbs = nqvalue().nbs;
                for(int i=0; i<nbs.size(); i++) {
                    // replace qvalue() with true for better performance
                    // send_message(nbs[i], qvalue());
                    send_message(nbs[i], true);
                }
            } else if (qvalue()==false) {   // not reached before
                qvalue() = true;    // mark as reached
                // broadcast to activate neighbors
                vector<int>& nbs = nqvalue().nbs;
                for(int i=0; i<nbs.size(); i++) {
                    // replace qvalue() with true for better performance
                    // send_message(nbs[i], qvalue());
                    send_message(nbs[i], true);
                }
            } else {
                vote_to_halt();
            }
        }
};

//--------------------------------------------------
//Step 6: define worker class
class BFSWorkerOL:public WorkerOL_auto<BFSVertex>
{
	public:
		char buf[50];

		//Step 6.1: UDF: line -> vertex
		virtual BFSVertex* toVertex(char* line)
		{
			char * pch;
			pch=strtok(line, "\t");
			BFSVertex* v=new BFSVertex;
			int id=atoi(pch);
			v->id=id;
			strtok(NULL, " ");//skip neighbor_number
			while(pch=strtok(NULL, " "))
			{
				int nb=atoi(pch);
				v->nqvalue().nbs.push_back(nb);
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
		virtual void dump(BFSVertex* vertex, BufferedWriter& writer)
		{
			// if(vertex->id != get_query().v2) return;
			sprintf(buf, "%d\t%d\n", vertex->id, vertex->qvalue());
			writer.write(buf);
		}
};

class BFSCombiner:public Combiner<bool>
{
	public:
		virtual void combine(bool & old, const bool & new_msg){
            old |= new_msg;
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

	param.force_write=false;    // no need to write output to disk
	param.native_dispatcher=false;

	BFSWorkerOL worker;
	BFSCombiner combiner;
    bool use_combiner = true;
	if(use_combiner) worker.setCombiner(&combiner);
	worker.run(param);
	return 0;
}