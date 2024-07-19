#include "basic/pregel-dev.h"
using namespace std;

struct FormatValue
{
    vector<VertexID> innbs;
    vector<VertexID> outnbs;
};

ibinstream& operator<<(ibinstream& m, const FormatValue& v)
{
    m << v.innbs;
    m << v.outnbs;
    return m;
}

obinstream& operator>>(obinstream& m, FormatValue& v)
{
    m >> v.innbs;
    m >> v.outnbs;
    return m;
}

class FormatVertex : public Vertex<VertexID, FormatValue, VertexID>
{
public:

    virtual void compute(MessageContainer& messages)
    {
    	if(step_num() == 1)
        {
		vector<VertexID> & outnbs = value().outnbs;
		for(int i=0; i<outnbs.size(); i++)
		{
			send_message(outnbs[i], id);
		}
        }
	else
	{
		vector<VertexID> & innbs = value().innbs;
		for(int i=0; i<messages.size(); i++)
		{
			innbs.push_back(messages[i]);
		}
		vote_to_halt();
	}
    }
};

class FormatWorker : public Worker<FormatVertex>
{
    char buf[100];

public:
    // vid \t num v1 v2 ...
    virtual FormatVertex* toVertex(char* line)
    {
        char* pch;
        pch = strtok(line, "\t");
        FormatVertex* v = new FormatVertex;
        v->id = atoi(pch);

        pch = strtok(NULL, " ");
        int num = atoi(pch);
        while (num--)
        {
            pch = strtok(NULL, " ");
            int vid = atoi(pch);
            v->value().outnbs.push_back(vid);
        }
        return v;
    }

	// vid \t in-num in1 in2 ... out-num out1 out2 ...
    virtual void toline(FormatVertex* v, BufferedWriter& writer)
	{
		vector<VertexID> & innbs = v->value().innbs;
		vector<VertexID> & outnbs = v->value().outnbs;
		sprintf(buf, "%d\t%d", v->id, innbs.size());
		writer.write(buf);
		for(int i=0; i<innbs.size(); i++)
		{
			sprintf(buf, " %d", innbs[i]);
			writer.write(buf);
		}
		sprintf(buf, " %d", outnbs.size());
		writer.write(buf);
		for(int i=0; i<outnbs.size(); i++)
		{
			sprintf(buf, " %d", outnbs[i]);
			writer.write(buf);
		}
		writer.write("\n");
	}
};

void pregel_format(string in_path, string out_path)
{
    WorkerParams param;
    param.input_path = in_path;
    param.output_path = out_path;
    param.force_write = true;
    param.native_dispatcher = false;
    FormatWorker worker;
    worker.run(param);
}
