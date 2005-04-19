// -*- C++ -*-
// $Id: inline_building_blocks_w.h,v 1.2 2005-04-19 20:05:22 edwards Exp $
/*! \file
 * \brief Inline construction of BuildingBlocks
 *
 * Building Blocks on forward and sequential props
 */

#ifndef __inline_building_blocks_h__
#define __inline_building_blocks_h__

#include "chromabase.h"
#include "meas/inline/abs_inline_measurement.h"
#include "io/qprop_io.h"

namespace Chroma 
{ 
  /*! \ingroup inlinehadron */
  namespace InlineBuildingBlocksEnv 
  {
    extern const std::string name;
    extern const bool registered;
  }

  //! Parameter structure
  /*! \ingroup inlinehadron */
  struct InlineBuildingBlocksParams 
  {
    InlineBuildingBlocksParams();
    InlineBuildingBlocksParams(XMLReader& xml_in, const std::string& path);
    void write(XMLWriter& xml_out, const std::string& path);

    unsigned long frequency;

    //! Parameters
    struct Param_t
    {
      int      mom2_max;           // (mom)^2 <= mom2_max
      int      links_max;          // maximum number of links
      multi1d<int> nrow;           // lattice size
    } param;

    //! Propagators
    struct Prop_t
    {
      std::string   BkwdPropFileName;   // backward propagator
      std::string   BkwdPropG5Format;   // backward propagators Gamma5 Format
      int           GammaInsertion;     // second gamma insertion
      std::string   BBFileNamePattern;  // BB output file name pattern
    };

    //! BB output
    struct BB_out_t
    {
      std::string       OutFileName;
      std::string       FrwdPropFileName;   // input forward prop
      multi1d<Prop_t>   BkwdProps;
    } bb;

  };


  //! Inline measurement of Wilson loops
  /*! \ingroup inlinehadron */
  class InlineBuildingBlocks : public AbsInlineMeasurement 
  {
  public:
    ~InlineBuildingBlocks() {}
    InlineBuildingBlocks(const InlineBuildingBlocksParams& p) : params(p) {}
    InlineBuildingBlocks(const InlineBuildingBlocks& p) : params(p.params) {}

    unsigned long getFrequency(void) const {return params.frequency;}

    //! Do the measurement
    void operator()(const multi1d<LatticeColorMatrix>& u,
		    XMLBufferWriter& gauge_xml,
		    const unsigned long update_no,
		    XMLWriter& xml_out); 

  private:
    InlineBuildingBlocksParams params;
  };

};

#endif
