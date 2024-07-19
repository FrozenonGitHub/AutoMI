#ifndef GLOBAL_OL_H
#define GLOBAL_OL_H

#include <stddef.h>

//-----------------------------------------
//Global vertex_set
void* global_vertexes = NULL; //type = vector<VertexOL*>

inline void set_vertexes(void* vertexes)
{
	global_vertexes = vertexes;
}

inline void* get_vertexes()
{
    return global_vertexes;
}

//-----------------------------------------
//WorkerOL should ensure "active_queries" up-to-date
void* active_queries = NULL; //type = hash_map<qid, Task>

inline void set_active_queries(void* aqs)
{
	active_queries = aqs;
}

inline void* get_active_queries()
{
    return active_queries;
}

//-----------------------------------------
//to make the query currently being processed transparent to vertex.compute()
//WorkerOL should ensure "global_query_id" and "global_query" up-to-date
int global_query_id;

inline int query_id()
{
    return global_query_id;
}

inline void set_qid(int qid)
{
    global_query_id=qid;
}

void* global_query; //type = Task

inline void* query_entry()
{
    return global_query;
}

inline void set_query_entry(void* q)
{
	global_query=q;
}
//-----------------------------------------

#endif
