#include "SpiceUsr.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

void check(){if(failed_c()){char msg[2048];getmsg_c("LONG",sizeof(msg),msg);reset_c();throw std::runtime_error(msg);}}
int main(int argc,char**argv){try{
    char action[]="RETURN",printing[]="NONE";erract_c("SET",0,action);errprt_c("SET",0,printing);
    std::string output,source,target="-82",observer="0";std::vector<double> epochs;
    for(int i=1;i<argc;++i){if(i+1>=argc)throw std::runtime_error("Missing argument");
        std::string opt=argv[i],val=argv[++i];
        if(opt=="--kernel"){furnsh_c(val.c_str());check();}
        else if(opt=="--utc"){double et;str2et_c(val.c_str(),&et);check();epochs.push_back(et);}
        else if(opt=="--et")epochs.push_back(std::stod(val));
        else if(opt=="--output")output=val;
        else if(opt=="--target")target=val;
        else if(opt=="--observer")observer=val;
        else if(opt=="--states")source=val;
        else if(opt=="--epochs"){std::ifstream f(val);if(!f)throw std::runtime_error("Missing epochs file");double et;while(f>>et)epochs.push_back(et);}
        else throw std::runtime_error("Unknown option "+opt);
    }
    std::ofstream f(output);if(!f)throw std::runtime_error("Output cannot be opened");
    f<<std::setprecision(17)<<"utc,et,x_m,y_m,z_m,vx_mps,vy_mps,vz_mps\n";
    auto query=[&](double et){double s[6],lt;spkezr_c(target.c_str(),et,"J2000","NONE",observer.c_str(),s,&lt);check();char utc[80];et2utc_c(et,"ISOC",6,sizeof(utc),utc);check();f<<utc<<"Z,"<<et;for(double v:s)f<<","<<v*1000.;f<<"\n";};
    for(double et:epochs)query(et);
    if(!source.empty()){std::ifstream in(source);if(!in)throw std::runtime_error("States cannot be opened");std::string line;std::getline(in,line);while(std::getline(in,line)){if(!line.empty())query(std::stod(line.substr(0,line.find(','))));}}
    if(!f)throw std::runtime_error("Output write failed");return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
