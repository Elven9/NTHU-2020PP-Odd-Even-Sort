
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <boost/sort/spreadsort/spreadsort.hpp>
#include <mpi.h>

#define EVEN_PHASE_EXCHANGE 1
#define ODD_PHASE_EXCHANGE 2

// Global Variable for Speed up
int n, partition, fIndex;
float *tmp;

int getPartitionCount(int rank);
void customMerge(float *data, float *mergeWith, int dataCount, int mergeCount, bool isSmallest, bool *isSorted);
void customMerge2(float *data, float *mergeWith, int dataCount, int mergeCount, bool isSmallest, bool *isSorted);

int main(int argc, char **argv)
{

    // Declaration
    int rank, size;

    // Initialization
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    MPI_File fIn, fOut;
    n = atoi(argv[1]);
    // 連這邊都會有問題，double 沒用的話，float 精度會在 testcases 32 出錯
    partition = std::ceil(n / (double)size);
    fIndex = partition * rank;

    // 因為用了 Ceil 所以要特別處理一下 Count(P>n)
    int count = getPartitionCount(rank);
    bool lastNode = fIndex <= n && fIndex + partition >= n;

    // List of Data Structure
    float *data = new float[count];
    float *buffer = new float[partition];
    bool isSorted = true, bufSorted = true;

    // Read Input From
    MPI_File_open(MPI_COMM_WORLD, argv[2], MPI_MODE_RDONLY, MPI_INFO_NULL, &fIn); // 輸入
    if (count != 0)
        MPI_File_read_at(fIn, sizeof(float) * fIndex, data, count, MPI_FLOAT, MPI_STATUS_IGNORE);
    MPI_File_close(&fIn);

    // Preprocess the Data
    // if (count != 0) std::sort(data, data+count);
    if (count != 0)
        boost::sort::spreadsort::spreadsort(data, data + count);

    // Calculate Neighbor Count
    int leftCount = rank - 1 >= 0 ? getPartitionCount(rank - 1) : 0;
    int rightCount = rank + 1 < size ? getPartitionCount(rank + 1) : 0;

    // Init Merge Tmp Buffer
    if (count != 0)
        tmp = new float[count];

    int loopCount = 0;

    while (true)
    {
        // Even Phase
        if (count != 0)
        {
            if (rank % 2 == 0)
            {
                if (!lastNode)
                {
                    // 確認是否需要交換大筆資料
                    MPI_Sendrecv(&data[count - 1], 1, MPI_FLOAT, rank + 1, EVEN_PHASE_EXCHANGE, buffer, 1, MPI_FLOAT, rank + 1, EVEN_PHASE_EXCHANGE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    if (data[count - 1] > buffer[0])
                    {
                        MPI_Sendrecv(data, count, MPI_FLOAT, rank + 1, EVEN_PHASE_EXCHANGE, buffer, rightCount, MPI_FLOAT, rank + 1, EVEN_PHASE_EXCHANGE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        customMerge2(data, buffer, count, rightCount, true, &isSorted);
                    }
                }
            }
            else
            {
                MPI_Sendrecv(data, 1, MPI_FLOAT, rank - 1, EVEN_PHASE_EXCHANGE, buffer, 1, MPI_FLOAT, rank - 1, EVEN_PHASE_EXCHANGE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                if (data[0] < buffer[0])
                {
                    MPI_Sendrecv(data, count, MPI_FLOAT, rank - 1, EVEN_PHASE_EXCHANGE, buffer, leftCount, MPI_FLOAT, rank - 1, EVEN_PHASE_EXCHANGE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    customMerge2(data, buffer, count, leftCount, false, &isSorted);
                }
            }

            // Odd
            if (rank % 2 == 1)
            {
                if (!lastNode)
                {
                    MPI_Sendrecv(&data[count - 1], 1, MPI_FLOAT, rank + 1, ODD_PHASE_EXCHANGE, buffer, 1, MPI_FLOAT, rank + 1, ODD_PHASE_EXCHANGE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                    if (data[count - 1] > buffer[0])
                    {
                        MPI_Sendrecv(data, count, MPI_FLOAT, rank + 1, ODD_PHASE_EXCHANGE, buffer, rightCount, MPI_FLOAT, rank + 1, ODD_PHASE_EXCHANGE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        customMerge2(data, buffer, count, rightCount, true, &isSorted);
                    }
                }
            }
            else
            {
                if (rank != 0)
                {
                    MPI_Sendrecv(data, 1, MPI_FLOAT, rank - 1, ODD_PHASE_EXCHANGE, buffer, 1, MPI_FLOAT, rank - 1, ODD_PHASE_EXCHANGE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                    if (data[0] < buffer[0])
                    {
                        MPI_Sendrecv(data, count, MPI_FLOAT, rank - 1, ODD_PHASE_EXCHANGE, buffer, leftCount, MPI_FLOAT, rank - 1, ODD_PHASE_EXCHANGE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        customMerge2(data, buffer, count, leftCount, false, &isSorted);
                    }
                }
            }
        }
        loopCount += 2;

        if (loopCount > size)
            break;
    }

    MPI_File_open(MPI_COMM_WORLD, argv[3], MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fOut);
    MPI_File_write_at(fOut, sizeof(float) * fIndex, data, count, MPI_FLOAT, MPI_STATUS_IGNORE);

    MPI_File_close(&fOut);

    MPI_Finalize();

    delete[] data;
    delete[] buffer;

    if (count != 0)
        delete[] tmp;

    return 0;
}

int getPartitionCount(int rank)
{
    if (partition * rank >= n)
        return 0;
    else
    {
        if (partition * (rank + 1) >= n)
        {
            // 最後一個有再做是的 Node
            return n - partition * rank;
        }
        else
        {
            return partition;
        }
    }
}

void customMerge(float *data, float *mergeWith, int dataCount, int mergeCount, bool isSmallest, bool *isSorted)
{
    std::vector<float> tmp(mergeCount + dataCount);
    std::merge(data, data + dataCount, mergeWith, mergeWith + mergeCount, tmp.begin());

    // Copy Required Data
    if (isSmallest)
    {
        std::vector<float>::iterator it = tmp.begin();
        for (int i = 0; i < dataCount; i++)
        {
            if (!(*isSorted) && data[i] != *it)
                *isSorted = true;
            data[i] = *it;
            it++;
        }
    }
    else
    {
        std::vector<float>::iterator it = tmp.end() - 1;
        for (int i = dataCount - 1; i >= 0; i--)
        {
            if (!(*isSorted) && data[i] != *it)
                *isSorted = true;
            data[i] = *it;
            it--;
        }
    }
}

void customMerge2(float *data, float *mergeWith, int dataCount, int mergeCount, bool isSmallest, bool *isSorted)
{
    // Copy to Tmp
    int i, j, k;
    if (isSmallest)
    {
        if (data[dataCount - 1] <= mergeWith[0])
            return;
        // 小 -> 大
        i = j = k = 0;
        while (true)
        {
            if (i < dataCount && j < mergeCount)
            {
                tmp[k++] = data[i] < mergeWith[j] ? data[i++] : mergeWith[j++];
            }
            else
            {
                if (i < dataCount)
                    tmp[k++] = data[i++];
                else
                    tmp[k++] = mergeWith[j++];
            }
            if (k >= dataCount)
                break;
        }
    }
    else
    {
        if (data[0] >= mergeWith[mergeCount - 1])
            return;
        // 大 -> 小
        i = dataCount - 1;
        j = mergeCount - 1;
        k = dataCount - 1;
        while (true)
        {
            if (i >= 0 && j >= 0)
            {
                tmp[k--] = data[i] > mergeWith[j] ? data[i--] : mergeWith[j--];
            }
            else
            {
                if (i >= 0)
                    tmp[k--] = data[i--];
                else
                    tmp[k--] = mergeWith[j--];
            }
            if (k < 0)
                break;
        }
    }

    // Copy Tmp to data
    for (k = 0; k < dataCount; k++)
    {
        data[k] = tmp[k];
    }
}