#include <dpp/dpp.h>

#include <filesystem>
#include <fstream>
#include <format>

#include "mimiron.h"

#include <boost/pfr.hpp>

#include "tools/string_literal.h"
#include "database/query.h"

struct test {
	int foo;
	uint32_t bar;
};

namespace stdfs = std::filesystem;

int main(int ac, char *av[])
{
	using namespace mimiron;

	return ::mimiron::mimiron{std::span{av, static_cast<size_t>(ac)}}.run();
}