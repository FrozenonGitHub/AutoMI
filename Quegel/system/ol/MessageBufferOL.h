#ifndef MESSAGEBUFFEROL_H
#define MESSAGEBUFFEROL_H

#include <vector>
#include "../utils/global.h"
#include "global_ol.h"
#include "../utils/Combiner.h"
#include "../utils/communication.h"
#include "../utils/vecs.h"
using namespace std;

//================== (de)serializing structure ==================
template <class KeyT, class MessageT>
struct qid_msgs_pair_ptr
{
	typedef vector<msgpair<KeyT, MessageT> > Vec;

	int qid;
	Vec* msgs;
};

//qid_msgs_pair_ptr is only used for serialization
template <class KeyT, class MessageT>
ibinstream& operator<<(ibinstream& m, const qid_msgs_pair_ptr<KeyT, MessageT>& v)
{
	m << v.qid;
	m << v.msgs;
	return m;
}

template <class KeyT, class MessageT>
struct vgroup_vec
{
	typedef vector<msgpair<KeyT, MessageT> > Vec;
	typedef vector<Vec> VecGroup;

	vector<int> qids;
	vector<VecGroup *> groups;

	void append(int qid, VecGroup& vec_group)
	{
		qids.push_back(qid);
		groups.push_back(&vec_group);
	}

	void parse(vector<vector<qid_msgs_pair_ptr<KeyT, MessageT> > >& send_buf)
	{
		int np = _num_workers;
		int ng = groups.size();
		for(int i=0; i<np; i++)
		{
			send_buf[i].resize(ng);
			for(int j=0; j<ng; j++)
			{
				typename vgroup_vec<KeyT, MessageT>::VecGroup * group_ptr=groups[j];
				typename vgroup_vec<KeyT, MessageT>::Vec& vec=(*group_ptr)[i];
				qid_msgs_pair_ptr<KeyT, MessageT> pair;
				pair.qid=qids[j];
				pair.msgs=&vec;
				send_buf[i][j]=pair;
			}
		}
	}
};

template <class KeyT, class MessageT>
struct qid_msgs_pair
{
	typedef vector<msgpair<KeyT, MessageT> > Vec;

	int qid;
	Vec msgs;
};

template <class KeyT, class MessageT>
ibinstream& operator<<(ibinstream& m, const qid_msgs_pair<KeyT, MessageT>& v)
{
	m << v.qid;
	m << v.msgs;
	return m;
}

template <class KeyT, class MessageT>
obinstream& operator>>(obinstream& m, qid_msgs_pair<KeyT, MessageT>& v)
{
	m >> v.qid;
	m >> v.msgs;
	return m;
}

//================================================

template <class VertexOLT>
class MessageBufferOL {
public:
    typedef typename VertexOLT::KeyType KeyT;
    typedef typename VertexOLT::MessageType MessageT;
    typedef typename VertexOLT::QueryType QueryT;
    typedef typename VertexOLT::HashType HashT;
    typedef vector<MessageT> MessageContainerT;
    typedef hash_map<KeyT, int> Map; //map[key] = position in vertex_vector
    typedef Vecs<KeyT, MessageT, HashT> VecsT;
    typedef typename VecsT::Vec Vec;
    typedef typename VecsT::VecGroup VecGroup;
    typedef typename Map::iterator MapIter;

    typedef typename VertexOLT::TaskT TaskT;
    typedef typename VertexOLT::QMap QMap;
    typedef typename QMap::iterator QMapIter;

    Map in_messages;
	vector<VertexOLT*>& vertexes;
	QMap& queries;

	MessageBufferOL():
		vertexes(*(vector<VertexOLT*>*)get_vertexes()),
		queries(*(QMap*)get_active_queries())
	{}

    void init(vector<VertexOLT*> vertexes)
    {
        int n = vertexes.size();
        for (int i = 0; i < n; i++) {
            VertexOLT* v = vertexes[i];
            in_messages[v->id] = i;
        }
    }

    int get_vpos(KeyT vertex_id)
	{
		//return -1 if the vertex with the specified id is not found
		MapIter it=in_messages.find(vertex_id);
		if(it == in_messages.end()) return -1;
		else return it->second;
	}

    void add_message(const KeyT& id, const MessageT& msg)
    {
    	TaskT& task=*(TaskT*)query_entry();
    	task.setBit(HAS_MSG_ORBIT);
    	task.out_messages.append(id, msg);
    }

    void combine()
    {
        //apply combiner
        Combiner<MessageT>* combiner = (Combiner<MessageT>*)get_combiner();
        if (combiner != NULL)
        {
        	for(QMapIter it = queries.begin(); it != queries.end(); it++)
        	{
        		TaskT& task=it->second;
        		if(task.superstep!=-1) task.out_messages.combine();
        	}
        }
    }

    void sync_messages()//>> add query states of vertices that receive msgs to active_set
    {
        int np = get_num_workers();

        vgroup_vec<KeyT, MessageT> gvec;
        for(QMapIter it = queries.begin(); it != queries.end(); it++)
		{
        	TaskT& task=it->second;
        	if(task.superstep!=-1) gvec.append(it->first, task.out_messages.vecs);
		}

        vector<vector<qid_msgs_pair_ptr<KeyT, MessageT> > > send_buf(np);
        gvec.parse(send_buf);
        vector<vector<qid_msgs_pair<KeyT, MessageT> > > recv_buf(np);

        //================================================
        //exchange msgs
        all_to_all(send_buf, recv_buf);

        //================================================
        //gather all remote messages
        for (int i = 0; i < np; i++) {
        	if(_my_rank != i)//remote msg
        	{
        		vector<qid_msgs_pair<KeyT, MessageT> > & qm_buf = recv_buf[i];
				for(int j=0; j<qm_buf.size(); j++)
				{
					qid_msgs_pair<KeyT, MessageT> & pair = qm_buf[j];
					int qid = pair.qid;
					set_qid(qid);//need to set qid before calling VertexOL.mbuf()
					QMap& queries=*(QMap*)get_active_queries();
					set_query_entry(&queries[qid]);//need to set query task before calling VertexOL.mbuf()
					Vec& msgBuf = pair.msgs;
					TaskT& task = queries[qid];
					for (int k = 0; k < msgBuf.size(); k++) {
						MapIter it = in_messages.find(msgBuf[k].key);
						if (it != in_messages.end()){//filter out msgs to non-existent vertices
							int vpos=it->second;
							vertexes[vpos]->mbuf().push_back(msgBuf[k].msg);//>> forward msg to vertex state's in-buf
							task.activate(vpos);//>> add query states of vertices that receive msgs to active_set
						}
					}
				}
        	}
        	else //local msg
        	{
        		vector<qid_msgs_pair_ptr<KeyT, MessageT> > & qm_buf = send_buf[i];
        		for(int j=0; j<qm_buf.size(); j++)
				{
					qid_msgs_pair_ptr<KeyT, MessageT> & pair = qm_buf[j];
					int qid = pair.qid;
					set_qid(qid);//need to set qid before calling VertexOL.mbuf()
					QMap& queries=*(QMap*)get_active_queries();
					set_query_entry(&queries[qid]);//need to set query task before calling VertexOL.mbuf()
					Vec& msgBuf = *(pair.msgs);
					TaskT& task = queries[qid];
					for (int k = 0; k < msgBuf.size(); k++) {
						MapIter it = in_messages.find(msgBuf[k].key);
						if (it != in_messages.end()){//filter out msgs to non-existent vertices
							int vpos=it->second;
							vertexes[vpos]->mbuf().push_back(msgBuf[k].msg);//>> forward msg to vertex state's in-buf
							task.activate(vpos);//>> add query states of vertices that receive msgs to active_set
						}
					}
				}
        	}
        }
        //clear out-msg-buf
        for(QMapIter it = queries.begin(); it != queries.end(); it++)
		{
        	TaskT& task=it->second;
			if(task.superstep!=-1) task.out_messages.clear();
		}
    }

    long long get_total_msg()
    {
    	long long sum=0;
    	for(QMapIter it = queries.begin(); it != queries.end(); it++)
		{
    		TaskT& task=it->second;
    		if(task.superstep!=-1) sum+=task.out_messages.get_total_msg();
		}
        return sum;
    }
};

#endif
