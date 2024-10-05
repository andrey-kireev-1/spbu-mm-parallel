#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

// Defining a very large value as a substitute for infinity
#define INF 1000000

// Function to determine the owner of the kth row
int calculate_owner(int k, int total_procs, int V)
{
    return k / (V / total_procs);
}

// Function to fetch the kth row of the matrix
void fetch_kth_row(int *matrix_data, int V, int total_procs, int *target_row,
                   int k)
{
    int local_row_index = k % (V / total_procs);
    for (int col = 0; col < V; col++) {
        target_row[col] = matrix_data[local_row_index * V + col];
    }
}

// Implementation of the Floyd-Warshall algorithm
void execute_floyd(int *matrix, int V, int process_id, int total_procs,
                   MPI_Comm communicator)
{
    int *target_row = (int *) malloc(V * sizeof(int));
    if (!target_row) {
        fprintf(stderr, "Failed to allocate memory for target_row\n");
        MPI_Abort(communicator, 1);
    }

    for (int k = 0; k < V; k++) {
        int root_process = calculate_owner(k, total_procs, V);
        if (process_id == root_process) {
            fetch_kth_row(matrix, V, total_procs, target_row, k);
            for (int i = 0; i < total_procs; i++) {
                if (i != process_id) {
                    MPI_Send(target_row, V, MPI_INT, i, 0, communicator);
                }
            }
        } else {
            MPI_Recv(target_row, V, MPI_INT, root_process, 0, communicator,
                     MPI_STATUS_IGNORE);
        }

        for (int i = 0; i < V / total_procs; i++) {
            for (int j = 0; j < V; j++) {
                int calculated_distance = matrix[i * V + k] + target_row[j];
                if (calculated_distance < matrix[i * V + j]) {
                    matrix[i * V + j] = calculated_distance;
                }
            }
        }
    }
    free(target_row);
}

// Initialize the matrix with zeros on the diagonal and infinity elsewhere
void initialize_matrix(int *matrix, int V)
{
    for (int i = 0; i < V; ++i) {
        for (int j = 0; j < V; ++j) {
            matrix[i * V + j] = (i == j) ? 0 : INF;
        }
    }
}

// Read the matrix from a file and distribute it among processes
void distribute_matrix_from_file(int *matrix, int V, int total_procs,
                                 const char *filename, MPI_Comm communicator)
{
    int process_id, *buffer = NULL;
    MPI_Comm_rank(communicator, &process_id);

    if (process_id == 0) {
        buffer = (int *) malloc(V * V * sizeof(int));
        if (!buffer) {
            fprintf(stderr, "Failed to allocate memory for buffer\n");
            MPI_Abort(communicator, 1);
        }

        FILE *file_ptr = fopen(filename, "r");
        if (!file_ptr) {
            fprintf(stderr, "Unable to open file: %s\n", filename);
            MPI_Abort(communicator, 1);
        }

        fscanf(file_ptr, "%*d");
        initialize_matrix(buffer, V);

        int src, dst, weight;
        while (fscanf(file_ptr, "%d %d %d", &src, &dst, &weight) == 3) {
            src--;
            dst--;
            if (buffer[src * V + dst] > weight) {
                buffer[src * V + dst] = weight;
                buffer[dst * V + src] = weight;
            }
        }

        fclose(file_ptr);

        MPI_Scatter(buffer, V * V / total_procs, MPI_INT, matrix,
                    V * V / total_procs, MPI_INT, 0, communicator);
        free(buffer);
    } else {
        MPI_Scatter(buffer, V * V / total_procs, MPI_INT, matrix,
                    V * V / total_procs, MPI_INT, 0, communicator);
    }
}

void print_matrix(int *matrix, int V, int process_id, int total_procs,
                  MPI_Comm communicator)
{
    int *aggregate_matrix = NULL;
    FILE *file = NULL;

    if (process_id == 0) {
        aggregate_matrix = (int *) malloc(V * V * sizeof(int));
        if (!aggregate_matrix) {
            fprintf(stderr, "Failed to allocate memory for aggregate_matrix\n");
            MPI_Abort(communicator, 1);
        }

        MPI_Gather(matrix, V * V / total_procs, MPI_INT, aggregate_matrix,
                   V * V / total_procs, MPI_INT, 0, communicator);

        file = fopen("Floyd.output", "w");
        if (!file) {
            fprintf(stderr, "Failed to open file Floyd.output\n");
            MPI_Abort(communicator, 1);
        }

        for (int i = 0; i < V; i++) {
            for (int j = 0; j < V; j++) {
                if (aggregate_matrix[i * V + j] == INF)
                    fprintf(file, "INF ");
                else
                    fprintf(file, "%d ", aggregate_matrix[i * V + j]);
            }
            fprintf(file, "\n");
        }
        free(aggregate_matrix);
        fclose(file);
    } else {
        MPI_Gather(matrix, V * V / total_procs, MPI_INT, NULL,
                   V * V / total_procs, MPI_INT, 0, communicator);
    }
}

int main(int argc, char *argv[])
{
    int V, *distributed_matrix;
    MPI_Comm comm = MPI_COMM_WORLD;
    int total_procs, process_id;
    double start_time, end_time;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(comm, &total_procs);
    MPI_Comm_rank(comm, &process_id);

    if (process_id == 0) {
        FILE *input_file = fopen("Floyd.input", "r");
        if (!input_file) {
            fprintf(stderr, "Failed to open the input file.\n");
            MPI_Abort(comm, 1);
        }
        fscanf(input_file, "%d", &V);
        fclose(input_file);

        if (V % total_procs != 0) {
            fprintf(
                stderr,
                "Number of rows in the adjacency matrix (%d) is not divisible "
                "by number of processes (%d). Uneven distribution is not "
                "supported, exiting..\n",
                V, total_procs);
            MPI_Abort(comm, 1);
        }
    }

    MPI_Bcast(&V, 1, MPI_INT, 0, comm);

    distributed_matrix = (int *) malloc(V * V / total_procs * sizeof(int));
    if (!distributed_matrix) {
        fprintf(stderr,
                "Failed to allocate memory for the distributed matrix.\n");
        MPI_Abort(comm, 1);
    }

    distribute_matrix_from_file(distributed_matrix, V, total_procs,
                                "Floyd.input", comm);

    if (process_id == 0) {
        start_time = MPI_Wtime();
    }

    execute_floyd(distributed_matrix, V, process_id, total_procs, comm);

    if (process_id == 0) {
        end_time = MPI_Wtime();
        printf("Time taken: %f seconds\n", end_time - start_time);
    }

    print_matrix(distributed_matrix, V, process_id, total_procs, comm);

    free(distributed_matrix);
    MPI_Finalize();

    return 0;
}
