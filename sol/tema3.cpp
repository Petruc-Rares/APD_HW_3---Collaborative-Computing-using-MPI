#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#define RANK_COORDINATOR_0 0
#define RANK_COORDINATOR_1 1
#define RANK_COORDINATOR_2 2
#define NO_COORDINATORS 3

#define MAX_NO_PROCESSORS 10000

typedef struct ClustersInfo {
    int noWorkers[NO_COORDINATORS];
    int *workers[NO_COORDINATORS];
} ClustersInfo;


std::string loggingMessages;
// as MAX_NO_PROCESSORS equals 10000
// our biggest possible rank can be 10000
// which has strlen equal to 5, so the buffer
// size should be 6 (in order to cover the '\0')
char buffer[6];

char helperBuffer[1000];

// number of neighbors of a communicator
int noNeighbors;

int parent = -1;

ClustersInfo clustersInfo;

int min(int a, int b) {
    return (a < b) ? a : b;
}

// http://www.strudel.org.uk/itoa/
char* itoa(int value, char* result, int base) {
    // check that the base if valid
    if (base < 2 || base > 36) { *result = '\0'; return result; }

    char* ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );

    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}

// fill info about the number of neighbors of current comunicator
// and the neighbors in the array
void fillInfo(int rank) {
    char fileName[16] = "cluster";
    itoa(rank, buffer, 10);
    strncat(fileName, buffer, strlen(buffer));
    strncat(fileName, ".txt", 5);

    FILE *inFile  = fopen(fileName, "r");
    if (inFile == NULL) {
        printf("ERROR! Could not open file with name: %s\n", fileName);
        exit(-1);
    }

    fscanf(inFile, "%d", &noNeighbors);

    clustersInfo.workers[rank] = (int *) malloc(noNeighbors * sizeof(int));
    if (clustersInfo.workers[rank] == NULL) {
        printf("ERROR! Could not malloc neighbors for processor with rank: %d\n", rank);
    }

    clustersInfo.noWorkers[rank] = noNeighbors;

    for (int i = 0; i < noNeighbors; i++) {
        fscanf(inFile, "%d", &clustersInfo.workers[rank][i]);
    }
    return;
}

void printTopology(int rank) {
    printf("%d -> ", rank);
    for (int i = 0; i < NO_COORDINATORS; i++) {
        printf("%d:", i);

        int noWorkers = clustersInfo.noWorkers[i];
        for (int j = 0; j < noWorkers - 1; j++) {
            printf("%d,", clustersInfo.workers[i][j]);
        }

        printf("%d ", clustersInfo.workers[i][noWorkers - 1]);
    }
    printf("\n");
}

std::string getTopology(int rank) {
    std::string topology = std::to_string(rank);
    topology.append(" -> ");


    for (int i = 0; i < NO_COORDINATORS; i++) {
        topology.append(std::to_string(i));
        topology.append(":");


        int noWorkers = clustersInfo.noWorkers[i];
        for (int j = 0; j < noWorkers - 1; j++) {
            topology.append(std::to_string(clustersInfo.workers[i][j]));
            topology.append(",");
        }

        topology.append(std::to_string(clustersInfo.workers[i][noWorkers - 1]));
        topology.append(" ");
    }
    topology.append("\n");

    return topology;
}

void printLogSent(int a, int b) {
    printf("M(%d,%d)\n", a, b);
}

std::string getLogSent(int a, int b) {
    std::string logMessage = "";
    logMessage.append("M(");
    logMessage.append(std::to_string(a));
    logMessage.append(",");
    logMessage.append(std::to_string(b));
    logMessage.append(")");
    logMessage.append("\n");

    return logMessage;
}

void printArray(int *arr, int sizeArr, int rank) {
    printf("Rezultat: ");

    for (int i = 0; i < sizeArr; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

void generateUnsortedVector(int* unsortedVector, int vectorSize) {
    for (int i = 0; i < vectorSize; i++) {
        unsortedVector[i] = i;
    }
}

int main(int argc, char*argv[]) {
    int rank;
	int nProcesses;
    int nProcesses_echo;
    int rank_echo;
    loggingMessages = "";

    MPI_Status status; 

    std::string topology;

	MPI_Init(&argc, &argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);

    int sizeArray;
    
    int *unsortedVector;
    int *sortedVector;
    if (rank == RANK_COORDINATOR_0) {
        sizeArray = atoi(argv[1]);
        unsortedVector = (int *) malloc(sizeArray * sizeof(int));
        sortedVector = (int *) malloc(sizeArray * sizeof(int));
    }

    // if coordinator
    if ((rank == RANK_COORDINATOR_0) || (rank == RANK_COORDINATOR_1) || (rank == RANK_COORDINATOR_2)) {
        fillInfo(rank);

        // send to other coordinators info about rank's cluster
        for (int i = 0; i < NO_COORDINATORS; i++) {
            if (rank != i) {
                loggingMessages.append(getLogSent(rank, i));
                MPI_Send(&noNeighbors, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                loggingMessages.append(getLogSent(rank, i));
                MPI_Send(clustersInfo.workers[rank], noNeighbors, MPI_INT, i, 0, MPI_COMM_WORLD);
            }
        }

        // receive from other coordinators info about their cluster
        for (int i = 0; i < NO_COORDINATORS - 1; i++) {
            MPI_Recv(&noNeighbors, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            clustersInfo.noWorkers[status.MPI_SOURCE] = noNeighbors;
            clustersInfo.workers[status.MPI_SOURCE] = (int *) malloc(noNeighbors * sizeof(int));

            MPI_Recv(clustersInfo.workers[status.MPI_SOURCE], noNeighbors, MPI_INT,
             status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        }

        topology = getTopology(rank);

        // now each coordinator should send the topology to his associated workers
        for (int i = 0; i < clustersInfo.noWorkers[rank]; i++) {

            // send data about each coordinator's cluster
            // number of workers and the workers' ranks
            for (int j = 0; j < NO_COORDINATORS; j++) {
                loggingMessages.append(getLogSent(rank, clustersInfo.workers[rank][i]));
                MPI_Send(&clustersInfo.noWorkers[j], 1, MPI_INT, clustersInfo.workers[rank][i], 0, MPI_COMM_WORLD);

                loggingMessages.append(getLogSent(rank, clustersInfo.workers[rank][i]));
                MPI_Send(clustersInfo.workers[j], clustersInfo.noWorkers[j], MPI_INT, clustersInfo.workers[rank][i], 0, MPI_COMM_WORLD);
            }
        }

        if (rank == RANK_COORDINATOR_0) {
            generateUnsortedVector(unsortedVector, sizeArray);

            // send the size of vector  and
            // send the part of vector associated with each worker's rank
            for (int it = 0; it < clustersInfo.noWorkers[rank]; it++) {
                int rankCrtWorker = clustersInfo.workers[rank][it];
                loggingMessages.append(getLogSent(rank, rankCrtWorker)); 
                MPI_Send(&sizeArray, 1, MPI_INT, rankCrtWorker, 0, MPI_COMM_WORLD);

                int start = (rankCrtWorker - NO_COORDINATORS) * (double) sizeArray / (nProcesses - NO_COORDINATORS);
                int end = min(((rankCrtWorker - NO_COORDINATORS) + 1) * (double) sizeArray / (nProcesses - NO_COORDINATORS), sizeArray);
                int chunkSize = end - start;                

                loggingMessages.append(getLogSent(rank, rankCrtWorker));
                MPI_Send(unsortedVector + start, chunkSize, MPI_INT, rankCrtWorker, 0, MPI_COMM_WORLD);
            }

            // send whole vector to coordinators 1 and 2
            for (int it = RANK_COORDINATOR_1; it <= RANK_COORDINATOR_2; it++) {
                loggingMessages.append(getLogSent(rank, it));
                MPI_Send(&sizeArray, 1, MPI_INT, it, 0, MPI_COMM_WORLD);

                loggingMessages.append(getLogSent(rank, it));
                MPI_Send(unsortedVector, sizeArray, MPI_INT, it, 0, MPI_COMM_WORLD);
            }

            // receive partially sorted vectors from workers in cluster 0
            for (int it = 0; it < clustersInfo.noWorkers[rank]; it++) {
                int rankCrtWorker = clustersInfo.workers[rank][it];

                int start = (rankCrtWorker - NO_COORDINATORS) * (double) sizeArray / (nProcesses - NO_COORDINATORS);
                int end = min(((rankCrtWorker - NO_COORDINATORS) + 1) * (double) sizeArray / (nProcesses - NO_COORDINATORS), sizeArray);
                int chunkSize = end - start;     

                MPI_Recv(sortedVector + start, chunkSize, MPI_INT, rankCrtWorker, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            }

            // receive partially sorted vectors from coordinator 1
            
            MPI_Recv(unsortedVector, sizeArray, MPI_INT, RANK_COORDINATOR_1, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            int noWorkers = clustersInfo.noWorkers[RANK_COORDINATOR_1];
            for (int itWorkers = 0; itWorkers < noWorkers; itWorkers++) {
                int rankCrtWorker = clustersInfo.workers[RANK_COORDINATOR_1][itWorkers];

                int start = (rankCrtWorker - NO_COORDINATORS) * (double) sizeArray / (nProcesses - NO_COORDINATORS);
                int end = min(((rankCrtWorker - NO_COORDINATORS) + 1) * (double) sizeArray / (nProcesses - NO_COORDINATORS), sizeArray);                
            
                for (int i = start; i < end; i++) {
                    sortedVector[i] = unsortedVector[i];
                }
            }
            // receive partially sorted vectors from coordinator 2
            MPI_Recv(unsortedVector, sizeArray, MPI_INT, RANK_COORDINATOR_2, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            noWorkers = clustersInfo.noWorkers[RANK_COORDINATOR_2];
            for (int itWorkers = 0; itWorkers < noWorkers; itWorkers++) {
                int rankCrtWorker = clustersInfo.workers[RANK_COORDINATOR_2][itWorkers];

                int start = (rankCrtWorker - NO_COORDINATORS) * (double) sizeArray / (nProcesses - NO_COORDINATORS);
                int end = min(((rankCrtWorker - NO_COORDINATORS) + 1) * (double) sizeArray / (nProcesses - NO_COORDINATORS), sizeArray);                
            
                for (int i = start; i < end; i++) {
                    sortedVector[i] = unsortedVector[i];
                }
            }

            printArray(sortedVector, sizeArray, rank);

        } else if (rank == RANK_COORDINATOR_1) {
            MPI_Recv(&sizeArray, 1, MPI_INT, RANK_COORDINATOR_0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            unsortedVector = (int *) malloc(sizeArray * sizeof(int));

            // receive unsorted vector from coordinator 0
            MPI_Recv(unsortedVector, sizeArray, MPI_INT, RANK_COORDINATOR_0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            // send unsorted vector to workers in cluster1
            for (int it = 0; it < clustersInfo.noWorkers[rank]; it++) {
                int rankCrtWorker = clustersInfo.workers[rank][it];

                loggingMessages.append(getLogSent(rank, rankCrtWorker));
                MPI_Send(&sizeArray, 1, MPI_INT, rankCrtWorker, 0, MPI_COMM_WORLD);

                int start = (rankCrtWorker - NO_COORDINATORS) * (double) sizeArray / (nProcesses - NO_COORDINATORS);
                int end = min(((rankCrtWorker - NO_COORDINATORS) + 1) * (double) sizeArray / (nProcesses - NO_COORDINATORS), sizeArray);
                int chunkSize = end - start;                

                loggingMessages.append(getLogSent(rank, rankCrtWorker));
                MPI_Send(unsortedVector + start, chunkSize, MPI_INT, rankCrtWorker, 0, MPI_COMM_WORLD);
            }        

            // receive sorted vector from workers in cluster1
            for (int it = 0; it < clustersInfo.noWorkers[rank]; it++) {
                int rankCrtWorker = clustersInfo.workers[rank][it];

                int start = (rankCrtWorker - NO_COORDINATORS) * (double) sizeArray / (nProcesses - NO_COORDINATORS);
                int end = min(((rankCrtWorker - NO_COORDINATORS) + 1) * (double) sizeArray / (nProcesses - NO_COORDINATORS), sizeArray);
                int chunkSize = end - start;     

                MPI_Recv(unsortedVector + start, chunkSize, MPI_INT, rankCrtWorker, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            }


            // send partially sorted vector to coordinator 0
            MPI_Send(unsortedVector, sizeArray, MPI_INT, RANK_COORDINATOR_0, 0, MPI_COMM_WORLD);
            loggingMessages.append(getLogSent(rank, RANK_COORDINATOR_0));
        } else if (rank == RANK_COORDINATOR_2) {
            MPI_Recv(&sizeArray, 1, MPI_INT, RANK_COORDINATOR_0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            unsortedVector = (int *) malloc(sizeArray * sizeof(int));
            // receive unsorted vector from coordinator 0
            MPI_Recv(unsortedVector, sizeArray, MPI_INT, RANK_COORDINATOR_0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            // send unsorted vector to workers in cluster2
            for (int it = 0; it < clustersInfo.noWorkers[rank]; it++) {
                int rankCrtWorker = clustersInfo.workers[rank][it];

                loggingMessages.append(getLogSent(rank, rankCrtWorker));
                MPI_Send(&sizeArray, 1, MPI_INT, rankCrtWorker, 0, MPI_COMM_WORLD);

                int start = (rankCrtWorker - NO_COORDINATORS) * (double) sizeArray / (nProcesses - NO_COORDINATORS);
                int end = min(((rankCrtWorker - NO_COORDINATORS) + 1) * (double) sizeArray / (nProcesses - NO_COORDINATORS), sizeArray);
                int chunkSize = end - start;                

                MPI_Send(unsortedVector + start, chunkSize, MPI_INT, rankCrtWorker, 0, MPI_COMM_WORLD);
                loggingMessages.append(getLogSent(rank, rankCrtWorker));
            }        
            // receive sorted vector from workers in cluster2
            for (int it = 0; it < clustersInfo.noWorkers[rank]; it++) {
                int rankCrtWorker = clustersInfo.workers[rank][it];

                int start = (rankCrtWorker - NO_COORDINATORS) * (double) sizeArray / (nProcesses - NO_COORDINATORS);
                int end = min(((rankCrtWorker - NO_COORDINATORS) + 1) * (double) sizeArray / (nProcesses - NO_COORDINATORS), sizeArray);
                int chunkSize = end - start;     

                MPI_Recv(unsortedVector + start, chunkSize, MPI_INT, rankCrtWorker, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            }

            // send partially sorted vector to coordinator 0
            MPI_Send(unsortedVector, sizeArray, MPI_INT, RANK_COORDINATOR_0, 0, MPI_COMM_WORLD);
            loggingMessages.append(getLogSent(rank, RANK_COORDINATOR_0));
        }
    } else {
        // here are the workers

        // receive from the associated coordinator info gathered from
        // all coordinataors
        for (int i = 0; i < NO_COORDINATORS; i++) {
            MPI_Recv(&noNeighbors, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            clustersInfo.noWorkers[i] = noNeighbors;
            clustersInfo.workers[i] = (int *) malloc(noNeighbors * sizeof(int));

            if (parent == -1) {
                parent = status.MPI_SOURCE;
            }

            MPI_Recv(clustersInfo.workers[i], noNeighbors, MPI_INT,
            status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        }

        topology = getTopology(rank);

        MPI_Recv(&sizeArray, 1, MPI_INT, parent, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        unsortedVector = (int *) malloc(sizeArray * sizeof(int));

        // receive array to do computations on from the coordinator of the cluster
        int start = (rank - NO_COORDINATORS) * (double) sizeArray / (nProcesses - NO_COORDINATORS);
        int end = min(((rank - NO_COORDINATORS) + 1) * (double) sizeArray / (nProcesses - NO_COORDINATORS), sizeArray);
        int chunkSize = end - start;

        MPI_Recv(unsortedVector + start, chunkSize, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        for (int i = start; i < end; i++) {
            unsortedVector[i] *= 2;
        }

        // send partially sorted vector back to coordinator
        MPI_Send(unsortedVector + start, chunkSize, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
        loggingMessages.append(getLogSent(rank, status.MPI_SOURCE));
    }

    memset(helperBuffer, 0, 1000);

    if (rank == RANK_COORDINATOR_0) {
        // print topologies
        // from current rank
        printf("%s", topology.c_str());
    
        // received from the other ranks
        for (int it = 0; it < nProcesses - 1; it++) {
            MPI_Recv(helperBuffer, 1000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            printf("%s", helperBuffer);
            memset(helperBuffer, 0, 1000);
        }
        // print logging messages


        MPI_Barrier(MPI_COMM_WORLD);
        
        printf("%s", loggingMessages.c_str());

        memset(helperBuffer, 0, 1000);

        for (int it = 0; it < nProcesses - 1; it++) {
            MPI_Recv(helperBuffer, 1000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);            
            printf("%s", helperBuffer);
            memset(helperBuffer, 0, 1000);
        }

    } else {
        // if rank_coordinator_1 or rank_coordinator_2 send to rank_coordinator_0
        if ((rank == RANK_COORDINATOR_1) || (rank == RANK_COORDINATOR_2)) {
            MPI_Send(topology.c_str(), topology.size(), MPI_CHAR, RANK_COORDINATOR_0, 0, MPI_COMM_WORLD);
            loggingMessages.append(getLogSent(rank, RANK_COORDINATOR_0));

            // receive topology from associated cluster and send it to RANK_COORDINATOR 0
            for (int itWorkers = 0; itWorkers < clustersInfo.noWorkers[rank]; itWorkers++) {
                MPI_Recv(helperBuffer, 1000, MPI_CHAR, clustersInfo.workers[rank][itWorkers], MPI_ANY_TAG, MPI_COMM_WORLD, &status);

                loggingMessages.append(getLogSent(rank, RANK_COORDINATOR_0));
                MPI_Send(helperBuffer, 1000, MPI_CHAR, RANK_COORDINATOR_0, 0, MPI_COMM_WORLD);
                memset(helperBuffer, 0, 1000);
            }

            MPI_Barrier(MPI_COMM_WORLD);



            loggingMessages.append(getLogSent(rank, RANK_COORDINATOR_0));

            // send rank's logging messages
            MPI_Send(loggingMessages.c_str(), loggingMessages.size(), MPI_CHAR, RANK_COORDINATOR_0, 0, MPI_COMM_WORLD);


            // receive from the workers associated with current cluster info about their logging
            for (int itWorkers = 0; itWorkers < clustersInfo.noWorkers[rank]; itWorkers++) {
                                memset(helperBuffer, 0, 1000);
                MPI_Recv(helperBuffer, 1000, MPI_CHAR, clustersInfo.workers[rank][itWorkers], MPI_ANY_TAG, MPI_COMM_WORLD, &status);

                loggingMessages = std::string(helperBuffer);
                loggingMessages.append(getLogSent(rank, RANK_COORDINATOR_0));
                MPI_Send(loggingMessages.c_str(), loggingMessages.size(), MPI_CHAR, RANK_COORDINATOR_0, 0, MPI_COMM_WORLD);
            }
        } else {
            // send topology from worker to parent
            MPI_Send(topology.c_str(), topology.size(), MPI_CHAR, parent, 0, MPI_COMM_WORLD);
            loggingMessages.append(getLogSent(rank, parent));      

            MPI_Barrier(MPI_COMM_WORLD);

            // send logging from worker to parent
            loggingMessages.append(getLogSent(rank, parent));
            MPI_Send(loggingMessages.c_str(), loggingMessages.size(), MPI_CHAR, parent, 0, MPI_COMM_WORLD);
        }
    }

    free(unsortedVector);
    if (rank == RANK_COORDINATOR_0) {
        free(sortedVector);
    }

    for (int i = 0; i < NO_COORDINATORS; i++) {
        free(clustersInfo.workers[i]);
    }

    MPI_Finalize();
}