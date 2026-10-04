#pragma once

#include "ft_core.hpp"

/** Install handles, wait for enumeration, run every test, uninstall. Emits results via r. */
void ft_run_all(ft::Reporter &r);

/** Number of dongles that were present when the suite ran (for the summary line). */
int ft_devices_present(void);
