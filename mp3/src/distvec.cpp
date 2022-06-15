#include "../lib/distvec.h"
#include <string>
#define DEBUG 1

// global varaibles
set<int> node_set;

// number of nodes & node map
int num_nodes = 0;
map<int, int> md;   // id to index
map<int, int> mx;   // index to id
// vec table: 2 dimonsions
map<int, map<int, int> > table;
// minimum cost table: 2 dimonsions
map<int, map<int, int> > cost;
// next hop for minimun cost path: 2 dimonsions
map<int, map<int, int> > hop;
// neighbor of each nodes
map<int, set<int> > neighbor;





/*########################################################
*                        Functions
##########################################################*/
void construct_graph(const char* filename)
{
    FILE* f = fopen (filename, "r");
    if (f == NULL) {
        cout << "topofile open failed\n";
        return;
    }

    // count for the node number
    while (1) {
        int node1, node2, cost;
        if (3 != fscanf(f, "%d%d%d", &node1, &node2, &cost))
            break;
        /* update node number */
        node_set.insert(node1);
        node_set.insert(node2);
    } fclose(f);
    num_nodes = node_set.size();

    for (int i = 0; i < num_nodes; i++)
    {

        for (int j = 0; j < num_nodes; j++)
        {
            table[i][j] = INF;
            cost[i][j] = INF;
            hop[i][j] = INVALID;
        }
    }


    // initialize the map
    int counter = 0;
    for (int i: node_set)
    {
        md[i] = counter;
        mx[counter] = i;
        counter += 1;
    }

    // reopen to set the graph
    FILE* fd = fopen (filename, "r");
    if (fd == NULL) {
        cout << "topofile open failed\n";
        return;
    }
    
    while (1) {
        int id1, id2, length;
        if (3 != fscanf(fd, "%d%d%d", &id1, &id2, &length))
            break;
        /* update the vec table */
        table[md[id1]][md[id2]] = length;
        table[md[id2]][md[id1]] = length;
        // cost[md[id1]][md[id2]] = length;
        // cost[md[id2]][md[id1]] = length;
        // update the next hop
        hop[md[id1]][md[id2]] = id2;
        hop[md[id2]][md[id1]] = id1;
        // update neighbors
        neighbor[md[id1]].insert(id2);
        neighbor[md[id2]].insert(id1);
    }

    // change self information
    for (int i = 0; i < num_nodes; i++)
    {
        table[i][i] = 0;
        cost[i][i] = 0;
        hop[i][i] = mx[i];
    }
    fclose(fd);
}


// not finished yet
int get_change(const char* filename, int change_times)
{
    FILE* f = fopen (filename, "r");
    if (f == NULL) {
        cout << "changefile open failed\n";
        return 0;
    }

    for (int i = 0; i < change_times; i++) {
        int id1, id2, length;

        /* check availability */
        if (3 != fscanf(f, "%d%d%d", &id1, &id2, &length))
            return 0;
        if (i != change_times - 1)
            continue;


        /* update the graph based on the change*/
        if (length != BROKEN)
        {
            table[md[id1]][md[id2]] = length;
            table[md[id2]][md[id1]] = length;
            neighbor[md[id1]].insert(id2);
            neighbor[md[id2]].insert(id1);
        }
        else
        {
            table[md[id1]][md[id2]] = INF;
            table[md[id2]][md[id1]] = INF;
            neighbor[md[id1]].erase(id2);
            neighbor[md[id2]].erase(id1);
        }
        break;
    }
    fclose(f);
    return 1;
}


void converge()
{
    // reset cost
    for (int i = 0; i < num_nodes; i++)
    {

        for (int j = 0; j < num_nodes; j++)
        {
            cost[i][j] = table[i][j];
        }
    }
    map<int, map<int, int> > cost_updated = cost;
    map<int, map<int, int> > hop_updated = hop;
    int converged = 0; 
    while(converged == 0)
    {
        int change = 0;
        // loop through all the nodes
        for (int i: node_set)
        {
            for (int j: node_set)
            {
                if (j == i)
                {
                    continue;
                }
                int current_cost = cost[md[i]][md[j]];
                int current_next_hop = hop[md[i]][md[j]];
                // loop to find if there is a better path: i -> k -> j
                for (int k: neighbor[md[i]])
                {
                    int new_cost = table[md[i]][md[k]] + cost[md[k]][md[j]];
                    // update. remember the tie breaker
                    if ((new_cost < current_cost) || ((new_cost == current_cost) && (k < current_next_hop)))
                    {
                        change = 1;
                        cost_updated[md[i]][md[j]] = new_cost;
                        hop_updated[md[i]][md[j]] = k;
                    }
                }
            }
        }
        cost = cost_updated;
        hop = hop_updated;
        // decide if need to converge again
        if (change == 0)
        {
            converged = 1;
        }
        else{
            converged = 0;
        }
    }
}


void output_topology()
{
    // create the file to write
    ofstream outfile;
    outfile.open("output.txt", ios::app);
    // output the topology info for node i
    for (int i = 0; i < num_nodes; i++)
    {
        for (int j = 0; j < num_nodes; j++)
        {
            if (cost[i][j] < INF)
            {
                outfile << mx[j] << " " << hop[i][j] << " " << cost[i][j] << endl;
            }
            else
            {
                // outfile << mx[j] << " " << "-1" << " " << "-999" << endl;
            }
        }
    }
    outfile.close();
}


void output_message(const char* filename)
{
    // read the file
    ifstream f(filename);
    if (!f.is_open()) {
        cout << "messagefile open failed\n";
        return;
    }
    // create the file to write
    ofstream outfile;
    outfile.open("output.txt", ios::app);

    // get message
    string line;
    while (getline(f, line)) {
        /* get message info */
        int space1_position = 0;
        for (int i = 0; i < line.size(); i++)
        {
            if(line[i] == ' ')
            {
                space1_position = i;
                break;
            }
        }
        if (space1_position == 0)
        {
            continue;
        }
        int space2_position = 0;
        for (int i = space1_position + 1; i < line.size(); i++)
        {
            if(line[i] == ' ')
            {
                space2_position = i;
                break;
            }
        }
        string sub1 = line.substr(0, space1_position);
        string sub2 = line.substr(space1_position + 1, space2_position - space1_position - 1);
        string msg = line.substr(space2_position + 1);
        int id1 = atoi(sub1.c_str());
        int id2 = atoi(sub2.c_str());

        if (cost[md[id1]][md[id2]] < INF)
        {
            // find the hops
            string hop_string = to_string(id1);
            int next_hop = hop[md[id1]][md[id2]];
            while(next_hop != id2)
            {
                hop_string += " ";
                hop_string += to_string(next_hop);
                next_hop = hop[md[next_hop]][md[id2]];
            }

            outfile << "from " << sub1 << " to " << sub2 << " cost " << cost[md[id1]][md[id2]] << " hops " << hop_string << " message " << msg << endl;
        }
        else
        {
            outfile << "from " << sub1 << " to " << sub2 << " cost " << "infinite" << " hops " << "unreachable" << " message " << msg << endl;
        }

    }
    outfile.close();
}





int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }

    /* clean the output file */ 
    ofstream temp("output.txt", ofstream::out | ofstream::trunc);
    temp.close();

    //request_t* req = read_msg_req(argv[2]);
    construct_graph(argv[1]);
    converge();
    output_topology();
    output_message(argv[2]);
    int change_time = 1;
    while(1 == get_change(argv[3], change_time))
    {
        converge();
        #if DEBUG
            cout << endl << endl << endl;
            cout << num_nodes << endl;
            cout << "2's neighbor: " << endl;
            for (int k: neighbor[md[2]])
            {
                cout << k << " ";
            }
            cout << endl;
            for (int i = 0; i < num_nodes; i++)
            {
                for (int j = 0; j < num_nodes; j++)
                {   cout << i + 1 << j + 1 << endl;
                    cout << "edge: " << table[i][j] << endl;
                    cout << "cost: " << cost[i][j] << endl;
                    cout << "next hop: " << hop[i][j] << endl;
                }
            }
        #endif
        //request_t* start_req = req;
        output_topology();
        output_message(argv[2]);
        change_time += 1;
    }
    return 0;
}




// // main for debug
// int main(int argc, char** argv) {
//     //printf("Number of arguments: %d", argc);
//     if (argc != 4) {
//         printf("Usage: ./distvec topofile messagefile changesfile\n");
//         return -1;
//     }

//     //request_t* req = read_msg_req(argv[2]);
//     construct_graph(argv[1]);
//     // get_change(argv[3], 1);
//     // get_change(argv[3], 2);
//     // get_change(argv[3], 3);
//     converge();
//     //request_t* start_req = req;
    
//     #if DEBUG
//         cout << num_nodes << endl;
//         for (int i = 0; i < num_nodes; i++)
//         {

//             for (int j = 0; j < num_nodes; j++)
//             {   cout << i + 1 << j + 1 << endl;
//                 cout << "edge: " << table[i][j] << endl;
//                 cout << "cost: " << cost[i][j] << endl;
//                 cout << "next hop: " << hop[i][j] << endl;
//             }
//         }
//     #endif




//     FILE *fpOut;
//     fpOut = fopen("output.txt", "w");
//     fclose(fpOut);

//     return 0;
// }