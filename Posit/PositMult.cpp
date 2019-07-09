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
#include "../ShiftersEtc/Shifters_signed.hpp"
#include "../IntMult/IntMultiplier.hpp"
#include "PositDecoder.hpp"

// include the header of the Operator
#include "PositMult.hpp"
using namespace std;

namespace flopoco {

	//extern vector<Operator *> oplist;


	PositMult::PositMult(Target* target, int N, int es, map<string, double> inputDelays) :
	Operator(target), N(N), es(es){
		/* constructor of the PositMult
		   Target is the targeted FPGA : Stratix, Virtex ... (see Target.hpp for more informations)
		   param0 and param1 are some parameters declared by this Operator developpers, 
		   any number can be declared, you will have to modify 
		   -> this function,  
		   -> the prototype of this function (available in PositMult.hpp)
		   ->  main.cpp to uncomment the RegisterFactory line
		*/

		// definition of the source file name, used for info and error reporting using REPORT 
		srcFileName="PositMult";

		// definition of the name of the operator
		ostringstream name;
		name << "PositMult_" << N << "_" << es ;
		setNameWithFreqAndUID(name.str());
		// Copyright 
		setCopyrightString("Raul Murillo, 2019");

		// the addition operators need the ieee_std_signed/unsigned libraries
		//useNumericStd();

		RegSize = intlog2(N);
		FracSize = N-es-2;


		/* SET UP THE IO SIGNALS */
		addInput  ( "InputA", N);
		addInput  ( "InputB", N);
		addOutput ( "Output", N);

//		addFullComment(" addFullComment for a large comment ");
//		addComment("addComment for small left-aligned comment");

		setCriticalPath( getMaxInputDelays(inputDelays) );

		/* Some piece of information can be delivered to the flopoco user if  the -verbose option is set
		   [eg: flopoco -verbose=0 PositMult 10 5 ]
		   , by using the REPORT function.
		   There is three level of details
		   -> INFO for basic information ( -verbose=1 )
		   -> DETAILED for complete information, includes the INFO level ( -verbose=2 )
		   -> DEBUG for complete and debug information, to be used for getting information 
		   during the debug step of operator development, includes the INFO and DETAILED levels ( -verbose=3 )
		*/
		// basic message
		REPORT(INFO,"Declaration of PositMult \n");

		// more detailed message
		REPORT(DETAILED, "this operator has received two parameters " << N << " and " << es);
  
		// debug message for developper
		REPORT(DEBUG,"debug of PositMult");

	//=========================================================================|
		addFullComment("Data Extraction");
	// ========================================================================|

		PositDecoder* decoderA = (PositDecoder*) newInstance("PositDecoder", "decoderA", "N=" + to_string(N) + " es=" + to_string(es), "Input=>InputA;Sign=>sign_A;Reg=>reg_A;Exp=>exp_A;Frac=>frac_A;z=>z_A;inf=>inf_A");
		PositDecoder* decoderB = (PositDecoder*) newInstance("PositDecoder", "decoderB", "N=" + to_string(N) + " es=" + to_string(es), "Input=>InputB;Sign=>sign_B;Reg=>reg_B;Exp=>exp_B;Frac=>frac_B;z=>z_B;inf=>inf_B");
		syncCycleFromSignal("reg_A");
		setCriticalPath(decoderA->getOutputDelay("Reg"));
		syncCycleFromSignal("reg_B");
		setCriticalPath(decoderB->getOutputDelay("Reg"));

		manageCriticalPath(target->localWireDelay() + target->lutDelay());

		addComment("Gather scale factors");
		vhdl << tab << declare("sf_A", RegSize+es) << " <= reg_A";
		if (es>0) vhdl << " & exp_A";
		vhdl << ";" << endl;
		vhdl << tab << declare("sf_B", RegSize+es) << " <= reg_B";
		if (es>0) vhdl << " & exp_B";
		vhdl << ";" << endl;
		
	//=========================================================================|
		addFullComment("Sign and Special Cases Computation");
	// ========================================================================|
	
		vhdl << tab << declare("sign") << " <= sign_A XOR sign_B;" << endl;
		vhdl << tab << declare("z") << " <= z_A OR z_B;" << endl;
		vhdl << tab << declare("inf") << " <= inf_A OR inf_B;" << endl;

	//=========================================================================|
		addFullComment("Multiply the fractions, add the exponent values");
	// ========================================================================|

		//int frac_size = getSignalByName("frac_A")->width();
		IntMultiplier* mult = (IntMultiplier*) newInstance("IntMultiplier", "mult", "wX=" + to_string(FracSize) + " wY=" + to_string(FracSize) + " wOut="+ to_string(0), "X=>frac_A;Y=>frac_B;R=>frac_mult");
		syncCycleFromSignal("frac_mult");
		setCriticalPath(mult->getOutputDelay("R"));

		manageCriticalPath(target->localWireDelay() + target->lutDelay());
		int mult_size = getSignalByName("frac_mult")->width();

		addComment("Adjust for overflow");
		vhdl << tab << declare("ovf_m") << " <= frac_mult(frac_mult'high);" << endl;

		vhdl << tab << declare("normFrac", mult_size+1) << " <= frac_mult & '0' when ovf_m = '0' else"
													<< " '0' & frac_mult;" << endl; // Equivalent to shift right ovf_m bits
		vhdl << tab << declare("sf_mult", RegSize+es+1) << " <= (sf_A(sf_A'high) & sf_A) + (sf_B(sf_B'high) & sf_B) + ovf_m;" << endl;

		vhdl << tab << declare("sf_sign") << " <= sf_mult(sf_mult'high);" << endl;

		manageCriticalPath(target->localWireDelay() + target->lutDelay());

	//=========================================================================|
		addFullComment("Compute Regime and Exponent value");
	// ========================================================================|

		vhdl << tab << declare("nzero") << " <= '0' when frac_mult = 0 else '1';" << endl;

		addComment("Unpack scaling factors");
		if (es>0)
			vhdl << tab << declare("ExpBits", es) << " <= sf_mult" << range(es-1,0) << ";" << endl;
		vhdl << tab << declare("RegimeAns_tmp", RegSize) << " <= sf_mult" << range(RegSize+es-1,es) << ";" << endl;
		nextCycle();
		addComment("Get Regime's absolute value");
		vhdl << tab << declare("RegimeAns", RegSize) << " <= (NOT RegimeAns_tmp)+1 when sf_sign = '1' else RegimeAns_tmp;" << endl;

		addComment("Check for Regime overflow");
		vhdl << tab << declare("ovf_reg") << " <= RegimeAns(RegimeAns'high);" << endl;
		vhdl << tab << declare("FinalRegime", RegSize) << " <= '0' & " << og(RegSize-1) << " when ovf_reg = '1' else "
														<< "RegimeAns;" << endl;

		nextCycle();											
		vhdl << tab << declare("ovf_regF") << " <= '1' when FinalRegime = ('0' & " << og(RegSize-1) << ") else '0';" << endl;
		nextCycle();
		if (es>0){
			vhdl << tab << declare("FinalExp", es) << " <= " << zg(es) << " when ((ovf_reg = '1') OR (ovf_regF = '1') OR (nzero='0')) else "
														<< "ExpBits;" << endl;
		}
		
	//=========================================================================|
		addFullComment("Packing Stage 1");
	// ========================================================================|

		vhdl << tab << declare("tmp1", 2+es+mult_size-1) << " <= nzero & '0' ";
		if (es>0)
			vhdl << "& FinalExp ";
		vhdl << "& normFrac" << range(mult_size-2,0) << ";" << endl;
		vhdl << tab << declare("tmp2", 2+es+mult_size-1) << " <= '0' & nzero ";
		if (es>0)
			vhdl << "& FinalExp ";
		vhdl << "& normFrac" << range(mult_size-2,0) << ";" << endl;
		
		vhdl << tab << declare("shift_neg", RegSize) << " <= FinalRegime - 2 when (ovf_regF = '1') else"
													 << " FinalRegime - 1;" << endl;
 		vhdl << tab << declare("shift_pos", RegSize) << " <= FinalRegime - 1 when (ovf_regF = '1') else"
													 << " FinalRegime;" << endl;

		vhdl << tab << declare("shifter_in", 2+es+mult_size-1) << " <= tmp2 when sf_sign = '1' else"
																<< " tmp1;" << endl;
		vhdl << tab << declare("shifter_S", RegSize) << " <= shift_neg when sf_sign = '1' else"
													<< " shift_pos;" << endl;										 
		Shifter_signed* rightShifter = (Shifter_signed*) newInstance("Shifter_signed", "RightShifterComponent", "wIn=" + to_string(2+es+mult_size-1) + " maxShift=" + to_string(N) + " dir=1", "X=>shifter_in;S=>shifter_S;R=>shifter_out");
		syncCycleFromSignal("shifter_out");
		setCriticalPath(rightShifter->getOutputDelay("R"));											 

		manageCriticalPath(target->localWireDelay() + target->lutDelay());
		int shift_size = getSignalByName("shifter_out")->width();
		vhdl << tab << declare("tmp_ans", N-1) << " <= shifter_out" << range(shift_size-1, shift_size-(N-1)) << ";" << endl;

	//=========================================================================|
		addFullComment("Packing Stage 2 - Unbiased Rounding");
	// ========================================================================|
		// Rounding implementation using L,G,R,S bits
		vhdl << tab << declare("LSB") << " <= shifter_out" << of(shift_size-(N-1)) << ";" << endl;
		vhdl << tab << declare("G") << " <= shifter_out" << of(shift_size-(N-1)-1) << ";" << endl;
		vhdl << tab << declare("R") << " <= shifter_out" << of(shift_size-(N-1)-2) << ";" << endl;
		vhdl << tab << declare("S") << " <= '0' when shifter_out" << range(shift_size-(N-1)-3, 0) << " = 0 else '1';" << endl;

		vhdl << tab << declare("round") << " <= G AND (LSB OR R OR S) when NOT((ovf_reg OR ovf_regF) = '1') else '0';" << endl;

		vhdl << tab << "Output <= '1' & " << zg(N-1) << " when inf = '1' else "
							<< zg(N) << " when z = '1' else"
							<< " '0' & (tmp_ans + round) when sign = '0' else"
							<< " NOT('0' & (tmp_ans + round))+1;" << endl;

	};

	
	void PositMult::emulate(TestCase * tc) {
		// get the inputs from the TestCase
		mpz_class svX = tc->getInputValue ( "InputA" );
		// complete the TestCase with this expected output
		tc->addExpectedOutput ( "Output", svX );

	}


//	void PositMult::buildStandardTestCases(TestCaseList * tcl) {
		// please fill me with regression tests or corner case tests!
//	}



	OperatorPtr PositMult::parseArguments(Target *target, vector<string> &args) {
		int N;
		UserInterface::parseStrictlyPositiveInt(args, "N", &N);
		int es;
		UserInterface::parsePositiveInt(args, "es", &es);
		return new PositMult(target, N, es);
		
	}

	
	void PositMult::registerFactory(){
		UserInterface::add("PositMult", // name
											 "A posit decoder with a single architecture.", // description, string
											 "Posit", // category, from the list defined in UserInterface.cpp
											 "", //seeAlso
											 // Now comes the parameter description string.
											 // Respect its syntax because it will be used to generate the parser and the docs
											 // Syntax is: a semicolon-separated list of parameterDescription;
											 // where parameterDescription is parameterName (parameterType)[=defaultValue]: parameterDescriptionString 
											 "N(int): A first parameter, here used as the input size; \
                        					 es(int): A second parameter, here used as the exponent size of the Posit;",
											 // More documentation for the HTML pages. If you want to link to your blog, it is here.
											 "Feel free to experiment with its code, it will not break anything in FloPoCo. <br> Also see the developper manual in the doc/ directory of FloPoCo.",
											 PositMult::parseArguments
											 ) ;
	}

}//namespace
