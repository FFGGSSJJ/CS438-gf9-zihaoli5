/**
 * @file linkstate.cpp
 * @author Guanshujie Fu
 * @brief self explained
 * @version 0.1
 * @date 2022-04-12
 * @note there is a malicious and tricky bug in function read_msg_req, I put some notes in main func.
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "../lib/linkstate.h"





/**
 * @brief construct the graph using the file 'topofile'
 * @author guanshujie fu
 * @param filename 
 * @return graph_t* 
 */
  graph_t* construct_graph(const char* filename)
{
    FILE* f = fopen (filename, "r");
    if (f == NULL) {
        cout << "topofile open failed\n";
        return NULL;
    }

    // count for the node number
    int num_nodes = 0;
    while (1) {
        int node1, node2, cost;
        if (3 != fscanf(f, "%d%d%d", &node1, &node2, &cost))
            break;
        /* update node number */
        num_nodes = node1 > num_nodes ? node1 : num_nodes;
        num_nodes = node2 > num_nodes ? node2 : num_nodes;
    } fclose(f);

    // set up graph
    graph_t* g;
    if (NULL == (g = (graph_t*) malloc(sizeof(graph_t)))) {
        perror("malloc failed");
        return NULL;
    }
    g->num_nodes = num_nodes;
    if (NULL == (g->the_nodes = (node_t*) malloc (g->num_nodes * sizeof (node_t)))) {
        perror("malloc failed");
        free(g);
        return NULL;
    }
    for (int j = 0; j < g->num_nodes; j++)
        g->the_nodes[j].id = INVALID;

    // reopen to set the graph
    FILE* fd = fopen (filename, "r");
    if (fd == NULL) {
        cout << "topofile open failed\n";
        return NULL;
    }

    while (1) {
        int node1, node2, cost;
        if (3 != fscanf(fd, "%d%d%d", &node1, &node2, &cost))
            break;
        /* update the graph */
        node_t* n1 = &g->the_nodes[node1 - 1];
        node_t* n2 = &g->the_nodes[node2 - 1];

        /* this part will waste a lot of space of there are many nodes */
        /* I simply malloc space according to the total number of nodes for the neighbors */
        /* But it can save a lot of time when searching for a neighbor, if I use a method like hash (not apply here) */
        if (n1->id == INVALID) {
            n1->id = node1 - 1;
            n1->num_neighbors = 0;
            n1->neighbors = (int*) malloc(g->num_nodes * sizeof(int));
            n1->costs = (int*) malloc(g->num_nodes * sizeof(int));
        }
        if (n2->id == INVALID) {
            n2->id = node2 - 1;
            n2->num_neighbors = 0;
            n2->neighbors = (int*) malloc(g->num_nodes * sizeof(int));
            n2->costs = (int*) malloc(g->num_nodes * sizeof(int));
        }

        /* update the neighbors and cost */
        if (cost == BROKEN) continue;
        n1->neighbors[n1->num_neighbors++] = node2 - 1;
        n1->costs[n1->num_neighbors-1] = cost;
        n2->neighbors[n2->num_neighbors++] = node1 - 1;
        n2->costs[n2->num_neighbors-1] = cost;
    }
    fclose(fd);
    return g;
}


/**
 * @brief 
 * 
 * @param filename 
 * @return request_t* a singly linked list
 */
  request_t* read_msg_req(const char* filename)
{
    ifstream f(filename);
    if (!f.is_open()) {
        cout << "messagefile open failed\n";
        return NULL;
    }

    /* define the first request */
    request_t* req = (request_t*) malloc(sizeof(request_t));
    request_t* start = req;
    if (req == NULL) {
        perror("malloc failed");
        f.close();
        return NULL;
    }
    req->src = INVALID;
    req->dst = INVALID;
    req->the_path = NULL;

    // get message
    string line;
    while (getline(f, line)) {
        /* get message info */
        int node1 = line[0] - 48;
        int node2 = line[2] - 48;
        string msg;
        for (int i = 4; i < line.length(); i++)
            msg.push_back(line[i]);
        
        /* update the request */
        if (req->src == INVALID) req->src = node1 - 1;
        if (req->dst == INVALID) req->dst = node2 - 1;
        req->msg = msg;
        req->next_req = (request_t*) malloc(sizeof(request_t));
        if (req->next_req == NULL) {
            perror("req malloc failed");
            free_req(req);
            f.close();
            return NULL;
        }
        req->next_req->src = INVALID;
        req->next_req->dst = INVALID;
        req->next_req->the_path = NULL;
        req = req->next_req;
        /**/
    }
    f.close();
    return start;
}

/**
 * @brief Get the change object
 * 
 * @param filename 
 * @param g 
 * @param change_times - number of change occurs
 * @return int : 1, change updated
 *             : 0, no change anymore
 */
 int get_change(const char* filename, graph_t* g, int change_times)
{
    FILE* f = fopen (filename, "r");
    if (f == NULL) {
        cout << "changefile open failed\n";
        return 0;
    }

    for (int i = 0; i < change_times; i++) {
        int node1, node2, cost;

        /* check availability */
        if (3 != fscanf(f, "%d%d%d", &node1, &node2, &cost))
            return 0;
        cost = cost == BROKEN ? INF : cost;
        if (i != change_times - 1)
            continue;


        /* update the graph */
        int flag = 0;
        node_t* n1 = &g->the_nodes[node1 - 1];
        node_t* n2 = &g->the_nodes[node2 - 1];
        
        /* search neighbors in n1*/
        for (int j = 0; j < n1->num_neighbors; j++){
            if (n1->neighbors[j] != node2 - 1)   continue;
            flag = 1;
            n1->costs[j] = cost;
            break;
        } 
        // if neighbors not exist
        if (flag == 0) {
            n1->neighbors[n1->num_neighbors++] = node2 - 1;
            n1->costs[n1->num_neighbors - 1] = cost;
        } flag = 0;

        /* search neighbors in n2*/
        for (int j = 0; j < n2->num_neighbors; j++){
            if (n2->neighbors[j] != node1 - 1)   continue;
            flag = 1;
            n2->costs[j] = cost;
            break;
        }
        // if neighbors not exist
        if (flag == 0) {
            n2->neighbors[n2->num_neighbors++] = node1 - 1;
            n2->costs[n2->num_neighbors - 1] = cost;
        } flag = 0;
        break;
    }
    fclose(f);
    return 1;
}


/**
 * @brief initialize the heap. 
 *        There is only source node in heap at first
 * 
 * @param r 
 * @param h 
 */
 void init_heap(graph_t* g, int src, heap_t* h)
{
    /* initialize all nodes in graph */
    for (int i = 0; i < g->num_nodes; i++) {
        g->the_nodes[i].cost_from_src = INF;
        g->the_nodes[i].in_heap = false;
        g->the_nodes[i].visited = false;
        g->the_nodes[i].pred = INVALID;
        h->id[i] = INVALID;
    }
    h->n_nodes = 0;

    /* add source node into the heap */
    int node1 = src;
    g->the_nodes[node1 - 1].heap_id = h->n_nodes;
    g->the_nodes[node1 - 1].in_heap = true;
    g->the_nodes[node1 - 1].cost_from_src = 0;
    g->the_nodes[node1 - 1].pred = INVALID;
    h->id[h->n_nodes] = node1 - 1;
    h->n_nodes++;
    return;
}



/**
 * @brief 
 * 
 * @param h 
 * @param g 
 * @return int 
 */
 int find_min(heap_t* h, graph_t* g)
{
    int min_id = -1;
    int min_cost = -1;
    for (int i = 0; i < h->n_nodes; i++) {
        if (g->the_nodes[h->id[i]].visited == false) {
            min_cost = g->the_nodes[h->id[i]].cost_from_src;
            break;
        } 
    }

    if (min_cost == -1) return min_id;

    for (int i = 0; i < h->n_nodes; i++) {
        if (g->the_nodes[h->id[i]].visited == false && g->the_nodes[h->id[i]].cost_from_src <= min_cost) {
            min_cost = g->the_nodes[h->id[i]].cost_from_src;
            min_id = h->id[i];
        } continue;
    }
    return min_id;
}




/**
 * @brief 
 * 
 * @param g 
 * @param src 
 * @param dst 
 * @return path_t* 
 */
 path_t* trace_path(graph_t* g, int src, int dst)
{
    if (g->the_nodes[dst - 1].pred == INVALID) {
        return NULL;
    }
    path_t* p = (path_t*) malloc(sizeof(path_t));
    node_t* temp = &g->the_nodes[dst - 1];
    p->n_nodes = 0;
    p->sum_costs = temp->cost_from_src;

    if (p->sum_costs >= INF)    return p;

    /* first check the number of nodes in a path */
    while (temp->pred != INVALID) {
        p->n_nodes++;
        temp = &g->the_nodes[temp->pred];
    } 
    p->n_nodes++;
    p->id = (int*) malloc(p->n_nodes * sizeof(int));

    /* constrcut the path */
    temp = &g->the_nodes[dst - 1];
    int counter = p->n_nodes;
    while (temp->pred != INVALID)  {
        p->id[--counter] = temp->id;
        temp = &g->the_nodes[temp->pred];
    } p->id[--counter] = g->the_nodes[src - 1].id;
    
    return p;
}



/**
 * @brief self explained
 *        comment to avoid warning
 * @param g 
 */
//  void show_graph(graph_t* g)
// {
//     #if DEBUG
//         for (int j = 0; j < g->num_nodes; j++) {
//             cout << "node: " << g->the_nodes[j].id << endl;
//             cout << "neighbors: ";
//             for (int k = 0; k < g->the_nodes[j].num_neighbors; k++)
//                 cout << g->the_nodes[j].neighbors[k] << "(" << g->the_nodes[j].costs[k] << ")" << " ";
//             cout << "\n\n";
//         }
//     #endif
// }

/**
 * @brief 
 * 
 * @param p - path pointer
 * @param filename 
 * @param srcflag - whether current dst is src
 */
 void show_forward_table(path_t* p, const char* filename, int srcflag)
{
    
    ofstream output;
    output.open(filename, ios::app);

    /* if src flag is set */
    if (srcflag){
        output << srcflag << " " << srcflag <<  " " << 0  << endl;
        output.close();
        return;
    }

    /* this can be deleted. */
    if (p == NULL || p->sum_costs >= INF) {
        output.close();
        return;
    }
    output << p->id[p->n_nodes-1]+1 << " " << p->id[1]+1 <<  " " << p->sum_costs  << endl;
    output.close();

    /* this memory free is critical */
    free_path(p);
    return;
}



/**
 * @brief 
 * 
 * @param req 
 * @param src 
 * @param dst 
 * @return request_t* 
 */
request_t* search_req_match(request_t* req, int src, int dst)
{
    request_t* temp = req;
    while (temp->src != INVALID) {
        if (temp->src == src - 1 && temp->dst == dst - 1) 
            return temp;
        temp = temp->next_req;
    } return NULL;
}


/**
 * @brief 
 * 
 * @param r - req pointer
 * @param filename 
 */
 void show_message(request_t* r, const char* filename)
{
    ofstream output;
    output.open(filename, ios::app);
    if (r == NULL) return;
    request_t* temp = r;
    while (temp != NULL && temp->src != INVALID) {
        /* if unreachbale */
        if (temp->the_path == NULL) {
            output << "from " << temp->src+1 << " to " << temp->dst+1 << " cost infinite hops ";
            output << "unreachable message " << temp->msg << endl;
            temp = temp->next_req;
            continue;
        }

        /* if reachable */
        output << "from " << temp->src+1 << " to " << temp->dst+1 << " cost " << temp->the_path->sum_costs; 
        output << " hops ";
        for(int i = 0; i < temp->the_path->n_nodes - 1; i++){
            output << temp->the_path->id[i]+1 << " ";
        }
        output << "message " << temp->msg << endl;
        temp = temp->next_req;
    }
    output.close();
}





/*########################################################
*           Free Functions start here
##########################################################*/
 void free_graph(graph_t* g)
{
    if (g == NULL)  return;
    for (int i = 0; i < g->num_nodes; i++) {
        free(g->the_nodes[i].neighbors);
        free(g->the_nodes[i].costs);
    }
    free(g);
    return;
}



 void free_req(request_t* r)
{
    if (r == NULL)  return;
    if (r->next_req != NULL)    free_req(r->next_req);
    if (r->the_path != NULL)    free_path(r->the_path);
    free(r);
    return;
}


 void free_path(path_t* p)
{
    if (p == NULL)  return;
    if (p->id != NULL)  free(p->id);
    free(p);
    return;
}

 void free_heap(heap_t* h)
{
    if (h == NULL)  return;
    if (h->id != NULL)  free(h->id);
    free(h);
    return;
}



/*########################################################
*           Link State Algorithm starts here
##########################################################*/

/**
 * @brief 
 * 
 * @param h - heap pointer
 * @param g - graph pointer
 * @param src - source node id
 * @return int 
 */
int dijkstra(heap_t* h, graph_t* g, int src)
{
    int current_min_id;     // id for graph, top element in heap. 

    /* initialize the heap first */
    init_heap(g, src, h);

    while (1)
    {
        current_min_id = find_min(h, g);
        if (current_min_id == -1) break;
        g->the_nodes[current_min_id].visited = true;

        /* loop through all neighbors of current closest node */
        for (int i = 0; i < g->the_nodes[current_min_id].num_neighbors; i++) {
            /* calculate neighbor's distance */
            if (g->the_nodes[current_min_id].costs[i] == INF)   continue;
            int neighbor = g->the_nodes[current_min_id].neighbors[i];
            int cost_neighbor = g->the_nodes[current_min_id].costs[i] + g->the_nodes[current_min_id].cost_from_src;

            /* check if the neighbor has been added into heap */
            if (g->the_nodes[neighbor].in_heap) {
                /* update the cost from source and the pred */
                if (cost_neighbor < g->the_nodes[neighbor].cost_from_src) {
                    g->the_nodes[neighbor].cost_from_src = cost_neighbor;
                    g->the_nodes[neighbor].pred = current_min_id;
                }
            } else {
                /* update the neighbor info */
                g->the_nodes[neighbor].cost_from_src = cost_neighbor;
                g->the_nodes[neighbor].pred = current_min_id;

                /* add current neighbor into the heap */
                g->the_nodes[neighbor].in_heap = true;
                g->the_nodes[neighbor].heap_id = h->n_nodes;
                h->id[h->n_nodes++] = neighbor;

            }
        }
    }
    /* after the while loop, all nodes in graph has updated */
    /* check from the destination and trace back to get the path */
    return 1;
}







/*########################################################
*           Main Functions starts here
##########################################################*/

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    /* this must be the first called function, or the program will fail due to seg fault */
    /* the reason might fall on how file descriptor and the read buffer is malloced in memory */
    /* but I have released the fd and free the malloced memory correctly, after reviewing my code */
    /***** i don't know exactly why, just put it here *****/
    request_t* req = read_msg_req(argv[2]);


    /* construct graph, malloc heap*/
    graph_t* g = construct_graph(argv[1]);
    heap_t* heap_ = (heap_t*) malloc(sizeof(heap_t));
    heap_->id = (int*) malloc(g->num_nodes * sizeof(int));

    request_t* start_req = req;


    // show_graph(g);

    /* clean the output file */ 
    ofstream tempf("output.txt", ofstream::out | ofstream::trunc);
    tempf.close();


    /* main while loop */
    int change_counter = 1;
    while (1) {
        /* constrcut forwarding table for all nodes */
        for (int src = 1; src <= g->num_nodes; src++) {
            /* use greedy algorithm to update all nodes */
            dijkstra(heap_, g, src);
            for (int dst = 1; dst <= g->num_nodes; dst++) {
                path_t* p = trace_path(g, src, dst);

                // path* p = NULL;
                // dijkstra(heap_, g, src);
                // path_t* psrc = trace_path(g, src, dst);
                // dijkstra(heap_, g, dst);
                // path_t* pdst = trace_path(g, dst, src);

                // if (psrc != NULL && pdst != NULL) {
                //     if ((psrc->id[1] <= pdst->id[pdst->n_nodes-2])) {
                //         p = psrc;
                //         free_path(pdst);
                //     } else {
                //         p = (path_t*) malloc(sizeof(path_t));
                //         p->id = (int*) malloc(sizeof(pdst->n_nodes));
                //         p->sum_costs = pdst->sum_costs;
                //         p->n_nodes = pdst->n_nodes;
                //         for (int i = pdst->n_nodes - 1; i >= 0; i++)
                //             p->id[pdst->n_nodes - i - 1] = pdst->id[i];
                //         free_path(pdst);
                //         free_path(psrc);
                //     }
                // }
                
                int srcflag = 0;
                if (src == dst) srcflag = src;

                /* message request matches, set path in request */
                request_t* temp;
                if ((p != NULL) && (NULL != (temp = search_req_match(req, src, dst)))) {
                    temp->the_path = (path_t*) malloc(sizeof(path_t));
                    temp->the_path->n_nodes = p->n_nodes;
                    temp->the_path->sum_costs = p->sum_costs;
                    temp->the_path->id = (int*) malloc(sizeof(p->n_nodes));
                    for (int i = 0; i < p->n_nodes; i++)
                        temp->the_path->id[i] = p->id[i];
                } else if ((p == NULL) && (NULL != (temp = search_req_match(req, src, dst)))) {
                    temp->the_path = NULL;
                }
                /* output forward table */
                show_forward_table(p, "output.txt", srcflag);
            }
        }

        /* output message */
        show_message(req, "output.txt");

        /* apply changes */
        if (0 == get_change(argv[3], g, change_counter++))
            break;

        /* free the path previously add into the req */
        req = start_req; 
        while (req->src != INVALID) {
            free_path(req->the_path);
            req = req->next_req;
        } req = start_req;
    }

    /* IMPORTANT */
    /* memory free */
    free_graph(g);
    free_req(req);
    free_heap(heap_);
    return 0;
}

