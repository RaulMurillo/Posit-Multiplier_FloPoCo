/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <string.h>
#include <gmp.h>
#include <mpfr.h>
#include <stdio.h>

#include "utils.hpp"
#include "Operator.hpp"
#include "../ShiftersEtc/LZOC.hpp"
#include "../ShiftersEtc/Shifters.hpp"

// include the header of the Operator
#include "PositDecoder.hpp"
using namespace std;

namespace flopoco {

	//extern vector<Operator *> oplist;


	PositDecoder::PositDecoder(Target* target, int N, int es, map<string, double> inputDelays) :
	Operator(target), N(N), es(es) {
		/* constructor of the PositDecoder
		   Target is the targeted FPGA : Stratix, Virtex ... (see Target.hpp for more informations)
		   param0 and param1 are some parameters declared by this Operator developpers, 
		   any number can be declared, you will have to modify 
		   -> this function,  
		   -> the prototype of this function (available in PositDecoder.hpp)
		   ->  main.cpp to uncomment the RegisterFactory line
		*/

		// definition of the source file name, used for info and error reporting using REPORT 
		srcFileName="PositDecoder";

		// definition of the name of the operator
		ostringstream name;
		name << "PositDecoder_" << N << "_" << es;
		setNameWithFreqAndUID(name.str());
		// Copyright 
		setCopyrightString("Raul Murillo, 2019");

		// the addition operators need the ieee_std_signed/unsigned libraries
		//useNumericStd();

		sizeRegime = intlog2(N);
		sizeFraction = N-es-2;

		/* SET UP THE IO SIGNALS */
		addInput  ( "Input"	, N);
		addOutput ( "Sign"	);
		addOutput ( "Reg"	, sizeRegime);
		if(es>0){
			addOutput ( "Exp"	, es);
		}
		else{
			addOutput ( "Exp", 1);
		}
		addOutput ( "Frac"  , sizeFraction);
		addOutput ( "z" );
		addOutput ( "inf" );

//		addFullComment(" addFullComment for a large comment ");
//		addComment("addComment for small left-aligned comment");

		setCriticalPath( getMaxInputDelays(inputDelays) );

		/* Some piece of information can be delivered to the flopoco user if  the -verbose option is set
		   [eg: flopoco -verbose=0 PositDecoder 10 5 ]
		   , by using the REPORT function.
		   There is three level of details
		   -> INFO for basic information ( -verbose=1 )
		   -> DETAILED for complete information, includes the INFO level ( -verbose=2 )
		   -> DEBUG for complete and debug information, to be used for getting information 
		   during the debug step of operator development, includes the INFO and DETAILED levels ( -verbose=3 )
		*/
		// basic message
		REPORT(INFO,"Declaration of PositDecoder \n");

		// more detailed message
		REPORT(DETAILED, "this operator has received two parameters " << N << " and " << es);
  
		// debug message for developper
		REPORT(DEBUG,"debug of PositDecoder");

	//=========================================================================|
		addFullComment("Special Cases");
	// ========================================================================|
		
		manageCriticalPath(target->localWireDelay() + target->lutDelay());
		vhdl << tab << declare("nzero") << " <= '0' when Input" << range(N-2, 0) <<" = 0 else '1';" << endl;
		manageCriticalPath(target->localWireDelay() + target->lutDelay());
		addComment("1 if Input is zero");
		vhdl << tab << "z <= Input" << of(N-1) << " NOR nzero;" << endl;		 // 1 if Input is zero
		addComment("1 if Input is infinity");
		vhdl << tab << "inf <= Input" << of(N-1) << " AND (NOT nzero);" << endl; // 1 if Input is infinity
		
	//=========================================================================|
		addFullComment("Extract Sign bit");
	// ========================================================================|
		vhdl << tab << declare("my_sign") << " <= Input" << of(N-1) << ";" << endl;
		vhdl << tab << "Sign <= my_sign;" << endl;

	//=========================================================================|
		addFullComment("2's Complement of Input");
	// ========================================================================|
		manageCriticalPath(target->localWireDelay() + target->lutDelay());
		vhdl << tab << declare("rep_sign", N-1) << " <= (others => my_sign);" << endl;
		vhdl << tab << declare("twos", N-1) << " <= (rep_sign XOR Input" << range(N-2,0) << ") + my_sign;" << endl;
		manageCriticalPath(target->localWireDelay() + target->lutDelay());
		vhdl << tab << declare("rc") << " <= twos" << of(N-2) << ";" << endl;	// Regime check

	//=========================================================================|
		addFullComment("Count leading zeros of regime");
	// ========================================================================|
		// count the sequence of 0 bits terminating in a 1 bit - regime
		// zc ← Leading Zero Detector (inv)
		// we use the FloPoCo pipelined operator LZOC

		vhdl << tab << declare("rep_rc", N-1) << " <= (others => rc);" << endl;
		addComment("Invert 2's");
		vhdl << tab << declare("inv", N-1) << " <= rep_rc XOR twos;" << endl;
	
		vhdl << tab << declare("zero_var") << " <= '0';" << endl;
		LZOC* lzc = (LZOC*) newInstance("LZOC", "LZOC_Component", "wIn=" + to_string(N-1), "I=>inv;OZB=>zero_var;O=>zc");
		syncCycleFromSignal("zc");
		setCriticalPath(lzc->getOutputDelay("O"));

	//=========================================================================|
		addFullComment("Shift out the regime");
	// ========================================================================|
		int zc_size = getSignalByName("zc")->width();
		vhdl << tab << declare("zc_sub", zc_size) << " <= zc - 1;" << endl;
		Shifter* leftShifter = (Shifter*) newInstance("Shifter", "LeftShifterComponent", "wIn=" + to_string(N-1) + " maxShift=" + to_string(N-1) + " dir=0", "X=>twos;S=>zc_sub;R=>shifted_twos");
		syncCycleFromSignal("shifted_twos");
		setCriticalPath(leftShifter->getOutputDelay("R"));

		manageCriticalPath(target->localWireDelay() + target->lutDelay());
		vhdl << tab << declare("tmp", N-3) << " <= shifted_twos" << range(N-4, 0) << ";"<<endl;

	//=========================================================================|
		addFullComment("Extract fraction and exponent");
	// ========================================================================|
		manageCriticalPath(target->localWireDelay() + target->lutDelay());
		vhdl << tab << "Frac <= nzero & tmp" << range(N-es-4,0) << ";" << endl;
		if(es>0){
			vhdl << tab << "Exp <= tmp" << range(N-4,N-es-3) << ";" << endl;
		}
		else{
			vhdl << tab << "Exp <= \"0\";" << endl;
		}
		 

	//=========================================================================|
		addFullComment("Select regime");
	// ========================================================================|
		vhdl << tab << "Reg <= '0' & zc_sub when rc = '1' else "
					<< "NOT('0' & zc) + 1;" << endl; //-zc

		//update output slack
		outDelayMap["Reg"] = getCriticalPath();

	};

	
/*	void PositDecoder::emulate(TestCase * tc) {
	}
*/

//	void PositDecoder::buildStandardTestCases(TestCaseList * tcl) {
		// please fill me with regression tests or corner case tests!
//	}





	OperatorPtr PositDecoder::parseArguments(Target *target, vector<string> &args) {
		int N;
		UserInterface::parseStrictlyPositiveInt(args, "N", &N);
		int es;
		UserInterface::parsePositiveInt(args, "es", &es);
		return new PositDecoder(target, N, es);
		
	}
	
	void PositDecoder::registerFactory(){
		UserInterface::add("PositDecoder", // name
											 "A posit decoder with a single architecture.", // description, string
											 "Posit", // category, from the list defined in UserInterface.cpp
											 "", //seeAlso
											 // Now comes the parameter description string.
											 // Respect its syntax because it will be used to generate the parser and the docs
											 // Syntax is: a semicolon-separated list of parameterDescription;
											 // where parameterDescription is parameterName (parameterType)[=defaultValue]: parameterDescriptionString 
											 "N(int)=8: A first parameter, here used as the input size; \
                        					 es(int): A second parameter, here used as the exponent size",
											 // More documentation for the HTML pages. If you want to link to your blog, it is here.
											 "Feel free to experiment with its code, it will not break anything in FloPoCo. <br> Also see the developper manual in the doc/ directory of FloPoCo.",
											 PositDecoder::parseArguments
											 ) ;
	}

}//namespace
