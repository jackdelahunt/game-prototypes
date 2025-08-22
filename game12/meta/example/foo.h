#pragma once

#include "meta.h"

meta enum BuildType {
	Client,
	Host,
	Server
};

bool not_server(BuildType build_type);

struct Asset {
	int id;
	char* path;
};

bool exists(Asset asset);
