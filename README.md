# Virtual-Machine-Manager
A virtual machine manager : project of Operating system 
Instructions to compile
gcc -g -Wall -o vcpu_scheduler vcpu_scheduler.c -lvirt

OR

Run the command make.

Code Description
Problem statement
Given a set of virtual machines having different workloads, load balance them across the available physical cpus.

Assumptions
There will be only 1 vcpu for a virtual machine.
No VMs are shutdown/started while the scheduler is running.
No pcpus are enabled/disabled while the scheduler is running.
Code description
Structures used
domainStats - contains following information

domain - pointer to the domain, type - virDomainPtr.
time - cumulative time used by the vcpu, in nanoseconds.
usage - usage in % for the vcpu, for the current run of the scheduler.
cpuNum - the pcpu on which the domain is running.
pcpuStats - contains the followng information

usage - usage in % of the pcpu, for the current run of the scheduler.
Algorithm
Connect to hypervisor using virConnectOpen().
Get the list of all active and running domains using virConnectListAllDomains().
Initialize prevDomainStats as an empty structure. This structure will be used to store the statistics information from previous run.
Create an array of domainStatsPtr called currDomainStats to hold the statistics information of each domain.
Invoke getCurrVcpuTime() to get the current vcpu statistics.
getCurrVcpuTime makes use of virDomainGetCPUStats() to get the statistics.
The vcpu time is extracted from cpu_time parameter.
Invoke virNodeGetInfo() to get the number of pcpus in the system.
Create an array of pcpuStatsPtr called currPcpuStats to hold the statistics about each pcpu.
If prevDomainStats i.e. information about domain statistics calculated in previous run is not empty
Invoke calculateDomainUsage() to calculate the vcpu usage for each domain.
Usage is calculated as (vcpu time in current run - vcpu time in previous run)/ (t * 10^9) where t is the time interval between which the scheduler runs.
Invoke calculatePcpuUsage() function to calculate the pcpu usage for each pcpu
Usage of pcpu P = sum of usages of all vcpus running on P.
Invoke pinVcpuToPcpu() to pin each vcpu to the best pcpu.
Find the maximum and minimum pcpu usage as well as the busiest and freest pcpu.
If the difference between maximum and minimum pcpu usage is greater than 15%
Find the busiest vcpu running on the busiest pcpu.
Pin the busiest vcpu to the freest pcpu.
Pin all the vcpus (except the busiest one if such a vcpu was found in previous step) to whichever pcpus they were running on.
Store currDomainStats in PrevDomainStats to use for the next run.
run make in this directory to compile vcpu scheduler, after changing the corresponding CPU_SCHED_PATH variable, it works as expected.
Sleep for the specified time interval t and then start again from step 4.
© 2022 GitHub, Inc.
Terms
Privacy
Security
Status
Docs
Contact GitHub
Pricing
API
Training
Blog
About
