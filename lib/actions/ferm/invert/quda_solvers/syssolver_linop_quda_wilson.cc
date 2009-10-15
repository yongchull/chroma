// $Id: syssolver_linop_quda_wilson.cc,v 1.5 2009-10-09 13:59:46 bjoo Exp $
/*! \file
 *  \brief Solve a MdagM*psi=chi linear system by CG2
 */

#include "actions/ferm/invert/syssolver_linop_factory.h"
#include "actions/ferm/invert/syssolver_linop_aggregate.h"
#include "actions/ferm/invert/quda_solvers/syssolver_quda_wilson_params.h"
#include "actions/ferm/invert/quda_solvers/syssolver_linop_quda_wilson.h"
#include "io/aniso_io.h"


#include "handle.h"
#include "actions/ferm/fermstates/periodic_fermstate.h"
#include "actions/ferm/linop/lwldslash_w.h"
#include "meas/glue/mesplq.h"
// QUDA Headers
#include <quda.h>
#include <util_quda.h>
#include <invert_quda.h>

namespace Chroma
{
  namespace LinOpSysSolverQUDAWilsonEnv
  {

    //! Anonymous namespace
    namespace
    {
      //! Name to be used
      const std::string name("QUDA_WILSON_INVERTER");

      //! Local registration flag
      bool registered = false;
    }



    LinOpSystemSolver<LatticeFermion>* createFerm(XMLReader& xml_in,	
						  const std::string& path,
						  Handle< FermState< LatticeFermion, multi1d<LatticeColorMatrix>, multi1d<LatticeColorMatrix> > > state, 
						  
						  Handle< LinearOperator<LatticeFermion> > A)
    {
      return new LinOpSysSolverQUDAWilson(A, state,SysSolverQUDAWilsonParams(xml_in, path));
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
  LinOpSysSolverQUDAWilson::qudaInvert(const T& chi_s,
				       T& psi_s) const{

    SystemSolverResults_t ret;
    
 
    StopWatch swatch1; 
    swatch1.reset();
    swatch1.start();

    void* spinorIn =(void *)&(chi_s.elem(rb[1].start()).elem(0).elem(0).real());
    void* spinorOut =(void *)&(psi_s.elem(rb[1].start()).elem(0).elem(0).real());

    invertQuda(spinorOut, spinorIn, (QudaInvertParam*)&quda_inv_param);

    swatch1.stop();


    QDPIO::cout << "Cuda Space Required" << endl;
    QDPIO::cout << "\t Spinor:" << quda_inv_param.spinorGiB << " GiB" << endl;
    QDPIO::cout << "\t Gauge :" << q_gauge_param.gaugeGiB << " GiB" << endl;
    QDPIO::cout << "QUDA_" << solver_string << "_WILSON_SOLVER: time="<< quda_inv_param.secs <<" s" ;
    QDPIO::cout << "\tPerformance="<<  quda_inv_param.gflops/quda_inv_param.secs<<" GFLOPS" ; 
    QDPIO::cout << "\tTotal Time (incl. load gauge)=" << swatch1.getTimeInSeconds() <<" s"<<endl;

    ret.n_count =quda_inv_param.iter;

    return ret;

  }
  

}
