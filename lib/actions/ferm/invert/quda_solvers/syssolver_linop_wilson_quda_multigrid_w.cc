/*! \file
 *  \QUDA MULTIGRID Wilson solver.
 */

#include "actions/ferm/invert/syssolver_linop_factory.h"
#include "actions/ferm/invert/syssolver_linop_aggregate.h"
#include "actions/ferm/invert/quda_solvers/syssolver_quda_multigrid_wilson_params.h"
#include "actions/ferm/invert/quda_solvers/syssolver_linop_wilson_quda_multigrid_w.h"
#include "io/aniso_io.h"


#include "handle.h"
#include "actions/ferm/fermstates/periodic_fermstate.h"
#include "actions/ferm/linop/lwldslash_w.h"
#include "meas/glue/mesplq.h"
// QUDA Headers
#include <quda.h>
// #include <util_quda.h>

namespace Chroma
{
  namespace LinOpSysSolverQUDAMULTIGRIDWilsonEnv
  {

    //! Anonymous namespace
    namespace
    {
      //! Name to be used
      const std::string name("QUDA_MULTIGRID_WILSON_INVERTER");

      //! Local registration flag
      bool registered = false;
    }



    LinOpSystemSolver<LatticeFermion>* createFerm(XMLReader& xml_in,	
						  const std::string& path,
						  Handle< FermState< LatticeFermion, multi1d<LatticeColorMatrix>, multi1d<LatticeColorMatrix> > > state, 
						  
						  Handle< LinearOperator<LatticeFermion> > A)
    {
      return new LinOpSysSolverQUDAMULTIGRIDWilson(A, state,SysSolverQUDAMULTIGRIDWilsonParams(xml_in, path));
    }

    //! Register all the factories
    bool registerAll() 
    {
      bool success = true; 
      if (! registered)
      {
	success &= Chroma::TheLinOpFermSystemSolverFactory::Instance().registerObject(name, createFerm);
	registered = true;
      }
      return success;
    }
  }

  SystemSolverResults_t 
  LinOpSysSolverQUDAMULTIGRIDWilson::qudaInvert(const T& chi_s,
				       T& psi_s) const{

    SystemSolverResults_t ret;

    void *spinorIn;

    T mod_chi;

    
    spinorIn =(void *)&(chi_s.elem(rb[1].start()).elem(0).elem(0).real());
    
    void* spinorOut =(void *)&(psi_s.elem(rb[1].start()).elem(0).elem(0).real());

    // Do the solve here 
    StopWatch swatch1; 
    swatch1.reset();
    swatch1.start();
    invertQuda(spinorOut, spinorIn, (QudaInvertParam*)&quda_inv_param);
    swatch1.stop();

    //chi_s[rb[1]] *= invMassParam;

    QDPIO::cout << "Cuda Space Required" << std::endl;
    QDPIO::cout << "\t Spinor:" << quda_inv_param.spinorGiB << " GiB" << std::endl;
    QDPIO::cout << "\t Gauge :" << q_gauge_param.gaugeGiB << " GiB" << std::endl;
    QDPIO::cout << "QUDA_"<<solver_string<<"_WILSON_SOLVER: time="<< quda_inv_param.secs <<" s" ;
    QDPIO::cout << "\tPerformance="<<  quda_inv_param.gflops/quda_inv_param.secs<<" GFLOPS" ; 
    QDPIO::cout << "\tTotal Time (incl. load gauge)=" << swatch1.getTimeInSeconds() <<" s"<<std::endl;

    ret.n_count =quda_inv_param.iter;

    return ret;

  }
  

}


 
