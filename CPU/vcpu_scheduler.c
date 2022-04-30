#include <stdio.h>
#include <libvirt/libvirt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../shared/domain_common.c"//

static const long NANOSECOND = 1000000000; // The following values are used to determine when a vCPU
// should be pinned to a different pCPU. If the usage// standard deviation for any of the pCPUs is larger than this
// (between periods), it will be reassigned to another vCPU.



struct DomainVcpusInfo{ 
    virVcpuInfoPtr info;
    int count;
    double *usage;
};//right now these are empty




void fillDomainVcpusInfo(struct DomainArray domain_array,struct DomainVcpusInfo *current_info){
    int i, maxinfo = 1;//assume the map won't be super large
    virVcpuInfoPtr info_entry;
    for(i=0;i<domain_array.size;i++){
        info_entry = calloc(maxinfo, sizeof(virVcpuInfo));
        if(virDomainGetVcpus(*(domain_array.domains+i),info_entry,maxinfo,NULL,0)==-1){
            fprintf(stderr,"failed to get domain Vcpus info");
            exit(-1);
        }
        //debug to make sure this works as we wanted!
        current_info[i].info = info_entry;
        current_info[i].count = maxinfo;
        current_info[i].usage = calloc(maxinfo, sizeof(double));// the calloc() in C is used to allocate multiple blocks of memory space while  Malloc() function is used to 
allocate a single block of memory space //
    }
}

int getpCPUNumber(virConnectPtr conn){
    virNodeInfoPtr info = NULL;//used for processing of cpu
    info = malloc(sizeof(virNodeInfo)); //size of a node determines the number of CPU cores,
    if(virNodeGetInfo(conn,info)!=0){
        fprintf(stderr,"can't get node information through virNodeGetInfo");
        exit(-1);
    }
    free(info);
    return info->cpus;
}



void initialPinVcpu(virDomainPtr domain,struct DomainVcpusInfo Vcpus_info_array, int pcpu_number){
    int i, physical_cpu, number, map_len;
    map_len = VIR_CPU_MAPLEN(pcpu_number);//returns the length (in bytes) required to store the
 complete CPU map between a single virtual & all physical CPUs of a domain.//
    unsigned char cpumaps;
    for(i=0;i<Vcpus_info_array.count;i++){
        cpumaps = 0x01;
        virVcpuInfoPtr vcpu_info = Vcpus_info_array.info+i;
        physical_cpu = vcpu_info->cpu;
        number = vcpu_info->number;
        cpumaps <<= physical_cpu;
        //set CPU pinning to initial physical cpu
        if(virDomainPinVcpu(domain,number,&cpumaps,map_len)!=0){
            fprintf(stderr,"failed to initial pin physical cpu");
            exit(-1);
        }
        printf("VM %s, virtual CPU %d is pinned to physical CPU %d\n",virDomainGetName(domain),number,physical_cpu);
    }

}

void computePcpuUsage(struct DomainVcpusInfo *previous_info_ptr,
                      struct DomainVcpusInfo *current_info_ptr,
                      int domian_count, int pcpu_number,double *pcpu_usage, int duration){
    int i,j;
    long long int temp, *pcpu_time_used = calloc(pcpu_number, sizeof(long long int));
    for(i=0;i<domian_count;i++){
        for(j=0;j<current_info_ptr[i].count;j++){

            int physical_cpu = current_info_ptr[i].info[j].cpu;
            pcpu_time_used[physical_cpu] += current_info_ptr[i].info[j].cpuTime - previous_info_ptr[i].info[j].cpuTime;
            temp = current_info_ptr[i].info[j].cpuTime - previous_info_ptr[i].info[j].cpuTime;
            current_info_ptr[i].usage[j] = (double)temp / (double)(duration * NANOSECOND)* 100;
        }
    }
    printf("pcpu usage:\n");
    for(i=0;i<pcpu_number;i++){
        pcpu_usage[i] = (double)pcpu_time_used[i] / (double)(duration * NANOSECOND) * 100.0;
        printf("cpu %d, usage %4f %%  ",i,pcpu_usage[i]);
    }
    printf("\n");
    free(pcpu_time_used);

}

int determinePinChanging(double *usage,int count, int *heaviest,int *freest){
    int h=0, f=0;
    for(int i=1;i<count;i++){
        if(usage[i]>usage[h]){
            h = i;
        }else if(usage[i]<usage[f]){
            f = i;
        }//else do nothing, go to the next loop;
    }
    printf("highest used cpu %d, %3f %%, lowest used cpu %d, %3f %%\n",h,usage[h],f,usage[f]);
    if(usage[h]-usage[f]>15.0){ //no sure if 15.0 is a good enough value, check later.
        *heaviest = h;
        *freest = f;
        return 1;
    }else{
        printf("decide not to change pinning at this time\n");
        return 0;
    }

}

void pinChanging(struct DomainArray domains,int heaviest,int freeest,
                 struct DomainVcpusInfo *current_info_ptr){
    int i,j,map_len;
    int lowest_vpuc_domain_index=0, lowest_vpuc_index=0;
    double lowest_load=100;
    for(i=0;i<domains.size;i++){
        for(j=0;j<current_info_ptr[i].count;j++){
            if(current_info_ptr[i].info[j].cpu==heaviest
                    &&current_info_ptr[i].usage[j]<lowest_load){
                lowest_load = current_info_ptr[i].usage[j];
                lowest_vpuc_domain_index = i;
                lowest_vpuc_index = j;
            }
        }
    }
    printf("lightest load vcpu on Pcpu %d is from domain %s, vcpu %d\n",
           heaviest,virDomainGetName(*(domains.domains+lowest_vpuc_domain_index)),lowest_vpuc_index);
    map_len = 1;
    char cpumap = 0x01;
    printf("pin this vcpu with lowest usage pcpu %d\n",freeest);
    cpumap <<= freeest;
    virDomainPinVcpu(*(domains.domains+lowest_vpuc_domain_index),
                     current_info_ptr[lowest_vpuc_domain_index].info[lowest_vpuc_index].number,
                     &cpumap,map_len);


}

int main(int argc, char *argv[]) {
    int pcpu_number, i, interval = atoi(argv[1]);
    virConnectPtr conn = NULL;
    double *pcpu_usage;
    struct DomainVcpusInfo *previous_info_ptr = NULL;
    struct DomainVcpusInfo *current_info_ptr = NULL;
    //del later/ move into readme:
    // we can sum pcpu time (information from i) up to get pcpu time for each pCPU.
    // a privous_pcpu and current_pcpu time is also required.
    // guess this strategy apply to


    //commands goes after this line.
    printf("Connecting to hypervisor...\n");
    conn = connectToKVM();
    printf("getting active Domains..\n");
    struct DomainArray domain_array = getActiveDomains(conn);
    pcpu_number = getpCPUNumber(conn);
    printf("physical CPU number is %d\n",pcpu_number);
    pcpu_usage = calloc(pcpu_number,sizeof(double));
    /*del later/ move into readme:
    //if my idea is right, the only query that we need is virDomainGetVcpus;
    //a initial step is pin vCPU with their current Pcpu (initially they may have loose affinity)
    then we can use the difference as cpu usage and assume it's pinned to the current Pcpu*/
    //get info from each domain, notice that they are sorted according to domainArray sequence;
    current_info_ptr = malloc(domain_array.size * sizeof(struct DomainVcpusInfo));
    memset(current_info_ptr,0,domain_array.size * sizeof(struct DomainVcpusInfo));
    //get currentVcpusInfo, per domain

    //now we have vcpu info about each domain;
    while(1){
        int change_pin_or_not = 0;
        fillDomainVcpusInfo(domain_array,current_info_ptr);
        if(previous_info_ptr == NULL){
            //this is the first operation at startup, we pin each vcpu with the current pCPU they are on;
            //in case they affinity is casual
            for(i=0;i<domain_array.size;i++){
                initialPinVcpu(*(domain_array.domains+i),*(current_info_ptr+i),pcpu_number);
            }

        }
        else{
            //we already have a previous time, now we calculate useage and see if we need to change pin;
            computePcpuUsage(previous_info_ptr,
                             current_info_ptr,
                             domain_array.size,pcpu_number,pcpu_usage,interval);
            int *heaviest, *freest;
            heaviest = malloc(sizeof(int));
            freest = malloc(sizeof(int));
            change_pin_or_not = determinePinChanging(pcpu_usage,pcpu_number,heaviest,freest);
            if(change_pin_or_not){
                pinChanging(domain_array,*heaviest,*freest,current_info_ptr);
            }
            free(heaviest);
            free(freest);
            for(i=0;i<domain_array.size;i++){
                free(previous_info_ptr[i].usage);
            }
            for(i=0;i<domain_array.size;i++){
                free(previous_info_ptr[i].info);
            }
            free(previous_info_ptr);
        }
        previous_info_ptr = current_info_ptr;
        current_info_ptr = malloc(domain_array.size * sizeof(struct DomainVcpusInfo));
        memset(current_info_ptr,0,domain_array.size * sizeof(struct DomainVcpusInfo));
        printf("sleep for a little bit\n");
        sleep(interval);
    }




    //to free: info and 'usage' inside info;





    return 0;
}