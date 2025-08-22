#include "foo.h"
#include "meta_foo.h"

#include <iostream>

int main() {
	BuildType build_type = Server;

	std::cout << "Count of BuildType: " << meta_count<BuildType>() << "\n";
	std::cout << meta_name(build_type) << "\n";
	std::cout << meta_value<BuildType>("Server") << "\n";

	return 0;
}