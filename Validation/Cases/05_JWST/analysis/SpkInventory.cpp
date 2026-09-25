#include "SpiceUsr.h"
#include <fstream>
#include <iomanip>
#include <iostream>

int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    SpiceInt handle; dafopr_c(argv[1],&handle);
    dafbfs_c(handle);
    SpiceBoolean found;daffna_c(&found);
    std::ofstream out(argv[2]);
    out<<"start_et,end_et,target,center,frame,type,first_address,last_address,segment_id\n"<<std::setprecision(17);
    while(found)
    {
        SpiceDouble summary[125],dc[2];SpiceInt ic[6];char name[256];
        dafgs_c(summary);dafus_c(summary,2,6,dc,ic);dafgn_c(sizeof(name),name);
        out<<dc[0]<<","<<dc[1];for(auto value:ic)out<<","<<value;
        out<<","<<std::quoted(name)<<"\n";daffna_c(&found);
    }
    std::ofstream comments(std::string(argv[2])+".comments.txt");
    SpiceBoolean done=SPICEFALSE;SpiceInt count;char buffer[64][1024];
    while(!done)
    {
        dafec_c(handle,64,1024,&count,buffer,&done);
        for(int i=0;i<count;++i)comments<<buffer[i]<<"\n";
    }
    dafcls_c(handle);
    std::cout<<"Read SPK segment metadata and comments.\n";
    return 0;
}
