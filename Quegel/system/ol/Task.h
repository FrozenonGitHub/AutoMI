#ifndef TASK_H
#define TASK_H

#include "../utils/global.h" //for obtaining message buffer
#include "global_ol.h" //for obtaining vertex_set
#include "MessageBufferOL.h"
#include "../utils/ydhdfs.h" //for supporting dumping
#include "../utils/vecs.h" //for Vecs
#include "../utils/communication.h" //for implementing is_computed()
#include "../utils/time.h"

//* UDF: init()
//in offline mode, every vertex will be parsed from HDFS to mem, and we can set active_tag
//in online mode, vertices are in mem, we need some efficient way to activate only a small initial vertex set without scanning every vertex

template <class VertexOLT>
class Task
{
	public:
		typedef typename VertexOLT::KeyType KeyT;
		typedef typename VertexOLT::MessageType MessageT;
		typedef typename VertexOLT::QueryType QueryT;
		typedef typename VertexOLT::HashType HashT;

		typedef Vecs<KeyT, MessageT, HashT> VecsT;

		//scheduler
		hash_set<int> active;//records positions of vertices
		hash_set<int> created;//records positions of vertices

		//query metadata
		QueryT query;
		int superstep;//=-1 means dumping, -2 means dumped
		VecsT out_messages;
		char bor_bitmap;
		void* aggregator;
		void* agg;
		double start_time;

		Task()
		{
			superstep=0;
			bor_bitmap=0;
			start_time=get_current_time();
		}

		Task(QueryT q)
		{
			query=q;
			superstep=0;
			bor_bitmap=0;
			start_time=get_current_time();
		}

		double get_runtime()
		{
			return (get_current_time() - start_time);
		}

		void clearBits()
		{
		    bor_bitmap = 0;
		}

		void setBit(int bit)
		{
		    bor_bitmap |= (1 << bit);
		}

		int getBit(int bit, char bitmap)
		{
		    return ((bitmap & (1 << bit)) == 0) ? 0 : 1;
		}

		void hasMsg()
		{
		    setBit(HAS_MSG_ORBIT);
		}

		void forceTerminate()
		{
		    setBit(FORCE_TERMINATE_ORBIT);
		}

		//called before starting another computing superstep
		void start_another_superstep()
		{
			superstep++;
			clearBits();
		}

		//stop condition check:
		//decides whether to start another computing superstep
		void check_termination()
		{
			char bits_bor = all_bor(bor_bitmap);
			if (getBit(FORCE_TERMINATE_ORBIT, bits_bor) == 1)
			{
				superstep=-1;
				return;
			}
			active_vnum() = all_sum(active.size());
			if(active_vnum() == 0 && getBit(HAS_MSG_ORBIT, bits_bor) == 0) superstep=-1;
		}

		//WorkerOL calls it to move active_set to a new set
		void move_active_vertices_to(hash_set<int>& dst)//dst should be empty
		{
			dst.swap(active);
		}

		//WorkerOL/UDF_init() calls activate() to add a vertex to active_set
		void activate(int vertex_position)
		{
			active.insert(vertex_position);
			created.insert(vertex_position);
		}

};

#endif
