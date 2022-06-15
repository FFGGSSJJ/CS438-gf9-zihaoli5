/**
 * @file linkstate.h
 * @author guanshujie fu
 * @brief 
 * @version 0.1
 * @date 2022-04-12
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef _LINKSTAT_H
#define _LINKSTAT_H

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <ostream>
#include <iostream>
#include <fstream>
using namespace::std;

#define BROKEN -999
#define INVALID -1
#define INF 99999999
#define DEBUG 0




/**
 * @brief struct definition for a single node
 * @arg id - node id, *** it is the node num - 1
 *      num_neighbors - 
 *      neighbors - array of neighbors
 *      costs - cost of links with neighbors
 */
typedef struct node node_t;
struct node
{
    int id;
    int num_neighbors;
    int* neighbors;
    int* costs;
    
    // for lk algorithm
    bool in_heap;
    bool visited;
    int heap_id;
    int cost_from_src;
    int pred;
};


/**
 * @brief struct definition for the whole graph
 * 
 */
typedef struct graph
{
    int num_nodes;
    node_t* the_nodes;
} graph_t;



/**
 * @brief struct definition for the path
 * 
 */
typedef struct path
{
    int n_nodes;		    // number of actual vertices in path
    int sum_costs;		    // sum of costs along edges in path
    int* id;                // graph vertex array indices for path, initialize according to the num nodes
} path_t;


/**
 * @brief struct definition for the requests message
 * @arg node1 - self exp
 *      node2 - self exp
 *      msg - self exp
 */
typedef struct request request_t;
struct request
{
    int src;
    int dst;
    string msg;
    path_t* the_path;
    request_t* next_req;
};


/**
 * @brief struct definition for the heap
 *        to be used in lk algorithm to store all nodes
 * 
 */
typedef struct heap
{
    int size;
    int n_nodes;
    int* id;
} heap_t;




/* functions to implement lk algorithm */

graph_t* construct_graph(const char* filename);


request_t* read_msg_req(const char* filename);


int get_change(const char* filename, graph_t* g, int change_times);


void init_heap(graph_t* g, int src, heap_t* h);


int find_min(heap_t* h, graph_t* g);


void swap_heap(int32_t* a, int32_t* b, graph_t* g);

int dijkstra(heap_t* h, graph_t* g, int src);

path_t* trace_path(graph_t* g, int src, int dst);

/* help function to show/write graph or table */


void show_graph(graph_t* g);


void show_forward_table(path_t* p, const char* filename, int srcflag);

request_t* search_req_match(request_t* req, int src, int dst);

void show_message(request_t* r, const char* filename);

/* free functions to free malloced memory */
void free_graph(graph_t* g);


void free_req(request_t* r);


void free_path(path_t* p);


void free_heap(heap_t* h);







#endif /* _LINKSTAT_H */