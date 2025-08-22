#include "foo.h"

#include "meta_foo.h"

bool not_server(BuildType build_type) {
	return meta_name(build_type) != "Server";
}

bool exists(Asset asset) {
	return !(asset.path == nullptr);
}
