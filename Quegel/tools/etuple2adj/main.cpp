/**
 * Created on 2024/06/17
 * Convert (weighted) edge list to adjacency list
 */

#include <mpi.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <cstring>
#include <zlib.h>

using namespace std;
// vertex ID -> (weight, neighbor ID)
// vertex_id_type : size_t
using AdjList = std::unordered_map<size_t, std::vector<std::pair<int, size_t>>>;

void wtuple_parser(AdjList& adjList, const std::string& str) {
    if (str.empty()) return;
    if (str[0] == '#') {
        std::cout << str << std::endl;
    } else if (!std::isdigit(str[0])) {
        std::cout << str << std::endl;
    } else {
        size_t source, target;
        char *targetptr;
        source = strtoul(str.c_str(), &targetptr, 10);
        if (targetptr == NULL) return;
        target = strtoul(targetptr + 1, &targetptr, 10);
        if (source != target) {
            int weight = 1; // Default weight
            weight = strtol(targetptr + 1, NULL, 10);
            adjList[source].emplace_back(weight, target);
        }
    }
}

void loadEdgeTuples(const std::string &filename, AdjList &adj_list) {
    // debug
    cout << "Loading file: " << filename << endl;
    
    gzFile infile = gzopen(filename.c_str(), "rb");
    
    char buffer[1024];
    while (gzgets(infile, buffer, sizeof(buffer)) != Z_NULL) {
        std::string line(buffer);
        wtuple_parser(adj_list, line);
    }
    gzclose(infile);
    // ifstream fin(filename);
    // std::string line;
    // while (std::getline(fin, line)) {
    //     wtuple_parser(adj_list, line);
    // }
}

void saveAdjList(const AdjList &adjList, const std::string &outputPath, int rank, int size) {
    std::stringstream filename;
    filename << outputPath << "_" << rank;
    std::ofstream outfile(filename.str());

    for (const auto &entry : adjList) {
        if (entry.first % size == rank) {
            outfile << entry.first << "\t";
            outfile << entry.second.size() << " ";
            for (const auto &neighbor : entry.second) {
                outfile << neighbor.first << " " << neighbor.second << " ";
            }
            outfile << "\n";
        }
    }
}

void mergeAdjLists(AdjList &globalAdjList, const AdjList &localAdjList) {
    for (const auto &entry : localAdjList) {
        globalAdjList[entry.first].insert(globalAdjList[entry.first].end(), entry.second.begin(), entry.second.end());
    }
}

void MPI_SendAdjList(const AdjList &adjList, int dest, int size) {
    for (const auto &entry : adjList) {
        int src = entry.first;
        if (src % size == dest) {
            const std::vector<std::pair<int, size_t>> &neighbors = entry.second;
            int neighbors_size = neighbors.size();
            MPI_Send(&src, 1, MPI_INT, dest, 0, MPI_COMM_WORLD);
            MPI_Send(&neighbors_size, 1, MPI_INT, dest, 0, MPI_COMM_WORLD);
            for (const auto &neighbor : neighbors) {
                MPI_Send(&neighbor.first, 1, MPI_INT, dest, 0, MPI_COMM_WORLD);
                MPI_Send(&neighbor.second, 1, MPI_INT, dest, 0, MPI_COMM_WORLD);
            }
        }
    }
    int endSignal = -1;
    MPI_Send(&endSignal, 1, MPI_INT, dest, 0, MPI_COMM_WORLD);
}

void MPI_RecvAdjList(AdjList &adjList, int src) {
    while (true) {
        int node;
        MPI_Recv(&node, 1, MPI_INT, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (node == -1) break;
        int size;
        MPI_Recv(&size, 1, MPI_INT, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        std::vector<std::pair<int, size_t>> neighbors(size);
        for (int i = 0; i < size; ++i) {
            int neighbor, weight;
            MPI_Recv(&weight, 1, MPI_INT, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&neighbor, 1, MPI_INT, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            neighbors[i] = std::make_pair(neighbor, weight);
        }
        adjList[node] = std::move(neighbors);
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        if (rank==0) {
            cout << "Usage: ./convert <in_path> <out_path>" << endl;
        }
        MPI_Finalize();
        return -1;
    }

    string input_path = string(argv[1]);
    string output_path = string(argv[2]);

    // Assuming filenames are in the format "edges_0", "edges_1", ..., "edges_31"
    int numFiles = 32;
    int filesPerProcess = numFiles / size;

    // Step 1: load edge tuples to local adjacency list
    AdjList localAdjList;

    for (int i = 0; i < filesPerProcess; i++) {
        std::stringstream filename;
        filename << input_path << "." << rank * filesPerProcess + i << "_of_32.gz";
        loadEdgeTuples(filename.str(), localAdjList);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    // debug
    if (rank == 0) {
        cout << "Rank 0: " << localAdjList.size() << endl;
    }

    // Step 2: merge local adjacency lists to global adjacency list
    AdjList globalAdjList;
    if (rank == 0) {
        globalAdjList = localAdjList;   // merge to MASTER thread local list
        for (int i = 1; i < size; i++) {
            AdjList recvAdjList;
            MPI_RecvAdjList(recvAdjList, i);
            mergeAdjLists(globalAdjList, recvAdjList);
        }
    } else {
        MPI_SendAdjList(localAdjList, rank, 0);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    // debug
    if (rank == 0) {
        cout << "Rank 0: " << globalAdjList.size() << endl;
    }

    // Step 3: partition global adjacency list and save to files
    if (rank == 0) {
        for (int i = 1; i < size; i++) {
            MPI_SendAdjList(globalAdjList, i, size);
        }
        // debug
        cout << "Rank 0: before saveAdjList" << endl;
        saveAdjList(globalAdjList, output_path, rank, size);
        // debug
        cout << "Rank 0: finished." << endl;
    } else {
        AdjList partialAdjList;
        // debug
        cout << "Rank: " << rank << ", before MPI_RecvAdjList" << endl;
        MPI_RecvAdjList(partialAdjList, 0);
        // debug
        cout << "Rank: " << rank << ", before saveAdjList" << endl;
        saveAdjList(partialAdjList, output_path, rank, size);
        // debug
        cout << "Rank: " << rank << ", finished." << endl;
    }

    MPI_Finalize();
    return 0;
}