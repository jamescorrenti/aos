#pragma once

#include <string>
#include <iostream>
#include <fstream>
// /#include <unordered_map>


using std::string;

/* CS6210_TASK Implement this data structureas per your implementation.
		You will need this when your worker is running the map task*/
struct BaseMapperInternal {
		friend class Worker;
		/* DON'T change this function's signature */
		BaseMapperInternal();

		/* DON'T change this function's signature */
		void emit(const std::string& key, const std::string& val);

		/* NOW you can add below, data members and member functions as per the need of your implementation*/
		string intermediate_file;
		//std::unordered_map<string, string> hashtable;

		friend void test(){
			std::cout << "This is a test" << std::endl;
			//return "???";
		}
};



/* CS6210_TASK Implement this function */
inline BaseMapperInternal::BaseMapperInternal() {
	std::cout << "init base Mapper" << std::endl;
}

/* CS6210_TASK Implement this function */
inline void BaseMapperInternal::emit(const std::string& key, const std::string& val) {
	string out = "output/int.txt";

	std::ofstream file { out, std::ios_base::app };
	if (file.is_open()){
		file << key << ", " << val << std::endl;
		file.close();
	}
	
	std::cout << "int_file" << out << "emit by BaseMapperInternal: " << key << ", " << val << std::endl;
}

/*-----------------------------------------------------------------------------------------------*/


/* CS6210_TASK Implement this data structureas per your implementation.
		You will need this when your worker is running the reduce task*/
struct BaseReducerInternal {

		/* DON'T change this function's signature */
		BaseReducerInternal();

		/* DON'T change this function's signature */
		void emit(const std::string& key, const std::string& val);

		/* NOW you can add below, data members and member functions as per the need of your implementation*/
};


/* CS6210_TASK Implement this function */
inline BaseReducerInternal::BaseReducerInternal() {

}


/* CS6210_TASK Implement this function */
inline void BaseReducerInternal::emit(const std::string& key, const std::string& val) {
	//std::cout << "Emit by BaseReducerInternal: " << key << ", " << val << std::endl;
}
