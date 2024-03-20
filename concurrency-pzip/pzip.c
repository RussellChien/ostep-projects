#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define QUEUE_SIZE 10
int queueSize, queueHead, queueTail = 0;

int totalTh;
int numFiles;
int isComplete = 0;

int totalPages;
int pageSize; // keep in mind pageSize = 4096 Bytes
int *pagesPerFile;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fileLock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t fill = PTHREAD_COND_INITIALIZER;

// used for the data for each file
struct Buffer
{
  int fileNum;
  int pageNum;
  int lastPageSize;
  char *address; // FN + PN
} buffer[QUEUE_SIZE];

struct Output
{
  char *data;
  int *count;
  int size;
} *out;

void enqueue(struct Buffer buff) // insert at queueHead index of queue
{
  buffer[queueHead] = buff;
  queueHead = (queueHead + 1) % QUEUE_SIZE;
  queueSize++;
}

struct Buffer dequeue() // remove at queueTail index of queue
{
  struct Buffer buff = buffer[queueTail];
  queueTail = (queueTail + 1) % QUEUE_SIZE;
  queueSize--;
  return buff;
}

// helper to map pages and enqueue them
void helper(char *mapAddress, int pagesInFile, int lastPageSize, int i)
{
  for (int j = 0; j < pagesInFile; j++)
  {
    pthread_mutex_lock(&lock);
    while (queueSize == QUEUE_SIZE)
    {
      pthread_cond_broadcast(&fill);
      pthread_cond_wait(&empty, &lock);
    }
    pthread_mutex_unlock(&lock);

    struct Buffer tp;
    if (j == pagesInFile - 1)
    {
      tp.lastPageSize = lastPageSize;
    }
    else
    {
      tp.lastPageSize = pageSize;
    }

    tp.pageNum = j;
    tp.fileNum = i;
    tp.address = mapAddress;

    mapAddress += pageSize;

    pthread_mutex_lock(&lock);
    enqueue(tp);
    pthread_mutex_unlock(&lock);
    pthread_cond_signal(&fill);
  }
}

// producer function
void *producer(void *arg)
{
  char **filenames = (char **)arg;
  struct stat pd;
  char *mapAddress;
  int file;

  for (int i = 0; i < numFiles; i++)
  {
    file = open(filenames[i], O_RDONLY);
    int pagesInFile = 0;
    int lastPageSize = 0;

    // file validation
    if (fstat(file, &pd) == -1)
    {
      close(file);
      exit(1);
    }
    if (pd.st_size == 0)
    {
      continue;
    }

    // page alignment
    pagesInFile = (pd.st_size / pageSize);
    if (((double)pd.st_size / pageSize) > pagesInFile)
    {
      pagesInFile += 1;
      lastPageSize = pd.st_size % pageSize;
    }
    else
    {
      lastPageSize = pageSize;
    }
    totalPages += pagesInFile;
    pagesPerFile[i] = pagesInFile;

    // use mmap to map the entire file
    mapAddress = mmap(NULL, pd.st_size, PROT_READ, MAP_SHARED, file, 0);

    if (mapAddress == MAP_FAILED)
    {
      close(file);
      exit(1);
    }

    // helper to map pages and enqueue them
    helper(mapAddress, pagesInFile, lastPageSize, i);

    close(file);
  }

  isComplete = 1;
  pthread_cond_broadcast(&fill);
  return 0;
}

struct Output compress(struct Buffer curr)
{
  struct Output compressedOutput;
  compressedOutput.count = malloc(curr.lastPageSize * sizeof(int));
  char *tempString = malloc(curr.lastPageSize);
  int countIndex = 0;

  for (int i = 0; i < curr.lastPageSize; i++)
  {
    tempString[countIndex] = curr.address[i];
    compressedOutput.count[countIndex] = 1;

    while (i + 1 < curr.lastPageSize && curr.address[i] == curr.address[i + 1])
    {
      compressedOutput.count[countIndex]++;
      i++;
    }
    countIndex++;
  }
  compressedOutput.size = countIndex;
  compressedOutput.data = realloc(tempString, countIndex);
  return compressedOutput;
}

// consumer function
void *consumer()
{
  do
  {
    pthread_mutex_lock(&lock);
    while (queueSize == 0 && isComplete == 0)
    {
      pthread_cond_signal(&empty);
      // start filling queue
      pthread_cond_wait(&fill, &lock);
    }
    // producer done mapping and queue is empty, exit
    if (isComplete == 1 && queueSize == 0)
    {
      pthread_mutex_unlock(&lock);
      return NULL;
    }

    // get next buffer from queue
    struct Buffer curr = dequeue();

    if (isComplete == 0)
    {
      pthread_cond_signal(&empty);
    }
    pthread_mutex_unlock(&lock);

    // find output position for buffer
    int pos = 0;
    for (int i = 0; i < curr.fileNum; i++)
    {
      pos += pagesPerFile[i];
    }
    pos += curr.pageNum;

    // compress buffer
    struct Output compressedOutput = compress(curr);
    out[pos] = compressedOutput;

  } while (!(isComplete == 1 && queueSize == 0));

  return NULL;
}

void writeCompressedOutput()
{
  char *output = malloc(totalPages * pageSize * (sizeof(int) + sizeof(char)));
  char *start = output;
  for (int i = 0; i < totalPages; i++)
  {
    if (i < totalPages - 1)
    {
      if (out[i].data[out[i].size - 1] == out[i + 1].data[0])
      {
        out[i + 1].count[0] += out[i].count[out[i].size - 1];
        out[i].size--;
      }
    }
    for (int j = 0; j < out[i].size; j++)
    {
      int num = out[i].count[j];
      char character = out[i].data[j];
      *((int *)output) = num;
      output += sizeof(int);
      *((char *)output) = character;
      output += sizeof(char);
    }
  }
  fwrite(start, output - start, 1, stdout);
}

int main(int argc, char *argv[])
{
  // validate arguments
  if (argc < 2)
  {
    printf("pzip: file1 [file2 ...]\n");
    exit(1);
  }

  pageSize = sysconf(_SC_PAGE_SIZE);
  numFiles = argc - 1;
  totalTh = get_nprocs(); // set to 4 for mac
  pagesPerFile = malloc(sizeof(int) * numFiles);

  out = malloc(sizeof(struct Output) * 512000 * 2);

  // producer
  pthread_t pid, cid[totalTh];
  pthread_create(&pid, NULL, producer, argv + 1);

  // consumer
  for (int i = 0; i < totalTh; i++)
  {
    pthread_create(&cid[i], NULL, consumer, NULL);
  }

  // wait for all consumers and producer to finish
  for (int i = 0; i < totalTh; i++)
  {
    pthread_join(cid[i], NULL);
  }
  pthread_join(pid, NULL);

  writeCompressedOutput();

  return 0;
}