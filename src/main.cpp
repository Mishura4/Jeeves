#include <dpp/dpp.h>

#include <filesystem>
#include <fstream>
#include <format>

#include "mimiron.h"

namespace stdfs = std::filesystem;

int main(int ac, char *av[])
{
	using mimiron::mimiron;

	return mimiron{std::span{av, static_cast<size_t>(ac)}}.run();
}