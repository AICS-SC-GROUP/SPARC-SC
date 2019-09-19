#pragma once

#define PRINT_WIDTH 18
#define PRINT_PRECISION 14

#include "energy.hpp"

//! IO Params container
class IOParamsContainer{
    PS::ReallocatableArray<PS::F64*> d_f64;
    PS::ReallocatableArray<PS::S64*> d_s64;
    PS::ReallocatableArray<PS::S32*> d_s32;
    PS::ReallocatableArray<std::string*> d_str;
    
public:
    
    void store(PS::F64* _item) {
        d_f64.push_back(_item);
    }

    void store(PS::S64* _item) {
        d_s64.push_back(_item);
    }

    void store(PS::S32* _item) {
        d_s32.push_back(_item);
    }

    void store(std::string* _item) {
        d_str.push_back(_item);
    }

    void writeAscii(FILE *_fout) {
        for(PS::S32 i=0; i<d_f64.size(); i++) fprintf(_fout, "%26.15e ", *d_f64[i]);
        for(PS::S32 i=0; i<d_s64.size(); i++) fprintf(_fout, "%lld ",    *d_s64[i]);
        for(PS::S32 i=0; i<d_s32.size(); i++) fprintf(_fout, "%d ",      *d_s32[i]);
        for(PS::S32 i=0; i<d_str.size(); i++) fprintf(_fout, "%s ",d_str[i]->c_str());
        fprintf(_fout,"\n");
    }
    
    void readAscii(FILE *_fin) {
        size_t rcount=0;
        for(PS::S32 i=0; i<d_f64.size(); i++) {
            rcount=fscanf(_fin, "%lf ", d_f64[i]);
            if (rcount<1) {
                std::cerr<<"Error: Data reading fails! requiring data number is 1, only obtain "<<rcount<<".\n";
                abort();
            }
        }
        for(PS::S32 i=0; i<d_s64.size(); i++) {
            rcount=fscanf(_fin, "%lld ", d_s64[i]);
            if (rcount<1) {
                std::cerr<<"Error: Data reading fails! requiring data number is 1, only obtain "<<rcount<<".\n";
                abort();
            }
        }
        for(PS::S32 i=0; i<d_s32.size(); i++) {
            rcount=fscanf(_fin, "%d ", d_s32[i]);
            if (rcount<1) {
                std::cerr<<"Error: Data reading fails! requiring data number is 1, only obtain "<<rcount<<".\n";
                abort();
            }
        }
        for(PS::S32 i=0; i<d_str.size(); i++) {
            char dtmp[1024];
            rcount=fscanf(_fin, "%s ", dtmp);
            if (rcount<1) {
                std::cerr<<"Error: Data reading fails! requiring data number is 1, only obtain "<<rcount<<".\n";
                abort();
            }
            *d_str[i] = dtmp;
        }
    }

#ifdef PARTICLE_SIMULATOR_MPI_PARALLEL        
    void mpi_broadcast() {
        for(PS::S32 i=0; i<d_f64.size(); i++) PS::Comm::broadcast(d_f64[i], 1, 0);
        for(PS::S32 i=0; i<d_s64.size(); i++) PS::Comm::broadcast(d_s64[i], 1, 0);
        for(PS::S32 i=0; i<d_s32.size(); i++) PS::Comm::broadcast(d_s32[i], 1, 0);
        for(PS::S32 i=0; i<d_str.size(); i++) {
            size_t str_size=d_str[i]->size();
            PS::Comm::broadcast(&str_size, 1, 0);
            char stmp[str_size+1];
            std::strcpy(stmp, d_str[i]->c_str());
            PS::Comm::broadcast(stmp, str_size, 0);
            *d_str[i]=std::string(stmp);
        }
    }
#endif
};

// IO Params
template <class Type>
struct IOParams{
    Type value;
    const char* name;
    const char* defaulted;

    IOParams(IOParamsContainer& _ioc, const Type& _value, const char* _name, const char* _defaulted=NULL): value(_value), name(_name), defaulted(_defaulted)  {
        _ioc.store(&value);
    }

    void print(std::ostream& os) const{
        os<<name<<":   "<<value<<std::endl;
    }
};

template <class Type>
std::ostream& operator <<(std::ostream& os, const IOParams<Type>& par) {
    if (par.defaulted!=NULL) os<<par.name<<": "<<par.defaulted;
    else os<<par.name<<": "<<par.value;
    return os;
}


class FileHeader{
public:
    PS::S64 nfile;  // file id
    PS::S64 n_body;
    //PS::S64 id_offset; // file id offset for add new artificial particles, should be larger than the maximum file id
    PS::F64 time;
    //PS::F64 dt_soft;   // tree time step should be recorded for restarting (soft kick of cm)
    //PS::S64 n_split;   // n_split is also needed for restarting (soft kick of cm)
    FileHeader(){
        n_body = 0;
        time = 0.0;
    }
    FileHeader(const PS::S64 ni, const PS::S64 n, const PS::F64 t){
        nfile = ni;
        n_body = n;
        time = t;
    }
    PS::S32 readAscii(FILE * fp){
        PS::S32 rcount=fscanf(fp, "%lld %lld %lf\n", &nfile, &n_body, &time);
        if (rcount<3) {
          std::cerr<<"Error: cannot read header, please check your data file header!\n";
          abort();
        }
        std::cout<<"Number of particles ="<<n_body<<";  Time="<<time<<std::endl;
        return n_body;
    }

    PS::S32 readBinary(FILE* fp){
        size_t rcount=fread(this, sizeof(FileHeader), 1, fp);
        if(rcount<1) {
            std::cerr<<"Error: Data reading fails! requiring data number is "<<1<<" bytes, only obtain "<<rcount<<" bytes.\n";
            abort();
        }
        std::cout<<"Number of particles ="<<n_body<<";  Time="<<time<<std::endl;
        return n_body;
    }

    void writeAscii(FILE* fp) const{
        fprintf(fp, "%lld %lld %26.17e\n", nfile, n_body, time);
    }

    void writeBinary(FILE* fp) const{
        fwrite(this, sizeof(FileHeader), 1, fp);
    }
};

template<class Tpsys, class Fheader, class Tpsoft>
void WriteFile(const Tpsys & psys,
               const char * file_name,
               const Fheader & file_header){
    PS::S32 my_rank = PS::Comm::getRank();
    PS::S32 n_proc = PS::Comm::getNumberOfProc();
    const PS::S64 n_glb = psys.getNumberOfParticleGlobal();
    const PS::S32 n_loc = psys.getNumberOfParticleLocal();
    FILE* fout;
    if(my_rank == 0){
      fout=fopen(file_name,"w");
      file_header.writeAscii(fout);
    }
    const PS::S32 n_tmp = 10;
    //FPSoft * ptcl_loc = new FPSoft[n_tmp];
    //FPSoft * ptcl_glb = new FPSoft[n_tmp];
    Tpsoft * ptcl_loc = new Tpsoft[n_tmp];
    Tpsoft * ptcl_glb = new Tpsoft[n_tmp];
    PS::S32 * n_ptcl_array = new PS::S32[n_proc];
    PS::S32 * n_ptcl_disp = new PS::S32[n_proc+1];
    for(PS::S32 i=0; i<(n_glb-1)/n_tmp+1; i++){
      PS::S32 i_head = i * n_tmp;
      PS::S32 i_tail = (i+1) * n_tmp;
      PS::S32 n_cnt = 0;
      for(PS::S32 j=0; j<n_loc; j++){
	    if( i_head<=psys[j].id && psys[j].id<i_tail){
            ptcl_loc[n_cnt] = psys[j];
            n_cnt++;
	    }
      }
      PS::Comm::allGather(&n_cnt, 1, n_ptcl_array);
      n_ptcl_disp[0] = 0;
      for(PS::S32 j=0; j<n_proc; j++){
	    n_ptcl_disp[j+1] = n_ptcl_disp[j] + n_ptcl_array[j];
      }
      PS::Comm::gatherV(ptcl_loc, n_cnt, ptcl_glb, n_ptcl_array, n_ptcl_disp);
      if(my_rank == 0){
	    const PS::S32 n = n_ptcl_disp[n_proc];
	    //std::sort(ptcl_glb, ptcl_glb+n, SortID());
	    std::sort(ptcl_glb, ptcl_glb+n,
		      //[](const FPSoft & left, const FPSoft & right)->bool{return left.id < right.id;}
		      [](const Tpsoft & left, const Tpsoft & right)->bool{return left.id < right.id;}
		      );
	    for(PS::S32 j=0; j<n; j++) ptcl_glb[j].writeAscii(fout);
      }
    }
    if(my_rank==0) fclose(fout);
}

/*
class DiskModel{
    PS::S32 n_planet_;
    PS::S32 n_planetesimal_;
    DiskModel(): n_planet_(0), n_planetesimal_(0){
    }
};
*/


#ifdef MAIN_DEBUG
// flag: 1: c.m; 2: individual; 
template<class Teng, class Tsys>
void write_p(FILE* fout, const PS::F64 time, const Tsys& p, const Teng &et) {
    fprintf(fout,"%20.14e ",time);
    fprintf(fout,"%20.14e %20.14e %20.14e %20.14e %20.14e %20.14e %20.14e %20.14e %20.14e %20.14e %20.14e %20.14e ",
            ediff.tot/et.tot, et.kin, et.pot, et.tot,
            ediff.Lt/et.Lt, ediff.L[0]/et.Lt, ediff.L[1]/et.Lt, ediff.L[2]/et.Lt,
               et.Lt,    et.L[0],    et.L[1],    et.L[2]);
    for (int i=0; i<p.getNumberOfParticleLocal(); i++) {
        if(p[i].status>0||p[i].id<0) continue;
        PS::F64 mi = p[i].mass;
        PS::F64vec vi = p[i].vel;
        fprintf(fout,"%20.14e %20.14e %20.14e %20.14e %20.14e %20.14e %20.14e ", 
                mi, p[i].pos[0], p[i].pos[1], p[i].pos[2], 
                vi[0], vi[1], vi[2]);
    }
    fprintf(fout,"\n");
}
#endif
