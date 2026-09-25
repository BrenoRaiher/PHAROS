#include "SpiceUsr.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

bool failed()
{
    if (!failed_c()) return false;
    char message[2048]; getmsg_c("LONG",sizeof(message),message);
    std::cerr<<message<<"\n"; reset_c(); return true;
}

int main(int argc,char** argv)
{
    char action[]="RETURN"; erract_c("SET",0,action);
    std::string output,etFile;
    std::vector<double> epochs;
    for(int i=1;i<argc;++i)
    {
        const std::string option=argv[i];
        if(i+1>=argc) return 2;
        const std::string value=argv[++i];
        if(option=="--kernel") {furnsh_c(value.c_str());if(failed())return 3;}
        else if(option=="--et-file")etFile=value;
        else if(option=="--utc") {SpiceDouble et;str2et_c(value.c_str(),&et);if(failed())return 4;epochs.push_back(et);}
        else if(option=="--output")output=value;
        else return 2;
    }
    if(!etFile.empty())
    {
        std::ifstream input(etFile);if(!input)return 5;
        double et;while(input>>et)epochs.push_back(et);
        if(!input.eof())return 5;
    }
    if(output.empty()||epochs.empty())return 2;
    std::ofstream stream(output);if(!stream)return 6;
    stream<<"utc,ephemeris_time_tdb_seconds_past_j2000,position_j2000_x_km,position_j2000_y_km,position_j2000_z_km,velocity_j2000_x_kmps,velocity_j2000_y_kmps,velocity_j2000_z_kmps\n"<<std::setprecision(17);
    for(double et:epochs)
    {
        SpiceDouble state[6],lighttime;
        spkezr_c("-170",et,"J2000","NONE","SOLAR SYSTEM BARYCENTER",state,&lighttime);
        char utc[80];et2utc_c(et,"ISOC",6,sizeof(utc),utc);
        if(failed())return 7;
        stream<<utc<<"Z,"<<et;
        for(double value:state)stream<<","<<value;
        stream<<"\n";
    }
    std::cout<<"Extracted "<<epochs.size()<<" geometric J2000/ICRF-axis barycentric JWST states at exact ET.\n";
    return stream?0:8;
}
