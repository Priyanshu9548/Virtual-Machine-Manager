#include "domain_common.h"



struct DomainArray getActiveDomains(virConnectPtr conn){
    virDomainPtr *domains = NULL;//will be malloc by function virConnectListAllDomains
    int sz;
    struct DomainArray domain_array;
    sz = virConnectListAllDomains(conn,&domains,
                                  VIR_CONNECT_LIST_DOMAINS_ACTIVE|VIR_CONNECT_LIST_DOMAINS_RUNNING);
    if(sz <= 0){
        fprintf(stderr,"error when finding active domains");
        exit(-1);
    }
    domain_array.domains = domains;
    domain_array.size = sz;
    return domain_array;
}

virConnectPtr connectToKVM(){
    virConnectPtr conn= NULL;
    conn = virConnectOpen("qemu:///system");
    if(conn == NULL){
        fprintf(stderr,"failed to connect to hypervisor");
        exit(-1);
    }

    return conn;
}