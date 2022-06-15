#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<ostream>
#include<iostream>
#include<fstream>
#include<list>
#include<set>
#include<map>
//#include<string>
using namespace::std;

#define BROKEN -999
#define INVALID -1
#define INF 999999999

// construct the graph table & neighbors
void construct_graph(const char* filename);

// get and apply the change_times'th change. change_time = 1 means first change
int get_change(const char* filename, int change_times);



// the algorithm
void converge();

// output the node topologies into the file
void output_topology();

// output the messages
void output_message();


// not used
// void free_graph(graph_t* g);

// void free_req(request_t* r);

// void free_path(path_t* p);

// useless structures defined by fuguan
// typedef struct node
// {
//     int id;
//     int num_neighbors;
//     int* neighbors;
//     int* costs;
    
// } node_t;

// typedef struct graph
// {
//     int num_nodes;
//     node_t* the_nodes;
// } graph_t;

// typedef struct path
// {
//     int n_nodes;		    // number of actual vertices in path
//     int sum_costs;		    // sum of costs along edges in path
//     int* id;                // graph vertex array indices for path, initialize according to the num nodes
// } path_t;

// typedef struct request request_t;
// struct request
// {
//     int src;
//     int dst;
//     string msg;
//     path_t* the_path;
//     request_t* next_req;
// };
