#include <iostream>
#include <sstream>
#include <fstream>
#include <omp.h> // Include OpenMP header
#include <zlib.h> // Include zlib header
#include <string>
#include <cstring>
#include <vector>
#include <unordered_map>

using namespace std;

struct Edge {
    int target;
    int weight;
};


// Load .gz compressed weighted edge tuple file into (thread) local adjacency list
void load_gzip_wtuple_file(int idx, vector<vector<Edge>>& local_adj_list,
                        string fin_prefix, int num_files) {
    string suffix = "." + to_string(idx) + "_of_" + to_string(num_files) + ".gz";
    string input_file = fin_prefix + suffix;
    gzFile file = gzopen(input_file.c_str(), "rb");
    if (!file) {
        std::cerr << "Failed to open file: " << input_file << std::endl;
    }
    

    const int bufferSize = 1024;
    char buffer[bufferSize];

    while (gzgets(file, buffer, bufferSize) != Z_NULL) {
        std::istringstream line_stream(buffer);
        int source, target, weight;
        char comma;
        if (line_stream >> source >> comma >> target >> comma >> weight) {
            local_adj_list[source].push_back({target, weight});
        }
    }

    gzclose(file);
}


void merge_adj_lists(vector<vector<Edge>>& global_adj_list,
            const vector<vector<vector<Edge>>>& local_adj_lists,
            int thread_id, int num_threads, int num_vertices) {
    for (int t = 0; t < num_threads; ++t) {
        for (int v = 0; v < num_vertices; ++v) {
            if (v % num_threads == thread_id) {
                for (const auto& edge : local_adj_lists[t][v]) {
                    global_adj_list[v].push_back(edge);
                }
            }
        }
    }
}

void save_to_files(const vector<vector<Edge>>& global_adj_list,
                int thread_id, int num_threads, int num_vertices, string output_prefix) {
    string output_file = output_prefix + "." + to_string(thread_id) + "-" + to_string(num_threads) + ".txt";
    std::ofstream out(output_file);

    for (int v = 0; v < num_vertices; ++v) {
        if (v % num_threads == thread_id) {
            out << v << "\t";
            out << global_adj_list[v].size() << " ";
            for (const auto& edge : global_adj_list[v]) {
                out << edge.target << " " << edge.weight << " ";
            }
            out << endl;
        }
    }
    out.close();
}


int main(int argc, char* argv[]) {

    int num_files = 32;
    // livejournal
    // int num_vertices = 4847571;
    // twitter
    // int num_vertices = 41652230;
    // friendster
    // int num_vertices = 68349466;
    // UKDomain
    int num_vertices = 105153952;
    char* input_file_prefix = argv[1];
    char* output_file_prefix = argv[2];

    if (argc > 3) {
        num_vertices = atoi(argv[3]);
    }

    // Set the number of threads
    int num_threads = 8; // Specify the number of threads
    int files_per_thread = num_files / num_threads;
    omp_set_num_threads(num_threads);

    // Array of local adjacency lists, one for each thread
    vector<vector<vector<Edge>>> local_adj_lists(num_threads);

    // prepare space for each thread
    for (int i = 0; i < num_threads; ++i) {
        local_adj_lists[i].resize(num_vertices);
    }

    // OpenMP parallel region loading etuple files
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        
        for (int idx = thread_id * files_per_thread + 1; idx <= (thread_id + 1) * files_per_thread; idx++) {
            load_gzip_wtuple_file(idx, local_adj_lists[thread_id], string(input_file_prefix), num_files);
        }
    }

    // Global adjacency list
    vector<vector<Edge>> global_adj_list;
    global_adj_list.resize(num_vertices);

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        merge_adj_lists(global_adj_list, local_adj_lists, thread_id, num_threads, num_vertices);
    }

    // #pragma omp parallel
    // {
    //     int thread_id = omp_get_thread_num();
    //     save_to_files(global_adj_list, thread_id, num_threads, num_vertices, string(output_file_prefix));
    // }

    // Save to one large (combined) file
    string output_file = string(output_file_prefix) + "_combined.txt";
    std::ofstream out(output_file);

    for (int v = 0; v < num_vertices; ++v) {
        out << v << "\t";
        out << global_adj_list[v].size() << " ";
        for (const auto& edge : global_adj_list[v]) {
            out << edge.target << " " << edge.weight << " ";
        }
        out << endl;
    }
    out.close();

    // debug print
    cout << "Global_adj_list size: " << global_adj_list.size() << endl;

    return 0;
}
