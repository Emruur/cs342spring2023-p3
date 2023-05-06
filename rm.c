#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include "rm.h"

// global variables

bool DA;  // indicates if deadlocks will be avoided or not
int N;   // number of processes
int M;   // number of resource types
int ExistingRes[MAXR]; // Existing resources vector
pthread_t tid_table[MAXP];

//..... other definitions/variables .....

    // BANKERS ALGO, detection
int max[MAXP][MAXR];
int allocation[MAXP][MAXR];
int request_matrix[MAXP][MAXR];
int need[MAXP][MAXR];
int available[MAXR];

//LOCK, CV
pthread_mutex_t global_lock;
pthread_cond_t global_cv;

//.....
//.....

// end of global variables

// Vector Addition
void vector_add(int* a, int* b, int* result, int n) {
    for(int i = 0; i < n; i++) {
        result[i] = a[i] + b[i];
    }
}
// Vector Subtraction
void vector_sub(int* a, int* b, int* result, int n) {
    for(int i = 0; i < n; i++) {
        result[i] = a[i] - b[i];
    }
}
// Checks if a vector is smaller or equal to a vector, returns 1. 0 otherwise
int vector_compare(int* a, int* b, int n) {
    for (int i= 0  ; i<n; i++) {
        if(a[i]>b[i])
            return 0;
    }
    return 1;
}
// Vector Copy
void vector_copy(int* src, int* dest, int n) {
    for(int i = 0; i < n; i++) {
        dest[i] = src[i];
    }
}
// Vector Init
void vector_init(int value, int* dest, int n) {
    for(int i = 0; i < n; i++) {
        dest[i] = value;
    }
}
// Print Vector
void print_vector(int* a, int n) {
    printf("[");
    for(int i = 0; i < n; i++) {
        printf("%d", a[i]);
        if (i < n - 1) {
            printf(", ");
        }
    }
    printf("]\n");
}

//returns 1 if equal, 0 othervise
bool vector_equal(int* a, int* b, int n){
    for(int i= 0; i< n; i++)
        if(a[i] != b[i])
            return 0;
    return 1;
}

int find_thread_index(pthread_t thread_id){
    for(int i= 0; i< N; i++)
        if(tid_table[i]==thread_id)
            return i;
    return -1;
}

int rm_thread_started(int tid)
{   
    if(tid >= N)
        return 1;

    pthread_mutex_lock(&global_lock);
    tid_table[tid]= pthread_self();
    pthread_mutex_unlock(&global_lock);
    return 0;
}

int rm_thread_ended()
{
    pthread_mutex_lock(&global_lock);
    int t_index= find_thread_index(pthread_self());
    if(t_index<0){
        pthread_mutex_unlock(&global_lock);
        return 1;
    }

    tid_table[t_index]= NULL;
    //TODO if all threads are terminated, do cleanup on locks, cvs and etc.
    pthread_mutex_unlock(&global_lock);
    return 0;
}

int rm_claim (int* claim)
{
    pthread_mutex_lock(&global_lock);
    int t_index= find_thread_index(pthread_self());

    if(t_index<0 || !vector_compare(claim, ExistingRes, M)){
        printf("CLAIM ERROR\n");
        pthread_mutex_unlock(&global_lock);
        return 1;
    }

    vector_copy(claim, max[t_index], N);
    vector_copy(claim, need[t_index], N);
    pthread_mutex_unlock(&global_lock);
    return 0;
}

int rm_init(int p_count, int r_count, int r_exist[],  int avoid)
{
    int i;
    int ret = 0;
    
    DA = (bool) avoid;
    N = p_count;
    M = r_count;
    // initialize (create) resources
    for (i = 0; i < M; ++i){
        ExistingRes[i] = r_exist[i];
        available[i]= r_exist[i];
    }
    for(i= 0; i< N; i++)
        tid_table[i]= NULL;

    for(int i= 0; i< N; i++){
        for(int j= 0; j< M; j++){
            max[i][j]= 0;
            allocation[i][j]= 0;
            need[i][j]= 0;
            request_matrix[i][j]= 0;
        }
    }
    if(pthread_mutex_init(&global_lock, NULL) != 0) {
      printf("Failed to Initialize Mutex Lock...\n");
      return 1;
    }
    if(pthread_cond_init (&global_cv, NULL) != 0) {
      printf("Failed to Initialize CV...\n");
      return 1;
    }
    // resources initialized (created)
    
    //....
    // ...
    return  (ret);
}

//returns the state, safe=0 unsafe=1
bool bankers_algo(int request[], int t_index){
    int work[MAXR];
    bool finished[MAXP];

    int allocation_temp[MAXP][MAXR];
    int need_temp[MAXP][MAXR];
    int available_temp[MAXR];

    //copy values
    for(int i= 0; i<N; i++){
        vector_copy(allocation[i], allocation_temp[i], M);
        vector_copy(need[i], need_temp[i], M);
    }
    vector_copy(available, available_temp, M);


    vector_sub(available_temp, request, available_temp, M);
    vector_add(allocation_temp[t_index], request, allocation_temp[t_index], M);
    vector_sub(need_temp[t_index], request, need_temp[t_index], N);

    //1
    vector_copy(available_temp, work, M);
    vector_init(0, (int*)finished, N);

    while(true){
        //2
        int i_found= -1;
        for (int i= 0; i<N; i++) {
            if(!finished[i] && vector_compare(need_temp[i], work, M))
                i_found= i;
        }
        if(i_found<0)
            break;
        //3
        vector_add(work, allocation_temp[i_found], work, M);
        finished[i_found]= true;
    }
    //4
    bool safe= 0;
    for(int i= 0; i< N; i++)
        safe |= !finished[i];

    return safe;
}


int rm_request (int request[])
{
    pthread_mutex_lock(&global_lock);
    int t_index= find_thread_index(pthread_self());
    //If thread is registered and request does not exceed the existing resources(not available ones)
    if(t_index<0 || !vector_compare(request, ExistingRes, M)){
        pthread_mutex_unlock(&global_lock);
        return 1;
    }

    //Add to the request matrix
    vector_add(request_matrix[t_index], request, request_matrix[t_index], M);

    if(DA){
        //If requested resources are within the limit of claimed ones
        if(!vector_compare(request, max[t_index], M)){
            vector_sub(request_matrix[t_index], request, request_matrix[t_index], M);
            pthread_mutex_unlock(&global_lock);
            return 1;
        }
        while (bankers_algo(request, t_index)) {
            pthread_cond_wait(&global_cv, &global_lock);
        }
    }

    //Allocate the resource if there are available
    while(!vector_compare(request, available, M)){
        pthread_cond_wait(&global_cv, &global_lock);
    }

    // allocate it
    vector_sub(available, request, available, M);
    vector_add(allocation[t_index], request, allocation[t_index], M);
    vector_sub(need[t_index], request, need[t_index], N);
    //pop from request matrix
    vector_sub(request_matrix[t_index], request, request_matrix[t_index], M);

    
    pthread_mutex_unlock(&global_lock);
    return 0;
}


int rm_release (int release[])
{
    pthread_mutex_lock(&global_lock);
    int t_index= find_thread_index(pthread_self());

    if(t_index<0 || !vector_compare(release, allocation[t_index], M)){
        pthread_mutex_unlock(&global_lock);
        return 1;
    }

    // de-allocate it
    vector_add(available, release, available, M);
    vector_sub(allocation[t_index], release, allocation[t_index], M);
    vector_add(need[t_index], release, need[t_index], N);

        //SIGNAL THE CV
    pthread_cond_broadcast(&global_cv);

    pthread_mutex_unlock(&global_lock);
    return 0;
}

int rm_detection()
{
    pthread_mutex_lock(&global_lock);
    int ret = 0;

    int work[MAXR];
    bool finished[MAXP];
    int empty[MAXR]= {0};

    // 1
    vector_copy(available, work, M);
    for(int i= 0; i<N; i++)
        finished[i]= vector_equal(allocation[i], empty , N);
    
    while(true){
        //2
        int i_found= -1;
        for(int i= 0; i< N; i++)
            if(!finished[i] && vector_compare(request_matrix[i], work, M)){
                i_found= i;
                break;
            }
        if(i_found<0)
            break;
        //3
        vector_add(work, allocation[i_found], work, M);
        finished[i_found]= true;
    }
    //4
    for(int i= 0; i<N; i++)
        if(!finished[i]){
            ret= 1;
            break;
        }

    pthread_mutex_unlock(&global_lock);
    return (ret);
}


//Altough read only, used lock for atomic printing
void rm_print_state(char headermsg[]) {
    pthread_mutex_lock(&global_lock);
    int i, j;

    printf("########################## %s ###########################\n", headermsg);
    printf("Registered Threads: \n");
    for(i = 0; i< N; i++)
        printf("Index %d: %ld\n",i,(long)tid_table[i]);

    //Existing resources
    printf("Exist:\n");
    printf("\t");
    for(int i= 0; i<M; i++)
        printf("R%d\t",i);
    printf("\n");
    printf("\t");
    for(int i= 0; i<M; i++){
        printf("%d\t",ExistingRes[i]);
    }
    printf("\n");

    //Available
    printf("\nAvailable:\n");
    printf("\t");
    for(int i= 0; i<M; i++)
        printf("R%d\t",i);
    printf("\n");
    printf("\t");
    for(int i= 0; i<M; i++){
        printf("%d\t",available[i]);
    }
    printf("\n");

    printf("\nAllocation:\n");
    printf("\t");
    for(int i= 0; i<M; i++)
        printf("R%d\t",i);
    printf("\n");
    for (i = 0; i < N; i++) {
        printf("T%d:\t", i);
        for (j = 0; j < M; j++)
            printf("%d\t", allocation[i][j]);
        printf("\n");
    }


    printf("\nRequest:\n");
    printf("\t");
    for(int i= 0; i<M; i++)
        printf("R%d\t",i);
    printf("\n");
    for (i = 0; i < N; i++) {
        printf("T%d:\t", i);
        for (j = 0; j < M; j++)
            printf("%d\t", request_matrix[i][j]);
        printf("\n");
    }

    printf("\nMax:\n");
    printf("\t");
    for(int i= 0; i<M; i++)
        printf("R%d\t",i);
    printf("\n");
    for (i = 0; i < N; i++) {
        printf("T%d:\t", i);
        for (j = 0; j < M; j++)
            printf("%d\t", max[i][j]);
        printf("\n");
    }

    printf("\nNeed:\n");
    printf("\t");
    for(int i= 0; i<M; i++)
        printf("R%d\t",i);
    printf("\n");
    for (i = 0; i < N; i++) {
        printf("T%d:\t", i);
        for (j = 0; j < M; j++)
            printf("%d\t", need[i][j]);
        printf("\n");
    }

    
    printf("###########################\n");
    pthread_mutex_unlock(&global_lock);
}


