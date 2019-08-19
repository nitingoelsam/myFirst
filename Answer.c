#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<string.h>

//definiton of memory pool data structure  
struct node {
	struct node *next;  //pointer to next node in list
	int data_size;		//actual size of data pointed 
	int size;			//total size of this node initially allocated 
	char *data;			//pointer to actual data 
	int in_progress;	// to check if this node is under process and if its 1, means this node cant be deleted now. 
};

// head node of the free queue
struct node *free_queue =NULL;

//head node of the data queue 
struct node *data_queue =NULL;

// last node in free queue
struct node *last_node_in_free_queue = NULL;

//last node in data queue
struct node *last_node_in_data_queue = NULL;

//Customizing is possible, and as of now 8kB data queue is max 
#define NUM_OF_BUCKETS_TYPES 6

//assuming number of buckets for each size are 10 here. 
#define NUM_BLOCKS_FOR_EACH_SIZE 10

//assuming the max size is 8192 Bytes, as per above config
#define bufferSizeInBytes 8192

pthread_mutex_t lock_rw_func;
pthread_cond_t space_available, item_available;

// total number of nodes in free queue
int max_node_free_queue= 0;

//total number of nodes available to be read
int read_ready_nodes = 0;

//added for code compilation
void process_data(char *buffer, int buffer_size)
{
	printf(" processed data \n");
	// Data is available in buffer with its size in bufer_size. 
	// Code here for data processing goes here
}

//added this function for testing and debugging purpose
/* find length of the list */
int length(struct node *temp)
{
    int i = 0;
    while(temp != NULL)
    {
        temp = temp->next;	
        i++;
    }
    return i;
}

/* @brief get a node from free queue of specified size.
 * @param [in] size
 * @returns struct node* if success and NULL otherwise
 */
struct node* get_free_node(int size)
{
	struct node *cur, *prev;
	cur = free_queue;
	prev = free_queue;
	 printf("Enter %s\n", __func__);	
	//if all nodes are occupied, return NULL from here
	if((read_ready_nodes == max_node_free_queue) || ( cur->next == NULL && cur->size < size))
	{
		return NULL;
	}
	
	while(cur->next != NULL) 
	{
		if(cur->size >= size)
			break;
		else
		{
			prev = cur;
			cur= cur->next;
		}
	}
	//if no node available as per size required
	if(cur->size <size)
	{
		return NULL;
	}
	
	if(cur == free_queue)
	{
		//first node is desired one
		free_queue = free_queue->next;
		cur->next = NULL;
	}
	else
	{
	    /* remove cur from free list*/
	    prev->next = cur->next;
	    cur->next = NULL;
	}
	
	/*move to data queue*/
	if(last_node_in_data_queue == NULL)
	{
	    data_queue = cur;
	    last_node_in_data_queue = data_queue;    
	}
	else 
	{
	    last_node_in_data_queue->next = cur;
	    last_node_in_data_queue = cur;
	}
	return cur;
}

/* @brief pushed the node from free queue to data queue after copying data.
 * @param [in] buffer
 * @param [in] data_size
 * @returns 
 */

void push_data_into_queue(char* buffer, int data_size)
{
    
    struct node *free_node;
	 printf("Enter %s\n", __func__);	
	pthread_mutex_lock(&lock_rw_func);
	while ((free_node = get_free_node(data_size)) == NULL)
		pthread_cond_wait(&space_available, &lock_rw_func);		


	free_node->data_size = data_size;
	memcpy(free_node->data, buffer, data_size);
	// for reliablity purpose, setting this bit to 1. 
	// In case of any error reset it to 0 which means this node is to be processed by another thread.
	free_node->in_progress = 1;
	read_ready_nodes++;
	
	pthread_cond_signal(&item_available); // we have a new data node to read 
	pthread_mutex_unlock(&lock_rw_func);
	
	return;
}

/* @brief pushed the node from data queue to free queue after copying data.
 * @param [out] buffer
 * @param [out] size
 * @returns 
 */

void fetch_data_node_from_queue(char* buffer, int* size)
{
	
	struct node* cur;
    printf("Enter %s\n", __func__);	
	pthread_mutex_lock(&lock_rw_func);
	/*if we dont have any data to read, lets wait */
	while(read_ready_nodes == 0) 
		pthread_cond_wait(&item_available, &lock_rw_func);
	
	/* fecth first node from data queue*/
	cur = data_queue;
	data_queue = data_queue->next; 
	if (data_queue == NULL)
	{
	    last_node_in_data_queue = NULL;
	}
	memcpy(buffer, cur->data, cur->data_size);
	*size = cur->data_size;
	read_ready_nodes--;
	
	//readying this node for free queue
	cur->data_size = 0;
	cur->next = NULL;
	cur->in_progress = 0; // this data node has reached to final processor
	memset(cur->data,'\0',cur->size);
	
    /* return this node to free queue*/	
	if (free_queue == NULL) {
		free_queue = cur;		
	}
	else if (free_queue->size >= cur->size){
		cur->next = free_queue;
		free_queue = cur;
	}
	else {
		struct node *temp = free_queue, *prev;
		while((temp!= NULL) && (temp->size < cur->size))
		{
			prev = temp;
			temp=temp->next;
		}
		cur->next = prev->next; 
		prev->next = cur;
	}

	pthread_cond_signal(&space_available);	
	pthread_mutex_unlock(&lock_rw_func);
}

 /*
  * @brief This thread is responsible for pulling data off of the shared data 
  * area and processing it using the process_data() API.
  */
void* reader_thread(void *arg) {
	
	 printf("Enter %s\n", __func__);	
	//allocating local buffer for reader thread 
	char* buffer = (char*)malloc(sizeof(char) * (bufferSizeInBytes));
	if(NULL == buffer)
	{
		printf("malloc Failure, FATAL error \n");
		//logging/ clean up 
		pthread_exit(NULL);
	}

	int buffer_size = 0;

   
	while(1) {
		memset(buffer, 0, bufferSizeInBytes);		
		fetch_data_node_from_queue(buffer, &buffer_size);
		process_data(buffer, buffer_size);
	}
	free(buffer);
	return NULL;
}


//added for code compilation and testing purpose */
int get_external_data(char *data, int size)
{
    char temp[] = "hey, i got something";
    memcpy(data, temp, strlen(temp)+1);
    return strlen(data)+1;
}

/*
 * @brief This thread is responsible for pulling data from a device using
 * the get_external_data() API and placing it into a shared area
 * for later processing by one of the reader threads.
 */
void* writer_thread(void *arg) {
	
	int received_data;
	char* buffer = NULL;

	 printf("Enter %s\n", __func__);	
	//allocating local buffer for writer thread 
	buffer = (char*)malloc(sizeof(char)*bufferSizeInBytes);
	if(NULL == buffer)
	{
		printf("malloc Failure, FATAL error \n");
		//logging/ clean up 
		pthread_exit(NULL);
	}
	
	while(1) {
		memset(buffer, 0, bufferSizeInBytes);
		received_data = get_external_data(buffer, bufferSizeInBytes);		
		if (received_data < 0){
			printf("failure from get_external_data, continuing for next \n");
			continue;
		}
		if (received_data == 0){
			printf("0 bytes from get_external_data, continuing for next \n");
			continue;
		}
		
		push_data_into_queue(buffer, received_data);
	}
	free(buffer);
	return NULL;
}
/* @brief allocate node for memory pool 
 */
struct node* allocate_node()
{
	struct node *temp;
	temp = (struct node*)malloc(sizeof(struct node));
	if(NULL == temp)
	{
		printf("malloc Failure, FATAL error \n");
		//logging/ clean up 
		exit(EXIT_FAILURE);
	}
	temp->next = NULL;
	max_node_free_queue++;
	return temp;
}

/* @brief Allocates memory for queues 
 * @param [in] size of the block 
 * @params [in] num_queue - total number of blocks for this size
 * @returns
 */
void allocate_memory_for_queue(int size, int num_queue)
{
	int i;
	for(i =0; i<num_queue; i++)
	{
		struct node* temp = NULL;
		temp = allocate_node();
		temp->size = size;
		temp->data = (char*)malloc(sizeof(char)*size);
		if(NULL == temp->data)
		{
			printf("malloc Failure, FATAL error \n");
			//logging/ clean up 
			exit(EXIT_FAILURE);
		}
		if(NULL == free_queue)
		{
			/*for first node */
			last_node_in_free_queue = free_queue = temp;
		}
		else 
		{
			last_node_in_free_queue->next = temp;
			last_node_in_free_queue = temp;
	    }
	}
}

/* @brief Allocates memory queues of 8, 32, 128, 512, 2048, 8132 Bytes each in 10 nos.
 * @param [in] void 
 * @returns 0 for success and 1 for failure 
 */
int init_memory_queue(void)
{
	int i, j =2;
	for(i = 0; i<NUM_OF_BUCKETS_TYPES; i++)
	{
		j = j * 4;
		allocate_memory_for_queue(j, NUM_BLOCKS_FOR_EACH_SIZE);
	}
	return 0; //success always
}

void free_memory_queue(struct node* temp)
{
	struct node* cur;
	while(temp!=NULL)
	{
		cur = temp;
		temp = temp->next;
		free(cur->data);
		free(cur);
	}
}

#define M 10
#define N 20
int main(int argc, char **argv) 
{
	int i, err;
	pthread_t read_tid, write_tid;
	
	/*mutex initialization for reader writer sync */
	if((err = pthread_mutex_init(&lock_rw_func, NULL)))
	{
		printf("pthread_mutex_init failed\n");
		//logging/ clean up 
		exit(EXIT_FAILURE);
	}
	
	/*conditional variable initialization to indicate free node available*/
	if((err = pthread_cond_init(&space_available, NULL)))
	{
		printf("space_available conditional varibale init failed\n"); 
		//logging/ clean up 
		exit(EXIT_FAILURE);
	}
	
	/*conditional variable initialization to indicate data available to read*/
	if((err = pthread_cond_init(&item_available, NULL)))
	{
		printf("item_available conditional varibale init failed\n");
		//logging/ clean up 
		exit(EXIT_FAILURE);
	}
	
	/*initalize the memory queue*/
	init_memory_queue();
	
	printf("total nodes in queue = %d\n", max_node_free_queue );
	
	//create reader threads
	for(i = 0; i < N; i++) { 
	    printf("Enter %s line = %d\n", __func__, __LINE__);	
		if((err = pthread_create(&read_tid, NULL, reader_thread, NULL)))
		{
			printf("failed in reader pthread_create\n");
			free_memory_queue(free_queue);
			exit(EXIT_FAILURE);
		}
	}
    
	//create writer threads
	for(i = 0; i < M; i++) { 
	    printf("Enter %s line = %d\n", __func__, __LINE__);	
		if((err= pthread_create(&write_tid, NULL, writer_thread, NULL)))
		{
			printf("Failed in writer pthread creation\n");
			free_memory_queue(free_queue);
			exit(EXIT_FAILURE);
		}

	}
	//lets wait until all threads finish execution
	for(i = 0; i < N; i++) { 
		if((err = pthread_join(read_tid, NULL)))
		{
			printf("failed in pthread join of reader thread\n");
			free_memory_queue(free_queue);
			//clean up / logging 
			exit(EXIT_FAILURE);
		}
	}
	for(i = 0; i < M; i++) { 
		if((err = pthread_join(write_tid, NULL)))
		{
			printf("Failed in pthread join of writer thread\n");
			free_memory_queue(free_queue);
			//clean up / logging 
			exit(EXIT_FAILURE);
		}
    }
	
	pthread_mutex_destroy(&lock_rw_func);
	pthread_cond_destroy(&space_available);
	pthread_cond_destroy(&item_available);
	// free memory queue & data queue if any 
	free_memory_queue(free_queue);
	free_memory_queue(data_queue);
	return 0;	
}