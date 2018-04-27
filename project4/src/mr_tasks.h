#pragma once

#include <string>
#include <iostream>
#include <fstream>
// /#include <unordered_map>


using std::string;

/* CS6210_TASK Implement this data structureas per your implementation.
		You will need this when your worker is running the map task*/
struct BaseMapperInternal {
		/* DON'T change this function's signature */
		BaseMapperInternal();

		/* DON'T change this function's signature */
		void emit(const std::string& key, const std::string& val);

		/* NOW you can add below, data members and member functions as per the need of your implementation*/
		string intermediate_file;
};



/* CS6210_TASK Implement this function */
inline BaseMapperInternal::BaseMapperInternal() {
	//std::cout << "init base Mapper" << std::endl;
}

/* CS6210_TASK Implement this function */
inline void BaseMapperInternal::emit(const std::string& key, const std::string& val) {
	std::ofstream file { intermediate_file, std::ios_base::app };
	if (file.is_open()){
		file << key << " " << val << std::endl;
		file.close();
	}
}

/*-----------------------------------------------------------------------------------------------*/


/* CS6210_TASK Implement this data structureas per your implementation.
		You will need this when your worker is running the reduce task*/
struct BaseReducerInternal {

		/* DON'T change this function's signature */
		BaseReducerInternal();

		/* DON'T change this function's signature */
		void emit(const std::string& key, const std::string& val);
		string out_file;
};


/* CS6210_TASK Implement this function */
inline BaseReducerInternal::BaseReducerInternal() {

}


/* CS6210_TASK Implement this function */
inline void BaseReducerInternal::emit(const std::string& key, const std::string& val) {
	std::ofstream file { out_file, std::ios_base::app};
	if(out_file.find("dummy") < out_file.length() ) {
		file << std::endl;
		return;
	}

	if (file.is_open()){
		file << key << " " << val << std::endl;
		file.close();
	}
}
