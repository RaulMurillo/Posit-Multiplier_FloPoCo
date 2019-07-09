/*
  Posit adder.

  Author : Raul Murillo Montero

  Initial software.
  Copyright © 
  2019.
  All rights reserved.

*/
#ifndef POSIT_MULT_HPP
#define POSIT_MULT_HPP

#include <vector>
#include <sstream>
#include <gmp.h>
#include <gmpxx.h>

#include "Operator.hpp"
#include "../Target.hpp"


/*  All flopoco operators and utility functions are declared within
    the flopoco namespace.
    You have to use flopoco:: or using namespace flopoco in order to access these
    functions.
*/

namespace flopoco {

	// new operator class declaration
	class PositMult : public Operator {
	private:
		/* operatorInfo is a user defined parameter (not a part of Operator class) for
		   stocking information about the operator. The user is able to defined any number of parameter in this class, as soon as it does not affect Operator parameters undeliberatly*/
		/** The total width of the posits */
		int N;
		/** The width of the exponent */ 
		int es;

		int RegSize;
		int FracSize;


	public:
		// definition of some function for the operator    

		/** The constructor 
		    * @param N The size of the inputs.
		    * @param es The width of the exponent.
		    */
		PositMult(Target* target,int N = 8, int es = 0, map<string, double> inputDelays = emptyDelayMap);

		// destructor
		~PositMult() {};


		// Below all the functions needed to test the operator
		/* the emulate function is used to simulate in software the operator
		   in order to compare this result with those outputed by the vhdl opertator */
		void emulate(TestCase * tc);

		/* function used to create Standard testCase defined by the developper */
//		void buildStandardTestCases(TestCaseList* tcl);


		/* function used to bias the (uniform by default) random test generator
		   See FPExp.cpp for an example */
//		TestCase* buildRandomTestCase(int i);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(Target *target , vector<string> &args);
		
		/** Factory register method */ 
		static void registerFactory();


	};


}//namespace


#endif