#include "SpiceUsr.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

bool failed()
{
    if(!failed_c()) return false;
    char message[2048];getmsg_c("LONG",sizeof(message),message);
    std::cerr<<message<<"\n";reset_c();return true;
}
int main(int argc,char**argv)
{
    char action[]="RETURN";erract_c("SET",0,action);
    std::string output,target;std::vector<double> epochs;
    for(int i=1;i<argc;++i)
    {
        if(i+1>=argc)return 2;
        const std::string option=argv[i],value=argv[++i];
        if(option=="--kernel"){furnsh_c(value.c_str());if(failed())return 3;}
        else if(option=="--target")target=value;
        else if(option=="--utc"){SpiceDouble et;str2et_c(value.c_str(),&et);if(failed())return 4;epochs.push_back(et);}
        else if(option=="--output")output=value;
        else return 2;
    }
    if(output.empty()||target.empty()||epochs.empty())return 2;
    std::ofstream stream(output);if(!stream)return 5;
    stream<<"utc,et,x_km,y_km,z_km,vx_kmps,vy_kmps,vz_kmps\n"<<std::setprecision(17);
    for(double et:epochs)
    {
        SpiceDouble state[6],lt;spkezr_c(target.c_str(),et,"J2000","NONE","SOLAR SYSTEM BARYCENTER",state,&lt);
        char utc[80];et2utc_c(et,"ISOC",6,sizeof(utc),utc);if(failed())return 6;
        stream<<utc<<"Z,"<<et;for(double v:state)stream<<","<<v;stream<<"\n";
    }
    return stream?0:7;
}
