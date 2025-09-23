/* @file f5cmisc_rocm.h
**
** miscellaneous definitions and function prototypes to f5c GPU framework
** @author: Hasindu Gamaarachchi (hasindu@unsw.edu.au)
** @@
******************************************************************************/

#ifndef F5CMISC_ROCM_H
#define F5CMISC_ROCM_H

#include <stdint.h>
#include <hip/hip_runtime.h>
#include "error.h"

/* if defined, perform ROCM_device_synchronise */
#define ROCM_DEBUG 1

/* if defined, performs CPU-GPU heteregeneous processing for fast performance*/
#define CPU_GPU_PROC 1

/* if defined, big dynamic arrays (arrays that sizes are determined at the runtime such as bands array)
   are dynamically allocated using hipMalloc instead of using the efficient custom allocator.
   note: only effective if CPU_GPU_PROC is defined */
//#define ROCM_DYNAMIC_MALLOC 1

/* if defined, the the post kernel in performed with a warp hack for fast performance*/
#define WARP_HACK 1


/* align-core-kernel options */
#define BLOCK_LEN_READS 1 //the block size along y axis (the number of reads) - never change this as you might end up with wrong answers
#define BLOCK_LEN_BANDWIDTH 128 //the block size along the x axis, should be >= ALN_BANDWIDTH
#define ALIGN_KERNEL_FLOAT 1 //(for 2d kernel only)

/* align-pre-kernel options */
#define BLOCK_LEN_NUMBAND 16    //the block size along the x axis (BANDWDITH)
#define BLOCK_LEN_READS2 16 // //the block size along y axis (the number of reads)

#define AVG_EVENTS_PER_KMER (core->opt.cuda_avg_events_per_kmer) // the average number of events per base/k-mer
//AVG_EVENTS_PER_KMER is used to pre-allocate arrays on GPU that depends on the number of events

//if average events per base of a read < AVG_EVENTS_PER_KMER_GPU_THRESH process on GPU else go for the CPU
#define AVG_EVENTS_PER_KMER_GPU_THRESH (core->opt.cuda_max_avg_events_per_kmer)

#define TEGRA_MEM_FACTOR 0.7f //in tegra we cannot grab all  (can be overriden by user options)
//the free memory as we have to reserve some space for RAM as well
//TEGRA_MEM_FACTOR is the factor of the free memory allocated for the gpu

#define MEM_FACTOR 0.9f //for non-tegra GPU. how much factor of the free memory to allocate (can be overriden by user options)

#define REVERSAL_ON_CPU 1 //reversal of the backtracked array is performed on the CPU instead of the GPU

/* check whether the last HIP ` or HIP kernel launch is erroneous and if yes an error message will be printed
and then the program will be aborted*/
#define HIP_CHK()                                                             \
    { gpu_assert(__FILE__, __LINE__); }


#define HIP_RET_CHK(error) { gpuRetAssert((error),__FILE__, __LINE__); }

__global__ void
//__launch_bounds__(MY_KERNEL_MAX_THREADS, MY_KERNEL_MIN_BLOCKS)
align_kernel_core_2d_shm(int32_t* read_len, ptr_t* read_ptr,
    event_t* event_table, int32_t* n_events1, ptr_t* event_ptr,
    scalings_t* scalings, int32_t n_bam_rec,model_t* model_kmer_caches,  uint32_t kmer_size,
    float *band,uint8_t *traces, EventKmerPair* band_lower_lefts) ;

__global__ void align_kernel_pre_2d(char* read,
    int32_t* read_len, ptr_t* read_ptr,
    int32_t* n_events, ptr_t* event_ptr, model_t* models,  uint32_t kmer_size,
    int32_t n_bam_rec,model_t* model_kmer_caches,float *bands1,uint8_t *trace1, EventKmerPair* band_lower_left1) ;


__global__ void align_kernel_post(AlignedPair* event_align_pairs,
    int32_t* n_event_align_pairs,
    int32_t* read_len, ptr_t* read_ptr,
    event_t* event_table, int32_t* n_events, ptr_t* event_ptr,
    scalings_t* scalings, int32_t n_bam_rec,model_t* model_kmer_caches, uint32_t kmer_size,
    float *bands1,uint8_t *trace1, EventKmerPair* band_lower_left1);

static inline void gpu_assert(const char* file, uint64_t line) {
    hipError_t code = hipGetLastError();
    if (code != hipSuccess) {
        fprintf(stderr, "[%s::ERROR]\033[1;31m Hip error: %s \n in file : %s line number : %lu\033[0m\n",
                __func__, hipGetErrorString(code), file, line);
        if (code == hipErrorLaunchTimeOut) { //TODOROCM is this a thing in ROCM?
            ERROR("%s", "The kernel timed out. You have to first disable the hip "
                        "time out.");
            fprintf(
                stderr,
                "On Ubuntu do the following\nOpen the file /etc/X11/xorg.conf\nYou "
                "will have a section about your NVIDIA device. Add the following "
                "line to it.\nOption \"Interactive\" \"0\"\nIf you do not have a "
                "section about your NVIDIA device in /etc/X11/xorg.conf or you do "
                "not have a file named /etc/X11/xorg.conf, run the command sudo "
                "nvidia-xconfig to generate a xorg.conf file and do as above.\n\n");
        }
        exit(-1);
    }
}

static inline void gpuRetAssert(hipError_t error, const char *file, int line) {
    if (error != hipSuccess) { \
    fprintf(stderr,"HIP error: %s, in file: %s, line number: %d\n", hipGetErrorString(error), file, line); \
    exit(1); \
    } \
}

static inline int32_t hip_exists() {
    //check hip devices
    int32_t nDevices=-1;
    hipError_t ret = hipGetDeviceCount(&nDevices);
    hipError_t code = hipGetLastError();
    if (code != hipSuccess || ret != hipSuccess) {
        fprintf(stderr, "[%s::ERROR]\033[1;31m Hip error: %s \n in file : %s line number : %d\033[0m\n",
                __func__, hipGetErrorString(code), __FILE__, __LINE__);
    }
    if (nDevices <= 0) { //TODOROCM change the error message
        fprintf(stderr, "[%s::ERROR]\033[1;31m Could not initialise a rocm/hip capable device. Some troubleshooting tips in order:\n"
                        "1. Do you have an AMD GPU? [lspci | grep -i \"vga\\|3d\\|display\"]\n"
                        "2. Have you installed the AMD driver? [lspci -nnk | grep -iA2 \"vga\\|3d\\|display\"]\n"
                        "3. If you GPU is iGPU is the current user belongs to the [video] user group?\n"
                        "4. Is your hip driver too old? (the release binary compiled using rocm 5.x)\n"
                        "Run with --disable-hip=yes to run on the CPU\033[0m\n",__func__);
        exit(1);
    }
    return nDevices;
}

static inline uint64_t hip_freemem(int32_t devicenum) {

    uint64_t freemem, total;
    hipError_t ret = hipMemGetInfo(&freemem, &total);
    HIP_CHK(); HIP_RET_CHK(ret);
    fprintf(stderr, "[%s] %.2f GB free of total %.2f GB GPU memory\n",__func__,
            freemem / double(1024 * 1024 * 1024),
            total / double(1024 * 1024 * 1024));

    return freemem;
}

static inline uint64_t igpu_freemem(int32_t devicenum) {

    uint64_t freemem, total;
    hipError_t ret = hipMemGetInfo(&freemem, &total);
    HIP_CHK(); HIP_RET_CHK(ret);

    // RAM
    FILE* f = fopen("/proc/meminfo", "r");
    int64_t totalRAMkB = -1, freeRAMkB = -1, memAvailablekB=-1, buffersRAMkB = -1, cachedRAMkB = -1;

    if(f)
    {
        // add if (blah) {} to get around compiler warning
        if (fscanf(f, "MemTotal: %ld kB\n", &totalRAMkB)) {}
        if (fscanf(f, "MemFree: %ld kB\n", &freeRAMkB)) {}
        if (fscanf(f, "MemAvailable: %ld kB\n", &memAvailablekB)) {}
        if (fscanf(f, "Buffers: %ld kB\n", &buffersRAMkB)) {}
        if (fscanf(f, "Cached: %ld kB\n", &cachedRAMkB)) {}
        fclose(f);
    }
    if(totalRAMkB>0 && freeRAMkB>0 && buffersRAMkB>0 && cachedRAMkB>0){
        freemem += (cachedRAMkB+buffersRAMkB)*1024;
    }
    else{
        WARNING("%s","Reading /proc/meminfo failed. Inferred free GPU memory might be wrong.");
    }

    fprintf(stderr, "[%s] %.2f GB free of total %.2f GB GPU memory\n",__func__,
            freemem / double(1024 * 1024 * 1024),
            total / double(1024 * 1024 * 1024));

    return freemem;
}

#endif
