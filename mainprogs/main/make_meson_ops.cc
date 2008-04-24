// $Id: make_meson_ops.cc,v 3.1 2008-04-24 03:13:33 edwards Exp $
/*! \file
 *  \brief Combine elemental meson ops into a meson operator
 *
 * Driver routine to combine elemental operators generated by 
 * STOCH_GROUP_MESON into group theoretical baryon operators 
 */


#include "chroma.h"

#include "stdio.h"
#include "string.h"

using namespace QDP;
using namespace Chroma;

struct Param_t
{
  multi1d<int> layout; //Lattice dimensions 
  int          decay_dir; //time direction
};

//Structure containing all the input elemental operator info
struct InputFiles_t
{
  //! 2-quark elemental operator structure
  struct ElementalOpFiles_t
  {
    struct Config_t
    {
      struct TimeFiles_t 
      {
	std::string src_file;  /*!< File containing the source operator */
	std::string snk_file;  /*!< File containing the sink operator */
      };
			
      multi1d<TimeFiles_t> time_files; /*!< Different dilution timeslices (most likely) will be in different files */
    };

    multi1d<Config_t> cfgs; /*!< List of source and sink op files for each config */
  };	

  multi1d<ElementalOpFiles_t> elem_op_files; /*!< Files containing available two quark ops */
  multi1d<std::string> coeff_files; //Files of all the group-theoretical ops to make 
};

//Structure containing all the output info
struct OutputInfo_t
{

  struct OutputPaths_t
  {
    std::string src_path; //Output path for source operator on this config
    std::string snk_path; //Output path for sink operator on this config
  };

  multi1d< OutputPaths_t > cfg_paths; 	//Group theoretical operator output paths for each config 
};

//! Mega-structure of all input
struct MakeOpsInput_t
{
  Param_t            param;
  OutputInfo_t       output_info;	
  InputFiles_t       input_files;
};

// Reader for input parameters
void read(XMLReader& xml, const string& path, Param_t& param)
{
  XMLReader paramtop(xml, path);

  int version;
  read(paramtop, "version", version);

  switch (version) 
  {
  case 1:
    read(paramtop, "Layout", param.layout );
    read(paramtop, "Decay_dir", param.decay_dir);
    break;

  default :
    /**************************************************************************/

    cerr << "Input parameter version " << version << " unsupported." << endl;
    exit(1);
  }
}

//! Reader for operator output paths  
void read(XMLReader& xml, const string& path, OutputInfo_t::OutputPaths_t& input)
{
  XMLReader inputtop(xml, path);

  read(inputtop, "SourceOpOutputPath", input.src_path);
  read(inputtop, "SinkOpOutputPath", input.snk_path);
}

//! Reader for output info  
void read(XMLReader& xml, const string& path, OutputInfo_t& input)
{
  XMLReader inputtop(xml, path);

  read(inputtop, "CfgOutputPaths", input.cfg_paths);
}

//! Reader for a timeslice of elemental op files  
void read(XMLReader& xml, const string& path, 
	  InputFiles_t::ElementalOpFiles_t::Config_t::TimeFiles_t& input)
{
  XMLReader inputtop(xml, path);

  read(inputtop, "CreationOperatorFile", input.src_file);
  read(inputtop, "AnnihilationOperatorFile", input.snk_file);
}

//! Reader for a single config of elemental operator files 
void read(XMLReader& xml, const string& path, InputFiles_t::ElementalOpFiles_t::Config_t& input)
{
  XMLReader inputtop(xml, path);

  read(inputtop, "DilutionTimeSlices", input.time_files);
}

//! Reader for elemental operator files 
void read(XMLReader& xml, const string& path, InputFiles_t::ElementalOpFiles_t& input)
{
  XMLReader inputtop(xml, path);

  read(inputtop, "Configs", input.cfgs);
}


//! Reader for input files 
void read(XMLReader& xml, const string& path, InputFiles_t& input)
{
  XMLReader inputtop(xml, path);

  read(inputtop, "CoeffFiles", input.coeff_files);
  read(inputtop, "ElementalOpFiles", input.elem_op_files);
}

// Reader for input parameters
void read(XMLReader& xml, const string& path, MakeOpsInput_t& input)
{
  XMLReader inputtop(xml, path);

  // Read all the input groups
  try
  {
    // Read program parameters
    read(inputtop, "Param", input.param);

    // Read in the input files 
    read(inputtop, "InputFiles", input.input_files);

    // Read in the ouput info 
    read(inputtop, "OutputInfo", input.output_info);
  }
  catch (const string& e) 
  {
    cerr << "Error reading make_ops data: " << e << endl;
    exit(1);
  }
}

//! Meson operator
struct MesonOperator_t
{
  //! Meson operator time slices corresponding to location of operator source
  struct TimeSlices_t
  {
    //! Meson operator dilutions
    struct Dilutions_t
    {
      //! Momentum projected correlator
      struct Mom_t
      {
	multi1d<int>       mom;         /*!< D-1 momentum of this operator */
	multi1d<DComplex>  op;          /*!< Momentum projected operator */
      };

      multi1d<Mom_t> mom_projs;         /*!< Holds momentum projections of the operator */
    };

    multi2d<Dilutions_t> dilutions;     /*!< Hybrid list indices */
    int t0;                             /*!< Acutal time corresponding to dilution timeslice */
  };

  GroupXML_t   quark_smearing;     /*!< String holding quark smearing xml */

  Seed          seed_l;            /*!< Id of left quark */
  Seed          seed_r;            /*!< Id of right quark */

  GroupXML_t    dilution_l;        /*!< Dilution scheme of left quark */ 
  GroupXML_t    dilution_r;        /*!< Dilution scheme of right quark */ 

  GroupXML_t    link_smearing; 
	
  std::string   quarkSources_l;    /*!< Sources used for left quark */ 
  std::string   quarkSources_r;    /*!< Sources used for right quark */ 

  std::string   configInfo;

  std::string   id;                /*!< Tag/ID used in analysis codes */

  int           mom2_max;          /*!< |\vec{p}|^2 */
  int           decay_dir;         /*!< Direction of decay */

  multi1d<TimeSlices_t> time_slices; /*!< Time slices of the lattice that are used */
};

struct TwoQuarkOp_t
{
  struct QuarkInfo_t
  {
    int  displacement;    /*!< Orig plus/minus 1-based directional displacements */
    int  spin;            /*!< 1-based spin index */
  };

  multi1d<QuarkInfo_t> quarks;
};

struct GroupMesonOperator_t
{
  struct Term_t
  { 
    TwoQuarkOp_t op;
    DComplex coeff;
  };

  multi1d<Term_t> term;
	
  std::string name; 
};


//! MesonOperator header reader
void read(XMLReader& xml, const string& path, MesonOperator_t& param)
{
  XMLReader paramtop(xml, path);

  try
  {
    int version;
    read(paramtop, "version", version);
    read(paramtop, "id", param.id);
    read(paramtop, "mom2_max", param.mom2_max);
    read(paramtop, "decay_dir", param.decay_dir);
    read(paramtop, "seed_l", param.seed_l);
    read(paramtop, "seed_r", param.seed_r);
      
    param.dilution_l = readXMLGroup(paramtop, "dilution_l/elem", "DilutionType");
    param.dilution_r = readXMLGroup(paramtop, "dilution_r/elem", "DilutionType");
  		
    param.quark_smearing = readXMLGroup(paramtop, "QuarkSmearing", "wvf_kind");
  }
  catch (const std::string& e) 
  {
    cerr << "MesonOperator: Error reading: " << e << endl;
    exit(1);
  }
}

//! MesonOperator header writer 
void write(XMLWriter& xml, const string& path, MesonOperator_t& param)
{
  push(xml, path);

  try
  {
    write(xml, "id", param.id);
    write(xml, "mom2_max", param.mom2_max);
    write(xml, "decay_dir", param.decay_dir);
    write(xml, "seed_l", param.seed_l);
    write(xml, "seed_r", param.seed_r);

    push(xml , "dilution_l");
    xml << param.dilution_l.xml;
    pop(xml);
    push(xml , "dilution_r");
    xml << param.dilution_r.xml;
    pop(xml);

    push(xml, "QuarkSources_l");
    write(xml, "TimeSlices", param.quarkSources_l);
    pop(xml);

    push(xml, "QuarkSources_r");
    write(xml, "TimeSlices", param.quarkSources_r);
    pop(xml);

    //write(xml, "Config_info", param.configInfo);

    xml << param.link_smearing.xml;
    xml << param.quark_smearing.xml;

  }
  catch (const std::string& e) 
  {
    cerr << "MesonOperator: Error writing: " << e << endl;
    exit(1);
  }

  pop(xml);
}

//GroupMesonOperator Writer
void write(XMLWriter &xml, const std::string &path,
	   const TwoQuarkOp_t::QuarkInfo_t &param)
{
  push(xml, path);

  write(xml, "Spin" , param.spin);
  write(xml, "Displacement" , param.displacement);

  pop(xml);
}

//GroupMesonOperator Writer
void write(XMLWriter &xml, const std::string &path, const TwoQuarkOp_t &param)
{
  push(xml, path);

  write(xml, "Quarks" , param.quarks);

  pop(xml);
}

//TwoQuarkOp Reader
void read(XMLReader &xml, const std::string &path,
	  TwoQuarkOp_t::QuarkInfo_t &param)
{
  XMLReader top(xml, path);

  read(top, "Spin" , param.spin);
  read(top, "Displacement" , param.displacement);
}

//Two Quark Op Reader
void read(XMLReader &xml, const std::string &path, TwoQuarkOp_t &param)
{
  XMLReader top(xml, path);

  read(top, "Quarks" , param.quarks);
}

//GroupMesonOperator Writer
void write(XMLWriter &xml, const std::string &path, const GroupMesonOperator_t::Term_t &param)
{
  push(xml, path);

  write(xml, "ElementalOperator" , param.op);
  write(xml, "Coefficient" , param.coeff);

  pop(xml);
}	

//GroupMesonOperator Writer
void write(XMLWriter &xml, const std::string &path, const GroupMesonOperator_t &param)
{
  push(xml, path);

  write(xml, "Name" , param.name);
  write(xml, "Terms" , param.term);

  pop(xml);
}	

//! MesonOperator binary reader
void read(BinaryReader& bin, MesonOperator_t::TimeSlices_t::Dilutions_t::Mom_t& param)
{
  read(bin, param.mom);
  read(bin, param.op);
}

//! MesonOperator binary reader 
void read(BinaryReader& bin, MesonOperator_t::TimeSlices_t::Dilutions_t& param)
{
  read(bin, param.mom_projs);
}

//! MesonOperator binary reader 
void read(BinaryReader& bin, MesonOperator_t::TimeSlices_t& param)
{
  read(bin, param.dilutions);
  read(bin, param.t0);
}

//! MesonOperator binary reader 
void read(BinaryReader& bin, MesonOperator_t& param)
{
  read(bin, param.seed_l);
  read(bin, param.seed_r);
  read(bin, param.mom2_max);
  read(bin, param.decay_dir);
  read(bin, param.time_slices);
}


//! MesonOperator binary writer
void write(BinaryWriter& bin, const MesonOperator_t::TimeSlices_t::Dilutions_t::Mom_t& param)
{
  write(bin, param.mom);
  write(bin, param.op);
}

//! MesonOperator binary writer
void write(BinaryWriter& bin, const MesonOperator_t::TimeSlices_t::Dilutions_t& param)
{
  write(bin, param.mom_projs);
}

//! MesonOperator binary writer
void write(BinaryWriter& bin, const MesonOperator_t::TimeSlices_t& param)
{
  write(bin, param.dilutions);
  write(bin, param.t0);
}

//! MesonOperator binary writer
void write(BinaryWriter& bin, const MesonOperator_t& param)
{
  write(bin, param.seed_l);
  write(bin, param.seed_r);
  write(bin, param.mom2_max);
  write(bin, param.decay_dir);
  write(bin, param.time_slices);
}

//This routine goes through (possibly several) coeff files and fills 
//the operator array
void readCoeffFiles(multi1d<GroupMesonOperator_t> &ops, const multi1d<std::string> coeff_files) 
{
  int nops = 0;

  //First determine how many ops total 
  for (int f = 0 ; f < coeff_files.size() ; ++f)
  {
    TextFileReader reader(coeff_files[f]);

    int op;
    reader >> op;

    reader.close();
    nops += op; 

    QDPIO::cout<< "Nops = "<<nops<<endl;
  }

  ops.resize(nops);
	
  // Now read the coeffs
  for (int f = 0 ; f < coeff_files.size() ; ++f)
  {
    int op;

    TextFileReader reader(coeff_files[f]);
    reader >> op;

    for (int l = 0 ; l < op ; ++l)
    {
      int nelem;
      std::string name; 
      reader >> nelem >> name;

      ops[ f + l ].name = name;
      ops[ f + l ].term.resize( nelem );

      for (int m = 0 ; m < nelem ; ++m)
      {
	int a,b,i,j;
	Real re,im;
	char lparen,comma,rparen;

	reader >> a >> b >> i >> j >> lparen >> re >> comma >> im >> rparen;

	ops[f + l].term[m].coeff = cmplx( re, im );

	ops[f + l].term[m].op.quarks.resize(2);

	ops[f + l].term[m].op.quarks[0].spin = a;
	ops[f + l].term[m].op.quarks[1].spin = b;

	ops[f + l].term[m].op.quarks[0].displacement = i;
	ops[f + l].term[m].op.quarks[1].displacement = j;
      } //m
    } //l 
	 		
    reader.close();
  } //i

}//void


//Fill the operator info from first elem. op so it isn't done for every 
//elem op
void initOp (MesonOperator_t &oper , const MesonOperator_t &elem_oper ) 
{
  oper.mom2_max= elem_oper.mom2_max;
  oper.decay_dir = elem_oper.decay_dir;
  oper.seed_l = elem_oper.seed_l;
  oper.seed_r = elem_oper.seed_r;
  oper.dilution_l = elem_oper.dilution_l;
  oper.dilution_r = elem_oper.dilution_r;
  oper.configInfo = elem_oper.configInfo;
  oper.quarkSources_l = elem_oper.quarkSources_l;
  oper.quarkSources_r = elem_oper.quarkSources_r;
  oper.quark_smearing = elem_oper.quark_smearing;
  oper.link_smearing = elem_oper.link_smearing;

  int Nt = 1;

  oper.time_slices.resize(Nt);

  for (int t0 = 0 ; t0 < Nt ; ++t0)
  {
    oper.time_slices[t0].t0 = elem_oper.time_slices[t0].t0;

    //Dilution sizes for each quark
    int Ni = elem_oper.time_slices[t0].dilutions.size1();
    int Nj = elem_oper.time_slices[t0].dilutions.size2();

    oper.time_slices[t0].dilutions.resize(Ni, Nj);

    for(int i = 0 ; i < Ni ; ++i)   	
      for(int j = 0 ; j < Nj ; ++j)   	
      {
	int Nmom =  elem_oper.time_slices[t0].dilutions(i,j).mom_projs.size();

	oper.time_slices[t0].dilutions(i,j).mom_projs.resize(Nmom);

	for (int m = 0 ; m < Nmom ; ++m)
	{
	  oper.time_slices[t0].dilutions(i,j).mom_projs[m].mom = 
	    elem_oper.time_slices[t0].dilutions(i,j).mom_projs[m].mom;

	  //zero operator
	  int Lop = elem_oper.time_slices[t0].dilutions(i,j).mom_projs[m].op.size();

	  oper.time_slices[t0].dilutions(i,j).mom_projs[m].op.resize(Lop);

	  for (int t = 0 ; t < Lop ; ++t)
	  {
	    oper.time_slices[t0].dilutions(i,j).mom_projs[m].op[t] = zero;
	  }
	} //m
      }//ij
  }//t_0
} //void



//Add the elemental op to the final operator
void addTo(MesonOperator_t &oper, const MesonOperator_t &elem_oper, 
	   const DComplex& coeff)
{
  int Nt = 1;

  for (int t0 = 0 ; t0 < Nt ; ++t0)
  {
    for(int i = 0 ; i < elem_oper.time_slices[t0].dilutions.size1() ; ++i)   	
      for(int j = 0 ; j < elem_oper.time_slices[t0].dilutions.size2() ; ++j)   	
      {
	for (int m = 0 ; m < elem_oper.time_slices[t0].dilutions(i,j).mom_projs.size() ; ++m)
	{
	  oper.time_slices[t0].dilutions(i,j).mom_projs[m].op += 
	    elem_oper.time_slices[t0].dilutions(i,j).mom_projs[m].op * coeff;
	} //m
      }//ij
  }//t_0
} //void

//Test if all elemental ops have the same configs, dilution schemes, seeds, decay_dirs 
//Will also return true for similiar inconsistentcies within an op 
void opsError(const multi1d<InputFiles_t::ElementalOpFiles_t> ops) 
{
  //Tests - This routing may need to be restructured to eleminate 
  //extra file io
		
  //Grab info from first op 
  int Nbins = ops[0].cfgs.size();
  int Nt = ops[0].cfgs[0].time_files.size();

  std::string propInfo;
  {
    XMLReader file_xml;

    const std::string& filename = ops[0].cfgs[0].time_files[0].src_file;

    QDPFileReader rdr(file_xml, filename, QDPIO_SERIAL);

    XMLReader xml_tmp(file_xml, "/SourceMesonOperator/QuarkSinks");
    std::ostringstream os;
    xml_tmp.print(os);

    propInfo = os.str();

    rdr.close();
  }


  for (int i = 0 ; i < ops.size() ; ++i )  	
  {
    //Grab info from the current op  
    int currNbins = ops[i].cfgs.size();

    std::string opInfo; 
    {
      XMLReader file_xml, record_xml;
      BinaryBufferReader dummy;

      const std::string& filename = ops[i].cfgs[0].time_files[0].src_file;

      QDPFileReader rdr(file_xml, filename, QDPIO_SERIAL);

      XMLReader xml_tmp(file_xml, "/SourceMesonOperator/Op_Info");
      std::ostringstream os;
      xml_tmp.print(os);

      opInfo = os.str();

      rdr.close();
    }
	
    if ( toBool( currNbins != Nbins ) )
    {
      QDPIO::cerr<< "Inconsistent(with first op) number of configs: op"
		 << i << endl; 
      QDP_abort(1);
    }

    for (int n = 0 ; n < Nbins ; ++n )
    {

      //check that all cfgs have same info within the op
      if ( ops[i].cfgs[n].time_files.size() != Nt )
      {
	QDPIO::cerr<< "Inconsistent number of time dilution files: op"
		   << i << " cfg " << n << endl; 
	QDP_abort(1);
      }

      //Grab cfgInfo from first op 
      std::string cfgInfo; 
      {
	XMLReader file_xml;
	const std::string & filename = 
	  ops[0].cfgs[n].time_files[0].src_file;

	QDPFileReader rdr(file_xml, filename, QDPIO_SERIAL);

	XMLReader xml_tmp(file_xml, "/SourceMesonOperator/Config_info");
	std::ostringstream os;
	xml_tmp.print(os);

	cfgInfo = os.str();

	rdr.close();
      }

      //Grab cfgInfo from current op
      std::string currCfgInfo; 
      {
	XMLReader file_xml;

	const std::string & filename = 
	  ops[i].cfgs[n].time_files[0].src_file;

	QDPFileReader rdr(file_xml, filename, QDPIO_SERIAL);

	XMLReader xml_tmp(file_xml, "/SourceMesonOperator/Config_info");
	std::ostringstream os;
	xml_tmp.print(os);

	currCfgInfo = os.str();

	rdr.close();
      }

      if (cfgInfo != currCfgInfo)
      {
	QDPIO::cerr<<"Configs do not match for all ops: op "<<
	  i << endl;
	QDP_abort(1);
      }

      for (int t0 = 0 ; t0 < Nt ; ++t0)
      {
	std::string currPropInfo;
	{
	  XMLReader file_xml;

	  const std::string & filename = 
	    ops[i].cfgs[n].time_files[t0].snk_file;

	  QDPFileReader rdr(file_xml, filename, QDPIO_SERIAL);

	  XMLReader xml_tmp(file_xml, "/SinkMesonOperator/QuarkSinks");
	  std::ostringstream os;
	  xml_tmp.print(os);

	  currPropInfo = os.str();

	  rdr.close();
	}


	if ( currPropInfo != propInfo)
	{
	  QDPIO::cerr << "Propagator parameters do not match: Op = " << 
	    i << " cfg = " << n << " t0 = " << t0 << endl;

	  QDP_abort(1);
	}

	//Open first op files	
	//Note: all time slices must match as well as src and snk 
	XMLReader firstFileXML;
	{
	  const std::string & filename = 
	    ops[0].cfgs[0].time_files[t0].src_file;

	  QDPFileReader rdr(firstFileXML, filename, QDPIO_SERIAL);
	}	

	//Open the current op files 
	XMLReader srcFileXML;
	XMLReader snkFileXML;
	{
	  const std::string & filename = 
	    ops[i].cfgs[n].time_files[t0].src_file;

	  QDPFileReader rdr(srcFileXML, filename, QDPIO_SERIAL);
	}	

	{
	  const std::string & filename = 
	    ops[i].cfgs[n].time_files[t0].snk_file;

	  QDPFileReader rdr(snkFileXML, filename, QDPIO_SERIAL);
	}	

	//Now check stuff for each file	
		
	//Do the cfgs match?
	std::string srcCfgInfo, snkCfgInfo; 
	{
	  XMLReader xml_tmp(srcFileXML, "/SourceMesonOperator/Config_info");
	  std::ostringstream os;
	  xml_tmp.print(os);

	  srcCfgInfo = os.str();
	}

	{
	  XMLReader xml_tmp(snkFileXML, "/SinkMesonOperator/Config_info");
	  std::ostringstream os;
	  xml_tmp.print(os);

	  snkCfgInfo = os.str();
	}

	if (snkCfgInfo != cfgInfo)	
	{
	  QDPIO::cout<<"Sink cfgInfo is inconsistent. : cfg = "<<n << " t0 = "<<
	    t0 << endl;

	  QDP_abort(1);
	}

	if (srcCfgInfo != cfgInfo)	
	{
	  QDPIO::cout<<"Source cfgInfo is inconsistent. : cfg = "<<n << " t0 = "<<
	    t0 << endl;

	  QDP_abort(1);
	}

	//Do the dilutions match?
	std::string firstDil, srcDil, sinkDil;
	{
	  XMLReader xml_tmp(snkFileXML, "/SinkMesonOperator/QuarkSources");
	  std::ostringstream os;
	  xml_tmp.print(os);
					
	  sinkDil = os.str();
	}

	//int currT0; 	
	{
	  XMLReader xml_tmp(srcFileXML, "/SourceMesonOperator/QuarkSources");
	  //read(xml_tmp, "Quark_l/TimeSlice/Dilutions/elem/Source/t_source", currT0);
	  std::ostringstream os;
	  xml_tmp.print(os);

	  srcDil = os.str();
	}
				
	{
	  XMLReader xml_tmp(firstFileXML, "/SourceMesonOperator/QuarkSources");
	  std::ostringstream os;
	  xml_tmp.print(os);

	  firstDil = os.str();
	}

	if (firstDil != sinkDil) 
	{
	  QDPIO::cerr<< "Dilution scheme does not match: snkOp = " <<i<<
	    " cfg = "<< n << " t0 = " << t0 <<endl;

	  QDPIO::cerr << "firstDil= XX" << firstDil << "XX" << endl
		      << "sinkDil= XX"  << sinkDil << "XX" << endl;

	  QDP_abort(1);
	}

			
	if (firstDil != srcDil) 
	{
	  QDPIO::cerr<< "Dilution scheme does not match: srcOp = " <<i<<
	    " cfg = "<< n << " t0 = " << t0 << endl;

	  QDP_abort(1);
	}

	//Furthermore, check that each timeslice has the same dilutions for all ops	
	/*
	  std::string firstT0Dil;
	  int firstT0;
	  {
	  XMLReader file_xml;

	  const std::string & filename = 
	  ops[i].cfgs[n].time_files[0].src_file;

	  QDPFileReader rdr(file_xml, filename, QDPIO_SERIAL);

	  XMLReader xml_tmp(file_xml, "/SourceMesonOperator/QuarkSources");
	  read(xml_tmp, "Quark_l/TimeSlices/elem/Dilutions/elem/Source/t_source", firstT0);
	  std::ostringstream os;
	  xml_tmp.print(os);

	  firstT0Dil = os.str();
	  }

	*/

	//Check that all files for a single op indeed belong to the same op

	//Grab opInfo from current op 
	std::string srcOpInfo; 
	{
	  XMLReader file_xml;

	  const std::string & filename = 
	    ops[i].cfgs[n].time_files[t0].src_file;

	  QDPFileReader rdr(file_xml, filename, QDPIO_SERIAL);

	  XMLReader xml_tmp(file_xml, "/SourceMesonOperator/Op_Info");
	  std::ostringstream os;
	  xml_tmp.print(os);

	  srcOpInfo = os.str();

	  rdr.close();
	}

	std::string snkOpInfo; 
	{
	  XMLReader file_xml;

	  const std::string & filename = 
	    ops[i].cfgs[n].time_files[t0].snk_file;

	  QDPFileReader rdr(file_xml, filename, QDPIO_SERIAL);

	  XMLReader xml_tmp(file_xml, "/SinkMesonOperator/Op_Info");
	  std::ostringstream os;
	  xml_tmp.print(os);

	  snkOpInfo = os.str();

	  rdr.close();
	}

			
	if ( opInfo != srcOpInfo)
	{
	  QDPIO::cerr<< "Src Op not the same: op = "<< i << " cfg = "
		     << n << " t0 = " << t0 << "opInfo = XX"<< opInfo<<
	    "XX srcOpInfo = XX"<<srcOpInfo<<"XX"<<endl;
	  QDP_abort(1);
	}

	if ( opInfo != snkOpInfo)
	{
	  QDPIO::cerr<< "Snk Op not the same: op = "<< i << " cfg = "
		     << n << " t0 = " << t0 << endl;

	  QDP_abort(1);
	}

      }//t0

    }//Nbins 

  }//Nops 

}//opsError

//Elemental Operator Maps 
struct ElementalOpKey_t
{
  TwoQuarkOp_t op; 
};

struct ElementalOpEntry_t
{
  InputFiles_t::ElementalOpFiles_t opFiles;
};

//support for elOpmap

bool operator<(const ElementalOpKey_t& keyOpA, const ElementalOpKey_t& keyOpB) 
{
  multi1d<int> lga(4);
  lga[0] = keyOpA.op.quarks[0].displacement;
  lga[1] = keyOpA.op.quarks[0].spin;
  lga[2] = keyOpA.op.quarks[1].displacement;
  lga[3] = keyOpA.op.quarks[1].spin;

  multi1d<int> lgb(4);
  lgb[0] = keyOpB.op.quarks[0].displacement;
  lgb[1] = keyOpB.op.quarks[0].spin;
  lgb[2] = keyOpB.op.quarks[1].displacement;
  lgb[3] = keyOpB.op.quarks[1].spin;

  return (lga < lgb);
}

class ElementalOpMap
{
public:
  //Constructor
  ElementalOpMap(const multi1d<InputFiles_t::ElementalOpFiles_t>& elOpFiles);
		
  //Destructor
  ~ElementalOpMap() {}

  //Acessors		
  MesonOperator_t getSourceOp(const ElementalOpKey_t& opKey, int cfg, int t0);	
		
  MesonOperator_t getSinkOp(const ElementalOpKey_t& opKey, int cfg, int t0);	

private:
  map<ElementalOpKey_t, ElementalOpEntry_t> elemMap; 
};	

//Constructor
ElementalOpMap::ElementalOpMap(
  const multi1d<InputFiles_t::ElementalOpFiles_t>& elOpFiles)
{

  int Nelem = elOpFiles.size();

  for( int i = 0 ; i < Nelem ; ++i )
  {
    //grab op info from src
    ElementalOpKey_t key;
    {
      XMLReader file_xml;

      const std::string & filename = 
	elOpFiles[i].cfgs[0].time_files[0].src_file;

      QDPFileReader rdr(file_xml, filename, QDPIO_SERIAL);
				
      read(file_xml, "/SourceMesonOperator/Op_Info", key.op); 
      rdr.close();
    }

    //If entry is not in map create it
    if ( elemMap.find(key) == elemMap.end() )
    {
			
      // Insert empty entries and then modify them. This saves on
      // copying the data around
      {
	ElementalOpEntry_t empty;
	elemMap.insert(std::make_pair(key, empty));

	// Sanity check - the entry better be there
	if ( elemMap.find(key) == elemMap.end() )
	{
	  QDPIO::cerr << __func__
		      << ": internal error - could not insert empty key in map"
		      << endl;
			
	  QDP_abort(1);
	}	
			
	// Modify the previous empty entry
			
	ElementalOpEntry_t& elem = elemMap.find(key)->second;

	elem.opFiles = elOpFiles[i]; 
		
      }

    }
    else
    {
      QDPIO::cerr<< "Multiple copies of the same op in input: Op = "<< i << endl;
      QDP_abort(1);
    }

  } //i 

}

MesonOperator_t ElementalOpMap::getSourceOp(const ElementalOpKey_t& opKey, int cfg, int t0) 	
{

  MesonOperator_t source;

  bool init = false; 

  if (elemMap.find(opKey) != elemMap.end() )
  {
    const InputFiles_t::ElementalOpFiles_t::Config_t::TimeFiles_t& opFiles = 
      elemMap.find(opKey)->second.opFiles.cfgs[cfg].time_files[t0];

    std::string sources_l, sources_r;

    XMLReader file_xml, record_xml;
    {
      BinaryBufferReader dummy;

      QDPFileReader rdr(file_xml, opFiles.src_file, QDPIO_SERIAL);
      read(rdr, record_xml, dummy);

      XMLReader temp_l(file_xml, "/SourceMesonOperator/QuarkSources/Quark_l/TimeSlice/Dilutions");
      XMLReader temp_r(file_xml, "/SourceMesonOperator/QuarkSources/Quark_r/TimeSlice/Dilutions");

      std::ostringstream str_l, str_r;
      temp_l.print(str_l);
      temp_r.print(str_r);

      source.quarkSources_l = str_l.str();
      source.quarkSources_r = str_r.str();

      read(dummy, source); 

      //read link smearing 
      source.link_smearing = readXMLGroup(file_xml, 
					  "/SourceMesonOperator/Params/LinkSmearing", "LinkSmearingType");

      read(record_xml, "/MesonCreationOperator" , source);

      rdr.close();
    }

    XMLReader tmp(file_xml, "/SourceMesonOperator/Config_info");
    std::ostringstream str_tmp;

    tmp.print(str_tmp);
    source.configInfo = str_tmp.str();

    //Assume each elem. Op file contains only one timeslice
    if ( source.time_slices.size() != 1)
    {
      QDPIO::cerr << "ERROR: each elemental op file must contain a single timeslice. Nt = " << source.time_slices.size() << endl;
      QDP_abort(1); 
    }

  } //if in map
  else
  {
    QDPIO::cerr<< "Source Elemental operator not found in map."<<endl;
    QDP_abort(1);
  }

  return source;
} //getSourceOP

MesonOperator_t ElementalOpMap::getSinkOp(const ElementalOpKey_t& opKey, int cfg, int t0) 	
{

  MesonOperator_t sink;

  if (elemMap.find(opKey) != elemMap.end() )
  {
    const InputFiles_t::ElementalOpFiles_t::Config_t::TimeFiles_t& opFiles = 
      elemMap.find(opKey)->second.opFiles.cfgs[cfg].time_files[t0];

    XMLReader file_xml, record_xml;
    {
      std::string snkFile = opFiles.snk_file;

      BinaryBufferReader dummy;

      QDPFileReader rdr(file_xml, snkFile, QDPIO_SERIAL);
      read(rdr, record_xml, dummy);

      XMLReader temp_l(file_xml, "/SinkMesonOperator/QuarkSources/Quark_l/TimeSlice/Dilutions");
      XMLReader temp_r(file_xml, "/SinkMesonOperator/QuarkSources/Quark_r/TimeSlice/Dilutions");

      std::ostringstream str_l, str_r;
      temp_l.print(str_l);
      temp_r.print(str_r);

      sink.quarkSources_l = str_l.str();
      sink.quarkSources_r = str_r.str();

      read(dummy, sink); 
      read(record_xml, "/MesonAnnihilationOperator" , sink);

      sink.link_smearing = readXMLGroup(file_xml, 
					"/SinkMesonOperator/Params/LinkSmearing", "LinkSmearingType");

      rdr.close();
    }

    XMLReader tmp(file_xml, "/SinkMesonOperator/Config_info");
    std::ostringstream str_tmp;

    tmp.print(str_tmp);
    sink.configInfo = str_tmp.str();

    //Assume each elem. Op file contains only one timeslice
    if ( sink.time_slices.size() != 1)
    {
      QDPIO::cerr << "ERROR: each elemental op file must contain a single timeslice" << endl;
      QDP_abort(1); 
    }

  } //if in map
  else
  {
    QDPIO::cerr<< "Sink Elemental operator not found in map."<<endl;
    QDP_abort(1);
  }

  return sink;

} //getSinkOp

int main(int argc, char **argv)
{

  // Put the machine into a known state
  Chroma::initialize(&argc, &argv);
  //  linkageHack();

  // Put this in to enable profiling etc.
  START_CODE();

  //Read Input params from xml
  MakeOpsInput_t input;

  XMLReader xml_in;

  StopWatch swatch, snoop;

  swatch.reset();
  swatch.start();

  try
  {
    xml_in.open(Chroma::getXMLInputFileName());
    read(xml_in, "/MakeMesonOps", input);
  }
  catch(const std::string& e) 
  {
    QDPIO::cerr << "MAKE_MESON_OPS: Caught Exception reading XML: " << e << endl;
    QDP_abort(1);
  }
  catch(std::exception& e) 
  {
    QDPIO::cerr << "MAKE_MESON_OPS: Caught standard library exception: " << e.what() << endl;
    QDP_abort(1);
  }
  catch(...)
  {
    QDPIO::cerr << "MAKE_MESON_OPS: caught generic exception reading XML" << endl;
    QDP_abort(1);
  }

  XMLFileWriter& xml_out = Chroma::getXMLOutputInstance();
  push(xml_out, "MakeMesonOps");

  // Write out the input
  write(xml_out, "Input", xml_in);

  Layout::setLattSize(input.param.layout);
  Layout::create();   // Setup the layout

  multi1d<GroupMesonOperator_t> final_ops;

  QDPIO::cout<< "Reading Coeff Files" << endl;

  //Read coeff files 
  readCoeffFiles(final_ops, input.input_files.coeff_files);

  int time_dir = input.param.decay_dir;
  int Nt = input.param.layout[time_dir]; 
  int Nops = final_ops.size();
  int Nelem = input.input_files.elem_op_files.size();
  int Nbins = input.input_files.elem_op_files[0].cfgs.size();
  int Nt0 = input.input_files.elem_op_files[0].cfgs[0].time_files.size();

  //-------------------------------------------------------------
  //Sanity Checks

  //Does the number of configs to ouput agree with input?
  if ( input.output_info.cfg_paths.size() != Nbins )
  {
    QDPIO::cerr << "Number of ouput config paths not equal to that of input." << endl;
    QDP_abort(1);
  }


  QDPIO::cout << "Performing Sanity checks" << endl;

  //Check consistencies with cfgs, dilutions for all elemental ops
#if 0
  opsError(input.input_files.elem_op_files);
#else
  QDPIO::cerr << "WARNING: skipping call to opsError - not checking for dilution sanity" << endl;
#endif

  //-------------------------------------------------------------
  QDPIO::cout << "Writing to xml " << endl;

  //Write oplist to output xml
  write(xml_out, "GroupMesonOperators", final_ops);

  QDPIO::cout << " MAKE_MESON_OPS: construct meson operators" << endl;

  //Elemental Operator maps 
  ElementalOpMap el_op_maps(input.input_files.elem_op_files);

  //loop over configurations
  for (int i = 0 ; i < Nbins ; ++i)
  {
    QDPIO::cout << "Forming Ops: Bin "<< i << endl; 

    for (int l = 0 ; l < Nops ; ++l)
    {
      for (int t0 = 0 ; t0 < Nt0 ; ++t0)
      {
	//Make Source
	{	
	  MesonOperator_t source;
	  source.id = final_ops[l].name; 

	  snoop.reset();
	  snoop.start();

	  QDPIO::cout<< "Making Source Meson Op: " << source.id << " t0 = " << t0 << endl;

	  int Nterms = final_ops[l].term.size();

	  bool init = false;

	  for (int m = 0 ; m < Nterms ; ++m)
	  {
	    ElementalOpKey_t elOpKey;

	    elOpKey.op = final_ops[l].term[m].op;

	    const MesonOperator_t & elem_source = el_op_maps.getSourceOp(elOpKey, i, t0 );

	    if (!init)
	    {
	      QDPIO::cout<<"init. group meson op source"<<endl;				
	      //Set all the headers, mom_projs, t_0's only once 
	      initOp(source , elem_source);

	      QDPIO::cout<<"Finished init ops"<<endl;
	      init = true;
	    }

	    QDPIO::cout<<"Adding elemental to group meson op"<<endl;

	    //add the elem op to the final op
	    addTo(source, elem_source, final_ops[l].term[m].coeff);

	  } //m

	  snoop.stop();
	  QDPIO::cout<<"Source op constructed: "<< snoop.getTimeInSeconds() << " secs " 
		     << endl;

	  /*
	    for (int t0 = 0 ; t0 < source.time_slices.size() ; ++t0)
	    {
	    QDPIO::cout << "Source op test val: (t0 = " << source.time_slices[t0].t0 <<
	    " , ord = " << ord << ") = " << 
	    source.time_slices[t0].dilutions(0,0).mom_projs[0].op[0]
	    << endl; 
	    }
	  */
	  snoop.reset();
	  snoop.start();

	  //Write Source Op

	  QDPIO::cout<<" Writing Source Op"<<endl;

	  XMLBufferWriter file_xml; 

	  //Put some stuff in the file xml; 
	  push(file_xml , "SourceGroupMesonOperator");
	  write(file_xml, "OpInfo", final_ops[l]);
	  write(file_xml, "Config_info", source.configInfo);
	  pop(file_xml);

	  BinaryBufferWriter source_bin; 

	  std::stringstream fstream; 
					
	  fstream << input.output_info.cfg_paths[i].src_path << final_ops[l].name << "_t"
		  << source.time_slices[0].t0 << "_src.lime" ;

	  std::string filename = fstream.str();

	  QDPIO::cout<<"Source Filename = "<<filename<<endl;

	  QDPFileWriter srcout(file_xml, filename, QDPIO_SINGLEFILE, QDPIO_SERIAL, QDPIO_OPEN);

	  XMLBufferWriter source_xml;

	  //put some stuff in the source xml
	  push(source_xml,  "CreationOperator");
	  write(source_xml, "OpInfo" , source);
	  pop(source_xml);

	  write(source_bin, source);
	  write(srcout, source_xml, source_bin);


	  snoop.stop();

	  QDPIO::cout<<"Source Op Written : time = "<< snoop.getTimeInSeconds() << "sec"<<endl;

	}//Source


	//Make Sink 
	{	
	  MesonOperator_t sink;
	  sink.id = final_ops[l].name; 

	  QDPIO::cout<< "Making Sink Meson Op: " << sink.id << endl;

	  int Nterms = final_ops[l].term.size();

	  bool init = false;

	  for (int m = 0 ; m < Nterms ; ++m)
	  {
	    ElementalOpKey_t elOpKey;

	    elOpKey.op = final_ops[l].term[m].op;

	    const MesonOperator_t & elem_sink = 
	      el_op_maps.getSinkOp(elOpKey, i, t0);

	    if (!init)
	    {
	      QDPIO::cout<<"init. group meson op sink"<<endl;				
	      //Set all the headers, mom_projs, t_0's only once 
	      initOp(sink , elem_sink);

	      QDPIO::cout<<"Finished init ops"<<endl;
	      init = true;
	    }

	    QDPIO::cout<<"Adding elemental to group meson op"<<endl;

	    //add the elem op to the final op
	    addTo(sink, elem_sink, final_ops[l].term[m].coeff);

	  } //m

	  //Write Sink Op
	  QDPIO::cout<<" Writing Sink Op"<<endl;

	  XMLBufferWriter file_xml; 

	  //Put some stuff in the file xml; 
	  push(file_xml, "SinkGroupMesonOperator");
	  write(file_xml, "OpInfo" , final_ops[l]);
	  write(file_xml, "Config_info", sink.configInfo);
	  pop(file_xml);

	  BinaryBufferWriter sink_bin; 

			
	  std::stringstream fstream;
	  fstream << input.output_info.cfg_paths[i].snk_path << final_ops[l].name << "_t" << 
	    sink.time_slices[0].t0 << "_snk.lime";

	  std::string filename = fstream.str();

	  QDPIO::cout << "Sink Filename = " << filename << endl;

	  QDPFileWriter snkout(file_xml, filename, QDPIO_SINGLEFILE, QDPIO_SERIAL, QDPIO_OPEN);

	  XMLBufferWriter sink_xml;

	  //put some stuff in the sink xml
	  push(sink_xml, "AnnihilationOperator"); 
	  write(sink_xml, "OpInfo" , sink);
	  pop(sink_xml);

	  write(sink_bin, sink);

	  QDPIO::cout<< "Sink written to binary buffer" << endl;

	  write(snkout, sink_xml, sink_bin);

	  snoop.stop();
	  QDPIO::cout<<"Sink Op Written : time = "<< snoop.getTimeInSeconds() << "sec"<<endl;
	}//Sink
      } //t0
    } //l
  }//i		

  pop(xml_out); //MakeMesonOps 

  swatch.stop();

  QDPIO::cout << "MakeMesonOps ran sucessfully: total time = " << swatch.getTimeInSeconds() 
	      << "sec" << endl;


  // Clean up QDP++
  QDP_finalize();
  exit(0);  // Normal exit
}


