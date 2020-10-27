#include "samplesort.h"

int cmpfunc(const void *a, const void *b)
{
	return (*(int *)a - *(int *)b);
}

int main(int argc, char *argv[])
{
	int num_tasks,
		task_id,
		num_workers,
		sample_vec_size,
		size;
	int opt, rc;
	int **local_buckets, **global_buckets;
	int *vector, *sample_vec, *local_splitter, *global_splitter;
	int *local_bucket_sizes, *global_bucket_buffer_sizes, *displ, *global_bucket_sizes;
	int local_splitter_element_size, global_splitter_size;
	int count;
	time_t t;
	MPI_Status status;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &num_tasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &task_id);

	if (num_tasks < 2)
	{
		printf("Need at least two MPI tasks. Quitting...\n");
		MPI_Abort(MPI_COMM_WORLD, rc);
		exit(1);
	}

	if (task_id == MASTER)
	{

		while ((opt = getopt(argc, argv, "s:h")) != -1)
		{
			switch (opt)
			{
			case 's':
				size = atoi(optarg);
				if ((size % num_tasks) != 0)
				{

					printf("Number of elements are not divisible by number of processes \n");
					MPI_Finalize();
					exit(0);
				}
				break;
			case 'h':
				printf("Usage: mpirun %s -np (nº processes) -s (size input) -h (help) \n", argv[0]);
				MPI_Finalize();
				exit(EXIT_SUCCESS);
			case '?':
				MPI_Finalize();
				exit(EXIT_FAILURE);
			default:
				MPI_Finalize();
				abort();
			}
		}
		vector = (int *)calloc(size, sizeof(int));
		srand(time(NULL));

		while (count < size)
		{
			int random_number = rand() % size + 1;
			int repeated = FALSE;
			for (int i = 0; i < size; i++)
			{
				if (vector[i] == random_number)
				{
					repeated = TRUE;
				}
			}
			if (!repeated)
			{
				vector[count] = random_number;

				count++;
			}
		}
	}

	MPI_Bcast(&size, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

	sample_vec_size = size / num_tasks;
	sample_vec = (int *)calloc(sample_vec_size, sizeof(int));

	MPI_Scatter(vector, sample_vec_size, MPI_INT, sample_vec,
				sample_vec_size, MPI_INT, MASTER, MPI_COMM_WORLD);

	qsort(sample_vec, sample_vec_size, sizeof(int), cmpfunc);

	local_splitter_element_size = num_tasks - 1;
	local_splitter = (int *)calloc(local_splitter_element_size, sizeof(int));
	for (int i = 0; i < local_splitter_element_size; i++)
	{
		local_splitter[i] = sample_vec[size / (num_tasks * num_tasks) * (i + 1)];
	}

	global_splitter_size = local_splitter_element_size * num_tasks;
	global_splitter = (int *)calloc(global_splitter_size, sizeof(int));
	MPI_Gather(local_splitter, local_splitter_element_size, MPI_INT, global_splitter, local_splitter_element_size,
			   MPI_INT, MASTER, MPI_COMM_WORLD);
	if (task_id == MASTER)
	{

		global_splitter[global_splitter_size - 1] = INT_MAX;
		qsort(global_splitter, global_splitter_size, sizeof(int), cmpfunc);
	}
	MPI_Bcast(global_splitter, global_splitter_size, MPI_INT, MASTER, MPI_COMM_WORLD);

	local_buckets = (int **)malloc(sizeof(int *) * global_splitter_size);
	for (int i = 0; i < global_splitter_size; i++)
	{
		local_buckets[i] = (int *)malloc(sizeof(int) * 2 * size / global_splitter_size);
	}

	local_bucket_sizes = (int *)calloc(global_splitter_size, sizeof(int));

	for (int i = 0; i < sample_vec_size; i++)
	{
		int count = 0;
		while (count < global_splitter_size)
		{
			if (sample_vec[i] < global_splitter[count])
			{
				local_buckets[count][local_bucket_sizes[count]] = sample_vec[i];
				local_bucket_sizes[count]++;
				count = global_splitter_size; // break
			}
			count++;
		}
	}

	printf("PROCESS %d\n", task_id);
	for (int i = 0; i < local_bucket_sizes[0]; i++)
	{
		printf("%d ", local_buckets[0][i]);
	}
	printf("\n");

	global_bucket_buffer_sizes = (int *)calloc(global_splitter_size, sizeof(int));
	displ = (int *)calloc(global_splitter_size, sizeof(int));
	global_buckets = (int **)malloc(sizeof(int *) * global_splitter_size);
	for (int i = 0; i < global_splitter_size; i++)
	{
		global_buckets[i] = (int *)malloc(sizeof(int) * 2 * size / global_splitter_size);
	}
	for (int i = 0; i < global_splitter_size; i++)
	{
		MPI_Gather(&local_bucket_sizes[i], 1, MPI_INT, global_bucket_buffer_sizes, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
		if (task_id == MASTER)
		{

			for (int j = 1; j < global_splitter_size; j++)
			{

				displ[j] = displ[j - 1] + global_bucket_buffer_sizes[j - 1];
			}
		}
		MPI_Gatherv(local_buckets[i], local_bucket_sizes[i], MPI_INT,
					global_buckets[i], global_bucket_buffer_sizes, displ, MPI_INT, MASTER, MPI_COMM_WORLD);
		if (task_id == MASTER)
		{
			for (int j = 0; j < global_splitter_size; j++)
			{
				global_bucket_buffer_sizes[j] = 0;
				displ[j] = 0;
			}
		}
	}
	// global_bucket_sizes = (int *)calloc(global_splitter_size, sizeof(int));
	// MPI_Reduce(local_bucket_sizes, global_bucket_sizes, global_splitter_size, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	// if (task_id == MASTER)
	// {
	// 	printf("ROOT\n");
	// 	for (int i = 0; i < global_bucket_sizes[0]; i++)
	// 	{
	// 		printf("%d ", global_buckets[0][i]);
	// 	}
	// 	printf("\n");
	// }

	MPI_Finalize();
	return 0;
}
